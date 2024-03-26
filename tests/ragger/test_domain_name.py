import pytest
from web3 import Web3

import ledger_app_clients.ethereum.response_parser as ResponseParser
from ledger_app_clients.ethereum.client import EthAppClient, StatusWord
from ledger_app_clients.ethereum.settings import SettingID, settings_toggle
from ragger.backend import BackendInterface
from ragger.firmware import Firmware
from ragger.error import ExceptionRAPDU
from ragger.navigator import Navigator, NavInsID

from constants import ROOT_SNAPSHOT_PATH


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
        moves = []
        if firmware.device.startswith("nano"):
            moves += [NavInsID.RIGHT_CLICK] * 4
            if verbose:
                moves += [NavInsID.RIGHT_CLICK]
            moves += [NavInsID.BOTH_CLICK]
        else:
            moves += [NavInsID.USE_CASE_REVIEW_TAP] * 2
            if verbose:
                moves += [NavInsID.USE_CASE_REVIEW_TAP]
            moves += [NavInsID.USE_CASE_REVIEW_CONFIRM]
        navigator.navigate_and_compare(ROOT_SNAPSHOT_PATH,
                                       "domain_name_verbose_" + str(verbose),
                                       moves)


def test_send_fund_wrong_challenge(firmware: Firmware, backend: BackendInterface):
    app_client = EthAppClient(backend)
    challenge = common(firmware, app_client)

    with pytest.raises(ExceptionRAPDU) as e:
        app_client.provide_domain_name(~challenge & 0xffffffff, NAME, ADDR)
    assert e.value.status == StatusWord.INVALID_DATA


def test_send_fund_wrong_addr(firmware: Firmware,
                              backend: BackendInterface,
                              navigator: Navigator):
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
        moves = []
        if firmware.device.startswith("nano"):
            moves += [NavInsID.RIGHT_CLICK] * 4
            moves += [NavInsID.BOTH_CLICK]
        else:
            moves += [NavInsID.USE_CASE_REVIEW_TAP] * 2
            moves += [NavInsID.USE_CASE_REVIEW_CONFIRM]
        navigator.navigate_and_compare(ROOT_SNAPSHOT_PATH,
                                       "domain_name_wrong_addr",
                                       moves)


def test_send_fund_non_mainnet(firmware: Firmware,
                               backend: BackendInterface,
                               navigator: Navigator):
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
        moves = []
        if firmware.device.startswith("nano"):
            moves += [NavInsID.RIGHT_CLICK] * 5
            moves += [NavInsID.BOTH_CLICK]
        else:
            moves += [NavInsID.USE_CASE_REVIEW_TAP] * 2
            moves += [NavInsID.USE_CASE_REVIEW_CONFIRM]
        navigator.navigate_and_compare(ROOT_SNAPSHOT_PATH,
                                       "domain_name_non_mainnet",
                                       moves)


def test_send_fund_unknown_chain(firmware: Firmware,
                                 backend: BackendInterface,
                                 navigator: Navigator):
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
        moves = []
        if firmware.device.startswith("nano"):
            moves += [NavInsID.RIGHT_CLICK] * 5
            moves += [NavInsID.BOTH_CLICK]
        else:
            moves += [NavInsID.USE_CASE_REVIEW_TAP] * 3
            moves += [NavInsID.USE_CASE_REVIEW_CONFIRM]
        navigator.navigate_and_compare(ROOT_SNAPSHOT_PATH,
                                       "domain_name_unknown_chain",
                                       moves)


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
