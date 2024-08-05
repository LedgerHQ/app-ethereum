import fnmatch
import os
from functools import partial
from pathlib import Path
import json
from typing import Optional
from ctypes import c_uint64
import pytest
from eth_account.messages import encode_typed_data
import web3

from ragger.backend import BackendInterface
from ragger.firmware import Firmware
from ragger.navigator import Navigator, NavInsID
from ragger.navigator.navigation_scenario import NavigateWithScenario

import client.response_parser as ResponseParser
from client.utils import recover_message
from client.client import EthAppClient
from client.eip712 import InputData
from client.settings import SettingID, settings_toggle


class SnapshotsConfig:
    test_name: str
    idx: int

    def __init__(self, test_name: str, idx: int = 0):
        self.test_name = test_name
        self.idx = idx


BIP32_PATH = "m/44'/60'/0'/0/0"
SNAPS_CONFIG: Optional[SnapshotsConfig] = None
WALLET_ADDR: Optional[bytes] = None


def eip712_json_path() -> str:
    return f"{os.path.dirname(__file__)}/eip712_input_files"


def input_files() -> list[str]:
    files = []
    for file in os.scandir(eip712_json_path()):
        if fnmatch.fnmatch(file, "*-data.json"):
            files.append(file.path)
    return sorted(files)


@pytest.fixture(name="input_file", params=input_files())
def input_file_fixture(request) -> str:
    return Path(request.param)


@pytest.fixture(name="verbose", params=[True, False])
def verbose_fixture(request) -> bool:
    return request.param


@pytest.fixture(name="filtering", params=[False, True])
def filtering_fixture(request) -> bool:
    return request.param


def get_wallet_addr(client: EthAppClient) -> bytes:
    global WALLET_ADDR

    # don't ask again if we already have it
    if WALLET_ADDR is None:
        with client.get_public_addr(display=False):
            pass
        _, WALLET_ADDR, _ = ResponseParser.pk_addr(client.response().data)
    return WALLET_ADDR


def test_eip712_legacy(backend: BackendInterface, scenario_navigator: NavigateWithScenario):
    app_client = EthAppClient(backend)

    with open(input_files()[0], encoding="utf-8") as file:
        data = json.load(file)
    smsg = encode_typed_data(full_message=data)
    with app_client.eip712_sign_legacy(BIP32_PATH, smsg.header, smsg.body):
        scenario_navigator.review_approve(custom_screen_text="Sign", do_comparison=False)

    vrs = ResponseParser.signature(app_client.response().data)
    recovered_addr = recover_message(data, vrs)

    assert recovered_addr == get_wallet_addr(app_client)


def autonext(firmware: Firmware, navigator: Navigator, default_screenshot_path: Path):
    moves = []
    if firmware.is_nano:
        moves = [NavInsID.RIGHT_CLICK]
    else:
        moves = [NavInsID.SWIPE_CENTER_TO_LEFT]
    if SNAPS_CONFIG is not None:
        navigator.navigate_and_compare(default_screenshot_path,
                                       SNAPS_CONFIG.test_name,
                                       moves,
                                       screen_change_before_first_instruction=False,
                                       screen_change_after_last_instruction=False,
                                       snap_start_idx=SNAPS_CONFIG.idx)
        SNAPS_CONFIG.idx += 1
    else:
        navigator.navigate(moves,
                           screen_change_before_first_instruction=False,
                           screen_change_after_last_instruction=False)


