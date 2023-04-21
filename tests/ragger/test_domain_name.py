import pytest
from ragger.error import ExceptionRAPDU
from app.client import EthereumClient, StatusWord
from app.setting import SettingType
import struct

# Values used across all tests
CHAIN_ID = 1
NAME = "ledger.eth"
ADDR = bytes.fromhex("0011223344556677889900112233445566778899")
KEY_ID = 1
ALGO_ID = 1
BIP32_PATH = "m/44'/60'/0'/0/0"
NONCE = 21
GAS_PRICE = 13000000000
GAS_LIMIT = 21000
AMOUNT = 1.22

@pytest.fixture(params=[False, True])
def verbose(request) -> bool:
    return request.param

def common(app_client: EthereumClient) -> int:
    if app_client._client.firmware.device == "nanos":
        pytest.skip("Not supported on LNS")
    return app_client.get_challenge()


def test_send_fund(app_client: EthereumClient, verbose: bool):
    challenge = common(app_client)

    if verbose:
        app_client.settings_set({
            SettingType.VERBOSE_ENS: True
        })

    app_client.provide_domain_name(challenge, NAME, ADDR)

    app_client.send_fund(BIP32_PATH,
                         NONCE,
                         GAS_PRICE,
                         GAS_LIMIT,
                         ADDR,
                         AMOUNT,
                         CHAIN_ID,
                         "domain_name_verbose_" + str(verbose))

def test_send_fund_wrong_challenge(app_client: EthereumClient):
    caught = False
    challenge = common(app_client)

    try:
        app_client.provide_domain_name(~challenge & 0xffffffff, NAME, ADDR)
    except ExceptionRAPDU as e:
        assert e.status == StatusWord.INVALID_DATA
    else:
        assert False # An exception should have been raised

def test_send_fund_wrong_addr(app_client: EthereumClient):
    challenge = common(app_client)

    app_client.provide_domain_name(challenge, NAME, ADDR)

    addr = bytearray(ADDR)
    addr.reverse()

    app_client.send_fund(BIP32_PATH,
                         NONCE,
                         GAS_PRICE,
                         GAS_LIMIT,
                         addr,
                         AMOUNT,
                         CHAIN_ID,
                         "domain_name_wrong_addr")

def test_send_fund_non_mainnet(app_client: EthereumClient):
    challenge = common(app_client)

    app_client.provide_domain_name(challenge, NAME, ADDR)

    app_client.send_fund(BIP32_PATH,
                         NONCE,
                         GAS_PRICE,
                         GAS_LIMIT,
                         ADDR,
                         AMOUNT,
                         5,
                         "domain_name_non_mainnet")

def test_send_fund_domain_too_long(app_client: EthereumClient):
    challenge = common(app_client)

    try:
        app_client.provide_domain_name(challenge, "ledger" + "0"*25 + ".eth", ADDR)
    except ExceptionRAPDU as e:
        assert e.status == StatusWord.INVALID_DATA
    else:
        assert False # An exception should have been raised

def test_send_fund_domain_invalid_character(app_client: EthereumClient):
    challenge = common(app_client)

    try:
        app_client.provide_domain_name(challenge, "l\xe8dger.eth", ADDR)
    except ExceptionRAPDU as e:
        assert e.status == StatusWord.INVALID_DATA
    else:
        assert False # An exception should have been raised

def test_send_fund_uppercase(app_client: EthereumClient):
    challenge = common(app_client)

    try:
        app_client.provide_domain_name(challenge, NAME.upper(), ADDR)
    except ExceptionRAPDU as e:
        assert e.status == StatusWord.INVALID_DATA
    else:
        assert False # An exception should have been raised

def test_send_fund_domain_non_ens(app_client: EthereumClient):
    challenge = common(app_client)

    try:
        app_client.provide_domain_name(challenge, "ledger.hte", ADDR)
    except ExceptionRAPDU as e:
        assert e.status == StatusWord.INVALID_DATA
    else:
        assert False # An exception should have been raised
