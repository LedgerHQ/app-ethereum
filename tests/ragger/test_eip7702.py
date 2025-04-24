import pytest

from ragger.error import ExceptionRAPDU
from ragger.backend import BackendInterface
from ragger.firmware import Firmware
from ragger.navigator.navigation_scenario import NavigateWithScenario
from ragger.navigator import NavInsID

from client.client import EthAppClient, StatusWord
from client.settings import SettingID, settings_toggle
import client.response_parser as ResponseParser
from client.tx_auth_7702 import TxAuth7702

BIP32_PATH = "m/44'/60'/0'/0/0"
TEST_ADDRESS_0 = bytes.fromhex("00" * 20)
TEST_ADDRESS_1 = bytes.fromhex("01" * 20)
TEST_ADDRESS_2 = bytes.fromhex("02" * 20)
TEST_ADDRESS_NO_WHITELIST = bytes.fromhex("42" * 20)
TEST_ADDRESS_MAX = bytes.fromhex("ff" * 20)
CHAIN_ID_0 = 0
CHAIN_ID_1 = 1
CHAIN_ID_2 = 2
CHAIN_ID_MAX = 0xFFFFFFFFFFFFFFFF
NONCE = 1337
NONCE_MAX = 0xFFFFFFFFFFFFFFFF

# Test vectors computed with
# cast wallet sign-auth $ADDRESS --mnemonic $MNEMONIC --mnemonic-derivation-path "m/44'/60'/0'/0/0" --nonce $NONCE --chain $CHAINID
# Decoded by https://codechain-io.github.io/rlp-debugger/


def common(firmware: Firmware,
           backend: BackendInterface,
           scenario: NavigateWithScenario,
           test_name: str,
           delegate: bytes,
           nonce: int,
           chain_id: int,
           v: int = None,
           r: int = None,
           s: int = None):

    app_client = EthAppClient(backend)

    if firmware.is_nano:
        end_text = "Accept"
    else:
        end_text = ".*Sign.*"

    auth_params = TxAuth7702(delegate, nonce, chain_id)
    with app_client.sign_eip7702_authorization(BIP32_PATH, auth_params):
        scenario.review_approve(test_name=test_name, custom_screen_text=end_text)
    vrs = ResponseParser.signature(app_client.response().data)
    assert vrs == (v, r, s)


def common_rejected(firmware: Firmware,
                    backend: BackendInterface,
                    scenario: NavigateWithScenario,
                    test_name: str,
                    delegate: bytes,
                    nonce: int,
                    chain_id: int):

    app_client = EthAppClient(backend)

    try:
        auth_params = TxAuth7702(delegate, nonce, chain_id)
        with app_client.sign_eip7702_authorization(BIP32_PATH, auth_params):
            moves = []
            if firmware.is_nano:
                moves += [NavInsID.BOTH_CLICK]
            else:
                moves += [NavInsID.USE_CASE_CHOICE_REJECT]
            scenario.navigator.navigate_and_compare(scenario.screenshot_path,
                                                    test_name,
                                                    moves)

    except ExceptionRAPDU as e:
        assert e.status == StatusWord.CONDITION_NOT_SATISFIED
    else:
        assert False  # An exception should have been raised


def test_eip7702_in_whitelist(firmware: Firmware,
                              backend: BackendInterface,
                              scenario_navigator: NavigateWithScenario,
                              test_name: str):
    if firmware == Firmware.NANOS:
        pytest.skip("Not supported on LNS")
    settings_toggle(firmware, scenario_navigator.navigator, [SettingID.EIP7702])
    common(firmware,
           backend,
           scenario_navigator,
           test_name,
           TEST_ADDRESS_1,
           NONCE,
           CHAIN_ID_1,
           0x00,
           0xf82e50a755fa989f4bb9b36b15afb4424ce9cda69752fd17a7eb14737d963e62,
           0x07c9c91d6140b45ea52f29de7a5effb9dd340607a26e225c40278e91c4054492)


