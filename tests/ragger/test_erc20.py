import json
from typing import Optional, Callable

import pytest
from web3 import Web3

from ragger.error import ExceptionRAPDU
from ragger.backend import BackendInterface
from ragger.navigator.navigation_scenario import NavigateWithScenario

from client.client import EthAppClient
from client.status_word import StatusWord

from constants import ABIS_FOLDER
from test_sign import common as common_tx, BIP32_PATH

TOKEN_ADDR = bytes.fromhex("A0b86991c6218b36c1d19D4a2e9Eb0cE3606eB48")
TOKEN_TICKER = "USDC"
TOKEN_DECIMALS = 6
TOKEN_CHAIN_ID = 1


def test_provide_erc20_token(backend: BackendInterface):
    app_client = EthAppClient(backend)

    response = app_client.provide_token_metadata(TOKEN_TICKER,
                                                 TOKEN_ADDR,
                                                 TOKEN_DECIMALS,
                                                 TOKEN_CHAIN_ID)
    assert response.status == StatusWord.OK


def test_provide_erc20_token_error(backend: BackendInterface):
    app_client = EthAppClient(backend)

    with pytest.raises(ExceptionRAPDU) as err:
        app_client.provide_token_metadata(TOKEN_TICKER,
                                          TOKEN_ADDR,
                                          TOKEN_DECIMALS,
                                          TOKEN_CHAIN_ID,
                                          bytes.fromhex("010203"))

    assert err.value.status == StatusWord.INVALID_DATA


def common_transfer(scenario_navigator: NavigateWithScenario,
                    amount: float,
                    extra_data: Optional[bytes] = None,
                    func: Callable = common_tx):
    app_client = EthAppClient(scenario_navigator.backend)

    with open(f"{ABIS_FOLDER}/erc20.json") as file:
        contract = Web3().eth.contract(
            abi=json.load(file),
            address=None
        )
    data = contract.encode_abi("transfer", [
        bytes.fromhex("d8dA6BF26964aF9D7eEd9e03E53415D37aA96045"),
        int(amount * pow(10, TOKEN_DECIMALS)),
    ])

    if extra_data is not None:
        data += extra_data.hex()

    tx_params = {
        "chainId": TOKEN_CHAIN_ID,
        "nonce": 1337,
        "maxPriorityFeePerGas": Web3.to_wei(0, "gwei"),
        "maxFeePerGas": Web3.to_wei(2.55, "gwei"),
        "gas": 94548,
        "to": TOKEN_ADDR,
        "value": Web3.to_wei(0, "ether"),
        "data": data,
    }
    app_client.provide_token_metadata(TOKEN_TICKER, TOKEN_ADDR, TOKEN_DECIMALS, TOKEN_CHAIN_ID)
    func(scenario_navigator, tx_params, scenario_navigator.test_name)


def common_approve(scenario_navigator: NavigateWithScenario,
                   amount: float,
                   extra_data: Optional[bytes] = None,
                   func: Callable = common_tx):
    app_client = EthAppClient(scenario_navigator.backend)

    with open(f"{ABIS_FOLDER}/erc20.json") as file:
        contract = Web3().eth.contract(
            abi=json.load(file),
            address=None
        )
    data = contract.encode_abi("approve", [
        bytes.fromhex("d8dA6BF26964aF9D7eEd9e03E53415D37aA96045"),
        int(amount * pow(10, TOKEN_DECIMALS)),
    ])

    if extra_data is not None:
        data += extra_data.hex()

    tx_params = {
        "chainId": TOKEN_CHAIN_ID,
        "nonce": 1337,
        "maxPriorityFeePerGas": Web3.to_wei(1.3, "gwei"),
        "maxFeePerGas": Web3.to_wei(2.62, "gwei"),
        "gas": 36007,
        "to": TOKEN_ADDR,
        "value": Web3.to_wei(0, "ether"),
        "data": data,
    }
    app_client.provide_token_metadata(TOKEN_TICKER, TOKEN_ADDR, TOKEN_DECIMALS, TOKEN_CHAIN_ID)
    func(scenario_navigator, tx_params, scenario_navigator.test_name)


def test_transfer_erc20(scenario_navigator: NavigateWithScenario):
    common_transfer(scenario_navigator, 10)


def test_approve_erc20(scenario_navigator: NavigateWithScenario):
    common_approve(scenario_navigator, 5)


# these extra data are not part of the ABI :
# - https://github.com/ethereum/ercs/blob/master/ERCS/erc-20.md#transfer
# - https://github.com/ethereum/ercs/blob/master/ERCS/erc-20.md#approve
# they are appended to the calldata by the dApp before being sent to the smart-contract
# the smart contract is not aware of them, they are usually used for tracking purposes


def test_transfer_erc20_extra_data(scenario_navigator: NavigateWithScenario):
    common_transfer(scenario_navigator,
                    5,
                    "cpis_1RnzUSEXxObdZZOcn8gPzPPS".encode())


def test_transfer_erc20_extra_data_nonascii(scenario_navigator: NavigateWithScenario):
    common_transfer(scenario_navigator, 10, bytes.fromhex("deadcafe0042"))


def test_transfer_erc20_extra_data_toolong(scenario_navigator: NavigateWithScenario):
    def check_error(scenario_navigator: NavigateWithScenario, tx_params: dict, test_name: str):
        app_client = EthAppClient(scenario_navigator.backend)

        with pytest.raises(ExceptionRAPDU) as err:
            with app_client.sign(BIP32_PATH, tx_params):
                pass
        assert err.value.status == StatusWord.INVALID_DATA

    common_transfer(scenario_navigator, 10, "Lorem ipsum dolor sit amet, consectetur adipiscing elit, sed do eiusmod tempor incididunt ut labore et dolore magna aliqua.".encode(), func=check_error)


def test_transfer_erc20_extra_data_nonascii_truncated(scenario_navigator: NavigateWithScenario):
    common_transfer(scenario_navigator, 10,
                    bytes.fromhex("0102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f202122232425262728292a2b2c2d2e2f303132333435363738393a3b3c3d3e3f"))


def test_approve_erc20_extra_data(scenario_navigator: NavigateWithScenario):
    common_approve(scenario_navigator,
                   5,
                   "cpis_1RnzUSEXxObdZZOcn8gPzPPS".encode())


def test_approve_erc20_extra_data_nonascii(scenario_navigator: NavigateWithScenario):
    common_approve(scenario_navigator, 10, bytes.fromhex("deadcafe0042"))
