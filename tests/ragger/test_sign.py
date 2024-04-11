from pathlib import Path
from web3 import Web3

from client.client import EthAppClient, StatusWord
import client.response_parser as ResponseParser
from client.settings import SettingID, settings_toggle
from client.utils import recover_transaction

from ragger.error import ExceptionRAPDU
from ragger.backend import BackendInterface
from ragger.firmware import Firmware
from ragger.navigator import Navigator, NavInsID


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


def common(firmware: Firmware,
           backend: BackendInterface,
           navigator: Navigator,
           default_screenshot_path: Path,
           tx_params: dict,
           test_name: str = "",
           path: str = BIP32_PATH):
    app_client = EthAppClient(backend)

    with app_client.get_public_addr(bip32_path=path, display=False):
        pass
    _, DEVICE_ADDR, _ = ResponseParser.pk_addr(app_client.response().data)

    with app_client.sign(path, tx_params):
        if firmware.device.startswith("nano"):
            next_action = NavInsID.RIGHT_CLICK
            confirm_action = NavInsID.BOTH_CLICK
            end_text = "Accept"
        else:
            next_action = NavInsID.USE_CASE_REVIEW_TAP
            confirm_action = NavInsID.USE_CASE_REVIEW_CONFIRM
            end_text = "Sign"

        if test_name:
            navigator.navigate_until_text_and_compare(next_action,
                                                      [confirm_action],
                                                      end_text,
                                                      default_screenshot_path,
                                                      test_name)
        else:
            navigator.navigate_until_text(next_action, [confirm_action], end_text)

    # verify signature
    vrs = ResponseParser.signature(app_client.response().data)
    addr = recover_transaction(tx_params, vrs)
    assert addr == DEVICE_ADDR


def common_reject(firmware: Firmware,
                  backend: BackendInterface,
                  navigator: Navigator,
                  default_screenshot_path: Path,
                  tx_params: dict,
                  test_name: str,
                  path: str = BIP32_PATH):
    app_client = EthAppClient(backend)

    try:
        with app_client.sign(path, tx_params):
            if firmware.device.startswith("nano"):
                next_action = NavInsID.RIGHT_CLICK
                confirm_action = NavInsID.BOTH_CLICK
                navigator.navigate_until_text_and_compare(next_action,
                                                          [confirm_action],
                                                          "Reject",
                                                          default_screenshot_path,
                                                          test_name)
            else:
                instructions = [NavInsID.USE_CASE_REVIEW_TAP] * 2
                instructions += [NavInsID.USE_CASE_CHOICE_REJECT,
                                NavInsID.USE_CASE_CHOICE_CONFIRM,
                                NavInsID.USE_CASE_STATUS_DISMISS]
                navigator.navigate_and_compare(default_screenshot_path,
                                               test_name,
                                               instructions)

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


def test_legacy(firmware: Firmware,
                backend: BackendInterface,
                navigator: Navigator,
                default_screenshot_path: Path):
    tx_params: dict = {
        "nonce": NONCE,
        "gasPrice": Web3.to_wei(GAS_PRICE, "gwei"),
        "gas": GAS_LIMIT,
        "to": ADDR,
        "value": Web3.to_wei(AMOUNT, "ether"),
        "chainId": CHAIN_ID
    }
    common(firmware, backend, navigator, default_screenshot_path, tx_params)


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
def test_legacy_send_bsc(firmware: Firmware,
                         backend: BackendInterface,
                         navigator: Navigator,
                         test_name: str,
                         default_screenshot_path: Path):
    tx_params: dict = {
        "nonce": 1,
        "gasPrice": Web3.to_wei(GAS_PRICE2, 'gwei'),
        "gas": GAS_LIMIT,
        "to": ADDR2,
        "value": Web3.to_wei(AMOUNT2, "ether"),
        "chainId": 56
    }
    common(firmware, backend, navigator, default_screenshot_path, tx_params, test_name, BIP32_PATH2)


# Transfer on network 112233445566 on Ethereum
def test_legacy_chainid(firmware: Firmware,
                         backend: BackendInterface,
                         navigator: Navigator,
                         test_name: str,
                         default_screenshot_path: Path):
    tx_params: dict = {
        "nonce": NONCE2,
        "gasPrice": Web3.to_wei(GAS_PRICE, 'gwei'),
        "gas": GAS_LIMIT,
        "to": ADDR2,
        "value": Web3.to_wei(AMOUNT2, "ether"),
        "chainId": 112233445566
    }
    common(firmware, backend, navigator, default_screenshot_path, tx_params, test_name, BIP32_PATH2)


# Try to blind sign with setting disabled
def test_legacy_contract(firmware: Firmware,
                         backend: BackendInterface,
                         navigator: Navigator,
                         test_name: str,
                         default_screenshot_path: Path):

    buffer = bytes.fromhex("058000002c8000003c800000010000000000000000f849208506fc23ac008303dc3194f650c3d88d12db855b8bf7d11be6c55a4e07dcc980a4a1712d6800000000000000000000000000000000000000000000000000000000000acbc7018080")
    app_client = EthAppClient(backend)

    try:
        with app_client.send_raw(0xe0, 0x04, 0x00, 0x00, buffer):
            if firmware.device.startswith("nano"):
                # No screens recorded on Stax
                moves = [
                    NavInsID.RIGHT_CLICK,
                    NavInsID.BOTH_CLICK
                ]
                navigator.navigate_and_compare(default_screenshot_path,
                                               test_name,
                                               moves)

    except ExceptionRAPDU as e:
        assert e.status == StatusWord.INVALID_DATA


