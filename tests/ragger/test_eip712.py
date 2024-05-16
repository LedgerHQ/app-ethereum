import fnmatch
import os
from functools import partial
from pathlib import Path
import json
from typing import Optional
import pytest
from eth_account.messages import encode_typed_data

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
    if firmware.device.startswith("nano"):
        moves = [NavInsID.RIGHT_CLICK]
    else:
        moves = [NavInsID.USE_CASE_REVIEW_TAP]
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
                      verbose: bool):
    assert InputData.process_data(app_client,
                                  json_data,
                                  filters,
                                  partial(autonext, firmware, navigator, default_screenshot_path))
    with app_client.eip712_sign_new(BIP32_PATH):
        moves = []
        if firmware.device.startswith("nano"):
            # need to skip the message hash
            if not verbose and filters is None:
                moves = [NavInsID.RIGHT_CLICK] * 2
            moves += [NavInsID.BOTH_CLICK]
        else:
            # need to skip the message hash
            if not verbose and filters is None:
                moves += [NavInsID.USE_CASE_REVIEW_TAP]
            moves += [NavInsID.USE_CASE_REVIEW_CONFIRM]
        if SNAPS_CONFIG is not None:
            navigator.navigate_and_compare(default_screenshot_path,
                                           SNAPS_CONFIG.test_name,
                                           moves,
                                           snap_start_idx=SNAPS_CONFIG.idx)
            SNAPS_CONFIG.idx += 1
        else:
            navigator.navigate(moves)
    return ResponseParser.signature(app_client.response().data)


def test_eip712_new(firmware: Firmware,
                    backend: BackendInterface,
                    navigator: Navigator,
                    default_screenshot_path: Path,
                    input_file: Path,
                    verbose: bool,
                    filtering: bool):
    app_client = EthAppClient(backend)
    if firmware.device == "nanos":
        pytest.skip("Not supported on LNS")
    if firmware.device == "flex":
        pytest.skip("Not yet available on Flex (due to swipe)")

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
                                verbose)

        recovered_addr = recover_message(data, vrs)

    assert recovered_addr == get_wallet_addr(app_client)


def test_eip712_address_substitution(firmware: Firmware,
                                     backend: BackendInterface,
                                     navigator: Navigator,
                                     default_screenshot_path: Path,
                                     test_name: str,
                                     verbose: bool):
    global SNAPS_CONFIG

    app_client = EthAppClient(backend)
    if firmware.device == "nanos":
        pytest.skip("Not supported on LNS")
    if firmware.device == "flex":
        pytest.skip("Not yet available on Flex (due to swipe)")

    if verbose:
        test_name += "_verbose"
    SNAPS_CONFIG = SnapshotsConfig(test_name)
    with open(f"{eip712_json_path()}/address_substitution.json", encoding="utf-8") as file:
        data = json.load(file)

    app_client.provide_token_metadata("DAI",
                                      bytes.fromhex(data["message"]["token"][2:]),
                                      18,
                                      1)

    challenge = ResponseParser.challenge(app_client.get_challenge().data)
    app_client.provide_domain_name(challenge,
                                   "vitalik.eth",
                                   bytes.fromhex(data["message"]["to"][2:]))

    if verbose:
        settings_toggle(firmware, navigator, [SettingID.VERBOSE_EIP712])
        filters = None
    else:
        filters = {
            "name": "Token test",
            "fields": {
                "amount": "Amount",
                "token": "Token",
                "to": "To",
            }
        }

    vrs = eip712_new_common(firmware,
                            navigator,
                            default_screenshot_path,
                            app_client,
                            data,
                            filters,
                            verbose)

    # verify signature
    addr = recover_message(data, vrs)
    assert addr == get_wallet_addr(app_client)
