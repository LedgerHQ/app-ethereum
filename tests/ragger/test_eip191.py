import pytest

from ragger.error import ExceptionRAPDU
from ragger.backend import BackendInterface
from ragger.firmware import Firmware
from ragger.navigator.navigation_scenario import NavigateWithScenario

from client.client import EthAppClient, StatusWord
import client.response_parser as ResponseParser
from client.utils import recover_message


BIP32_PATH = "m/44'/60'/0'/0/0"


def common(backend: BackendInterface,
           scenario: NavigateWithScenario,
           test_name: str,
           msg: str):

    app_client = EthAppClient(backend)

    with app_client.get_public_addr(display=False):
        pass
    _, DEVICE_ADDR, _ = ResponseParser.pk_addr(app_client.response().data)

    with app_client.personal_sign(BIP32_PATH, msg.encode('utf-8')):
        scenario.review_approve(test_name=test_name, custom_screen_text="Sign")

    # verify signature
    vrs = ResponseParser.signature(app_client.response().data)
    addr = recover_message(msg.encode('utf-8'), vrs)
    assert addr == DEVICE_ADDR


def test_personal_sign_metamask(backend: BackendInterface,
                                scenario_navigator: NavigateWithScenario,
                                test_name: str):

    msg = "Example `personal_sign` message"
    common(backend, scenario_navigator, test_name, msg)


def test_personal_sign_non_ascii(backend: BackendInterface,
                                 scenario_navigator: NavigateWithScenario,
                                 test_name: str):

    msg = "0x9c22ff5f21f0b81b113e63f7db6da94fedef11b2119b4088b89664fb9a3cb658"
    common(backend, scenario_navigator, test_name, msg)


def test_personal_sign_opensea(firmware: Firmware,
                               backend: BackendInterface,
                               scenario_navigator: NavigateWithScenario,
                               test_name: str):

    if firmware == Firmware.NANOS:
        pytest.skip("Not supported on LNS")

    msg = "Welcome to OpenSea!\n\n"
    msg += "Click to sign in and accept the OpenSea Terms of Service: https://opensea.io/tos\n\n"
    msg += "This request will not trigger a blockchain transaction or cost any gas fees.\n\n"
    msg += "Your authentication status will reset after 24 hours.\n\n"
    msg += "Wallet address:\n0x9858effd232b4033e47d90003d41ec34ecaeda94\n\nNonce:\n2b02c8a0-f74f-4554-9821-a28054dc9121"
    common(backend, scenario_navigator, test_name, msg)


def test_personal_sign_reject(firmware: Firmware,
                              backend: BackendInterface,
                              scenario_navigator: NavigateWithScenario):

    msg = "This is an reject sign"

    app_client = EthAppClient(backend)

    try:
        with app_client.personal_sign(BIP32_PATH, msg.encode('utf-8')):
            if firmware.is_nano:
                end_text = "Cancel"
            else:
                end_text = "Sign"
            scenario_navigator.review_reject(custom_screen_text=end_text)

    except ExceptionRAPDU as e:
        assert e.status == StatusWord.CONDITION_NOT_SATISFIED
    else:
        assert False  # An exception should have been raised
