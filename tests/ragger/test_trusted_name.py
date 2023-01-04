import pytest
from app.client import EthereumClient, SettingType
import struct
from keys import key_sign, Key

def compute_sig(challenge: int,
                addr: bytes,
                name: str,
                chain_id: int,
                key_id: int,
                algo_id: int) -> bytes:
    to_sign = bytearray()
    to_sign += struct.pack(">I", challenge)
    to_sign += addr
    to_sign += name.encode()
    to_sign += struct.pack(">Q", chain_id)
    to_sign.append(key_id)
    to_sign.append(algo_id)
    return key_sign(Key.TRUSTED_NAME, to_sign)

# Values used across all tests
CHAIN_ID = 1
NAME = "ledger.eth"
LONG_NAME = "ledger-LONG" + "1234567890"*24 + ".eth"
ADDR = bytes.fromhex("0011223344556677889900112233445566778899")
KEY_ID = 1
ALGO_ID = 1
BIP32_PATH = [ 44, 60, 0, None, None ]
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

    sig = compute_sig(challenge, ADDR, NAME, CHAIN_ID, KEY_ID, ALGO_ID)

    app_client.provide_trusted_name(NAME, KEY_ID, ALGO_ID, sig)

    app_client.send_fund(BIP32_PATH,
                         NONCE,
                         GAS_PRICE,
                         GAS_LIMIT,
                         ADDR,
                         AMOUNT,
                         CHAIN_ID,
                         "trusted_name_" + str(verbose))

def test_send_fund_wrong_challenge(app_client: EthereumClient):
    challenge = common(app_client)

    sig = compute_sig(~challenge & 0xffffffff, ADDR, NAME, CHAIN_ID, KEY_ID, ALGO_ID)

    app_client.provide_trusted_name(NAME, KEY_ID, ALGO_ID, sig)

    app_client.send_fund(BIP32_PATH,
                         NONCE,
                         GAS_PRICE,
                         GAS_LIMIT,
                         ADDR,
                         AMOUNT,
                         CHAIN_ID,
                         "trusted_name_wrong_challenge")

def test_send_fund_wrong_addr(app_client: EthereumClient):
    challenge = common(app_client)

    sig = compute_sig(challenge, ADDR, NAME, CHAIN_ID, KEY_ID, ALGO_ID)

    app_client.provide_trusted_name(NAME, KEY_ID, ALGO_ID, sig)

    addr = bytearray(ADDR)
    addr.reverse()

    app_client.send_fund(BIP32_PATH,
                         NONCE,
                         GAS_PRICE,
                         GAS_LIMIT,
                         addr,
                         AMOUNT,
                         CHAIN_ID,
                         "trusted_name_wrong_addr")

def test_send_fund_wrong_chainid(app_client: EthereumClient):
    challenge = common(app_client)

    sig = compute_sig(challenge, ADDR, NAME, CHAIN_ID, KEY_ID, ALGO_ID)

    app_client.provide_trusted_name(NAME, KEY_ID, ALGO_ID, sig)

    app_client.send_fund(BIP32_PATH,
                         NONCE,
                         GAS_PRICE,
                         GAS_LIMIT,
                         ADDR,
                         AMOUNT,
                         5,
                         "trusted_name_wrong_chainid")

def test_send_fund_very_long_trusted_name(app_client: EthereumClient):
    challenge = common(app_client)

    sig = compute_sig(challenge, ADDR, LONG_NAME, CHAIN_ID, KEY_ID, ALGO_ID)

    app_client.provide_trusted_name(LONG_NAME, KEY_ID, ALGO_ID, sig)

    app_client.send_fund(BIP32_PATH,
                         NONCE,
                         GAS_PRICE,
                         GAS_LIMIT,
                         ADDR,
                         AMOUNT,
                         CHAIN_ID,
                         "trusted_name_long_name")