def test_1559(firmware: Firmware,
              backend: BackendInterface,
              navigator: Navigator,
              default_screenshot_path: Path):
    tx_params: dict = {
        "nonce": NONCE,
        "maxFeePerGas": Web3.to_wei(145, "gwei"),
        "maxPriorityFeePerGas": Web3.to_wei(1.5, "gwei"),
        "gas": GAS_LIMIT,
        "to": ADDR,
        "value": Web3.to_wei(AMOUNT, "ether"),
        "chainId": CHAIN_ID
    }
    common(firmware, backend, navigator, default_screenshot_path, tx_params)


def test_sign_simple(firmware: Firmware,
                     backend: BackendInterface,
                     navigator: Navigator,
                     test_name: str,
                     default_screenshot_path: Path):
    tx_params: dict = {
        "nonce": NONCE2,
        "gasPrice": Web3.to_wei(GAS_PRICE, 'gwei'),
        "gas": GAS_LIMIT,
        "to": ADDR2,
        "value": Web3.to_wei(AMOUNT2, "ether"),
        "chainId": CHAIN_ID
    }
    common(firmware, backend, navigator, default_screenshot_path, tx_params, test_name, "m/44'/60'/1'/0/0")


def test_sign_limit_nonce(firmware: Firmware,
                          backend: BackendInterface,
                          navigator: Navigator,
                          test_name: str,
                          default_screenshot_path: Path):
    tx_params: dict = {
        "nonce": 2**64-1,
        "gasPrice": 10,
        "gas": 50000,
        "to": ADDR2,
        "value": 0x08762,
        "chainId": CHAIN_ID
    }
    common(firmware, backend, navigator, default_screenshot_path, tx_params, test_name, "m/44'/60'/1'/0/0")


def test_sign_nonce_display(firmware: Firmware,
                            backend: BackendInterface,
                            navigator: Navigator,
                            test_name: str,
                            default_screenshot_path: Path):

    settings_toggle(firmware, navigator, [SettingID.NONCE])

    tx_params: dict = {
        "nonce": NONCE2,
        "gasPrice": Web3.to_wei(GAS_PRICE, 'gwei'),
        "gas": GAS_LIMIT,
        "to": ADDR2,
        "value": Web3.to_wei(AMOUNT2, "ether"),
        "chainId": CHAIN_ID
    }
    common(firmware, backend, navigator, default_screenshot_path, tx_params, test_name, "m/44'/60'/1'/0/0")


def test_sign_blind_simple(firmware: Firmware,
                           backend: BackendInterface,
                           navigator: Navigator,
                           test_name: str,
                           default_screenshot_path: Path):
    settings_toggle(firmware, navigator, [SettingID.BLIND_SIGNING])

    data = "ok"
    tx_params: dict = {
        "nonce": NONCE2,
        "gasPrice": Web3.to_wei(GAS_PRICE, 'gwei'),
        "gas": GAS_LIMIT,
        "to": ADDR2,
        "value": Web3.to_wei(AMOUNT2, "ether"),
        "chainId": CHAIN_ID,
        "data": data.encode('utf-8').hex()
    }
    common(firmware, backend, navigator, default_screenshot_path, tx_params, test_name, "m/44'/60'/1'/0/0")


def test_sign_blind_and_nonce_display(firmware: Firmware,
                                      backend: BackendInterface,
                                      navigator: Navigator,
                                      test_name: str,
                                      default_screenshot_path: Path):
    settings_toggle(firmware, navigator, [SettingID.NONCE, SettingID.BLIND_SIGNING])

    data = "That's a little message :)"
    tx_params: dict = {
        "nonce": 1844674,
        "gasPrice": Web3.to_wei(GAS_PRICE, 'gwei'),
        "gas": GAS_LIMIT,
        "to": ADDR2,
        "value": Web3.to_wei(AMOUNT2, "ether"),
        "chainId": CHAIN_ID,
        "data": data.encode('utf-8').hex()
    }
    common(firmware, backend, navigator, default_screenshot_path, tx_params, test_name, "m/44'/60'/1'/0/0")


def test_sign_reject(firmware: Firmware,
                     backend: BackendInterface,
                     navigator: Navigator,
                     test_name: str,
                     default_screenshot_path: Path):
    tx_params: dict = {
        "nonce": NONCE2,
        "gasPrice": Web3.to_wei(GAS_PRICE, 'gwei'),
        "gas": GAS_LIMIT,
        "to": ADDR2,
        "value": Web3.to_wei(AMOUNT2, "ether"),
        "chainId": CHAIN_ID
    }
    common_reject(firmware, backend, navigator, default_screenshot_path, tx_params, test_name, "m/44'/60'/1'/0/0")


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


def test_sign_blind_error_disabled(backend: BackendInterface):
    data = "ok"
    tx_params: dict = {
        "nonce": NONCE2,
        "gasPrice": Web3.to_wei(GAS_PRICE, 'gwei'),
        "gas": GAS_LIMIT,
        "to": ADDR2,
        "value": Web3.to_wei(AMOUNT2, "ether"),
        "chainId": CHAIN_ID,
        "data": data.encode('utf-8').hex()
    }

    common_fail(backend, tx_params, StatusWord.INVALID_DATA, BIP32_PATH2)


def test_sign_eip_2930(firmware: Firmware,
                       backend: BackendInterface,
                       navigator: Navigator,
                       test_name: str,
                       default_screenshot_path: Path):

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
    common(firmware, backend, navigator, default_screenshot_path, tx_params, test_name)
