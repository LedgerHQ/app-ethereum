import json
from collections.abc import Callable

import pytest
from client.client import EthAppClient
from client.settings import SettingID, settings_toggle
from client.status_word import StatusWord
from client.token_info import EthTUID, TokenInfo
from constants import ABIS_FOLDER
from ragger.backend import BackendInterface
from ragger.error import ExceptionRAPDU
from ragger.navigator import Navigator
from ragger.navigator.navigation_scenario import NavigateWithScenario
from test_sign import BIP32_PATH, sign_dummy_tx
from test_sign import common as common_tx
from web3 import Web3

APPNAME = "Ethereum"
TOKEN_ADDR = bytes.fromhex("A0b86991c6218b36c1d19D4a2e9Eb0cE3606eB48")
TOKEN_TICKER = "USDC"
TOKEN_DECIMALS = 6
TOKEN_CHAIN_ID = 1


def test_provide_erc20_token(scenario_navigator: NavigateWithScenario):
    app_client = EthAppClient(scenario_navigator.backend)

    response = app_client.provide_token_metadata(TOKEN_TICKER, TOKEN_ADDR, TOKEN_DECIMALS, TOKEN_CHAIN_ID)
    assert response.status == StatusWord.SWO_SUCCESS
    sign_dummy_tx(scenario_navigator)


def test_provide_erc20_token_error(backend: BackendInterface):
    app_client = EthAppClient(backend)

    with pytest.raises(ExceptionRAPDU) as err:
        app_client.provide_token_metadata(
            TOKEN_TICKER,
            TOKEN_ADDR,
            TOKEN_DECIMALS,
            TOKEN_CHAIN_ID,
            bytes.fromhex("010203"),
        )

    assert err.value.status == StatusWord.SWO_INCORRECT_DATA


def test_erc20_ignores_nft_metadata(backend: BackendInterface):
    """An ERC-20 selector on a contract that only has NFT metadata must not reuse it.

    `approve(address,uint256)` has the same selector in ERC-20 and ERC-721, and the
    ERC-20 internal plugin matches any address, so it is selected without any plugin
    registration. `extraInfo_t` being an untagged union, an NFT record handed to it
    would be read as a token: collection name as ticker, its 52nd byte as decimals.
    """
    app_client = EthAppClient(backend)

    # NFT metadata only, deliberately for the address the ERC-20 flow will look up
    response = app_client.provide_nft_metadata("Bored Ape Yacht Club", TOKEN_ADDR, TOKEN_CHAIN_ID)
    assert response.status == StatusWord.SWO_SUCCESS

    with open(f"{ABIS_FOLDER}/erc20.json", encoding="utf-8") as file:
        contract = Web3().eth.contract(abi=json.load(file), address=None)
    data = contract.encode_abi(
        "approve",
        [bytes.fromhex("d8dA6BF26964aF9D7eEd9e03E53415D37aA96045"), 0x100],
    )
    tx_params = {
        "nonce": 0,
        "maxFeePerGas": Web3.to_wei(145, "gwei"),
        "maxPriorityFeePerGas": Web3.to_wei(1.5, "gwei"),
        "gas": 21000,
        "to": TOKEN_ADDR,
        "value": 0,
        "chainId": TOKEN_CHAIN_ID,
        "data": data,
    }

    # No token metadata is known, so the plugin falls back instead of rendering the
    # NFT record, and blind signing is disabled by default: the flow is refused.
    with pytest.raises(ExceptionRAPDU) as err:
        with app_client.sign(BIP32_PATH, tx_params):
            pass
    assert err.value.status == StatusWord.SWO_INCORRECT_DATA


def common_transfer(
    scenario_navigator: NavigateWithScenario,
    amount: float,
    extra_data: bytes | None = None,
    func: Callable = common_tx,
):
    app_client = EthAppClient(scenario_navigator.backend)

    with open(f"{ABIS_FOLDER}/erc20.json", encoding="utf-8") as file:
        contract = Web3().eth.contract(abi=json.load(file), address=None)
    data = contract.encode_abi(
        "transfer",
        [
            bytes.fromhex("d8dA6BF26964aF9D7eEd9e03E53415D37aA96045"),
            int(amount * pow(10, TOKEN_DECIMALS)),
        ],
    )

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


def common_approve(
    scenario_navigator: NavigateWithScenario,
    amount: float,
    extra_data: bytes | None = None,
    func: Callable = common_tx,
):
    app_client = EthAppClient(scenario_navigator.backend)

    with open(f"{ABIS_FOLDER}/erc20.json", encoding="utf-8") as file:
        contract = Web3().eth.contract(abi=json.load(file), address=None)
    data = contract.encode_abi(
        "approve",
        [
            bytes.fromhex("d8dA6BF26964aF9D7eEd9e03E53415D37aA96045"),
            int(amount * pow(10, TOKEN_DECIMALS)),
        ],
    )

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


