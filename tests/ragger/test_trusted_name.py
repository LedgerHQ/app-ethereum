from pathlib import Path
import json

import pytest
from web3 import Web3

from ledgered.devices import DeviceType

from ragger.backend import BackendInterface
from ragger.error import ExceptionRAPDU
from ragger.navigator.navigation_scenario import NavigateWithScenario
from ragger.navigator import Navigator, NavInsID, NavIns

from dynamic_networks_cfg import get_network_config

from client.utils import CoinType
import client.response_parser as ResponseParser
from client.client import EthAppClient, SignMode
from client.status_word import StatusWord
from client.dynamic_networks import DynamicNetwork
from client.trusted_name import TrustedName, TrustedNameType, TrustedNameSource

from constants import ABIS_FOLDER

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

    app_client.provide_trusted_name(TrustedName(1, ADDR, NAME, challenge=challenge, coin_type=CoinType.ETH))

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

    app_client.provide_trusted_name(TrustedName(1, ADDR, NAME, challenge=challenge, coin_type=CoinType.ETH))

    moves = []
    if device.is_nano:
        moves += [NavInsID.RIGHT_CLICK] * 3
        moves += [NavInsID.BOTH_CLICK] + [NavInsID.RIGHT_CLICK] + [NavInsID.BOTH_CLICK]
    else:
        moves += [NavInsID.SWIPE_CENTER_TO_LEFT]
        ENS_POSITIONS = {
            DeviceType.FLEX: (420, 380),
            DeviceType.STAX: (350, 360),
            DeviceType.APEX_P: (270, 250)
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
        app_client.provide_trusted_name(TrustedName(1, ADDR, NAME, challenge=(~challenge & 0xffffffff), coin_type=CoinType.ETH))
    assert e.value.status == StatusWord.INVALID_DATA


def test_trusted_name_v1_wrong_addr(scenario_navigator: NavigateWithScenario, test_name: str):
    backend = scenario_navigator.backend
    app_client = EthAppClient(backend)
    challenge = common(app_client)

    app_client.provide_trusted_name(TrustedName(1, ADDR, NAME, challenge=challenge, coin_type=CoinType.ETH))

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

    app_client.provide_trusted_name(TrustedName(1, ADDR, NAME, challenge=challenge, coin_type=CoinType.ETH))

    with app_client.sign(BIP32_PATH, tx_params):
        scenario_navigator.review_approve(test_name=test_name)


def test_trusted_name_v1_unknown_chain(scenario_navigator: NavigateWithScenario, test_name: str):
    backend = scenario_navigator.backend
    app_client = EthAppClient(backend)
    challenge = common(app_client)

    app_client.provide_trusted_name(TrustedName(1, ADDR, NAME, challenge=challenge, coin_type=CoinType.ETH))

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
        app_client.provide_trusted_name(TrustedName(1, ADDR, "ledger" + "0"*25 + ".eth", challenge=challenge, coin_type=CoinType.ETH))
    assert e.value.status == StatusWord.INVALID_DATA


def test_trusted_name_v1_name_invalid_character(backend: BackendInterface):
    app_client = EthAppClient(backend)
    challenge = common(app_client)

    with pytest.raises(ExceptionRAPDU) as e:
        app_client.provide_trusted_name(TrustedName(1, ADDR, "l\xe8dger.eth", challenge=challenge, coin_type=CoinType.ETH))
    assert e.value.status == StatusWord.INVALID_DATA


def test_trusted_name_v1_uppercase(backend: BackendInterface):
    app_client = EthAppClient(backend)
    challenge = common(app_client)

    with pytest.raises(ExceptionRAPDU) as e:
        app_client.provide_trusted_name(TrustedName(1, ADDR, NAME.upper(), challenge=challenge, coin_type=CoinType.ETH))
    assert e.value.status == StatusWord.INVALID_DATA


def test_trusted_name_v1_name_non_ens(backend: BackendInterface):
    app_client = EthAppClient(backend)
    challenge = common(app_client)

    with pytest.raises(ExceptionRAPDU) as e:
        app_client.provide_trusted_name(TrustedName(1, ADDR, "ledger.hte", challenge=challenge, coin_type=CoinType.ETH))
    assert e.value.status == StatusWord.INVALID_DATA


def test_trusted_name_v2(scenario_navigator: NavigateWithScenario, test_name: str):
    backend = scenario_navigator.backend
    app_client = EthAppClient(backend)
    challenge = common(app_client)

    app_client.provide_trusted_name(TrustedName(2,
                                                ADDR,
                                                NAME,
                                                tn_type=TrustedNameType.ACCOUNT,
                                                tn_source=TrustedNameSource.ENS,
                                                chain_id=CHAIN_ID,
                                                challenge=challenge))

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

    app_client.provide_trusted_name(TrustedName(2,
                                                ADDR,
                                                NAME,
                                                tn_type=TrustedNameType.ACCOUNT,
                                                tn_source=TrustedNameSource.ENS,
                                                chain_id=CHAIN_ID,
                                                challenge=challenge))

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
        app_client.provide_trusted_name(TrustedName(2,
                                                    ADDR,
                                                    NAME,
                                                    tn_type=TrustedNameType.ACCOUNT,
                                                    tn_source=TrustedNameSource.ENS,
                                                    chain_id=CHAIN_ID))
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
        app_client.provide_trusted_name(TrustedName(2,
                                                    ADDR,
                                                    NAME,
                                                    tn_type=TrustedNameType.ACCOUNT,
                                                    tn_source=TrustedNameSource.ENS,
                                                    chain_id=CHAIN_ID,
                                                    challenge=challenge,
                                                    not_valid_after=app_version))
    assert e.value.status == StatusWord.INVALID_DATA


def test_trusted_name_mab_idle(backend: BackendInterface) -> None:
    app_client = EthAppClient(backend)

    with pytest.raises(ExceptionRAPDU) as e:
        # This will fail since outside of signing a TX/message, the derivation path is unset/empty
        app_client.provide_trusted_name(TrustedName(2,
                                                    ADDR,
                                                    NAME,
                                                    tn_type=TrustedNameType.ACCOUNT,
                                                    tn_source=TrustedNameSource.MULTISIG_ADDRESS_BOOK,
                                                    chain_id=CHAIN_ID,
                                                    challenge=ResponseParser.challenge(app_client.get_challenge().data),
                                                    owner=b"\x11" * 20))
    assert e.value.status == StatusWord.INVALID_DATA


def test_trusted_name_mab_wrong_owner(backend: BackendInterface) -> None:
    app_client = EthAppClient(backend)

    with Path(f"{ABIS_FOLDER}/erc20.json").open(encoding="utf-8") as file:
        contract = Web3().eth.contract(
            abi=json.load(file),
            address=None
        )
    data = contract.encode_abi("transfer", [
        bytes.fromhex("d8dA6BF26964aF9D7eEd9e03E53415D37aA96045"),
        int(100 * pow(10, 6)),
    ])
    tx_params = {
        "chainId": 1,
        "nonce": 1337,
        "maxPriorityFeePerGas": Web3.to_wei(0, "gwei"),
        "maxFeePerGas": Web3.to_wei(2.55, "gwei"),
        "gas": 94548,
        "to": bytes.fromhex("dAC17F958D2ee523a2206206994597C13D831ec7"),
        "value": Web3.to_wei(0, "ether"),
        "data": data,
    }
    with app_client.sign("m/44'/60'/0'/0/0", tx_params, mode=SignMode.STORE):
        pass
    with pytest.raises(ExceptionRAPDU) as e:
        app_client.provide_trusted_name(TrustedName(2,
                                                    ADDR,
                                                    NAME,
                                                    tn_type=TrustedNameType.ACCOUNT,
                                                    tn_source=TrustedNameSource.MULTISIG_ADDRESS_BOOK,
                                                    chain_id=tx_params["chainId"],
                                                    challenge=ResponseParser.challenge(app_client.get_challenge().data),
                                                    owner=b"\x00" * 20))
    assert e.value.status == StatusWord.INVALID_DATA
