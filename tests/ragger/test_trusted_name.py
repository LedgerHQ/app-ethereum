from pathlib import Path
import pytest
from web3 import Web3

from ledgered.devices import DeviceType

from ragger.backend import BackendInterface
from ragger.error import ExceptionRAPDU
from ragger.navigator.navigation_scenario import NavigateWithScenario
from ragger.navigator import Navigator, NavInsID, NavIns

from dynamic_networks_cfg import get_network_config

import client.response_parser as ResponseParser
from client.client import EthAppClient, TrustedNameType, TrustedNameSource
from client.status_word import StatusWord
from client.dynamic_networks import DynamicNetwork


# Values used across all tests
CHAIN_ID = 1
NAME = "ledger.eth"
ADDR = bytes.fromhex("0011223344556677889900112233445566778899")
KEY_ID = 1
ALGO_ID = 1
BIP32_PATH = "m/44'/60'/0'/0/0"
NONCE = 21
GAS_PRICE = 13
GAS_LIMIT = 21000
AMOUNT = 1.22


def common(app_client: EthAppClient) -> int:
    challenge = app_client.get_challenge()
    return ResponseParser.challenge(challenge.data)


def test_trusted_name_v1(scenario_navigator: NavigateWithScenario,
                         test_name: str):
    backend = scenario_navigator.backend
    app_client = EthAppClient(backend)

    challenge = common(app_client)

    app_client.provide_trusted_name_v1(ADDR, NAME, challenge)

    with app_client.sign(BIP32_PATH,
                         {
                             "nonce": NONCE,
                             "gasPrice": Web3.to_wei(GAS_PRICE, "gwei"),
                             "gas": GAS_LIMIT,
                             "to": ADDR,
                             "value": Web3.to_wei(AMOUNT, "ether"),
                             "chainId": CHAIN_ID
                         }):
        scenario_navigator.review_approve(test_name=test_name)


def test_trusted_name_v1_verbose(navigator: Navigator,
                                 scenario_navigator: NavigateWithScenario,
                                 default_screenshot_path: Path,
                                 test_name: str):
    backend = scenario_navigator.backend
    app_client = EthAppClient(backend)
    device = backend.device

    challenge = common(app_client)

    app_client.provide_trusted_name_v1(ADDR, NAME, challenge)

    moves = []
    if device.is_nano:
        moves += [NavInsID.RIGHT_CLICK] * 3
        moves += [NavInsID.BOTH_CLICK] + [NavInsID.RIGHT_CLICK] + [NavInsID.BOTH_CLICK]
    else:
        moves += [NavInsID.SWIPE_CENTER_TO_LEFT]
        ENS_POSITIONS = {
            DeviceType.FLEX: (420, 380),
            DeviceType.STAX: (350, 360)
        }
        moves += [NavIns(NavInsID.TOUCH, ENS_POSITIONS[device.type])]
        moves += [NavInsID.LEFT_HEADER_TAP]

    with app_client.sign(BIP32_PATH,
                         {
                             "nonce": NONCE,
                             "gasPrice": Web3.to_wei(GAS_PRICE, "gwei"),
                             "gas": GAS_LIMIT,
                             "to": ADDR,
                             "value": Web3.to_wei(AMOUNT, "ether"),
                             "chainId": CHAIN_ID
                         }):
        navigator.navigate_and_compare(default_screenshot_path,
                                       f"{test_name}/part1",
                                       moves,
                                       screen_change_after_last_instruction=False)
        scenario_navigator.review_approve(test_name=f"{test_name}/part2")


def test_trusted_name_v1_wrong_challenge(backend: BackendInterface):
    app_client = EthAppClient(backend)
    challenge = common(app_client)

    with pytest.raises(ExceptionRAPDU) as e:
        app_client.provide_trusted_name_v1(ADDR, NAME, ~challenge & 0xffffffff)
    assert e.value.status == StatusWord.INVALID_DATA


def test_trusted_name_v1_wrong_addr(scenario_navigator: NavigateWithScenario, test_name: str):
    backend = scenario_navigator.backend
    app_client = EthAppClient(backend)
    challenge = common(app_client)

    app_client.provide_trusted_name_v1(ADDR, NAME, challenge)

    addr = bytearray(ADDR)
    addr.reverse()

    with app_client.sign(BIP32_PATH,
                         {
                             "nonce": NONCE,
                             "gasPrice": Web3.to_wei(GAS_PRICE, "gwei"),
                             "gas": GAS_LIMIT,
                             "to": bytes(addr),
                             "value": Web3.to_wei(AMOUNT, "ether"),
                             "chainId": CHAIN_ID
                         }):

        scenario_navigator.review_approve(test_name=test_name)


def test_trusted_name_v1_non_mainnet(scenario_navigator: NavigateWithScenario, test_name: str):
    backend = scenario_navigator.backend
    app_client = EthAppClient(backend)

    challenge = common(app_client)

    tx_params: dict = {
        "nonce": NONCE,
        "gasPrice": Web3.to_wei(GAS_PRICE, "gwei"),
        "gas": GAS_LIMIT,
        "to": ADDR,
        "value": Web3.to_wei(AMOUNT, "ether"),
        "chainId": 5
    }

    # Send Network information (name, ticker, icon)
    name, ticker, icon = get_network_config(backend.device.type, tx_params["chainId"])
    if name and ticker:
        app_client.provide_network_information(DynamicNetwork(name, ticker, tx_params["chainId"], icon))

    app_client.provide_trusted_name_v1(ADDR, NAME, challenge)

    with app_client.sign(BIP32_PATH, tx_params):
        scenario_navigator.review_approve(test_name=test_name)