def eip712_new_common(firmware: Firmware,
                      navigator: Navigator,
                      default_screenshot_path: Path,
                      app_client: EthAppClient,
                      json_data: dict,
                      filters: Optional[dict],
                      verbose: bool,
                      golden_run: bool):
    assert InputData.process_data(app_client,
                                  json_data,
                                  filters,
                                  partial(autonext, firmware, navigator, default_screenshot_path),
                                  golden_run)
    with app_client.eip712_sign_new(BIP32_PATH):
        moves = []
        if firmware.is_nano:
            # need to skip the message hash
            if not verbose and filters is None:
                moves += [NavInsID.RIGHT_CLICK] * 2
            moves += [NavInsID.BOTH_CLICK]
        else:
            # this move is necessary most of the times, but can't be 100% sure with the fields grouping
            moves += [NavInsID.SWIPE_CENTER_TO_LEFT]
            # need to skip the message hash
            if not verbose and filters is None:
                moves += [NavInsID.SWIPE_CENTER_TO_LEFT]
            moves += [NavInsID.USE_CASE_REVIEW_CONFIRM]
        if SNAPS_CONFIG is not None:
            # Could break (time-out) if given a JSON that requires less moves
            # TODO: Maybe take list of moves as input instead of trying to guess them ?
            navigator.navigate_and_compare(default_screenshot_path,
                                           SNAPS_CONFIG.test_name,
                                           moves,
                                           snap_start_idx=SNAPS_CONFIG.idx)
        else:
            # Do them one-by-one to prevent an unnecessary move from timing-out and failing the test
            for move in moves:
                navigator.navigate([move],
                                   screen_change_before_first_instruction=False,
                                   screen_change_after_last_instruction=False)
    return ResponseParser.signature(app_client.response().data)


def test_eip712_new(firmware: Firmware,
                    backend: BackendInterface,
                    navigator: Navigator,
                    default_screenshot_path: Path,
                    input_file: Path,
                    verbose: bool,
                    filtering: bool):
    app_client = EthAppClient(backend)
    if firmware == Firmware.NANOS:
        pytest.skip("Not supported on LNS")

    test_path = f"{input_file.parent}/{'-'.join(input_file.stem.split('-')[:-1])}"

    filters = None
    if filtering:
        try:
            filterfile = Path(f"{test_path}-filter.json")
            with open(filterfile, encoding="utf-8") as f:
                filters = json.load(f)
        except (IOError, json.decoder.JSONDecodeError) as e:
            pytest.skip(f"{filterfile.name}: {e.strerror}")

    if verbose:
        settings_toggle(firmware, navigator, [SettingID.VERBOSE_EIP712])

    with open(input_file, encoding="utf-8") as file:
        data = json.load(file)
        vrs = eip712_new_common(firmware,
                                navigator,
                                default_screenshot_path,
                                app_client,
                                data,
                                filters,
                                verbose,
                                False)

        recovered_addr = recover_message(data, vrs)

    assert recovered_addr == get_wallet_addr(app_client)


class DataSet():
    data: dict
    filters: dict
    suffix: str

    def __init__(self, data: dict, filters: dict, suffix: str = ""):
        self.data = data
        self.filters = filters
        self.suffix = suffix


