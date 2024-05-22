from pathlib import Path
import pytest
from web3 import Web3

from ragger.backend import BackendInterface
from ragger.firmware import Firmware
from ragger.error import ExceptionRAPDU
from ragger.navigator import Navigator
from ragger.navigator.navigation_scenario import NavigateWithScenario

import client.response_parser as ResponseParser
from client.client import EthAppClient, StatusWord
from client.settings import SettingID, settings_toggle


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


@pytest.fixture(name="verbose", params=[False, True])
def verbose_fixture(request) -> bool:
    return request.param


def common(firmware: Firmware, app_client: EthAppClient) -> int:

    if firmware.device == "nanos":
        pytest.skip("Not supported on LNS")
    challenge = app_client.get_challenge()
    return ResponseParser.challenge(challenge.data)


def test_send_fund(firmware: Firmware,
                   backend: BackendInterface,
                   navigator: Navigator,
                   scenario_navigator: NavigateWithScenario,
                   default_screenshot_path: Path,
                   verbose: bool):
    app_client = EthAppClient(backend)
    challenge = common(firmware, app_client)

    if verbose:
        settings_toggle(firmware, navigator, [SettingID.VERBOSE_ENS])

    app_client.provide_domain_name(challenge, NAME, ADDR)

    with app_client.sign(BIP32_PATH,
                         {
                             "nonce": NONCE,
                             "gasPrice": Web3.to_wei(GAS_PRICE, "gwei"),
                             "gas": GAS_LIMIT,
                             "to": ADDR,
                             "value": Web3.to_wei(AMOUNT, "ether"),
                             "chainId": CHAIN_ID
                         }):
        if firmware.device.startswith("nano"):
            end_text = "Accept"
        else:
            end_text = "Sign"

        scenario_navigator.review_approve(default_screenshot_path, f"domain_name_verbose_{str(verbose)}", end_text)


def test_send_fund_wrong_challenge(firmware: Firmware, backend: BackendInterface):
    app_client = EthAppClient(backend)
    challenge = common(firmware, app_client)

    with pytest.raises(ExceptionRAPDU) as e:
        app_client.provide_domain_name(~challenge & 0xffffffff, NAME, ADDR)
    assert e.value.status == StatusWord.INVALID_DATA


def test_send_fund_wrong_addr(firmware: Firmware,
                              backend: BackendInterface,
                              scenario_navigator: NavigateWithScenario,
                              default_screenshot_path: Path):
    app_client = EthAppClient(backend)
    challenge = common(firmware, app_client)

    app_client.provide_domain_name(challenge, NAME, ADDR)

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
        if firmware.device.startswith("nano"):
            end_text = "Accept"
        else:
            end_text = "Sign"

        scenario_navigator.review_approve(default_screenshot_path, "domain_name_wrong_addr", end_text)


def test_send_fund_non_mainnet(firmware: Firmware,
                               backend: BackendInterface,
                               scenario_navigator: NavigateWithScenario,
                               default_screenshot_path: Path):
    app_client = EthAppClient(backend)
    challenge = common(firmware, app_client)

    app_client.provide_domain_name(challenge, NAME, ADDR)

    with app_client.sign(BIP32_PATH,
                         {
                             "nonce": NONCE,
                             "gasPrice": Web3.to_wei(GAS_PRICE, "gwei"),
                             "gas": GAS_LIMIT,
                             "to": ADDR,
                             "value": Web3.to_wei(AMOUNT, "ether"),
                             "chainId": 5
                         }):
        if firmware.device.startswith("nano"):
            end_text = "Accept"
        else:
            end_text = "Sign"

        scenario_navigator.review_approve(default_screenshot_path, "domain_name_non_mainnet", end_text)


def test_send_fund_unknown_chain(firmware: Firmware,
                                 backend: BackendInterface,
                                 scenario_navigator: NavigateWithScenario,
                                 default_screenshot_path: Path):
    app_client = EthAppClient(backend)
    challenge = common(firmware, app_client)

    app_client.provide_domain_name(challenge, NAME, ADDR)

    with app_client.sign(BIP32_PATH,
                         {
                             "nonce": NONCE,
                             "gasPrice": Web3.to_wei(GAS_PRICE, "gwei"),
                             "gas": GAS_LIMIT,
                             "to": ADDR,
                             "value": Web3.to_wei(AMOUNT, "ether"),
                             "chainId": 9
                         }):
        if firmware.device.startswith("nano"):
            end_text = "Accept"
        else:
            end_text = "Sign"

        scenario_navigator.review_approve(default_screenshot_path, "domain_name_unknown_chain", end_text)


def test_send_fund_domain_too_long(firmware: Firmware, backend: BackendInterface):
    app_client = EthAppClient(backend)
    challenge = common(firmware, app_client)

    with pytest.raises(ExceptionRAPDU) as e:
        app_client.provide_domain_name(challenge, "ledger" + "0"*25 + ".eth", ADDR)
    assert e.value.status == StatusWord.INVALID_DATA


def test_send_fund_domain_invalid_character(firmware: Firmware, backend: BackendInterface):
    app_client = EthAppClient(backend)
    challenge = common(firmware, app_client)

    with pytest.raises(ExceptionRAPDU) as e:
        app_client.provide_domain_name(challenge, "l\xe8dger.eth", ADDR)
    assert e.value.status == StatusWord.INVALID_DATA


def test_send_fund_uppercase(firmware: Firmware, backend: BackendInterface):
    app_client = EthAppClient(backend)
    challenge = common(firmware, app_client)

    with pytest.raises(ExceptionRAPDU) as e:
        app_client.provide_domain_name(challenge, NAME.upper(), ADDR)
    assert e.value.status == StatusWord.INVALID_DATA


def test_send_fund_domain_non_ens(firmware: Firmware, backend: BackendInterface):
    app_client = EthAppClient(backend)
    challenge = common(firmware, app_client)

    with pytest.raises(ExceptionRAPDU) as e:
        app_client.provide_domain_name(challenge, "ledger.hte", ADDR)
    assert e.value.status == StatusWord.INVALID_DATA
