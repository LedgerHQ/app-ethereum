from pathlib import Path
import json
import pytest
from web3 import Web3

from client.client import EthAppClient, StatusWord

from ragger.backend import BackendInterface
from ragger.firmware import Firmware
from ragger.navigator import Navigator, NavInsID
from ragger.error import ExceptionRAPDU

from constants import ABIS_FOLDER


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
        with app_client.sign("m/44'/60'/0'/0/0", tx_params):
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
