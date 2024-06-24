from pathlib import Path
import json
from typing import Optional
import pytest
from web3 import Web3

from ragger.backend import BackendInterface
from ragger.firmware import Firmware
from ragger.navigator import Navigator, NavInsID
from ragger.navigator.navigation_scenario import NavigateWithScenario
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
            if firmware.device.startswith("nano"):
                if firmware.device == "nanos":
                    moves += [NavInsID.RIGHT_CLICK] * 2
                else:
                    moves += [NavInsID.RIGHT_CLICK] * 4

                if not sign:
                    moves += [NavInsID.RIGHT_CLICK]

                moves += [NavInsID.BOTH_CLICK]

                if sign:
                    if firmware.device == "nanos":
                        moves += [NavInsID.RIGHT_CLICK] * 10
                    else:
                        moves += [NavInsID.RIGHT_CLICK] * 6
                    moves += [NavInsID.BOTH_CLICK]
            else:
                if sign:
                    moves += [NavInsID.USE_CASE_CHOICE_REJECT]
                    moves += [NavInsID.USE_CASE_CHOICE_CONFIRM]
                    moves += [NavInsID.USE_CASE_REVIEW_TAP] * 3
                    moves += [NavInsID.USE_CASE_REVIEW_CONFIRM]
                else:
                    moves += [NavInsID.USE_CASE_CHOICE_CONFIRM]
            navigator.navigate_and_compare(default_screenshot_path,
                                           test_name,
                                           moves)
    except ExceptionRAPDU as e:
        assert e.status == StatusWord.INVALID_DATA
    else:
        assert sign is True
        # verify signature
        vrs = ResponseParser.signature(app_client.response().data)
        addr = recover_transaction(tx_params, vrs)
        assert addr == DEVICE_ADDR


def test_blind_sign_reject_in_risk_review(firmware: Firmware,
                                          backend: BackendInterface,
                                          navigator: Navigator,
                                          default_screenshot_path: Path,
                                          test_name: str):
    app_client = EthAppClient(backend)

    if firmware.device not in ["stax", "flex"]:
        pytest.skip("Not supported on non-NBGL apps")

    try:
        with app_client.sign(BIP32_PATH, common_tx_params()):
            moves = [NavInsID.USE_CASE_CHOICE_REJECT] * 2
            navigator.navigate_and_compare(default_screenshot_path,
                                           test_name,
                                           moves)
    except ExceptionRAPDU as e:
        assert e.status == StatusWord.INVALID_DATA
    else:
        assert False  # Should have thrown


# Token approval, would require loading the "internal plugin" &
# providing the token metadata from the CAL
def test_sign_parameter_selector(firmware: Firmware,
                                 backend: BackendInterface,
                                 navigator: Navigator,
                                 scenario_navigator: NavigateWithScenario,
                                 test_name: str,
                                 default_screenshot_path: Path):
    global DEVICE_ADDR
    app_client = EthAppClient(backend)

    if DEVICE_ADDR is None:
        with app_client.get_public_addr(bip32_path=BIP32_PATH, display=False):
            pass
        _, DEVICE_ADDR, _ = ResponseParser.pk_addr(app_client.response().data)

    settings_toggle(firmware, navigator, [SettingID.DEBUG_DATA])

    tx_params = common_tx_params()
    data_len = len(bytes.fromhex(tx_params["data"][2:]))
    # selector
    flows = 1
    data_len -= 4
    # parameters
    flows += data_len // 32
    with app_client.sign(BIP32_PATH, tx_params):
        moves = []
        if firmware.device.startswith("nano"):
            if firmware.device == "nanos":
                moves += [NavInsID.RIGHT_CLICK] * 2 + [NavInsID.BOTH_CLICK]
                # Parameters on Nano S are split on multiple pages, hardcoded because the two parameters don't use the
                # same amount of pages because of non-monospace fonts
                moves += [NavInsID.RIGHT_CLICK] * 4 + [NavInsID.BOTH_CLICK]
                moves += [NavInsID.RIGHT_CLICK] * 3 + [NavInsID.BOTH_CLICK]
            else:
                moves += ([NavInsID.RIGHT_CLICK] * 2 + [NavInsID.BOTH_CLICK]) * flows

            if firmware.device == "nanos":
                moves += [NavInsID.RIGHT_CLICK] * 2
            else:
                moves += [NavInsID.RIGHT_CLICK] * 4
            moves += [NavInsID.BOTH_CLICK]

            if firmware.device == "nanos":
                pass
                moves += [NavInsID.RIGHT_CLICK] * 9
            else:
                moves += [NavInsID.RIGHT_CLICK] * 5
            moves += [NavInsID.BOTH_CLICK]
        else:
            moves += ([NavInsID.USE_CASE_REVIEW_TAP] * 2 + [NavInsID.USE_CASE_REVIEW_CONFIRM]) * flows
            moves += [NavInsID.USE_CASE_CHOICE_REJECT]
            moves += [NavInsID.USE_CASE_CHOICE_CONFIRM]
            moves += [NavInsID.USE_CASE_REVIEW_TAP] * 3
            moves += [NavInsID.USE_CASE_REVIEW_CONFIRM]
        navigator.navigate_and_compare(default_screenshot_path,
                                       test_name,
                                       moves)

    # verify signature
    vrs = ResponseParser.signature(app_client.response().data)
    addr = recover_transaction(tx_params, vrs)
    assert addr == DEVICE_ADDR
