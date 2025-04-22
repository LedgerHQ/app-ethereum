import pytest

from ragger.error import ExceptionRAPDU
from ragger.backend import BackendInterface
from ragger.firmware import Firmware
from ragger.navigator.navigation_scenario import NavigateWithScenario
from ragger.navigator import NavInsID

from client.client import EthAppClient, StatusWord
from client.settings import SettingID, settings_toggle
import client.response_parser as ResponseParser
from client.utils import recover_message
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
           v: bytes = None,
           r: bytes = None,
           s: bytes = None):

    app_client = EthAppClient(backend)

    if firmware.is_nano:
        end_text = "Accept"
    else:
        end_text = ".*Sign.*"

    auth_params = TxAuth7702(BIP32_PATH, delegate, nonce, chain_id)
    with app_client.sign_eip7702_authorization(auth_params):
        scenario.review_approve(test_name=test_name, custom_screen_text=end_text)
    vrs = ResponseParser.signature(app_client.response().data)
    if v != None:
        assert v == vrs[0]
        assert r == vrs[1]
        assert s == vrs[2]


def common_rejected(firmware: Firmware,
           backend: BackendInterface,
           scenario: NavigateWithScenario,
           test_name: str,
           delegate: bytes,
           nonce: int,
           chain_id: int):

    app_client = EthAppClient(backend)

#    try:
#        with app_client.sign_eip7702_authorization(BIP32_PATH, delegate, nonce, chain_id):
#            scenario.review_reject(custom_screen_text=".*7702*")
    try:
        auth_params = TxAuth7702(BIP32_PATH, delegate, nonce, chain_id)
        with app_client.sign_eip7702_authorization(auth_params):
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
    common(firmware, backend, scenario_navigator, test_name, TEST_ADDRESS_1, NONCE, CHAIN_ID_1, 
        bytes.fromhex("00"),
        bytes.fromhex("f82e 50a7 55fa 989f 4bb9 b36b 15af b442 4ce9 cda6 9752 fd17 a7eb 1473 7d96 3e62"),
        bytes.fromhex("07c9 c91d 6140 b45e a52f 29de 7a5e ffb9 dd34 0607 a26e 225c 4027 8e91 c405 4492"))

def test_eip7702_in_whitelist_all_chain_whitelisted(firmware: Firmware,
                                backend: BackendInterface,
                                scenario_navigator: NavigateWithScenario,
                                test_name: str):
    if firmware == Firmware.NANOS:
        pytest.skip("Not supported on LNS")
    settings_toggle(firmware, scenario_navigator.navigator, [SettingID.EIP7702])
    common(firmware, backend, scenario_navigator, test_name, TEST_ADDRESS_0, NONCE, CHAIN_ID_2,
        bytes.fromhex("00"),
        bytes.fromhex("0378 f7ac 482e c728 b65d 19d0 3943 bbb3 fe73 07c7 2c64 6e7d 2d0c 11be e81e b2b9"),
        bytes.fromhex("3322 66ec 3ef9 96bf 835c 50a8 3300 6b4c 8039 8d59 7d0e 6846 19db 4d51 a384 a38d"))

def test_eip7702_in_whitelist_all_chain_param(firmware: Firmware,
                                backend: BackendInterface,
                                scenario_navigator: NavigateWithScenario,
                                test_name: str):
    if firmware == Firmware.NANOS:
        pytest.skip("Not supported on LNS")
    settings_toggle(firmware, scenario_navigator.navigator, [SettingID.EIP7702])
    common(firmware, backend, scenario_navigator, test_name, TEST_ADDRESS_2, NONCE, CHAIN_ID_0,
        bytes.fromhex("01"),
        bytes.fromhex("a24f 35ca fc6b 408c e325 39d4 bd89 a67e dd4d 6303 fc67 6dfd df93 b984 05b7 ee5e"),
        bytes.fromhex("1594 56ba be65 6692 959c a3d8 29ca 269e 8f82 387c 91e4 0a33 633d 190d da7a 3c5c"))

def test_eip7702_in_whitelist_max(firmware: Firmware,
                                backend: BackendInterface,
                                scenario_navigator: NavigateWithScenario,
                                test_name: str):
    if firmware == Firmware.NANOS:
        pytest.skip("Not supported on LNS")
    settings_toggle(firmware, scenario_navigator.navigator, [SettingID.EIP7702])
    common(firmware, backend, scenario_navigator, test_name, TEST_ADDRESS_MAX, NONCE_MAX, CHAIN_ID_MAX,
        bytes.fromhex("00"),
        bytes.fromhex("a07c 6808 9449 8c21 e80f ebb0 11ae 5d62 ede6 645d 77d7 c902 db06 5d5a 082d d220"),
        bytes.fromhex("6b5c 6222 5cf6 5a62 40c2 583d acc1 641c 8d69 2ed3 bd00 473c c3e2 8fe1 a4f5 2294"))


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
