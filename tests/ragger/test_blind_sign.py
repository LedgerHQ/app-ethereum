from pathlib import Path
import json
from typing import Optional
import pytest
from web3 import Web3

from ragger.backend import BackendInterface
from ragger.firmware import Firmware
from ragger.navigator import Navigator, NavInsID
from ragger.error import ExceptionRAPDU

from constants import ABIS_FOLDER

from client.client import EthAppClient, StatusWord
from client.settings import SettingID, settings_toggle
import client.response_parser as ResponseParser
from client.utils import recover_transaction


BIP32_PATH = "m/44'/60'/0'/0/0"
DEVICE_ADDR: Optional[bytes] = None

# TODO: do one test with nonce display


@pytest.fixture(name="sign", params=[True, False])
def sign_fixture(request) -> bool:
    return request.param


def common_tx_params() -> dict:
    with open(f"{ABIS_FOLDER}/erc20.json", encoding="utf-8") as file:
        contract = Web3().eth.contract(
            abi=json.load(file),
            address=None
        )
    data = contract.encodeABI("approve", [
        # Uniswap Protocol: Permit2
        bytes.fromhex("000000000022d473030f116ddee9f6b43ac78ba3"),
        Web3.to_wei("2", "ether")
    ])
    return {
        "nonce": 235,
        "maxFeePerGas": Web3.to_wei(100, "gwei"),
        "maxPriorityFeePerGas": Web3.to_wei(10, "gwei"),
        "gas": 44001,
        # Maker: Dai Stablecoin
        "to": bytes.fromhex("6b175474e89094c44da98b954eedeac495271d0f"),
        "data": data,
        "chainId": 1
    }


# Token approval, would require loading the "internal plugin" &
# providing the token metadata from the CAL
def test_blind_sign(firmware: Firmware,
                    backend: BackendInterface,
                    navigator: Navigator,
                    default_screenshot_path: Path,
                    test_name: str,
                    sign: bool):
    global DEVICE_ADDR
    app_client = EthAppClient(backend)

    settings_toggle(firmware, navigator, [SettingID.BLIND_SIGNING])
    if DEVICE_ADDR is None:
        with app_client.get_public_addr(bip32_path=BIP32_PATH, display=False):
            pass
        _, DEVICE_ADDR, _ = ResponseParser.pk_addr(app_client.response().data)

    tx_params = common_tx_params()
    try:
        with app_client.sign(BIP32_PATH, tx_params):
            if sign:
                test_name += "_signed"
            else:
                test_name += "_rejected"

            moves = []
            if firmware.is_nano:
                # blind signing warning
                moves += [NavInsID.RIGHT_CLICK]

                # review | from | amount | to | fees
                moves += [NavInsID.RIGHT_CLICK] * 5

                if firmware == Firmware.NANOS:
                    # for the two addresses
                    moves += [NavInsID.RIGHT_CLICK] * 4

                if not sign:
                    moves += [NavInsID.RIGHT_CLICK]

                moves += [NavInsID.BOTH_CLICK]
            else:
                moves += [NavInsID.USE_CASE_CHOICE_REJECT]

                if firmware == Firmware.STAX:
                    moves += [NavInsID.SWIPE_CENTER_TO_LEFT] * 2
                else:
                    moves += [NavInsID.SWIPE_CENTER_TO_LEFT] * 3

                if sign:
                    moves += [NavInsID.USE_CASE_REVIEW_CONFIRM]
                else:
                    moves += [NavInsID.USE_CASE_REVIEW_REJECT]
                    moves += [NavInsID.USE_CASE_CHOICE_CONFIRM]

            navigator.navigate_and_compare(default_screenshot_path,
                                           test_name,
                                           moves)
    except ExceptionRAPDU as e:
        assert not sign
        assert e.status == StatusWord.CONDITION_NOT_SATISFIED
    else:
        assert sign
        # verify signature
        vrs = ResponseParser.signature(app_client.response().data)
        addr = recover_transaction(tx_params, vrs)
        assert addr == DEVICE_ADDR