def test_eip7702_in_whitelist_all_chain_whitelisted(firmware: Firmware,
                                                    backend: BackendInterface,
                                                    scenario_navigator: NavigateWithScenario,
                                                    test_name: str):
    if firmware == Firmware.NANOS:
        pytest.skip("Not supported on LNS")
    settings_toggle(firmware, scenario_navigator.navigator, [SettingID.EIP7702])
    common(firmware,
           backend,
           scenario_navigator,
           test_name,
           TEST_ADDRESS_0,
           NONCE,
           CHAIN_ID_2,
           0x00,
           0x0378f7ac482ec728b65d19d03943bbb3fe7307c72c646e7d2d0c11bee81eb2b9,
           0x332266ec3ef996bf835c50a833006b4c80398d597d0e684619db4d51a384a38d)


def test_eip7702_in_whitelist_all_chain_param(firmware: Firmware,
                                              backend: BackendInterface,
                                              scenario_navigator: NavigateWithScenario,
                                              test_name: str):
    if firmware == Firmware.NANOS:
        pytest.skip("Not supported on LNS")
    settings_toggle(firmware, scenario_navigator.navigator, [SettingID.EIP7702])
    common(firmware,
           backend,
           scenario_navigator,
           test_name,
           TEST_ADDRESS_2,
           NONCE,
           CHAIN_ID_0,
           0x01,
           0xa24f35cafc6b408ce32539d4bd89a67edd4d6303fc676dfddf93b98405b7ee5e,
           0x159456babe656692959ca3d829ca269e8f82387c91e40a33633d190dda7a3c5c)


def test_eip7702_in_whitelist_max(firmware: Firmware,
                                  backend: BackendInterface,
                                  scenario_navigator: NavigateWithScenario,
                                  test_name: str):
    if firmware == Firmware.NANOS:
        pytest.skip("Not supported on LNS")
    settings_toggle(firmware, scenario_navigator.navigator, [SettingID.EIP7702])
    common(firmware,
           backend,
           scenario_navigator,
           test_name,
           TEST_ADDRESS_MAX,
           NONCE_MAX,
           CHAIN_ID_MAX,
           0x00,
           0xa07c680894498c21e80febb011ae5d62ede6645d77d7c902db065d5a082dd220,
           0x6b5c62225cf65a6240c2583dacc1641c8d692ed3bd00473cc3e28fe1a4f52294)


def test_eip7702_in_whitelist_wrong_chain(firmware: Firmware,
                                          backend: BackendInterface,
                                          scenario_navigator: NavigateWithScenario,
                                          test_name: str):
    if firmware == Firmware.NANOS:
        pytest.skip("Not supported on LNS")
    settings_toggle(firmware, scenario_navigator.navigator, [SettingID.EIP7702])
    common_rejected(firmware, backend, scenario_navigator, test_name, TEST_ADDRESS_2, NONCE, CHAIN_ID_1)


def test_eip7702_not_in_whitelist(firmware: Firmware,
                                  backend: BackendInterface,
                                  scenario_navigator: NavigateWithScenario,
                                  test_name: str):
    if firmware == Firmware.NANOS:
        pytest.skip("Not supported on LNS")
    settings_toggle(firmware, scenario_navigator.navigator, [SettingID.EIP7702])
    common_rejected(firmware, backend, scenario_navigator, test_name, TEST_ADDRESS_NO_WHITELIST, NONCE, CHAIN_ID_1)


def test_eip7702_not_enabled(firmware: Firmware,
                             backend: BackendInterface,
                             scenario_navigator: NavigateWithScenario,
                             test_name: str):
    if firmware == Firmware.NANOS:
        pytest.skip("Not supported on LNS")
    common_rejected(firmware, backend, scenario_navigator, test_name, TEST_ADDRESS_1, NONCE, CHAIN_ID_1)
