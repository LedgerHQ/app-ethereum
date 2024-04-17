from pathlib import Path
import json
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

# Token approval, would require loading the "internal plugin" &
# providing the token metadata from the CAL
def test_blind_sign(firmware: Firmware,
                    backend: BackendInterface,
                    navigator: Navigator,
                    default_screenshot_path: Path):
    app_client = EthAppClient(backend)

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
    tx_params = {
        "nonce": 235,
        "maxFeePerGas": Web3.to_wei(100, "gwei"),
        "maxPriorityFeePerGas": Web3.to_wei(10, "gwei"),
        "gas": 44001,
        # Maker: Dai Stablecoin
        "to": bytes.fromhex("6b175474e89094c44da98b954eedeac495271d0f"),
        "data": data,
        "chainId": 1
    }
    with pytest.raises(ExceptionRAPDU) as e:
        with app_client.sign(BIP32_PATH, tx_params):
            pass
    assert e.value.status == StatusWord.INVALID_DATA

    moves = []
    if firmware.device.startswith("nano"):
        if firmware.device == "nanos":
            moves += [NavInsID.RIGHT_CLICK]
        moves += [NavInsID.BOTH_CLICK]
    else:
        moves += [NavInsID.USE_CASE_CHOICE_CONFIRM]
    navigator.navigate_and_compare(default_screenshot_path,
                                   "blind-signed_approval",
                                   moves)


# Token approval, would require loading the "internal plugin" &
# providing the token metadata from the CAL
def test_sign_parameter_selector(firmware: Firmware,
                                 backend: BackendInterface,
                                 navigator: Navigator,
                                 scenario_navigator: NavigateWithScenario,
                                 test_name: str,
                                 default_screenshot_path: Path):
    app_client = EthAppClient(backend)

    with app_client.get_public_addr(bip32_path=BIP32_PATH, display=False):
        pass
    _, DEVICE_ADDR, _ = ResponseParser.pk_addr(app_client.response().data)

    with open(f"{ABIS_FOLDER}/erc20.json", encoding="utf-8") as file:
        abi = json.load(file)

    contract_name = "approve"
    count = 0
    for elt in abi:
        if elt["name"] == contract_name:
            count = len(elt["inputs"])
            break
    assert count == 2, "Invalid inputs number"
    tx_params = {
        "nonce": 235,
        "maxFeePerGas": Web3.to_wei(100, "gwei"),
        "maxPriorityFeePerGas": Web3.to_wei(10, "gwei"),
        "gas": 44001,
        # Maker: Dai Stablecoin
        "to": bytes.fromhex("6b175474e89094c44da98b954eedeac495271d0f"),
        "data": Web3().eth.contract(abi=abi).encodeABI(contract_name, [
            # Uniswap Protocol: Permit2
            bytes.fromhex("000000000022d473030f116ddee9f6b43ac78ba3"),
            Web3.to_wei("2", "ether")
        ]),
        "chainId": 1
    }

    settings_toggle(firmware, navigator, [SettingID.DEBUG_DATA, SettingID.BLIND_SIGNING])

    with app_client.sign(BIP32_PATH, tx_params):
        if firmware.device.startswith("nano"):
            end_text = "Approve"
            nav_inst = NavInsID.RIGHT_CLICK
            valid_instr = [NavInsID.BOTH_CLICK]
        else:
            end_text = "Confirm"
            nav_inst = NavInsID.USE_CASE_REVIEW_TAP
            valid_instr = [NavInsID.USE_CASE_REVIEW_CONFIRM]

        # Loop for "Selector" + the contract inputs
        for step in range(count + 1):
            navigator.navigate_until_text_and_compare(nav_inst,
                                                      valid_instr,
                                                      end_text,
                                                      default_screenshot_path,
                                                      f"{test_name}/step_{step}",
                                                      screen_change_after_last_instruction=False)
            step +=1

        # Transaction review
        if firmware.device.startswith("nano"):
            end_text = "Accept"
        else:
            end_text = "Sign"
        scenario_navigator.review_approve(default_screenshot_path, f"{test_name}/step_{step}", end_text)

    # verify signature
    vrs = ResponseParser.signature(app_client.response().data)
    addr = recover_transaction(tx_params, vrs)
    assert addr == DEVICE_ADDR
