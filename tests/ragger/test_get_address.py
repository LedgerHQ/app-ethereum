from pathlib import Path
from typing import Optional
import pytest

from py_ecc.bls import G2ProofOfPossession as bls

from staking_deposit.key_handling.key_derivation.path import mnemonic_and_path_to_key

from ragger.bip.seed import SPECULOS_MNEMONIC
from ragger.error import ExceptionRAPDU
from ragger.firmware import Firmware
from ragger.backend import BackendInterface
from ragger.navigator import Navigator, NavInsID
from ragger.bip import calculate_public_key_and_chaincode, CurveChoice

from client.client import EthAppClient, StatusWord
import client.response_parser as ResponseParser


@pytest.fixture(name="with_chaincode", params=[True, False])
def with_chaincode_fixture(request) -> bool:
    return request.param


@pytest.fixture(name="chain", params=[None, 1, 2, 5, 137])
def chain_fixture(request) -> Optional[int]:
    return request.param


def get_moves(firmware: Firmware,
              chain: Optional[int] = None,
              reject: bool = False,
              pk_eth2: bool = False):
    moves = []

    if firmware.is_nano:
        moves += [NavInsID.RIGHT_CLICK]
        if firmware.device == "nanos":
            moves += [NavInsID.RIGHT_CLICK] * 3
        else:
            moves += [NavInsID.RIGHT_CLICK]
        if reject:
            moves += [NavInsID.RIGHT_CLICK]
        if pk_eth2:
            if firmware.device == "nanos":
                moves += [NavInsID.RIGHT_CLICK] * 2
            moves += [NavInsID.RIGHT_CLICK]
        moves += [NavInsID.BOTH_CLICK]
    else:
        if not pk_eth2:
            moves += [NavInsID.USE_CASE_REVIEW_TAP]
        if chain is not None and chain > 1:
            moves += [NavInsID.USE_CASE_ADDRESS_CONFIRMATION_TAP]
        if reject:
            moves += [NavInsID.USE_CASE_ADDRESS_CONFIRMATION_CANCEL]
        else:
            moves += [NavInsID.USE_CASE_ADDRESS_CONFIRMATION_CONFIRM]

    return moves


@pytest.mark.parametrize(
    "path, suffix", 
    [
        ("m/44'/60'/0'/0/0", "60"),
        ("m/44'/700'/1'/0/0", "700")
    ],
)
def test_get_pk_rejected(firmware: Firmware,
                         backend: BackendInterface,
                         navigator: Navigator,
                         default_screenshot_path: Path,
                         path,
                         suffix):
    app_client = EthAppClient(backend)

    with pytest.raises(ExceptionRAPDU) as e:
        with app_client.get_public_addr(bip32_path=path):
            navigator.navigate_and_compare(default_screenshot_path,
                                           f"get_pk_rejected_{suffix}",
                                           get_moves(firmware, reject=True))
    assert e.value.status == StatusWord.CONDITION_NOT_SATISFIED


def test_get_pk(firmware: Firmware,
                backend: BackendInterface,
                navigator: Navigator,
                default_screenshot_path: Path,
                with_chaincode: bool,
                chain: Optional[int]):
    app_client = EthAppClient(backend)

    with app_client.get_public_addr(chaincode=with_chaincode, chain_id=chain):
        navigator.navigate_and_compare(default_screenshot_path,
                                       f"get_pk_{chain}",
                                       get_moves(firmware, chain=chain))
    pk, _, chaincode = ResponseParser.pk_addr(app_client.response().data, with_chaincode)
    ref_pk, ref_chaincode = calculate_public_key_and_chaincode(curve=CurveChoice.Secp256k1,
                                                               path="m/44'/60'/0'/0/0")
    assert pk.hex() == ref_pk
    if with_chaincode:
        assert chaincode.hex() == ref_chaincode


def test_get_eth2_pk(firmware: Firmware,
                     backend: BackendInterface,
                     navigator: Navigator,
                     test_name: str,
                     default_screenshot_path: Path):
    app_client = EthAppClient(backend)

    path="m/12381/3600/0/0"
    with app_client.get_eth2_public_addr(bip32_path=path):
        navigator.navigate_and_compare(default_screenshot_path,
                                       test_name,
                                       get_moves(firmware, pk_eth2=True))

    pk = app_client.response().data
    ref_pk = bls.SkToPk(mnemonic_and_path_to_key(SPECULOS_MNEMONIC, path))
    if firmware.name == "stax":
        pk = pk[1:49]

    assert pk == ref_pk
