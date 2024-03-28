import pytest

from ledger_app_clients.ethereum.client import EthAppClient, StatusWord
import ledger_app_clients.ethereum.response_parser as ResponseParser
from ledger_app_clients.ethereum.utils import recover_message

from ragger.error import ExceptionRAPDU
from ragger.backend import BackendInterface
from ragger.firmware import Firmware
from ragger.navigator import Navigator, NavInsID

from constants import ROOT_SNAPSHOT_PATH


BIP32_PATH = "m/44'/60'/0'/0/0"


def test_personal_sign_metamask(firmware: Firmware,
                                backend: BackendInterface,
                                navigator: Navigator,
                                test_name: str):

    msg = "Example `personal_sign` message"

    app_client = EthAppClient(backend)

    with app_client.get_public_addr(display=False):
        pass
    _, DEVICE_ADDR, _ = ResponseParser.pk_addr(app_client.response().data)

    with app_client.personal_sign(BIP32_PATH, msg.encode('utf-8')):
        if firmware.device.startswith("nano"):
            next_action = NavInsID.RIGHT_CLICK
            confirm_action = NavInsID.BOTH_CLICK
            initial_instructions = [NavInsID.RIGHT_CLICK]
            # Skip 1st screen because 'Sign' is already present
            navigator.navigate(initial_instructions,
                               screen_change_after_last_instruction=False)
        else:
            next_action = NavInsID.USE_CASE_REVIEW_TAP
            confirm_action = NavInsID.USE_CASE_REVIEW_CONFIRM

        navigator.navigate_until_text_and_compare(next_action,
                                                  [confirm_action],
                                                  "Sign",
                                                  ROOT_SNAPSHOT_PATH,
                                                  test_name)

    # verify signature
    vrs = ResponseParser.signature(app_client.response().data)
    addr = recover_message(msg.encode('utf-8'), vrs)
    assert addr == DEVICE_ADDR


def test_personal_sign_non_ascii(firmware: Firmware,
                                 backend: BackendInterface,
                                 navigator: Navigator,
                                 test_name: str):

    msg = "0x9c22ff5f21f0b81b113e63f7db6da94fedef11b2119b4088b89664fb9a3cb658"

    app_client = EthAppClient(backend)

    with app_client.get_public_addr(display=False):
        pass
    _, DEVICE_ADDR, _ = ResponseParser.pk_addr(app_client.response().data)

    with app_client.personal_sign(BIP32_PATH, msg.encode('utf-8')):
        if firmware.device.startswith("nano"):
            next_action = NavInsID.RIGHT_CLICK
            confirm_action = NavInsID.BOTH_CLICK
            initial_instructions = [NavInsID.RIGHT_CLICK]
            # Skip 1st screen because 'Sign' is already present
            navigator.navigate(initial_instructions,
                               screen_change_before_first_instruction=False,
                               screen_change_after_last_instruction=False)
        else:
            next_action = NavInsID.USE_CASE_REVIEW_TAP
            confirm_action = NavInsID.USE_CASE_REVIEW_CONFIRM

        navigator.navigate_until_text_and_compare(next_action,
                                                  [confirm_action],
                                                  "Sign",
                                                  ROOT_SNAPSHOT_PATH,
                                                  test_name)

    # verify signature
    vrs = ResponseParser.signature(app_client.response().data)
    addr = recover_message(msg.encode('utf-8'), vrs)
    assert addr == DEVICE_ADDR


def test_personal_sign_opensea(firmware: Firmware,
                               backend: BackendInterface,
                               navigator: Navigator,
                               test_name: str):

    msg = "Welcome to OpenSea!\n\nClick to sign in and accept the OpenSea Terms of Service: https://opensea.io/tos\n\nThis request will not trigger a blockchain transaction or cost any gas fees.\n\nYour authentication status will reset after 24 hours.\n\nWallet address:\n0x9858effd232b4033e47d90003d41ec34ecaeda94\n\nNonce:\n2b02c8a0-f74f-4554-9821-a28054dc9121"

    app_client = EthAppClient(backend)

    with app_client.get_public_addr(display=False):
        pass
    _, DEVICE_ADDR, _ = ResponseParser.pk_addr(app_client.response().data)

    if firmware.device == "nanos":
        pytest.skip("Not supported on LNS")
    with app_client.personal_sign(BIP32_PATH, msg.encode('utf-8')):
        if firmware.device.startswith("nano"):
            next_action = NavInsID.RIGHT_CLICK
            confirm_action = NavInsID.BOTH_CLICK
            initial_instructions = [NavInsID.RIGHT_CLICK]
            # Skip 1st screen because 'Sign' is already present
            navigator.navigate(initial_instructions,
                               screen_change_before_first_instruction=False,
                               screen_change_after_last_instruction=False)
        else:
            next_action = NavInsID.USE_CASE_REVIEW_TAP
            confirm_action = NavInsID.USE_CASE_REVIEW_CONFIRM

        navigator.navigate_until_text_and_compare(next_action,
                                                  [confirm_action],
                                                  "Sign",
                                                  ROOT_SNAPSHOT_PATH,
                                                  test_name)

    # verify signature
    vrs = ResponseParser.signature(app_client.response().data)
    addr = recover_message(msg.encode('utf-8'), vrs)
    assert addr == DEVICE_ADDR


def test_personal_sign_reject(firmware: Firmware,
                              backend: BackendInterface,
                              navigator: Navigator,
                              test_name: str):

    msg = "This is an reject sign"

    app_client = EthAppClient(backend)

    try:
        with app_client.personal_sign(BIP32_PATH, msg.encode('utf-8')):
            if firmware.device.startswith("nano"):
                next_action = NavInsID.RIGHT_CLICK
                confirm_action = NavInsID.BOTH_CLICK
                navigator.navigate_until_text_and_compare(next_action,
                                                         [confirm_action],
                                                         "Cancel",
                                                         ROOT_SNAPSHOT_PATH,
                                                         test_name)
            else:
                # instructions = [NavInsID.USE_CASE_REVIEW_TAP]
                instructions = [NavInsID.USE_CASE_CHOICE_REJECT,
                                NavInsID.USE_CASE_CHOICE_CONFIRM,
                                NavInsID.USE_CASE_STATUS_DISMISS]
                navigator.navigate_and_compare(ROOT_SNAPSHOT_PATH,
                                               test_name,
                                               instructions)


    except ExceptionRAPDU as e:
        assert e.status == StatusWord.CONDITION_NOT_SATISFIED
    else:
        assert False  # An exception should have been raised
