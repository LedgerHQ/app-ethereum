import pytest
from web3 import Web3

from ragger.error import ExceptionRAPDU
from ragger.backend import BackendInterface
from ragger.navigator import Navigator
from ragger.navigator.navigation_scenario import NavigateWithScenario

from dynamic_networks_cfg import get_network_config

from client.client import EthAppClient
from client.status_word import StatusWord
import client.response_parser as ResponseParser
from client.settings import SettingID, settings_toggle
from client.utils import recover_transaction, get_authorization_obj
from client.dynamic_networks import DynamicNetwork


# Values used across all tests
CHAIN_ID = 1
ADDR = bytes.fromhex("0011223344556677889900112233445566778899")
ADDR2 = bytes.fromhex("5a321744667052affa8386ed49e00ef223cbffc3")
ADDR3 = bytes.fromhex("dac17f958d2ee523a2206206994597c13d831ec7")
ADDR4 = bytes.fromhex("b2bb2b958afa2e96dab3f3ce7162b87daea39017")
BIP32_PATH = "m/44'/60'/0'/0/0"
BIP32_PATH2 = "m/44'/60'/1'/0/0"
NONCE = 21
NONCE2 = 68
GAS_PRICE = 13
GAS_PRICE2 = 5
GAS_LIMIT = 21000
AMOUNT = 1.22
AMOUNT2 = 0.31415


@pytest.fixture(name="display_hash", params=[True, False])
def display_hash_fixture(request) -> bool:
    return request.param


def common(scenario_navigator: NavigateWithScenario,
           tx_params: dict,
           test_name: str = "",
           path: str = BIP32_PATH,
           with_simu: bool = False):
    backend = scenario_navigator.backend
    app_client = EthAppClient(backend)

    # Send Network information (name, ticker, icon)
    name, ticker, icon = get_network_config(backend.device.type, tx_params["chainId"])
    if name and ticker:
        app_client.provide_network_information(DynamicNetwork(name, ticker, tx_params["chainId"], icon))

    with app_client.get_public_addr(bip32_path=path, display=False):
        pass
    _, DEVICE_ADDR, _ = ResponseParser.pk_addr(app_client.response().data)

    with app_client.sign(path, tx_params):
        if with_simu:
            scenario_navigator.review_approve_with_warning(test_name=test_name, do_comparison=test_name != "")
        else:
            scenario_navigator.review_approve(test_name=test_name, do_comparison=test_name != "")

    # verify signature
    vrs = ResponseParser.signature(app_client.response().data)
    addr = recover_transaction(tx_params, vrs)
    assert addr == DEVICE_ADDR


def common_reject(scenario_navigator: NavigateWithScenario,
                  tx_params: dict,
                  path: str = BIP32_PATH):
    backend = scenario_navigator.backend
    app_client = EthAppClient(backend)

    try:
        with app_client.sign(path, tx_params):
            scenario_navigator.review_reject()

    except ExceptionRAPDU as e:
        assert e.status == StatusWord.CONDITION_NOT_SATISFIED
    else:
        assert False  # An exception should have been raised


def common_fail(backend: BackendInterface,
                tx_params: dict,
                expected: StatusWord,
                path: str = BIP32_PATH):
    app_client = EthAppClient(backend)

    try:
        with app_client.sign(path, tx_params):
            pass

    except ExceptionRAPDU as e:
        assert e.status == expected
    else:
        assert False  # An exception should have been raised


def test_legacy(scenario_navigator: NavigateWithScenario):
    tx_params: dict = {
        "nonce": NONCE,
        "gasPrice": Web3.to_wei(GAS_PRICE, "gwei"),
        "gas": GAS_LIMIT,
        "to": ADDR,
        "value": Web3.to_wei(AMOUNT, "ether"),
        "chainId": CHAIN_ID
    }
    common(scenario_navigator, tx_params)


# Transfer amount >= 2^87 Eth on Ethereum app should fail
def test_legacy_send_error(backend: BackendInterface):
    tx_params: dict = {
        "nonce": 38,
        "gasPrice": 56775612312210000000001234554332,
        "gas": GAS_LIMIT,
        "to": ADDR3,
        "value": 12345678912345678912345678000000000000000000,
        "chainId": CHAIN_ID
    }
    common_fail(backend, tx_params, StatusWord.EXCEPTION_OVERFLOW, path=BIP32_PATH2)


# Transfer bsc
def test_sign_legacy_send_bsc(scenario_navigator: NavigateWithScenario, test_name: str):
    tx_params: dict = {
        "nonce": 1,
        "gasPrice": Web3.to_wei(GAS_PRICE2, 'gwei'),
        "gas": GAS_LIMIT,
        "to": ADDR2,
        "value": Web3.to_wei(AMOUNT2, "ether"),
        "chainId": 56
    }
    common(scenario_navigator, tx_params, test_name, BIP32_PATH2)