def test_blind_sign_reject_in_risk_review(firmware: Firmware,
                                          backend: BackendInterface,
                                          navigator: Navigator):
    app_client = EthAppClient(backend)

    if firmware.is_nano:
        pytest.skip("Not supported on non-NBGL apps")

    settings_toggle(firmware, navigator, [SettingID.BLIND_SIGNING])

    try:
        with app_client.sign(BIP32_PATH, common_tx_params()):
            moves = [NavInsID.USE_CASE_CHOICE_CONFIRM]
            navigator.navigate(moves)
    except ExceptionRAPDU as e:
        assert e.status == StatusWord.CONDITION_NOT_SATISFIED
    else:
        assert False  # Should have thrown


# Token approval, would require loading the "internal plugin" &
# providing the token metadata from the CAL
def test_sign_parameter_selector(firmware: Firmware,
                                 backend: BackendInterface,
                                 navigator: Navigator,
                                 test_name: str,
                                 default_screenshot_path: Path):
    global DEVICE_ADDR
    app_client = EthAppClient(backend)

    if DEVICE_ADDR is None:
        with app_client.get_public_addr(bip32_path=BIP32_PATH, display=False):
            pass
        _, DEVICE_ADDR, _ = ResponseParser.pk_addr(app_client.response().data)

    settings_toggle(firmware, navigator, [SettingID.BLIND_SIGNING, SettingID.DEBUG_DATA])

    tx_params = common_tx_params()
    data_len = len(bytes.fromhex(tx_params["data"][2:]))
    params = (data_len - 4) // 32
    with app_client.sign(BIP32_PATH, tx_params):
        moves = []
        if firmware.is_nano:
            # verify | selector
            moves += [NavInsID.RIGHT_CLICK] * 2 + [NavInsID.BOTH_CLICK]
            if firmware == Firmware.NANOS:
                # hardcoded for each because for some params take more pages than others
                # parameter 1
                moves += [NavInsID.RIGHT_CLICK] * 4 + [NavInsID.BOTH_CLICK]
                # parameter 2
                moves += [NavInsID.RIGHT_CLICK] * 3 + [NavInsID.BOTH_CLICK]
                # blind signing | review | from | amount | to | fees
                moves += [NavInsID.RIGHT_CLICK] * 10
            else:
                # (verify | parameter) * flows
                moves += ([NavInsID.RIGHT_CLICK] * 2 + [NavInsID.BOTH_CLICK]) * params
                # blind signing | review | from | amount | to | fees
                moves += [NavInsID.RIGHT_CLICK] * 6
            moves += [NavInsID.BOTH_CLICK]
        else:
            moves += ([NavInsID.SWIPE_CENTER_TO_LEFT] * 2 + [NavInsID.USE_CASE_REVIEW_CONFIRM]) * (1 + params)
            moves += [NavInsID.USE_CASE_CHOICE_REJECT]
            if firmware == Firmware.STAX:
                tap_number = 2
            else:
                tap_number = 3
            moves += [NavInsID.SWIPE_CENTER_TO_LEFT] * tap_number
            moves += [NavInsID.USE_CASE_REVIEW_CONFIRM]
        navigator.navigate_and_compare(default_screenshot_path,
                                       test_name,
                                       moves)

    # verify signature
    vrs = ResponseParser.signature(app_client.response().data)
    addr = recover_transaction(tx_params, vrs)
    assert addr == DEVICE_ADDR


def test_blind_sign_not_enabled_error(firmware: Firmware,
                                      backend: BackendInterface,
                                      navigator: Navigator,
                                      test_name: str,
                                      default_screenshot_path: Path):
    app_client = EthAppClient(backend)

    try:
        with app_client.sign(BIP32_PATH, common_tx_params()):
            moves = []
            if firmware.is_nano:
                if firmware == Firmware.NANOS:
                    moves += [NavInsID.RIGHT_CLICK]
                moves += [NavInsID.BOTH_CLICK]
            else:
                moves += [NavInsID.USE_CASE_CHOICE_REJECT]
            navigator.navigate_and_compare(default_screenshot_path,
                                           test_name,
                                           moves)
    except ExceptionRAPDU as e:
        assert e.status == StatusWord.INVALID_DATA
    else:
        assert False  # Should have thrown
