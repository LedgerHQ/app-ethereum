import fnmatch
import os
import time
from configparser import ConfigParser
from functools import partial
from pathlib import Path
import json
from typing import Optional
import pytest

import ledger_app_clients.ethereum.response_parser as ResponseParser
from ledger_app_clients.ethereum.client import EthAppClient
from ledger_app_clients.ethereum.eip712 import InputData
from ledger_app_clients.ethereum.settings import SettingID, settings_toggle

from ragger.backend import BackendInterface
from ragger.firmware import Firmware
from ragger.navigator import Navigator, NavInsID
from constants import ROOT_SNAPSHOT_PATH


class SnapshotsConfig:
    test_name: str
    idx: int

    def __init__(self, test_name: str, idx: int = 0):
        self.test_name = test_name
        self.idx = idx


BIP32_PATH = "m/44'/60'/0'/0/0"
snaps_config: Optional[SnapshotsConfig] = None


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


def test_eip712_legacy(firmware: Firmware,
                       backend: BackendInterface,
                       navigator: Navigator):
    app_client = EthAppClient(backend)
    with app_client.eip712_sign_legacy(
            BIP32_PATH,
            bytes.fromhex('6137beb405d9ff777172aa879e33edb34a1460e701802746c5ef96e741710e59'),
            bytes.fromhex('eb4221181ff3f1a83ea7313993ca9218496e424604ba9492bb4052c03d5c3df8')):
        moves = []
        if firmware.device.startswith("nano"):
            moves += [NavInsID.RIGHT_CLICK]
            if firmware.device == "nanos":
                screens_per_hash = 4
            else:
                screens_per_hash = 2
            moves += [NavInsID.RIGHT_CLICK] * screens_per_hash * 2
            moves += [NavInsID.BOTH_CLICK]
        else:
            moves += [NavInsID.USE_CASE_REVIEW_TAP] * 2
            moves += [NavInsID.USE_CASE_REVIEW_CONFIRM]
        navigator.navigate(moves)

    v, r, s = ResponseParser.signature(app_client.response().data)

    assert v == bytes.fromhex("1c")
    assert r == bytes.fromhex("ea66f747173762715751c889fea8722acac3fc35db2c226d37a2e58815398f64")
    assert s == bytes.fromhex("52d8ba9153de9255da220ffd36762c0b027701a3b5110f0a765f94b16a9dfb55")


def autonext(firmware: Firmware, navigator: Navigator):
    moves = []
    if firmware.device.startswith("nano"):
        moves = [NavInsID.RIGHT_CLICK]
    else:
        moves = [NavInsID.USE_CASE_REVIEW_TAP]
    if snaps_config is not None:
        navigator.navigate_and_compare(ROOT_SNAPSHOT_PATH,
                                 snaps_config.test_name,
                                 moves,
                                 screen_change_before_first_instruction=False,
                                 screen_change_after_last_instruction=False,
                                 snap_start_idx=snaps_config.idx)
        snaps_config.idx += 1
    else:
        navigator.navigate(moves,
                     screen_change_before_first_instruction=False,
                     screen_change_after_last_instruction=False)


def eip712_new_common(firmware: Firmware,
                      navigator: Navigator,
                      app_client: EthAppClient,
                      json_data: dict,
                      filters: Optional[dict],
                      verbose: bool):
    assert InputData.process_data(app_client,
                                  json_data,
                                  filters,
                                  partial(autonext, firmware, navigator))
    with app_client.eip712_sign_new(BIP32_PATH):
        moves = []
        if firmware.device.startswith("nano"):
            # need to skip the message hash
            if not verbose and filters is None:
                moves = [NavInsID.RIGHT_CLICK] * 2
            moves += [NavInsID.BOTH_CLICK]
        else:
            time.sleep(1.5)
            # need to skip the message hash
            if not verbose and filters is None:
                moves += [NavInsID.USE_CASE_REVIEW_TAP]
            moves += [NavInsID.USE_CASE_REVIEW_CONFIRM]
        if snaps_config is not None:
            navigator.navigate_and_compare(ROOT_SNAPSHOT_PATH,
                                     snaps_config.test_name,
                                     moves,
                                     snap_start_idx=snaps_config.idx)
            snaps_config.idx += 1
        else:
            navigator.navigate(moves)
    return ResponseParser.signature(app_client.response().data)


def test_eip712_new(firmware: Firmware,
                    backend: BackendInterface,
                    navigator: Navigator,
                    input_file: Path,
                    verbose: bool,
                    filtering: bool):
    app_client = EthAppClient(backend)
    if firmware.device == "nanos":
        pytest.skip("Not supported on LNS")
    else:
        test_path = f"{input_file.parent}/{'-'.join(input_file.stem.split('-')[:-1])}"
        conf_file = f"{test_path}.ini"

        filters = None
        if filtering:
            try:
                with open(f"{test_path}-filter.json", encoding="utf-8") as f:
                    filters = json.load(f)
            except (IOError, json.decoder.JSONDecodeError) as e:
                pytest.skip(f"Filter file error: {e.strerror}")

        config = ConfigParser()
        config.read(conf_file)

        # sanity check
        assert "signature" in config.sections()
        assert "v" in config["signature"]
        assert "r" in config["signature"]
        assert "s" in config["signature"]

        if verbose:
            settings_toggle(firmware, navigator, [SettingID.VERBOSE_EIP712])

        with open(input_file, encoding="utf-8") as file:
            v, r, s = eip712_new_common(firmware,
                                        navigator,
                                        app_client,
                                        json.load(file),
                                        filters,
                                        verbose)

    assert v == bytes.fromhex(config["signature"]["v"])
    assert r == bytes.fromhex(config["signature"]["r"])
    assert s == bytes.fromhex(config["signature"]["s"])


def test_eip712_address_substitution(firmware: Firmware,
                                     backend: BackendInterface,
                                     navigator: Navigator,
                                     verbose: bool):
    global snaps_config

    app_client = EthAppClient(backend)
    if firmware.device == "nanos":
        pytest.skip("Not supported on LNS")
    else:
        test_name = "eip712_address_substitution"
        if verbose:
            test_name += "_verbose"
        snaps_config = SnapshotsConfig(test_name)
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

            v, r, s = eip712_new_common(firmware,
                                        navigator,
                                        app_client,
                                        data,
                                        filters,
                                        verbose)

    assert v == bytes.fromhex("1b")
    assert r == bytes.fromhex("d4a0e058251cdc3845aaa5eb8409d8a189ac668db7c55a64eb3121b0db7fd8c0")
    assert s == bytes.fromhex("3221800e4f45272c6fa8fafda5e94c848d1a4b90c442aa62afa8e8d6a9af0f00")
