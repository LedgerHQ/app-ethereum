import client.response_parser as ResponseParser
import pytest
from client.client import EthAppClient
from client.status_word import StatusWord
from client.tx_auth_7702 import TxAuth7702
from ragger.bip import CurveChoice, calculate_public_key_and_chaincode
from ragger.error import ExceptionRAPDU
from ragger.navigator.navigation_scenario import NavigateWithScenario
from test_sign import common
from web3 import Web3

# Values used across all tests
ADDR = bytes.fromhex("5a321744667052affa8386ed49e00ef223cbffc3")
BIP32_PATH = "m/44'/1001'/0'/0/0"
NONCE = 68
GAS_PRICE = 13
GAS_LIMIT = 21000
VALUE = 0.31415
CHAIN_ID = 108
ETHEREUM_MAINNET_CHAIN_ID = 1
CHAIN_ID_ALL = 0
SIMPLE_7702_ACCOUNT = bytes.fromhex("4cd241e8d1510e30b2076397afc7508ae59c66c9")


# Transfer on Clone app
@pytest.mark.needs_setup("lib_mode")
def test_clone_thundercore_tx(scenario_navigator: NavigateWithScenario, test_name: str) -> None:
    tx_params: dict = {
        "nonce": NONCE,
        "gasPrice": Web3.to_wei(GAS_PRICE, "gwei"),
        "gas": GAS_LIMIT,
        "to": ADDR,
        "value": Web3.to_wei(VALUE, "ether"),
        "chainId": CHAIN_ID,
    }
    common(scenario_navigator, tx_params, test_name, BIP32_PATH)


# Get address on Clone app
@pytest.mark.needs_setup("lib_mode")
def test_clone_thundercore_get_address(scenario_navigator: NavigateWithScenario, test_name: str) -> None:
    app_client = EthAppClient(scenario_navigator.backend)

    with app_client.get_public_addr(bip32_path=BIP32_PATH):
        scenario_navigator.address_review_approve(test_name=test_name)

    pk, _, _ = ResponseParser.pk_addr(app_client.response().data)
    ref_pk, _ = calculate_public_key_and_chaincode(curve=CurveChoice.Secp256k1, path=BIP32_PATH)
    assert pk.hex() == ref_pk


# EIP-7702 authorization on Clone app for another chain
@pytest.mark.needs_setup("lib_mode")
@pytest.mark.parametrize("chain_id", [ETHEREUM_MAINNET_CHAIN_ID, CHAIN_ID_ALL])
def test_clone_thundercore_eip7702_other_chain(scenario_navigator: NavigateWithScenario, chain_id: int) -> None:
    app_client = EthAppClient(scenario_navigator.backend)

    with pytest.raises(ExceptionRAPDU) as e:
        with app_client.sign_eip7702_authorization(BIP32_PATH, TxAuth7702(SIMPLE_7702_ACCOUNT, NONCE, chain_id)):
            pass
    assert e.value.status == StatusWord.SWO_INCORRECT_DATA
