from typing import Optional
import pytest

from ledger_app_clients.ethereum.client import EthAppClient, StatusWord
import ledger_app_clients.ethereum.response_parser as ResponseParser

from ragger.error import ExceptionRAPDU
from ragger.firmware import Firmware
from ragger.backend import BackendInterface
from ragger.navigator import Navigator, NavInsID
from ragger.bip import calculate_public_key_and_chaincode, CurveChoice

from constants import ROOT_SNAPSHOT_PATH


@pytest.fixture(name="with_chaincode", params=[True, False])
def with_chaincode_fixture(request) -> bool:
    return request.param


@pytest.fixture(name="chain", params=[None, 1, 2, 5, 137])
def chain_fixture(request) -> Optional[int]:
    return request.param


def get_moves(firmware: Firmware,
              chain: Optional[int] = None,
              reject: bool = False):
    moves = []

    if firmware.is_nano:
        moves += [NavInsID.RIGHT_CLICK]
        if firmware.device == "nanos":
            moves += [NavInsID.RIGHT_CLICK] * 3
        else:
            moves += [NavInsID.RIGHT_CLICK]
        if reject:
            moves += [NavInsID.RIGHT_CLICK]
        moves += [NavInsID.BOTH_CLICK]
    else:
        moves += [NavInsID.USE_CASE_REVIEW_TAP]
        if chain is not None and chain > 1:
            moves += [NavInsID.USE_CASE_ADDRESS_CONFIRMATION_TAP]
        if reject:
            moves += [NavInsID.USE_CASE_ADDRESS_CONFIRMATION_CANCEL]
        else:
            moves += [NavInsID.USE_CASE_ADDRESS_CONFIRMATION_CONFIRM]

    return moves


def test_get_pk_rejected(firmware: Firmware,
                         backend: BackendInterface,
                         navigator: Navigator):
    app_client = EthAppClient(backend)

    with pytest.raises(ExceptionRAPDU) as e:
        with app_client.get_public_addr():
            navigator.navigate_and_compare(ROOT_SNAPSHOT_PATH,
                                           "get_pk_rejected",
                                           get_moves(firmware, reject=True))
    assert e.value.status == StatusWord.CONDITION_NOT_SATISFIED


def test_get_pk(firmware: Firmware,
                backend: BackendInterface,
                navigator: Navigator,
                with_chaincode: bool,
                chain: Optional[int]):
    app_client = EthAppClient(backend)

    with app_client.get_public_addr(chaincode=with_chaincode, chain_id=chain):
        navigator.navigate_and_compare(ROOT_SNAPSHOT_PATH,
                                       f"get_pk_{chain}",
                                       get_moves(firmware, chain=chain))
    pk, _, chaincode = ResponseParser.pk_addr(app_client.response().data, with_chaincode)
    ref_pk, ref_chaincode = calculate_public_key_and_chaincode(curve=CurveChoice.Secp256k1,
                                                               path="m/44'/60'/0'/0/0")
    assert pk.hex() == ref_pk
    if with_chaincode:
        assert chaincode.hex() == ref_chaincode
