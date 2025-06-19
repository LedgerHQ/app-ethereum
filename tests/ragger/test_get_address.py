from typing import Optional
import pytest

from py_ecc.bls import G2ProofOfPossession as bls

from staking_deposit.key_handling.key_derivation.path import mnemonic_and_path_to_key

from ragger.bip.seed import SPECULOS_MNEMONIC
from ragger.error import ExceptionRAPDU
from ragger.navigator.navigation_scenario import NavigateWithScenario
from ragger.bip import calculate_public_key_and_chaincode, CurveChoice

from dynamic_networks_cfg import get_network_config

from client.client import EthAppClient
from client.status_word import StatusWord
import client.response_parser as ResponseParser
from client.dynamic_networks import DynamicNetwork


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
def test_get_pk_rejected(scenario_navigator: NavigateWithScenario,
                         test_name: str,
                         path: str,
                         suffix: str):
    backend = scenario_navigator.backend
    app_client = EthAppClient(backend)

    test_name += f"_{suffix}"
    with pytest.raises(ExceptionRAPDU) as e:
        with app_client.get_public_addr(bip32_path=path):
            scenario_navigator.address_review_reject(test_name=test_name)

    assert e.value.status == StatusWord.CONDITION_NOT_SATISFIED


def test_get_pk(scenario_navigator: NavigateWithScenario,
                test_name: str,
                with_chaincode: bool,
                chain: Optional[int]):
    backend = scenario_navigator.backend
    app_client = EthAppClient(backend)

    # Send Network information (name, ticker, icon)
    if chain is not None:
        name, ticker, icon = get_network_config(backend.device.type, chain)
        if name and ticker:
            app_client.provide_network_information(DynamicNetwork(name, ticker, chain, icon))

    test_name += f"_{chain}"
    with app_client.get_public_addr(chaincode=with_chaincode, chain_id=chain):
        scenario_navigator.address_review_approve(test_name=test_name)

    pk, _, chaincode = ResponseParser.pk_addr(app_client.response().data, with_chaincode)
    ref_pk, ref_chaincode = calculate_public_key_and_chaincode(curve=CurveChoice.Secp256k1,
                                                               path="m/44'/60'/0'/0/0")
    assert pk.hex() == ref_pk
    if with_chaincode:
        assert chaincode.hex() == ref_chaincode


def test_get_eth2_pk(scenario_navigator: NavigateWithScenario,
                     test_name: str):

    backend = scenario_navigator.backend
    app_client = EthAppClient(backend)

    path="m/12381/3600/0/0"
    with app_client.get_eth2_public_addr(bip32_path=path):
        scenario_navigator.address_review_approve(test_name=test_name)

    pk = app_client.response().data
    pk = pk[1:49]
    ref_pk = bls.SkToPk(mnemonic_and_path_to_key(SPECULOS_MNEMONIC, path))

    assert pk == ref_pk
