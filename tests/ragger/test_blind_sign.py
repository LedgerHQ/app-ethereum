from pathlib import Path
import json
from typing import Optional

import pytest
from web3 import Web3

from ragger.backend import BackendInterface
from ragger.navigator import Navigator, NavInsID
from ragger.navigator.navigation_scenario import NavigateWithScenario
from ragger.error import ExceptionRAPDU

from constants import ABIS_FOLDER

from client.client import EthAppClient
from client.status_word import StatusWord
from client.settings import SettingID, settings_toggle
import client.response_parser as ResponseParser
from client.utils import recover_transaction
from client.tx_simu import TxSimu


BIP32_PATH = "m/44'/60'/0'/0/0"
DEVICE_ADDR: Optional[bytes] = None


@pytest.fixture(name="reject", params=[False, True])
def reject_fixture(request) -> bool:
    return request.param


@pytest.fixture(name="amount", params=[0.0, 1.2])
def amount_fixture(request) -> float:
    return request.param


def common_get_address(app_client: EthAppClient) -> None:
    global DEVICE_ADDR
    if DEVICE_ADDR is None:
        with app_client.get_public_addr(bip32_path=BIP32_PATH, display=False):
            pass
        _, DEVICE_ADDR, _ = ResponseParser.pk_addr(app_client.response().data)


def common_tx_params(amount: float) -> dict:
    with open(f"{ABIS_FOLDER}/erc20.json", encoding="utf-8") as file:
        contract = Web3().eth.contract(
            abi=json.load(file),
            address=None
        )
    # this function is not handled by the internal plugin, so won't check if value == 0
    data = contract.encode_abi("balanceOf", [
        bytes.fromhex("d8dA6BF26964aF9D7eEd9e03E53415D37aA96045"),
    ])

    return {
        "nonce": 235,
        "maxFeePerGas": Web3.to_wei(100, "gwei"),
        "maxPriorityFeePerGas": Web3.to_wei(10, "gwei"),
        "gas": 44001,
        # Maker: Dai Stablecoin
        "to": bytes.fromhex("6b175474e89094c44da98b954eedeac495271d0f"),
        "data": data,
        "chainId": 1,
        "value": Web3.to_wei(amount, "ether"),
    }


def common_blind_sign(scenario_navigator: NavigateWithScenario,
                      test_name: str,
                      app_client: EthAppClient,
                      tx_params: dict,
                      reject: bool = False):
    try:
        with app_client.sign(BIP32_PATH, tx_params):
            if reject:
                test_name += "_rejected"

            if tx_params["value"] > 0.0:
                test_name += "_nonzero"

            if reject:
                scenario_navigator.review_reject_with_warning(test_name=test_name)
            else:
                scenario_navigator.review_approve_with_warning(test_name=test_name)

    except ExceptionRAPDU as e:
        assert reject
        assert e.status == StatusWord.CONDITION_NOT_SATISFIED
    else:
        assert not reject
        # verify signature
        vrs = ResponseParser.signature(app_client.response().data)
        addr = recover_transaction(tx_params, vrs)
        assert addr == DEVICE_ADDR


# Token approval, would require providing the token metadata from the CAL
def test_blind_sign(navigator: Navigator,
                    scenario_navigator: NavigateWithScenario,
                    test_name: str,
                    reject: bool,
                    amount: float,
                    simu_params: Optional[TxSimu] = None):
    if reject and amount > 0.0:
        pytest.skip()

    backend = scenario_navigator.backend
    app_client = EthAppClient(backend)
    device = backend.device

    common_get_address(app_client)

    settings_toggle(device, navigator, [SettingID.BLIND_SIGNING])

    tx_params = common_tx_params(amount)

    if not reject and simu_params is not None:
        _, tx_hash = app_client.serialize_tx(tx_params)
        simu_params.tx_hash = tx_hash
        simu_params.chain_id = tx_params["chainId"]
        simu_params.from_addr = DEVICE_ADDR
        response = app_client.provide_tx_simulation(simu_params)
        assert response.status == StatusWord.OK

    common_blind_sign(scenario_navigator,
                      test_name,
                      app_client,
                      tx_params,
                      reject)


# Token approval, would require providing the token metadata from the CAL
def test_blind_sign_nonce(navigator: Navigator,
                          scenario_navigator: NavigateWithScenario,
                          test_name: str):
    backend = scenario_navigator.backend
    app_client = EthAppClient(backend)
    device = backend.device

    common_get_address(app_client)

    settings_toggle(device, navigator, [SettingID.BLIND_SIGNING, SettingID.NONCE])

    tx_params = common_tx_params(1.2)
    common_blind_sign(scenario_navigator,
                      test_name,
                      app_client,
                      tx_params)


def test_blind_sign_reject_in_risk_review(backend: BackendInterface, navigator: Navigator):
    app_client = EthAppClient(backend)
    device = backend.device

    settings_toggle(device, navigator, [SettingID.BLIND_SIGNING])

    moves = []
    if device.is_nano:
        moves += [NavInsID.RIGHT_CLICK] + [NavInsID.BOTH_CLICK]
    else:
        moves += [NavInsID.USE_CASE_CHOICE_CONFIRM]
    try:
        with app_client.sign(BIP32_PATH, common_tx_params(0.0)):
            navigator.navigate(moves)
    except ExceptionRAPDU as e:
        assert e.status == StatusWord.CONDITION_NOT_SATISFIED
    else:
        assert False  # Should have thrown


# Token approval, would require loading the "internal plugin" &
# providing the token metadata from the CAL
def test_sign_parameter_selector(backend: BackendInterface,
                                 navigator: Navigator,
                                 test_name: str,
                                 default_screenshot_path: Path):
    app_client = EthAppClient(backend)
    device = backend.device

    common_get_address(app_client)

    settings_toggle(device, navigator, [SettingID.BLIND_SIGNING, SettingID.DEBUG_DATA])

    tx_params = common_tx_params(0.0)
    data_len = len(bytes.fromhex(tx_params["data"][2:]))
    params = (data_len - 4) // 32
    with app_client.sign(BIP32_PATH, tx_params):
        moves = []
        if device.is_nano:
            # verify | selector
            moves += [NavInsID.RIGHT_CLICK] * 2 + [NavInsID.BOTH_CLICK]
            # (verify | parameter) * flows
            moves += ([NavInsID.RIGHT_CLICK] * 2 + [NavInsID.BOTH_CLICK]) * params
            # blind signing | review | from | amount | to | fees
            moves += [NavInsID.BOTH_CLICK] + [NavInsID.RIGHT_CLICK] * 6 + [NavInsID.BOTH_CLICK]
        else:
            moves += ([NavInsID.SWIPE_CENTER_TO_LEFT] * 2 + [NavInsID.USE_CASE_REVIEW_CONFIRM]) * (1 + params)
            moves += [NavInsID.USE_CASE_CHOICE_REJECT]
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


def test_blind_sign_not_enabled_error(backend: BackendInterface,
                                      navigator: Navigator,
                                      test_name: str,
                                      default_screenshot_path: Path):
    app_client = EthAppClient(backend)
    device = backend.device

    try:
        with app_client.sign(BIP32_PATH, common_tx_params(0.0)):
            moves = []
            if device.is_nano:
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
