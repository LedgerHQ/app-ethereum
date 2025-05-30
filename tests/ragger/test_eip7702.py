from typing import Optional

from ragger.error import ExceptionRAPDU
from ragger.navigator.navigation_scenario import NavigateWithScenario
from ragger.navigator import NavInsID

from client.client import EthAppClient
from client.status_word import StatusWord
from client.settings import SettingID, settings_toggle
import client.response_parser as ResponseParser
from client.tx_auth_7702 import TxAuth7702

from client.utils import recover_authorization

BIP32_PATH = "m/44'/60'/0'/0/0"
ADDRESS_REVOCATION = bytes.fromhex("00" * 20)
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

DEVICE_ADDR: Optional[bytes] = None

# pylint: disable=line-too-long
# Test vectors computed with
# cast wallet sign-auth $ADDRESS --mnemonic $MNEMONIC --mnemonic-derivation-path "m/44'/60'/0'/0/0" --nonce $NONCE --chain $CHAINID
# Decoded by https://codechain-io.github.io/rlp-debugger/
# pylint: enable=line-too-long


def common(scenario_navigator: NavigateWithScenario,
           test_name: str,
           delegate: bytes,
           nonce: int,
           chain_id: int):

    global DEVICE_ADDR
    backend = scenario_navigator.backend
    app_client = EthAppClient(backend)

    if DEVICE_ADDR is None:
        with app_client.get_public_addr(bip32_path=BIP32_PATH, display=False):
            pass
        _, DEVICE_ADDR, _ = ResponseParser.pk_addr(app_client.response().data)
    auth_params = TxAuth7702(delegate, nonce, chain_id)
    with app_client.sign_eip7702_authorization(BIP32_PATH, auth_params):
        scenario_navigator.review_approve(test_name=test_name)
    vrs = ResponseParser.signature(app_client.response().data)
    assert recover_authorization(chain_id, nonce, delegate, vrs) == DEVICE_ADDR


def common_rejected(scenario_navigator: NavigateWithScenario,
                    test_name: str,
                    delegate: bytes,
                    nonce: int,
                    chain_id: int):

    backend = scenario_navigator.backend
    app_client = EthAppClient(backend)
    device = backend.device

    try:
        auth_params = TxAuth7702(delegate, nonce, chain_id)
        with app_client.sign_eip7702_authorization(BIP32_PATH, auth_params):
            moves = []
            if device.is_nano:
                moves += [NavInsID.RIGHT_CLICK] * 3
                moves += [NavInsID.BOTH_CLICK]
            else:
                moves += [NavInsID.USE_CASE_CHOICE_REJECT]
            scenario_navigator.navigator.navigate_and_compare(scenario_navigator.screenshot_path,
                                                    test_name,
                                                    moves)

    except ExceptionRAPDU as e:
        assert e.status == StatusWord.CONDITION_NOT_SATISFIED
    else:
        assert False  # An exception should have been raised


def test_eip7702_in_whitelist(scenario_navigator: NavigateWithScenario, test_name: str):
    device = scenario_navigator.backend.device
    settings_toggle(device, scenario_navigator.navigator, [SettingID.EIP7702])
    common(scenario_navigator,
           test_name,
           TEST_ADDRESS_1,
           NONCE,
           CHAIN_ID_1)


def test_eip7702_in_whitelist_all_chain_whitelisted(scenario_navigator: NavigateWithScenario, test_name: str):
    device = scenario_navigator.backend.device
    settings_toggle(device, scenario_navigator.navigator, [SettingID.EIP7702])
    common(scenario_navigator,
           test_name,
           # Simple7702Account, which is whitelisted for all chains
           bytes.fromhex("4Cd241E8d1510e30b2076397afc7508Ae59C66c9"),
           NONCE,
           CHAIN_ID_2)


def test_eip7702_in_whitelist_all_chain_param(scenario_navigator: NavigateWithScenario, test_name: str):
    device = scenario_navigator.backend.device
    settings_toggle(device, scenario_navigator.navigator, [SettingID.EIP7702])
    common(scenario_navigator,
           test_name,
           TEST_ADDRESS_2,
           NONCE,
           CHAIN_ID_0)


def test_eip7702_in_whitelist_max(scenario_navigator: NavigateWithScenario, test_name: str):
    device = scenario_navigator.backend.device
    settings_toggle(device, scenario_navigator.navigator, [SettingID.EIP7702])
    common(scenario_navigator,
           test_name,
           TEST_ADDRESS_MAX,
           NONCE_MAX,
           CHAIN_ID_MAX)


def test_eip7702_in_whitelist_wrong_chain(scenario_navigator: NavigateWithScenario, test_name: str):
    device = scenario_navigator.backend.device
    settings_toggle(device, scenario_navigator.navigator, [SettingID.EIP7702])
    common_rejected(scenario_navigator, test_name, TEST_ADDRESS_2, NONCE, CHAIN_ID_1)


def test_eip7702_not_in_whitelist(scenario_navigator: NavigateWithScenario, test_name: str):
    device = scenario_navigator.backend.device
    settings_toggle(device, scenario_navigator.navigator, [SettingID.EIP7702])
    common_rejected(scenario_navigator, test_name, TEST_ADDRESS_NO_WHITELIST, NONCE, CHAIN_ID_1)


def test_eip7702_not_enabled(scenario_navigator: NavigateWithScenario, test_name: str):
    common_rejected(scenario_navigator, test_name, TEST_ADDRESS_1, NONCE, CHAIN_ID_1)


def test_eip7702_revocation(scenario_navigator: NavigateWithScenario, test_name: str):
    device = scenario_navigator.backend.device
    settings_toggle(device, scenario_navigator.navigator, [SettingID.EIP7702])
    common(scenario_navigator,
           test_name,
           ADDRESS_REVOCATION,
           NONCE,
           CHAIN_ID_1)
