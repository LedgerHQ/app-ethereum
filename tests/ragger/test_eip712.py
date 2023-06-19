import pytest
import os
import fnmatch
from typing import List
from ragger.firmware import Firmware
from ragger.backend import BackendInterface
from ragger.navigator import Navigator, NavInsID
from app.client import EthAppClient
from app.settings import SettingID, settings_toggle
from eip712 import InputData
from pathlib import Path
from configparser import ConfigParser
import app.response_parser as ResponseParser
from functools import partial
import time

BIP32_PATH = "m/44'/60'/0'/0/0"


def input_files() -> List[str]:
    files = []
    for file in os.scandir("%s/eip712/input_files" % (os.path.dirname(__file__))):
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


def test_eip712_legacy(firmware: Firmware,
                       backend: BackendInterface,
                       navigator: Navigator):
    app_client = EthAppClient(backend)
    with app_client.eip712_sign_legacy(
            BIP32_PATH,
            bytes.fromhex('6137beb405d9ff777172aa879e33edb34a1460e701802746c5ef96e741710e59'),
            bytes.fromhex('eb4221181ff3f1a83ea7313993ca9218496e424604ba9492bb4052c03d5c3df8')):
        moves = list()
        if firmware.device.startswith("nano"):
            moves += [ NavInsID.RIGHT_CLICK ]
            if firmware.device == "nanos":
                screens_per_hash = 4
            else:
                screens_per_hash = 2
            moves += [ NavInsID.RIGHT_CLICK ] * screens_per_hash * 2
            moves += [ NavInsID.BOTH_CLICK ]
        else:
            moves += [ NavInsID.USE_CASE_REVIEW_TAP ] * 2
            moves += [ NavInsID.USE_CASE_REVIEW_CONFIRM ]
        navigator.navigate(moves)

    v, r, s = ResponseParser.signature(app_client.response().data)

    assert v == bytes.fromhex("1c")
    assert r == bytes.fromhex("ea66f747173762715751c889fea8722acac3fc35db2c226d37a2e58815398f64")
    assert s == bytes.fromhex("52d8ba9153de9255da220ffd36762c0b027701a3b5110f0a765f94b16a9dfb55")


def autonext(fw: Firmware, nav: Navigator):
    moves = list()
    if fw.device.startswith("nano"):
        moves = [ NavInsID.RIGHT_CLICK ]
    else:
        moves = [ NavInsID.USE_CASE_REVIEW_TAP ]
    nav.navigate(moves, screen_change_before_first_instruction=False, screen_change_after_last_instruction=False)


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
        test_path = "%s/%s" % (input_file.parent, "-".join(input_file.stem.split("-")[:-1]))
        conf_file = "%s.ini" % (test_path)
        filter_file = None

        if filtering:
            filter_file = "%s-filter.json" % (test_path)

        config = ConfigParser()
        config.read(conf_file)

        # sanity check
        assert "signature" in config.sections()
        assert "v" in config["signature"]
        assert "r" in config["signature"]
        assert "s" in config["signature"]

        if not filtering or Path(filter_file).is_file():
            if verbose:
                settings_toggle(firmware, navigator, [SettingID.VERBOSE_EIP712])

            assert InputData.process_file(app_client,
                                          input_file,
                                          filter_file,
                                          partial(autonext, firmware, navigator)) == True
            with app_client.eip712_sign_new(BIP32_PATH, verbose):
                time.sleep(0.5) # tight on timing, needed by the CI otherwise might fail sometimes
                moves = list()
                if firmware.device.startswith("nano"):
                    if not verbose and not filtering: # need to skip the message hash
                        moves = [ NavInsID.RIGHT_CLICK ] * 2
                    moves += [ NavInsID.BOTH_CLICK ]
                else:
                    if not verbose and not filtering: # need to skip the message hash
                        moves += [ NavInsID.USE_CASE_REVIEW_TAP ]
                    moves += [ NavInsID.USE_CASE_REVIEW_CONFIRM ]
                navigator.navigate(moves)
            v, r, s = ResponseParser.signature(app_client.response().data)
            #print("[signature]")
            #print("v = %s" % (v.hex()))
            #print("r = %s" % (r.hex()))
            #print("s = %s" % (s.hex()))

            assert v == bytes.fromhex(config["signature"]["v"])
            assert r == bytes.fromhex(config["signature"]["r"])
            assert s == bytes.fromhex(config["signature"]["s"])
        else:
            pytest.skip("No filter file found")