ADVANCED_DATA_SETS = [
    DataSet(
        {
            "domain": {
                "chainId": 1,
                "name": "Advanced test",
                "verifyingContract": "0xCcCCccccCCCCcCCCCCCcCcCccCcCCCcCcccccccC",
                "version": "1"
            },
            "message": {
                "with": "0xd8dA6BF26964aF9D7eEd9e03E53415D37aA96045",
                "value_recv": 10000000000000000,
                "token_send": "0x6B175474E89094C44Da98b954EedeAC495271d0F",
                "value_send": 24500000000000000000,
                "token_recv": "0xc02aaa39b223fe8d0a0e5c4f27ead9083c756cc2",
                "expires": 1714559400,
            },
            "primaryType": "Transfer",
            "types": {
                "EIP712Domain": [
                    {"name": "name", "type": "string"},
                    {"name": "version", "type": "string"},
                    {"name": "chainId", "type": "uint256"},
                    {"name": "verifyingContract", "type": "address"}
                ],
                "Transfer": [
                    {"name": "with", "type": "address"},
                    {"name": "value_recv", "type": "uint256"},
                    {"name": "token_send", "type": "address"},
                    {"name": "value_send", "type": "uint256"},
                    {"name": "token_recv", "type": "address"},
                    {"name": "expires", "type": "uint64"},
                ]
            }
        },
        {
            "name": "Advanced Filtering",
            "tokens": [
                {
                    "addr": "0x6b175474e89094c44da98b954eedeac495271d0f",
                    "ticker": "DAI",
                    "decimals": 18,
                    "chain_id": 1,
                },
                {
                    "addr": "0xc02aaa39b223fe8d0a0e5c4f27ead9083c756cc2",
                    "ticker": "WETH",
                    "decimals": 18,
                    "chain_id": 1,
                },
            ],
            "fields": {
                "value_send": {
                    "type": "amount_join_value",
                    "name": "Send",
                    "token": 0,
                },
                "token_send": {
                    "type": "amount_join_token",
                    "token": 0,
                },
                "value_recv": {
                    "type": "amount_join_value",
                    "name": "Receive",
                    "token": 1,
                },
                "token_recv": {
                    "type": "amount_join_token",
                    "token": 1,
                },
                "with": {
                    "type": "raw",
                    "name": "With",
                },
                "expires": {
                    "type": "datetime",
                    "name": "Will Expire"
                },
            }
        }
    ),
    DataSet(
        {
            "types": {
                "EIP712Domain": [
                    {"name": "name", "type": "string"},
                    {"name": "version", "type": "string"},
                    {"name": "chainId", "type": "uint256"},
                    {"name": "verifyingContract", "type": "address"},
                ],
                "Permit": [
                    {"name": "owner", "type": "address"},
                    {"name": "spender", "type": "address"},
                    {"name": "value", "type": "uint256"},
                    {"name": "nonce", "type": "uint256"},
                    {"name": "deadline", "type": "uint256"},
                ]
            },
            "primaryType": "Permit",
            "domain": {
                "name": "ENS",
                "version": "1",
                "verifyingContract": "0xC18360217D8F7Ab5e7c516566761Ea12Ce7F9D72",
                "chainId": 1,
            },
            "message": {
                "owner": "0xd8dA6BF26964aF9D7eEd9e03E53415D37aA96045",
                "spender": "0x5B38Da6a701c568545dCfcB03FcB875f56beddC4",
                "value": 4200000000000000000,
                "nonce": 0,
                "deadline": 1719756000,
            }
        },
        {
            "name": "Permit filtering",
            "tokens": [
                {
                    "addr": "0xC18360217D8F7Ab5e7c516566761Ea12Ce7F9D72",
                    "ticker": "ENS",
                    "decimals": 18,
                    "chain_id": 1,
                },
            ],
            "fields": {
                "value": {
                    "type": "amount_join_value",
                    "name": "Send",
                },
                "deadline": {
                    "type": "datetime",
                    "name": "Deadline",
                },
            }
        },
        "_permit"
    ),
    DataSet(
        {
            "types": {
                "EIP712Domain": [
                    {"name": "name", "type": "string"},
                    {"name": "version", "type": "string"},
                    {"name": "chainId", "type": "uint256"},
                    {"name": "verifyingContract", "type": "address"},
                ],
                "Root": [
                    {"name": "token_big", "type": "address"},
                    {"name": "value_big", "type": "uint256"},
                    {"name": "token_biggest", "type": "address"},
                    {"name": "value_biggest", "type": "uint256"},
                ]
            },
            "primaryType": "Root",
            "domain": {
                "name": "test",
                "version": "1",
                "verifyingContract": "0x0000000000000000000000000000000000000000",
                "chainId": 1,
            },
            "message": {
                "token_big": "0x6b175474e89094c44da98b954eedeac495271d0f",
                "value_big": c_uint64(-1).value,
                "token_biggest": "0x6b175474e89094c44da98b954eedeac495271d0f",
                "value_biggest": int(web3.constants.MAX_INT, 0),
            }
        },
        {
            "name": "Unlimited test",
            "tokens": [
                {
                    "addr": "0x6b175474e89094c44da98b954eedeac495271d0f",
                    "ticker": "DAI",
                    "decimals": 18,
                    "chain_id": 1,
                },
            ],
            "fields": {
                "token_big": {
                    "type": "amount_join_token",
                    "token": 0,
                },
                "value_big": {
                    "type": "amount_join_value",
                    "name": "Big",
                    "token": 0,
                },
                "token_biggest": {
                    "type": "amount_join_token",
                    "token": 0,
                },
                "value_biggest": {
                    "type": "amount_join_value",
                    "name": "Biggest",
                    "token": 0,
                },
            }
        },
        "_unlimited"
    ),
]