def test_transfer_erc20_nonce_display(navigator: Navigator, scenario_navigator: NavigateWithScenario):
    settings_toggle(scenario_navigator.backend.device, navigator, [SettingID.NONCE])
    common_transfer(scenario_navigator, 10)


# these extra data are not part of the ABI :
# - https://github.com/ethereum/ercs/blob/master/ERCS/erc-20.md#transfer
# - https://github.com/ethereum/ercs/blob/master/ERCS/erc-20.md#approve
# they are appended to the calldata by the dApp before being sent to the smart-contract
# the smart contract is not aware of them, they are usually used for tracking purposes


def test_transfer_erc20_extra_data(scenario_navigator: NavigateWithScenario):
    common_transfer(scenario_navigator, 5, b"cpis_1RnzUSEXxObdZZOcn8gPzPPS")


def test_transfer_erc20_extra_data_nonascii(scenario_navigator: NavigateWithScenario):
    common_transfer(scenario_navigator, 10, bytes.fromhex("deadcafe0042"))


def test_transfer_erc20_extra_data_toolong(scenario_navigator: NavigateWithScenario):
    def check_error(scenario_navigator: NavigateWithScenario, tx_params: dict, _test_name: str):
        app_client = EthAppClient(scenario_navigator.backend)

        with pytest.raises(ExceptionRAPDU) as err:
            with app_client.sign(BIP32_PATH, tx_params):
                pass
        assert err.value.status == StatusWord.SWO_INCORRECT_DATA

    common_transfer(
        scenario_navigator,
        10,
        b"Lorem ipsum dolor sit amet, consectetur adipiscing elit, sed do eiusmod tempor incididunt ut labore et dolore magna aliqua.",  # noqa: E501
        func=check_error,
    )


def test_transfer_erc20_extra_data_nonascii_truncated(
    scenario_navigator: NavigateWithScenario,
):
    common_transfer(
        scenario_navigator,
        10,
        bytes.fromhex(
            "0102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f202122232425262728292a2b2c2d2e2f303132333435363738393a3b3c3d3e3f"
        ),
    )


def test_approve_erc20_extra_data(scenario_navigator: NavigateWithScenario):
    common_approve(scenario_navigator, 5, b"cpis_1RnzUSEXxObdZZOcn8gPzPPS")


def test_approve_erc20_extra_data_nonascii(scenario_navigator: NavigateWithScenario):
    common_approve(scenario_navigator, 10, bytes.fromhex("deadcafe0042"))


def test_token_info_v2(scenario_navigator: NavigateWithScenario):
    app_client = EthAppClient(scenario_navigator.backend)
    amount = 5

    with open(f"{ABIS_FOLDER}/erc20.json", encoding="utf-8") as file:
        contract = Web3().eth.contract(abi=json.load(file), address=None)
    data = contract.encode_abi(
        "approve",
        [
            bytes.fromhex("d8dA6BF26964aF9D7eEd9e03E53415D37aA96045"),
            int(amount * pow(10, TOKEN_DECIMALS)),
        ],
    )

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
    app_client.provide_token_info(
        TokenInfo(
            1,
            "Ethereum",
            TOKEN_TICKER,
            TOKEN_DECIMALS,
            tuid=EthTUID(TOKEN_CHAIN_ID, TOKEN_ADDR),
        )
    )
    # use the same snapshots as the legacy test to make sure there are no visible differences
    common_tx(scenario_navigator, tx_params, "test_approve_erc20")


def test_token_info_v2_wrong_coin_type(scenario_navigator: NavigateWithScenario):
    app_client = EthAppClient(scenario_navigator.backend)

    with pytest.raises(ExceptionRAPDU) as e:
        app_client.provide_token_info(
            TokenInfo(
                1,
                APPNAME,
                TOKEN_TICKER,
                TOKEN_DECIMALS,
                tuid=EthTUID(TOKEN_CHAIN_ID, TOKEN_ADDR),
                coin_type=501,
            )
        )
    assert e.value.status == StatusWord.SWO_INCORRECT_DATA


def test_token_info_v2_unknown_chain_id(scenario_navigator: NavigateWithScenario):
    app_client = EthAppClient(scenario_navigator.backend)

    with pytest.raises(ExceptionRAPDU) as e:
        app_client.provide_token_info(
            TokenInfo(
                1,
                APPNAME,
                TOKEN_TICKER,
                TOKEN_DECIMALS,
                tuid=EthTUID(2, TOKEN_ADDR),
            )
        )
    assert e.value.status == StatusWord.SWO_INCORRECT_DATA
