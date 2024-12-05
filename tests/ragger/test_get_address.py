from typing import Optional
import pytest

from py_ecc.bls import G2ProofOfPossession as bls

from staking_deposit.key_handling.key_derivation.path import mnemonic_and_path_to_key

from ragger.bip.seed import SPECULOS_MNEMONIC
from ragger.error import ExceptionRAPDU
from ragger.firmware import Firmware
from ragger.backend import BackendInterface
from ragger.navigator.navigation_scenario import NavigateWithScenario
from ragger.bip import calculate_public_key_and_chaincode, CurveChoice

from client.client import EthAppClient, StatusWord
import client.response_parser as ResponseParser


@pytest.fixture(name="with_chaincode", params=[True, False])
def with_chaincode_fixture(request) -> bool:
    return request.param


@pytest.fixture(name="chain", params=[None, 1, 2, 5, 137])
def chain_fixture(request) -> Optional[int]:
    return request.param


@pytest.mark.parametrize(
    "path, suffix",
    [
        ("m/44'/60'/0'/0/0", "60"),
        ("m/44'/700'/1'/0/0", "700")
    ],
)
def test_get_pk_rejected(backend: BackendInterface,
                         scenario_navigator: NavigateWithScenario,
                         path,
                         suffix):
    app_client = EthAppClient(backend)

    with pytest.raises(ExceptionRAPDU) as e:
        with app_client.get_public_addr(bip32_path=path):
            scenario_navigator.address_review_reject(test_name=f"get_pk_rejected_{suffix}")

    assert e.value.status == StatusWord.CONDITION_NOT_SATISFIED


def test_get_pk(backend: BackendInterface,
                scenario_navigator: NavigateWithScenario,
                with_chaincode: bool,
                chain: Optional[int]):
    app_client = EthAppClient(backend)

    if chain == 5:
        name = "Goerli"
        ticker = "ETH"
        # pylint: disable=line-too-long
        icon = "400040000195000093001f8b08000000000002ff85ceb10dc4300805d01fb970e9113c4a467346cb288ce092223ace80382b892ef9cd93ac0f0678881c4616d980b4bb99aa0801a5874d844ff695b5d7f6c23ad79058f79c8df7e8c5dc7d9fff13ffc61d71d7bcf32549bcef5672c5a430bb1cd6073f68c3cd3d302cd3feea88f547d6a99eb8e87ecbcdecd255b8f869033cfd932feae0c09000020000"
        # pylint: enable=line-too-long
    elif chain == 137:
        name = "Polygon"
        ticker = "POL"
        # pylint: disable=line-too-long
        icon = "400040002188010086011f8b08000000000002ffbd55416ac33010943018422f0e855c42fee01e4b09f94a7a0c3df82b3905440ffd4243a1507ae81b4c1e9047180a2604779d58f6ae7664e8250b0ec613ad7646b3aba6b949fc9a2eee8f105f7bdc14083e653d3e1f4d6f4c82f0ed809b3780e70c5f6ab8a6cf8f1b474175a41a2f8db1d7b47b7a3b2276c9506881d8cd87d7bb10afd8a2356048ec12bfe90130cc59d12d9585166f05ffdcb363295b863f21bb54662bfe85cb8ca522402bec2a92ad8d336936e9b50416e19a55e000f837d2d2a2e3f79a7d39f78aec938eb9bf549ae91378fa16a118ca982ea3febe46aa18ca90f59c14ce1c21fbd3c744e087cc582a921e7bf9552a4a761fb1461f39c5114f75f1b9732f5117499f484f813ec84386416f6f75a30b17689fd519ef3cd6f3b83114c300a7dd66b129a276d3a3a3d20209cdc09ce1fac039c50480738e09b8471dc116c18e1a18d6784eb7453bb773ee19cff9d29b37c3f744cdfc0dedc7ee0968dff7487fe97b0acf8bf3c3b48be236f772f307b129c97400080000"
        # pylint: enable=line-too-long
    else:
        name = ""

    if name:
        app_client.provide_network_information(name, ticker, chain, bytes.fromhex(icon))

    with app_client.get_public_addr(chaincode=with_chaincode, chain_id=chain):
        scenario_navigator.address_review_approve(test_name=f"get_pk_{chain}")

    pk, _, chaincode = ResponseParser.pk_addr(app_client.response().data, with_chaincode)
    ref_pk, ref_chaincode = calculate_public_key_and_chaincode(curve=CurveChoice.Secp256k1,
                                                               path="m/44'/60'/0'/0/0")
    assert pk.hex() == ref_pk
    if with_chaincode:
        assert chaincode.hex() == ref_chaincode


def test_get_eth2_pk(firmware: Firmware,
                     backend: BackendInterface,
                     scenario_navigator: NavigateWithScenario,
                     test_name: str):

    app_client = EthAppClient(backend)

    path="m/12381/3600/0/0"
    with app_client.get_eth2_public_addr(bip32_path=path):
        scenario_navigator.address_review_approve(test_name=test_name)

    pk = app_client.response().data
    ref_pk = bls.SkToPk(mnemonic_and_path_to_key(SPECULOS_MNEMONIC, path))
    if firmware in (Firmware.STAX, Firmware.FLEX):
        pk = pk[1:49]

    assert pk == ref_pk
