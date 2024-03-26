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
from typing import Optional
from constants import ROOT_SNAPSHOT_PATH
from eth_account.messages import encode_defunct, encode_typed_data

import ledger_app_clients.ethereum.response_parser as ResponseParser
from ledger_app_clients.ethereum.client import EthAppClient
from ledger_app_clients.ethereum.eip712 import InputData
from ledger_app_clients.ethereum.settings import SettingID, settings_toggle
from ledger_app_clients.ethereum.utils import recover_message


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
    return "%s/eip712_input_files" % (os.path.dirname(__file__))


def input_files() -> list[str]:
    files = []
    for file in os.scandir(eip712_json_path()):
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


def get_wallet_addr(client: EthAppClient) -> bytes:
    global WALLET_ADDR

    # don't ask again if we already have it
    if WALLET_ADDR is None:
        with client.get_public_addr(display=False):
            pass
        _, WALLET_ADDR, _ = ResponseParser.pk_addr(client.response().data)
    return WALLET_ADDR


def test_eip712_legacy(firmware: Firmware,
                       backend: BackendInterface,
                       navigator: Navigator):
    app_client = EthAppClient(backend)

    with open(input_files()[0]) as file:
        data = json.load(file)
        smsg = encode_typed_data(full_message=data)
        with app_client.eip712_sign_legacy(BIP32_PATH, smsg.header, smsg.body):
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
        recovered_addr = recover_message(data, (v, r, s))

    assert recovered_addr == get_wallet_addr(app_client)


def autonext(fw: Firmware, nav: Navigator):
    moves = list()
    if fw.device.startswith("nano"):
        moves = [NavInsID.RIGHT_CLICK]
    else:
        moves = [NavInsID.USE_CASE_REVIEW_TAP]
    if SNAPS_CONFIG is not None:
        nav.navigate_and_compare(ROOT_SNAPSHOT_PATH,
                                 SNAPS_CONFIG.test_name,
                                 moves,
                                 screen_change_before_first_instruction=False,
                                 screen_change_after_last_instruction=False,
                                 snap_start_idx=SNAPS_CONFIG.idx)
        SNAPS_CONFIG.idx += 1
    else:
        nav.navigate(moves,
                     screen_change_before_first_instruction=False,
                     screen_change_after_last_instruction=False)


def eip712_new_common(fw: Firmware,
                      nav: Navigator,
                      app_client: EthAppClient,
                      json_data: dict,
                      filters: Optional[dict],
                      verbose: bool):
    assert InputData.process_data(app_client,
                                  json_data,
                                  filters,
                                  partial(autonext, fw, nav))
    with app_client.eip712_sign_new(BIP32_PATH):
        moves = list()
        if fw.device.startswith("nano"):
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
        if SNAPS_CONFIG is not None:
            nav.navigate_and_compare(ROOT_SNAPSHOT_PATH,
                                     SNAPS_CONFIG.test_name,
                                     moves,
                                     snap_start_idx=SNAPS_CONFIG.idx)
            SNAPS_CONFIG.idx += 1
        else:
            nav.navigate(moves)
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
        test_path = "%s/%s" % (input_file.parent, "-".join(input_file.stem.split("-")[:-1]))

        filters = None
        if filtering:
            try:
                with open("%s-filter.json" % (test_path)) as f:
                    filters = json.load(f)
            except (IOError, json.decoder.JSONDecodeError) as e:
                pytest.skip("Filter file error: %s" % (e.strerror))

        if verbose:
            settings_toggle(firmware, navigator, [SettingID.VERBOSE_EIP712])

        with open(input_file) as file:
            data = json.load(file)
            v, r, s = eip712_new_common(firmware,
                                        navigator,
                                        app_client,
                                        data,
                                        filters,
                                        verbose)

            recovered_addr = recover_message(data, (v, r, s))

    assert recovered_addr == get_wallet_addr(app_client)


def test_eip712_address_substitution(firmware: Firmware,
                                     backend: BackendInterface,
                                     navigator: Navigator,
                                     verbose: bool):
    global SNAPS_CONFIG

    app_client = EthAppClient(backend)
    if firmware.device == "nanos":
        pytest.skip("Not supported on LNS")
    else:
        test_name = "eip712_address_substitution"
        if verbose:
            test_name += "_verbose"
        SNAPS_CONFIG = SnapshotsConfig(test_name)
        with open("%s/address_substitution.json" % (eip712_json_path())) as file:
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
            recovered_addr = recover_message(data, (v, r, s))

    assert recovered_addr == get_wallet_addr(app_client)