# Transfer on network 112233445566 on Ethereum
def test_sign_legacy_chainid(scenario_navigator: NavigateWithScenario, test_name: str):
    tx_params: dict = {
        "nonce": NONCE2,
        "gasPrice": Web3.to_wei(GAS_PRICE, 'gwei'),
        "gas": GAS_LIMIT,
        "to": ADDR2,
        "value": Web3.to_wei(AMOUNT2, "ether"),
        "chainId": 112233445566
    }
    common(scenario_navigator, tx_params, test_name, BIP32_PATH2)


def test_1559(scenario_navigator: NavigateWithScenario):
    tx_params: dict = {
        "nonce": NONCE,
        "maxFeePerGas": Web3.to_wei(145, "gwei"),
        "maxPriorityFeePerGas": Web3.to_wei(1.5, "gwei"),
        "gas": GAS_LIMIT,
        "to": ADDR,
        "value": Web3.to_wei(AMOUNT, "ether"),
        "chainId": CHAIN_ID
    }
    common(scenario_navigator, tx_params)


def test_sign_simple(scenario_navigator: NavigateWithScenario, test_name: str, display_hash: bool):
    tx_params: dict = {
        "nonce": NONCE2,
        "gasPrice": Web3.to_wei(GAS_PRICE, 'gwei'),
        "gas": GAS_LIMIT,
        "to": ADDR2,
        "value": Web3.to_wei(AMOUNT2, "ether"),
        "chainId": CHAIN_ID
    }

    if display_hash:
        settings_toggle(scenario_navigator.backend.device, scenario_navigator.navigator, [SettingID.DISPLAY_HASH])
        test_name += "_display_hash"
    common(scenario_navigator, tx_params, test_name, BIP32_PATH2)


def test_sign_limit_nonce(scenario_navigator: NavigateWithScenario, test_name: str):
    tx_params: dict = {
        "nonce": 2**64-1,
        "gasPrice": 10,
        "gas": 50000,
        "to": ADDR2,
        "value": 0x08762,
        "chainId": CHAIN_ID
    }
    common(scenario_navigator, tx_params, test_name, BIP32_PATH2)


def test_sign_nonce_display(navigator: Navigator,
                            scenario_navigator: NavigateWithScenario,
                            test_name: str):

    device = scenario_navigator.backend.device
    settings_toggle(device, navigator, [SettingID.NONCE])

    tx_params: dict = {
        "nonce": NONCE2,
        "gasPrice": Web3.to_wei(GAS_PRICE, 'gwei'),
        "gas": GAS_LIMIT,
        "to": ADDR2,
        "value": Web3.to_wei(AMOUNT2, "ether"),
        "chainId": CHAIN_ID
    }
    common(scenario_navigator, tx_params, test_name, BIP32_PATH2)


def test_sign_reject(scenario_navigator: NavigateWithScenario):
    tx_params: dict = {
        "nonce": NONCE2,
        "gasPrice": Web3.to_wei(GAS_PRICE, 'gwei'),
        "gas": GAS_LIMIT,
        "to": ADDR2,
        "value": Web3.to_wei(AMOUNT2, "ether"),
        "chainId": CHAIN_ID
    }
    common_reject(scenario_navigator, tx_params, BIP32_PATH2)


def test_sign_error_transaction_type(backend: BackendInterface):
    tx_params: dict = {
        "type": 0,
        "nonce": 0,
        "gasPrice": 10,
        "gas": 50000,
        "to": ADDR2,
        "value": 0x19,
        "chainId": CHAIN_ID
    }

    app_client = EthAppClient(backend)
    try:
        with app_client.sign(BIP32_PATH2, tx_params):
            pass

    except TypeError:
        pass
    else:
        assert False  # An exception should have been raised


def test_sign_eip_2930(scenario_navigator: NavigateWithScenario, test_name: str):

    tx_params = {
        "nonce": NONCE,
        "gasPrice": Web3.to_wei(GAS_PRICE2, "gwei"),
        "gas": GAS_LIMIT,
        "to": ADDR4,
        "value": Web3.to_wei(0.01, "ether"),
        "chainId": 3,
        "accessList": [
            {
                "address": "0x0000000000000000000000000000000000000001",
                "storageKeys": [
                    "0x0100000000000000000000000000000000000000000000000000000000000000"
                ]
            }
        ],
    }
    common(scenario_navigator, tx_params, test_name)


def test_sign_eip_7702(scenario_navigator: NavigateWithScenario, test_name: str):
    tx_params = {
        "chainId": 1,
        "nonce": 1337,
        "maxPriorityFeePerGas": 0x01,
        "maxFeePerGas": 0x01,
        "gas": 21000,
        "to": bytes.fromhex("1212121212121212121212121212121212121212"),
        "value": Web3.to_wei(0.01, "ether"),
        "authorizationList": [
            get_authorization_obj(0, 1337, bytes.fromhex("1212121212121212121212121212121212121212"), (
                0x01,
                0xa24f35cafc6b408ce32539d4bd89a67edd4d6303fc676dfddf93b98405b7ee5e,
                0x159456babe656692959ca3d829ca269e8f82387c91e40a33633d190dda7a3c5c,
            ))
        ],
    }
    common(scenario_navigator, tx_params, test_name)
