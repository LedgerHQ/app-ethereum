import fnmatch
import os
import pytest
import time
from configparser import ConfigParser
from functools import partial
from pathlib import Path
from ragger.backend import BackendInterface
from ragger.firmware import Firmware
from ragger.navigator import Navigator, NavInsID
import json

import ledger_app_clients.ethereum.response_parser as ResponseParser
from ledger_app_clients.ethereum.client import EthAppClient
from ledger_app_clients.ethereum.eip712 import InputData
from ledger_app_clients.ethereum.settings import SettingID, settings_toggle


BIP32_PATH = "m/44'/60'/0'/0/0"


def input_files() -> list[str]:
    files = []
    for file in os.scandir("%s/eip712_input_files" % (os.path.dirname(__file__))):
        if fnmatch.fnmatch(file, "*-data.json"):
            files.append(file.path)
    return sorted(files)


@pytest.fixture(params=input_files())
def input_file(request) -> str:
    return Path(request.param)


@pytest.fixture(params=[True, False])
def verbose(request) -> bool:
    return request.param


@pytest.fixture(params=[False, True])
def filtering(request) -> bool:
    return request.param


def notest_eip712_legacy(firmware: Firmware,
                         backend: BackendInterface,
                         navigator: Navigator):
    app_client = EthAppClient(backend)
    with app_client.eip712_sign_legacy(
            BIP32_PATH,
            bytes.fromhex('6137beb405d9ff777172aa879e33edb34a1460e701802746c5ef96e741710e59'),
            bytes.fromhex('eb4221181ff3f1a83ea7313993ca9218496e424604ba9492bb4052c03d5c3df8')):
        moves = list()
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


def autonext(fw: Firmware, nav: Navigator):
    moves = list()
    if fw.device.startswith("nano"):
        moves = [NavInsID.RIGHT_CLICK]
    else:
        moves = [NavInsID.USE_CASE_REVIEW_TAP]
    nav.navigate(moves, screen_change_before_first_instruction=False, screen_change_after_last_instruction=False)


def notest_eip712_new(firmware: Firmware,
                      backend: BackendInterface,
                      navigator: Navigator,
                      input_file: Path,
                      verbose: bool,
                      filtering: bool):
    app_client = EthAppClient(backend)
    if firmware.device == "nanos":
        pytest.skip("Not supported on LNS")
    else:
        test_path = "%s/%s" % (input_file.parent, "-".join(input_file.stem.split("-")[:-1]))
        conf_file = "%s.ini" % (test_path)

        filters = None
        if filtering:
            try:
                with open("%s-filter.json" % (test_path)) as f:
                    filters = json.load(f)
            except (IOError, json.decoder.JSONDecodeError) as e:
                pytest.skip("Filter file error: %s" % (e.strerror))

        config = ConfigParser()
        config.read(conf_file)

        # sanity check
        assert "signature" in config.sections()
        assert "v" in config["signature"]
        assert "r" in config["signature"]
        assert "s" in config["signature"]

        if verbose:
            settings_toggle(firmware, navigator, [SettingID.VERBOSE_EIP712])

        with open(input_file) as file:
            assert InputData.process_data(app_client,
                                          json.load(file),
                                          filters,
                                          partial(autonext, firmware, navigator))
            with app_client.eip712_sign_new(BIP32_PATH):
                # tight on timing, needed by the CI otherwise might fail sometimes
                time.sleep(0.5)

                moves = list()
                if firmware.device.startswith("nano"):
                    if not verbose and not filtering:  # need to skip the message hash
                        moves = [NavInsID.RIGHT_CLICK] * 2
                    moves += [NavInsID.BOTH_CLICK]
                else:
                    if not verbose and not filtering:  # need to skip the message hash
                        moves += [NavInsID.USE_CASE_REVIEW_TAP]
                    moves += [NavInsID.USE_CASE_REVIEW_CONFIRM]
                navigator.navigate(moves)
            v, r, s = ResponseParser.signature(app_client.response().data)

        assert v == bytes.fromhex(config["signature"]["v"])
        assert r == bytes.fromhex(config["signature"]["r"])
        assert s == bytes.fromhex(config["signature"]["s"])


def test_problematic(firmware: Firmware,
                     backend: BackendInterface,
                     navigator: Navigator):
    notest_eip712_new(firmware,
                      backend,
                      navigator,
                      Path("eip712_input_files/13-empty_arrays-data.json"),
                      False,
                      True)