@pytest.fixture(name="data_set", params=ADVANCED_DATA_SETS)
def data_set_fixture(request) -> DataSet:
    return request.param


def test_eip712_advanced_filtering(firmware: Firmware,
                                   backend: BackendInterface,
                                   navigator: Navigator,
                                   default_screenshot_path: Path,
                                   test_name: str,
                                   data_set: DataSet,
                                   golden_run: bool):
    global SNAPS_CONFIG

    app_client = EthAppClient(backend)
    if firmware == Firmware.NANOS:
        pytest.skip("Not supported on LNS")

    SNAPS_CONFIG = SnapshotsConfig(test_name + data_set.suffix)

    vrs = eip712_new_common(firmware,
                            navigator,
                            default_screenshot_path,
                            app_client,
                            data_set.data,
                            data_set.filters,
                            False,
                            golden_run)

    # verify signature
    addr = recover_message(data_set.data, vrs)
    assert addr == get_wallet_addr(app_client)


def test_eip712_filtering_empty_array(firmware: Firmware,
                                      backend: BackendInterface,
                                      navigator: Navigator,
                                      default_screenshot_path: Path,
                                      test_name: str,
                                      golden_run: bool):
    global SNAPS_CONFIG

    app_client = EthAppClient(backend)
    if firmware == Firmware.NANOS:
        pytest.skip("Not supported on LNS")

    SNAPS_CONFIG = SnapshotsConfig(test_name)

    data = {
        "types": {
            "EIP712Domain": [
                {"name": "name", "type": "string"},
                {"name": "version", "type": "string"},
                {"name": "chainId", "type": "uint256"},
                {"name": "verifyingContract", "type": "address"},
            ],
            "Person": [
                {"name": "name", "type": "string"},
                {"name": "addr", "type": "address"},
            ],
            "Message": [
                {"name": "title", "type": "string"},
                {"name": "to", "type": "Person[]"},
            ],
            "Root": [
                {"name": "text", "type": "string"},
                {"name": "subtext", "type": "string[]"},
                {"name": "msg_list1", "type": "Message[]"},
                {"name": "msg_list2", "type": "Message[]"},
            ],
        },
        "primaryType": "Root",
        "domain": {
            "name": "test",
            "version": "1",
            "verifyingContract": "0x0000000000000000000000000000000000000000",
            "chainId": 1,
        },
        "message": {
            "text": "This is a test",
            "subtext": [],
            "msg_list1": [
                {
                    "title": "This is a test",
                    "to": [],
                }
            ],
            "msg_list2": [],
        }
    }
    filters = {
        "name": "Empty array filtering",
        "fields": {
            "text": {
                "type": "raw",
                "name": "Text",
            },
            "subtext.[]": {
                "type": "raw",
                "name": "Sub-Text",
            },
            "msg_list1.[].to.[].addr": {
                "type": "raw",
                "name": "(1) Recipient addr",
            },
            "msg_list2.[].to.[].addr": {
                "type": "raw",
                "name": "(2) Recipient addr",
            },
        }
    }
    vrs = eip712_new_common(firmware,
                            navigator,
                            default_screenshot_path,
                            app_client,
                            data,
                            filters,
                            False,
                            golden_run)

    # verify signature
    addr = recover_message(data, vrs)
    assert addr == get_wallet_addr(app_client)