def test_trusted_name_v1_unknown_chain(scenario_navigator: NavigateWithScenario, test_name: str):
    backend = scenario_navigator.backend
    app_client = EthAppClient(backend)
    challenge = common(app_client)

    app_client.provide_trusted_name_v1(ADDR, NAME, challenge)

    with app_client.sign(BIP32_PATH,
                         {
                             "nonce": NONCE,
                             "gasPrice": Web3.to_wei(GAS_PRICE, "gwei"),
                             "gas": GAS_LIMIT,
                             "to": ADDR,
                             "value": Web3.to_wei(AMOUNT, "ether"),
                             "chainId": 9
                         }):

        scenario_navigator.review_approve(test_name=test_name)


def test_trusted_name_v1_name_too_long(backend: BackendInterface):
    app_client = EthAppClient(backend)
    challenge = common(app_client)

    with pytest.raises(ExceptionRAPDU) as e:
        app_client.provide_trusted_name_v1(ADDR, "ledger" + "0"*25 + ".eth", challenge)
    assert e.value.status == StatusWord.INVALID_DATA


def test_trusted_name_v1_name_invalid_character(backend: BackendInterface):
    app_client = EthAppClient(backend)
    challenge = common(app_client)

    with pytest.raises(ExceptionRAPDU) as e:
        app_client.provide_trusted_name_v1(ADDR, "l\xe8dger.eth", challenge)
    assert e.value.status == StatusWord.INVALID_DATA


def test_trusted_name_v1_uppercase(backend: BackendInterface):
    app_client = EthAppClient(backend)
    challenge = common(app_client)

    with pytest.raises(ExceptionRAPDU) as e:
        app_client.provide_trusted_name_v1(ADDR, NAME.upper(), challenge)
    assert e.value.status == StatusWord.INVALID_DATA


def test_trusted_name_v1_name_non_ens(backend: BackendInterface):
    app_client = EthAppClient(backend)
    challenge = common(app_client)

    with pytest.raises(ExceptionRAPDU) as e:
        app_client.provide_trusted_name_v1(ADDR, "ledger.hte", challenge)
    assert e.value.status == StatusWord.INVALID_DATA


def test_trusted_name_v2(scenario_navigator: NavigateWithScenario, test_name: str):
    backend = scenario_navigator.backend
    app_client = EthAppClient(backend)
    challenge = common(app_client)

    app_client.provide_trusted_name_v2(ADDR,
                                       NAME,
                                       TrustedNameType.ACCOUNT,
                                       TrustedNameSource.ENS,
                                       CHAIN_ID,
                                       challenge=challenge)

    with app_client.sign(BIP32_PATH,
                         {
                             "nonce": NONCE,
                             "gasPrice": Web3.to_wei(GAS_PRICE, "gwei"),
                             "gas": GAS_LIMIT,
                             "to": ADDR,
                             "value": Web3.to_wei(AMOUNT, "ether"),
                             "chainId": CHAIN_ID
                         }):

        scenario_navigator.review_approve(test_name=test_name)


def test_trusted_name_v2_wrong_chainid(scenario_navigator: NavigateWithScenario, test_name: str):
    backend = scenario_navigator.backend
    app_client = EthAppClient(backend)
    challenge = common(app_client)

    app_client.provide_trusted_name_v2(ADDR,
                                       NAME,
                                       TrustedNameType.ACCOUNT,
                                       TrustedNameSource.ENS,
                                       CHAIN_ID,
                                       challenge=challenge)

    with app_client.sign(BIP32_PATH,
                         {
                             "nonce": NONCE,
                             "gasPrice": Web3.to_wei(GAS_PRICE, "gwei"),
                             "gas": GAS_LIMIT,
                             "to": ADDR,
                             "value": Web3.to_wei(AMOUNT, "ether"),
                             "chainId": CHAIN_ID + 1,
                         }):

        scenario_navigator.review_approve(test_name=test_name)


def test_trusted_name_v2_missing_challenge(backend: BackendInterface):
    app_client = EthAppClient(backend)

    with pytest.raises(ExceptionRAPDU) as e:
        app_client.provide_trusted_name_v2(ADDR,
                                           NAME,
                                           TrustedNameType.ACCOUNT,
                                           TrustedNameSource.ENS,
                                           CHAIN_ID)
    assert e.value.status == StatusWord.INVALID_DATA


def test_trusted_name_v2_expired(backend: BackendInterface, app_version: tuple[int, int, int]):
    app_client = EthAppClient(backend)
    challenge = common(app_client)

    # convert to list and reverse
    app_version_list: list[int] = list(app_version)
    app_version_list.reverse()
    # simulate a previous version number by decrementing the first non-zero value
    for idx, v in enumerate(app_version_list):
        if v > 0:
            app_version_list[idx] -= 1
            break
    # reverse and convert back
    app_version_list.reverse()
    app_version = (app_version_list[0], app_version_list[1], app_version_list[2])  # Ensure exactly 3 elements

    with pytest.raises(ExceptionRAPDU) as e:
        app_client.provide_trusted_name_v2(ADDR,
                                           NAME,
                                           TrustedNameType.ACCOUNT,
                                           TrustedNameSource.ENS,
                                           CHAIN_ID,
                                           challenge=challenge,
                                           not_valid_after=app_version)
    assert e.value.status == StatusWord.INVALID_DATA
