from web3 import Web3

from ledger_app_clients.ethereum.client import EthAppClient, StatusWord
import ledger_app_clients.ethereum.response_parser as ResponseParser
from ledger_app_clients.ethereum.utils import recover_transaction

from ragger.error import ExceptionRAPDU
from ragger.backend import BackendInterface
from ragger.firmware import Firmware
from ragger.navigator import Navigator, NavInsID, NavIns

from constants import ROOT_SNAPSHOT_PATH


# Values used across all tests
CHAIN_ID = 1
ADDR = bytes.fromhex("0011223344556677889900112233445566778899")
ADDR2 = bytes.fromhex("5a321744667052affa8386ed49e00ef223cbffc3")
BIP32_PATH = "m/44'/60'/0'/0/0"
NONCE = 21
NONCE2 = 68
GAS_LIMIT = 21000
AMOUNT = 1.22


def common(firmware: Firmware,
           backend: BackendInterface,
           navigator: Navigator,
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
                                                      ROOT_SNAPSHOT_PATH,
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
                                                          ROOT_SNAPSHOT_PATH,
                                                          test_name)
            else:
                instructions = [NavInsID.USE_CASE_REVIEW_TAP] * 2
                instructions += [NavInsID.USE_CASE_CHOICE_REJECT,
                                NavInsID.USE_CASE_CHOICE_CONFIRM,
                                NavInsID.USE_CASE_STATUS_DISMISS]
                navigator.navigate_and_compare(ROOT_SNAPSHOT_PATH,
                                               test_name,
                                               instructions)

    except ExceptionRAPDU as e:
        assert e.status == StatusWord.CONDITION_NOT_SATISFIED
    else:
        assert False  # An exception should have been raised


def test_legacy(firmware: Firmware, backend: BackendInterface, navigator: Navigator):
    tx_params: dict = {
        "nonce": NONCE,
        "gasPrice": Web3.to_wei(13, "gwei"),
        "gas": GAS_LIMIT,
        "to": ADDR,
        "value": Web3.to_wei(AMOUNT, "ether"),
        "chainId": CHAIN_ID
    }
    common(firmware, backend, navigator, tx_params)


def test_1559(firmware: Firmware, backend: BackendInterface, navigator: Navigator):
    tx_params: dict = {
        "nonce": NONCE,
        "maxFeePerGas": Web3.to_wei(145, "gwei"),
        "maxPriorityFeePerGas": Web3.to_wei(1.5, "gwei"),
        "gas": GAS_LIMIT,
        "to": ADDR,
        "value": Web3.to_wei(AMOUNT, "ether"),
        "chainId": CHAIN_ID
    }
    common(firmware, backend, navigator, tx_params)


def test_sign_simple(firmware: Firmware,
                     backend: BackendInterface,
                     navigator: Navigator,
                     test_name: str):
    tx_params: dict = {
        "nonce": NONCE2,
        "gasPrice": Web3.to_wei(13, 'gwei'),
        "gas": GAS_LIMIT,
        "to": ADDR2,
        "value": 0x6f9c9e7bf61818,
        "chainId": CHAIN_ID
    }
    common(firmware, backend, navigator, tx_params, test_name, "m/44'/60'/1'/0/0")


def test_sign_limit_nonce(firmware: Firmware,
                          backend: BackendInterface,
                          navigator: Navigator,
                          test_name: str):
    tx_params: dict = {
        "nonce": 2**64-1,
        "gasPrice": 10,
        "gas": 50000,
        "to": ADDR2,
        "value": 0x08762,
        "chainId": CHAIN_ID
    }
    common(firmware, backend, navigator, tx_params, test_name, "m/44'/60'/1'/0/0")


def test_sign_nonce_display(firmware: Firmware,
                            backend: BackendInterface,
                            navigator: Navigator,
                            test_name: str):
    # Activate nonce display
    if firmware.device.startswith("nano"):
        initial_instructions = [
            NavInsID.LEFT_CLICK,    # Application is ready
            NavInsID.LEFT_CLICK,    # Quit
            NavInsID.BOTH_CLICK,    # Blind signing
            NavInsID.RIGHT_CLICK,   # Debug data
            NavInsID.RIGHT_CLICK,   # Nonce display
            NavInsID.BOTH_CLICK,
            NavInsID.RIGHT_CLICK,
        ]
        if firmware.device != "nanos":
            initial_instructions += [NavInsID.RIGHT_CLICK] * 2
        initial_instructions += [NavInsID.BOTH_CLICK]    # Back
    else:
        initial_instructions = [
            NavInsID.USE_CASE_HOME_SETTINGS,    # Settings
            NavInsID.USE_CASE_SETTINGS_NEXT,    # Next page
            NavIns(NavInsID.TOUCH, (340, 440)), # Nonce
            NavInsID.USE_CASE_SETTINGS_MULTI_PAGE_EXIT,
        ]

    # Navigate to settings menu to avoid 1st screen with random serial no
    navigator.navigate(initial_instructions,
                        screen_change_before_first_instruction=False)

    tx_params: dict = {
        "nonce": NONCE2,
        "gasPrice": Web3.to_wei(13, 'gwei'),
        "gas": GAS_LIMIT,
        "to": ADDR2,
        "value": 0x6f9c9e7bf61818,
        "chainId": CHAIN_ID
    }
    common(firmware, backend, navigator, tx_params, test_name, "m/44'/60'/1'/0/0")


