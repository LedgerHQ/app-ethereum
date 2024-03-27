from typing import Optional, Tuple
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
                         path,
                         suffix):
    app_client = EthAppClient(backend)

    with pytest.raises(ExceptionRAPDU) as e:
        with app_client.get_public_addr(bip32_path=path):
            navigator.navigate_and_compare(ROOT_SNAPSHOT_PATH,
                                           f"get_pk_rejected_{suffix}",
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


def test_get_pk2(firmware: Firmware,
                 backend: BackendInterface,
                 navigator: Navigator):
    app_client = EthAppClient(backend)

    path="m/44'/700'/1'/0/0"
    with app_client.get_public_addr(bip32_path=path, chaincode=True):
        navigator.navigate_and_compare(ROOT_SNAPSHOT_PATH,
                                       "get_pk_700",
                                       get_moves(firmware))
    pk, _, chaincode = ResponseParser.pk_addr(app_client.response().data, True)
    ref_pk, ref_chaincode = calculate_public_key_and_chaincode(curve=CurveChoice.Secp256k1,
                                                               path=path)
    assert pk.hex() == ref_pk
    assert chaincode.hex() == ref_chaincode


def test_get_public_key(backend: BackendInterface):
    app_client = EthAppClient(backend)

    with app_client.get_public_addr(bip32_path="m/44'/60'/1'/0/0", display=False, chaincode=True):
        pass

    response = app_client.response()
    assert response.status == 0x9000

    # response = pub_key_len (1)
    #            pub_key (var)
    #            eth_addr_len (1)
    #            eth_addr (var)
    #            chain_code (var)
    buffer, pub_key_len, pub_key = _pop_size_prefixed_buf_from_buf(response.data)
    buffer, eth_addr_len, eth_addr = _pop_size_prefixed_buf_from_buf(buffer)
    buffer, chain_code = _pop_sized_buf_from_buffer(buffer, 32)

    assert len(response.data) == 2 + pub_key_len + eth_addr_len + 32
    assert pub_key == b'\x04\xea\x02&\x91\xc7\x87\x00\xd2\xc3\xa0\xc7E\xbe\xa4\xf2\xb8\xe5\xe3\x13\x97j\x10B\xf6\xa1Vc\\\xb2\x05\xda\x1a\xcb\xfe\x04*\nZ\x89eyn6"E\x89\x0eT\xbd-\xbex\xec\x1e\x18df\xf2\xe9\xd0\xf5\xd5\xd8\xdf'
    assert eth_addr == b'463e4e114AA57F54f2Fd2C3ec03572C6f75d84C2'
    assert chain_code == b'\xaf\x89\xcd)\xea${8I\xec\xc80\xc2\xc8\x94\\e1\xd6P\x87\x07?\x9f\xd09\x00\xa0\xea\xa7\x96\xc8'


def _pop_sized_buf_from_buffer(buffer: bytes, size: int) -> Tuple[bytes, bytes]:
    """Parse buffer and returns: remainder, data[size]"""

    return buffer[size:], buffer[0:size]


def _pop_size_prefixed_buf_from_buf(buffer:bytes) -> Tuple[bytes, int, bytes]:
    """ Parse buffer and returns: remainder, data_len, data """

    data_len = buffer[0]
    return buffer[1+data_len:], data_len, buffer[1:data_len+1]