def test_sign_blind_simple(firmware: Firmware,
                           backend: BackendInterface,
                           navigator: Navigator,
                           test_name: str):
    # Activate nonce display
    if firmware.device.startswith("nano"):
        initial_instructions = [
            NavInsID.LEFT_CLICK,    # Application is ready
            NavInsID.LEFT_CLICK,    # Quit
            NavInsID.BOTH_CLICK,    # Blind signing
            NavInsID.BOTH_CLICK,
            NavInsID.RIGHT_CLICK,
            NavInsID.RIGHT_CLICK,
            NavInsID.RIGHT_CLICK,
        ]
        if firmware.device != "nanos":
            initial_instructions += [NavInsID.RIGHT_CLICK] * 2
        initial_instructions += [NavInsID.BOTH_CLICK]    # Back
    else:
        initial_instructions = [
            NavInsID.USE_CASE_HOME_SETTINGS,    # Settings
            NavInsID.USE_CASE_SETTINGS_NEXT,    # Next page
            NavIns(NavInsID.TOUCH, (340, 120)), # Blind signing
            NavInsID.USE_CASE_SETTINGS_MULTI_PAGE_EXIT,
        ]

    # Navigate to settings menu to avoid 1st screen with random serial no
    navigator.navigate(initial_instructions,
                        screen_change_before_first_instruction=False)

    data = "ok"
    tx_params: dict = {
        "nonce": NONCE2,
        "gasPrice": Web3.to_wei(13, 'gwei'),
        "gas": GAS_LIMIT,
        "to": ADDR2,
        "value": 0x6f9c9e7bf61818,
        "chainId": CHAIN_ID,
        "data": data.encode('utf-8').hex()
    }
    common(firmware, backend, navigator, tx_params, test_name, "m/44'/60'/1'/0/0")


def test_sign_blind_and_nonce_display(firmware: Firmware,
                                      backend: BackendInterface,
                                      navigator: Navigator,
                                      test_name: str):
    # Activate nonce display
    if firmware.device.startswith("nano"):
        initial_instructions = [
            NavInsID.LEFT_CLICK,    # Application is ready
            NavInsID.LEFT_CLICK,    # Quit
            NavInsID.BOTH_CLICK,    # Blind signing
            NavInsID.BOTH_CLICK,
            NavInsID.RIGHT_CLICK,
            NavInsID.RIGHT_CLICK,   # Nonce display
            NavInsID.BOTH_CLICK,
            NavInsID.RIGHT_CLICK,
        ]
        if firmware.device != "nanos":
            initial_instructions += [NavInsID.RIGHT_CLICK] * 2
        initial_instructions += [NavInsID.BOTH_CLICK]    # Back
    else:
        initial_instructions = [
            NavInsID.USE_CASE_HOME_SETTINGS,    # Settings
            NavInsID.USE_CASE_SETTINGS_NEXT,    # Next page
            NavIns(NavInsID.TOUCH, (340, 120)), # Blind signing
            NavIns(NavInsID.TOUCH, (340, 440)), # Nonce
            NavInsID.USE_CASE_SETTINGS_MULTI_PAGE_EXIT,
        ]

    # Navigate to settings menu to avoid 1st screen with random serial no
    navigator.navigate(initial_instructions,
                        screen_change_before_first_instruction=False)

    data = "That's a little message :)"
    tx_params: dict = {
        "nonce": 1844674,
        "gasPrice": Web3.to_wei(13, 'gwei'),
        "gas": GAS_LIMIT,
        "to": ADDR2,
        "value": 0x6f9c9e7bf61818,
        "chainId": CHAIN_ID,
        "data": data.encode('utf-8').hex()
    }
    common(firmware, backend, navigator, tx_params, test_name, "m/44'/60'/1'/0/0")


def test_sign_reject(firmware: Firmware,
                     backend: BackendInterface,
                     navigator: Navigator,
                     test_name: str):
    tx_params: dict = {
        "nonce": NONCE2,
        "gasPrice": Web3.to_wei(13, 'gwei'),
        "gas": GAS_LIMIT,
        "to": ADDR2,
        "value": 0x6f9c9e7bf61818,
        "chainId": CHAIN_ID
    }
    common_reject(firmware, backend, navigator, tx_params, test_name, "m/44'/60'/1'/0/0")


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
        with app_client.sign("m/44'/60'/1'/0/0", tx_params):
            pass

    except TypeError:
        pass
    else:
        assert False  # An exception should have been raised


def test_sign_blind_error_disabled(backend: BackendInterface):
    data = "ok"
    tx_params: dict = {
        "nonce": NONCE2,
        "gasPrice": Web3.to_wei(13, 'gwei'),
        "gas": GAS_LIMIT,
        "to": ADDR2,
        "value": 0x6f9c9e7bf61818,
        "chainId": CHAIN_ID,
        "data": data.encode('utf-8').hex()
    }

    app_client = EthAppClient(backend)
    try:
        with app_client.sign("m/44'/60'/1'/0/0", tx_params):
            pass

    except ExceptionRAPDU as e:
        assert e.status == StatusWord.INVALID_DATA
    else:
        assert False  # An exception should have been raised
