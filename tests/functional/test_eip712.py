# Large test file containing multiple test cases for EIP-712 signing
import fnmatch
import hashlib
import json
import os
from collections.abc import Callable
from ctypes import c_uint64
from pathlib import Path
from typing import Any

import client.response_parser as ResponseParser
import pytest
import web3
from client.client import EIP712CalldataParamPresence, EIP712FieldType, EthAppClient
from client.eip712 import InputData
from client.gating import Gating
from client.gcs import (
    ContainerPath,
    DataPath,
    Field,
    ParamCalldata,
    ParamRaw,
    ParamTokenAmount,
    PathLeaf,
    PathLeafType,
    PathTuple,
    TxInfo,
    TypeFamily,
    Value,
)
from client.proxy_info import ProxyInfo
from client.settings import SettingID, settings_toggle
from client.status_word import StatusWord
from client.trusted_name import TrustedName, TrustedNameSource, TrustedNameType
from client.tx_simu import TxSimu
from client.utils import get_selector_from_data, recover_message
from constants import ABIS_FOLDER
from eth_account.messages import encode_typed_data
from fields_utils import get_all_paths, get_all_tuple_array_paths
from ragger.backend import BackendInterface
from ragger.error import ExceptionRAPDU
from ragger.navigator import NavigateWithScenario
from test_gcs import compute_inst_hash

BIP32_PATH = "m/44'/60'/0'/0/0"
DEVICE_ADDR: bytes | None = None


def set_wallet_addr(backend: BackendInterface) -> None:
    global DEVICE_ADDR

    # don't ask again if we already have it
    if DEVICE_ADDR is None:
        client = EthAppClient(backend)
        with client.get_public_addr(display=False):
            pass
        _, DEVICE_ADDR, _ = ResponseParser.pk_addr(client.response().data)


@pytest.fixture(autouse=True)
def init_wallet_addr(backend: BackendInterface):
    """Initialize wallet address before each test"""
    set_wallet_addr(backend)


def eip712_json_path() -> str:
    return f"{os.path.dirname(__file__)}/eip712_input_files"


def input_files() -> list[str]:
    files = []
    for file in os.scandir(eip712_json_path()):
        if fnmatch.fnmatch(file.name, "*-data.json"):
            files.append(file.path)
    return sorted(files)


def test_eip712_v0(scenario_navigator: NavigateWithScenario, simu_params: TxSimu | None = None):
    app_client = EthAppClient(scenario_navigator.backend)

    settings_toggle(
        scenario_navigator.backend.device,
        scenario_navigator.navigator,
        [SettingID.BLIND_SIGNING],
    )
    with open(input_files()[0], encoding="utf-8") as file:
        data = json.load(file)
    smsg = encode_typed_data(full_message=data)

    if simu_params is not None:
        set_wallet_addr(scenario_navigator.backend)
        simu_params.from_addr = DEVICE_ADDR
        simu_params.tx_hash = smsg.body
        simu_params.domain_hash = smsg.header
        response = app_client.provide_tx_simulation(simu_params)
        assert response.status == StatusWord.SWO_SUCCESS

    with app_client.eip712_sign_legacy(BIP32_PATH, smsg.header, smsg.body):
        scenario_navigator.review_approve_with_warning(do_comparison=False)

    vrs = ResponseParser.signature(app_client.response().data)
    assert DEVICE_ADDR == recover_message(data, vrs)


def eip712_new_common(
    scenario_navigator: NavigateWithScenario,
    data: dict,
    filters: dict | None = None,
    snapshots_dirname: str | None = None,
    nb_warnings: int = 0,
) -> None:
    app_client = EthAppClient(scenario_navigator.backend)

    InputData.process_data(app_client, data, filters)
    do_compare = snapshots_dirname is not None
    with app_client.eip712_sign_new(BIP32_PATH):
        if nb_warnings > 0:
            # Warning screen
            scenario_navigator.review_approve_with_warning(
                test_name=snapshots_dirname,
                do_comparison=do_compare,
                nb_warnings=nb_warnings,
            )
        else:
            scenario_navigator.review_approve(test_name=snapshots_dirname, do_comparison=do_compare)

    vrs = ResponseParser.signature(app_client.response().data)
    # verify signature
    assert DEVICE_ADDR == recover_message(data, vrs)


def test_eip712_datetime_out_of_range(scenario_navigator: NavigateWithScenario):
    """A filtered datetime the device cannot represent must be refused, not narrowed.

    The value is rendered through a signed time_t. Before, a value past its maximum
    was cast anyway and the screen showed a different date than the one the signature
    commits to.
    """
    app_client = EthAppClient(scenario_navigator.backend)

    data = {
        "domain": {
            "chainId": 1,
            "name": "Advanced test",
            "verifyingContract": "0xCcCCccccCCCCcCCCCCCcCcCccCcCCCcCcccccccC",
            "version": "1",
        },
        # past INT64_MAX, so no signed time_t can hold it
        "message": {"expires": 0xFFFFFFFFFFFFFFF0},
        "primaryType": "Expiry",
        "types": {
            "EIP712Domain": [
                {"name": "name", "type": "string"},
                {"name": "version", "type": "string"},
                {"name": "chainId", "type": "uint256"},
                {"name": "verifyingContract", "type": "address"},
            ],
            "Expiry": [{"name": "expires", "type": "uint64"}],
        },
    }
    filters = {
        "name": "Advanced Filtering",
        "fields": {"expires": {"type": "datetime", "name": "Will Expire"}},
    }

    with pytest.raises(ExceptionRAPDU) as e:
        InputData.process_data(app_client, data, filters)
    assert e.value.status == StatusWord.SWO_PARAMETER_ERROR_NO_INFO


def test_eip712_amount_join_repeated_value(scenario_navigator: NavigateWithScenario):
    """Repeated amounts of one join must not be signed while absent from the review.

    An amount join is keyed by token index and dropped as soon as its token/value pair
    has been rendered. A second value for the same join then had no token half left to
    pair with, so it was hashed but never displayed.
    """
    app_client = EthAppClient(scenario_navigator.backend)

    data = {
        "domain": {
            "chainId": 1,
            "name": "Advanced test",
            "verifyingContract": "0xCcCCccccCCCCcCCCCCCcCcCccCcCCCcCcccccccC",
            "version": "1",
        },
        "message": {
            "token": "0x6B175474E89094C44Da98b954EedeAC495271d0F",
            # two amounts for a single token/value join
            "values": [10000000000000000, 24500000000000000000],
        },
        "primaryType": "Transfer",
        "types": {
            "EIP712Domain": [
                {"name": "name", "type": "string"},
                {"name": "version", "type": "string"},
                {"name": "chainId", "type": "uint256"},
                {"name": "verifyingContract", "type": "address"},
            ],
            "Transfer": [
                {"name": "token", "type": "address"},
                {"name": "values", "type": "uint256[]"},
            ],
        },
    }
    filters = {
        "name": "Advanced Filtering",
        "fields": {
            "token": {"type": "amount_join_token", "id": 0},
            "values.[]": {"type": "amount_join_value", "name": "Send", "id": 0},
        },
    }

    InputData.process_data(app_client, data, filters)
    with pytest.raises(ExceptionRAPDU) as e:
        with app_client.eip712_sign_new(BIP32_PATH):
            pass
    assert e.value.status == StatusWord.SWO_REFERENCED_DATA_NOT_FOUND


@pytest.mark.parametrize(
    "fields,message,filter_fields",
    [
        (
            [{"name": "values", "type": "uint256[]"}, {"name": "token", "type": "address"}],
            {
                "values": [10000000000000000, 24500000000000000000],
                "token": "0x6B175474E89094C44Da98b954EedeAC495271d0F",
            },
            {
                "values.[]": {"type": "amount_join_value", "name": "Send", "id": 0},
                "token": {"type": "amount_join_token", "id": 0},
            },
        ),
        (
            [{"name": "tokens", "type": "address[]"}, {"name": "value", "type": "uint256"}],
            {
                "tokens": [
                    "0x6B175474E89094C44Da98b954EedeAC495271d0F",
                    "0xA0b86991c6218b36c1d19D4a2e9Eb0cE3606eB48",
                ],
                "value": 24500000000000000000,
            },
            {
                "tokens.[]": {"type": "amount_join_token", "id": 0},
                "value": {"type": "amount_join_value", "name": "Send", "id": 0},
            },
        ),
    ],
    ids=["values_before_token", "tokens_before_value"],
)
def test_eip712_amount_join_half_overwritten(
    scenario_navigator: NavigateWithScenario, fields: list, message: dict, filter_fields: dict
):
    """A token/value join half already received must not be replaced before it is shown."""
    app_client = EthAppClient(scenario_navigator.backend)

    data = {
        "domain": {
            "chainId": 1,
            "name": "Advanced test",
            "verifyingContract": "0xCcCCccccCCCCcCCCCCCcCcCccCcCCCcCcccccccC",
            "version": "1",
        },
        "message": message,
        "primaryType": "Transfer",
        "types": {
            "EIP712Domain": [
                {"name": "name", "type": "string"},
                {"name": "version", "type": "string"},
                {"name": "chainId", "type": "uint256"},
                {"name": "verifyingContract", "type": "address"},
            ],
            "Transfer": fields,
        },
    }
    filters = {"name": "Advanced Filtering", "fields": filter_fields}

    with pytest.raises(ExceptionRAPDU) as e:
        InputData.process_data(app_client, data, filters)
    assert e.value.status == StatusWord.SWO_PARAMETER_ERROR_NO_INFO


@pytest.mark.parametrize("other_filter_first", [False, True], ids=["alone", "after_other_filter"])
def test_eip712_array_element_filtered_late(
    scenario_navigator: NavigateWithScenario, monkeypatch: pytest.MonkeyPatch, other_filter_first: bool
):
    """An array element sent before its path's filter must not be signed unfiltered."""
    app_client = EthAppClient(scenario_navigator.backend)

    fields = [{"name": "values", "type": "uint256[]"}]
    message: dict = {"values": [1, 2]}
    filter_fields: dict = {"values.[]": {"type": "raw", "name": "Value"}}
    if other_filter_first:
        fields.insert(0, {"name": "to", "type": "address"})
        message["to"] = "0x6B175474E89094C44Da98b954EedeAC495271d0F"
        filter_fields = {"to": {"type": "raw", "name": "To"}, **filter_fields}

    data = {
        "domain": {
            "chainId": 1,
            "name": "Advanced test",
            "verifyingContract": "0xCcCCccccCCCCcCCCCCCcCcCccCcCCCcCcccccccC",
            "version": "1",
        },
        "message": message,
        "primaryType": "Transfer",
        "types": {
            "EIP712Domain": [
                {"name": "name", "type": "string"},
                {"name": "version", "type": "string"},
                {"name": "chainId", "type": "uint256"},
                {"name": "verifyingContract", "type": "address"},
            ],
            "Transfer": fields,
        },
    }
    filters = {"name": "Advanced Filtering", "fields": filter_fields}

    send_filter = InputData.send_filter
    skipped: list[str] = []

    def send_filter_skip_first_element(path: str, discarded: bool):
        if path == "values.[]" and not skipped:
            skipped.append(path)
            return None
        return send_filter(path, discarded)

    monkeypatch.setattr(InputData, "send_filter", send_filter_skip_first_element)

    InputData.process_data(app_client, data, filters)
    with pytest.raises(ExceptionRAPDU) as e:
        with app_client.eip712_sign_new(BIP32_PATH):
            pass
    assert e.value.status == StatusWord.SWO_REFERENCED_DATA_NOT_FOUND


@pytest.mark.parametrize(
    "filtering,status",
    [(False, StatusWord.SWO_INCORRECT_DATA), (True, StatusWord.SWO_PARAMETER_ERROR_NO_INFO)],
    ids=["no_filter", "filter"],
)
def test_eip712_string_embedded_nul(scenario_navigator: NavigateWithScenario, filtering: bool, status: StatusWord):
    """A string holding a NUL byte must be refused, not displayed up to the NUL."""
    app_client = EthAppClient(scenario_navigator.backend)

    data = {
        "domain": {
            "chainId": 1,
            "name": "Advanced test",
            "verifyingContract": "0xCcCCccccCCCCcCCCCCCcCcCccCcCCCcCcccccccC",
            "version": "1",
        },
        "message": {"note": "Approve\x00 and transfer everything"},
        "primaryType": "Note",
        "types": {
            "EIP712Domain": [
                {"name": "name", "type": "string"},
                {"name": "version", "type": "string"},
                {"name": "chainId", "type": "uint256"},
                {"name": "verifyingContract", "type": "address"},
            ],
            "Note": [{"name": "note", "type": "string"}],
        },
    }

    with pytest.raises(ExceptionRAPDU) as e:
        if filtering:
            filters = {"name": "Advanced Filtering", "fields": {"note": {"type": "raw", "name": "Note"}}}
            InputData.process_data(app_client, data, filters)
        else:
            InputData.process_data(app_client, data)
            with app_client.eip712_sign_new(BIP32_PATH):
                pass
    assert e.value.status == status


@pytest.mark.parametrize("delta", [-1, 1], ids=["short", "long"])
def test_eip712_fixed_bytes_wrong_length(scenario_navigator: NavigateWithScenario, monkeypatch: pytest.MonkeyPatch, delta: int):
    """A bytesN value must be exactly N bytes long."""
    app_client = EthAppClient(scenario_navigator.backend)

    def encode_bytes_fix_resized(value: str, typesize: int) -> bytes:
        return bytes(typesize + delta)

    monkeypatch.setitem(InputData.encoding_functions, EIP712FieldType.FIX_BYTES, encode_bytes_fix_resized)

    data = {
        "domain": {
            "chainId": 1,
            "name": "Fixed bytes test",
            "verifyingContract": "0xCcCCccccCCCCcCCCCCCcCcCccCcCCCcCcccccccC",
            "version": "1",
        },
        "message": {"salt": "0x" + "11" * 16},
        "primaryType": "Salted",
        "types": {
            "EIP712Domain": [
                {"name": "name", "type": "string"},
                {"name": "version", "type": "string"},
                {"name": "chainId", "type": "uint256"},
                {"name": "verifyingContract", "type": "address"},
            ],
            "Salted": [{"name": "salt", "type": "bytes16"}],
        },
    }

    with pytest.raises(ExceptionRAPDU) as e:
        InputData.process_data(app_client, data)
    assert e.value.status == StatusWord.SWO_PARAMETER_ERROR_NO_INFO


def get_filter_file_from_data_file(data_file: Path) -> Path:
    test_path = f"{data_file.parent}/{'-'.join(data_file.stem.split('-')[:-1])}"
    return Path(f"{test_path}-filter.json")


def pytest_generate_tests(metafunc: pytest.Metafunc) -> None:
    """Conditionally parametrize eip712_new test.

    (input_file, filtering) are correlated: filtering=True only when a matching
    filter file exists. They are parametrized together to avoid invalid combos.
    verbose_raw is orthogonal and parametrized independently; pytest computes
    the cartesian product of the two parametrize calls automatically.
    """
    if "input_file" in metafunc.fixturenames:
        params = []
        for f in input_files():
            data_file = Path(f)
            base = "-".join(data_file.stem.split("-")[:-1])
            params.append(pytest.param(data_file, False, id=f"{base}-no_filter"))
            if get_filter_file_from_data_file(data_file).is_file():
                params.append(pytest.param(data_file, True, id=f"{base}-filter"))
        metafunc.parametrize("input_file,filtering", params)
    if "verbose_raw" in metafunc.fixturenames:
        metafunc.parametrize("verbose_raw", [True, False])


def test_eip712_new(
    scenario_navigator: NavigateWithScenario,
    input_file: Path,
    verbose_raw: bool,
    filtering: bool,
    gating_params: Gating | None = None,
):
    settings_to_toggle: list[SettingID] = []

    filters = None
    if filtering:
        filterfile = get_filter_file_from_data_file(input_file)
        with open(filterfile, encoding="utf-8") as f:
            filters = json.load(f)
    else:
        settings_to_toggle.append(SettingID.BLIND_SIGNING)

    if verbose_raw:
        settings_to_toggle.append(SettingID.VERBOSE_EIP712)

    if len(settings_to_toggle) > 0:
        settings_toggle(
            scenario_navigator.backend.device,
            scenario_navigator.navigator,
            settings_to_toggle,
        )

    with open(input_file, encoding="utf-8") as file:
        data = json.load(file)

    snapshots_dirname: str | None = None
    nb_warnings = 1 if not filters or verbose_raw else 0
    if gating_params is not None:
        app_client = EthAppClient(scenario_navigator.backend)
        sig_ctx: dict[str, Any] = {}
        InputData.init_signature_context(sig_ctx, data["types"], data["domain"], filters or {})
        gating_params.selector = sig_ctx["schema_hash"]
        response = app_client.provide_gating(gating_params)
        assert response and response.status == StatusWord.SWO_SUCCESS
        nb_warnings += 1
        snapshots_dirname = "test_gating_eip712"

    eip712_new_common(scenario_navigator, data, filters, snapshots_dirname, nb_warnings)


class DataSet:
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
                "version": "1",
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
                    {"name": "verifyingContract", "type": "address"},
                ],
                "Transfer": [
                    {"name": "with", "type": "address"},
                    {"name": "value_recv", "type": "uint256"},
                    {"name": "token_send", "type": "address"},
                    {"name": "value_send", "type": "uint256"},
                    {"name": "token_recv", "type": "address"},
                    {"name": "expires", "type": "uint64"},
                ],
            },
        },
        {
            "name": "Advanced Filtering",
            "tokens": [
                {
                    "addr": "0xc02aaa39b223fe8d0a0e5c4f27ead9083c756cc2",
                    "ticker": "WETH",
                    "decimals": 18,
                    "chain_id": 1,
                },
                {
                    "addr": "0x6b175474e89094c44da98b954eedeac495271d0f",
                    "ticker": "DAI",
                    "decimals": 18,
                    "chain_id": 1,
                },
            ],
            "fields": {
                "value_send": {
                    "type": "amount_join_value",
                    "name": "Send",
                    "id": 1,
                },
                "token_send": {
                    "type": "amount_join_token",
                    "id": 1,
                },
                "value_recv": {
                    "type": "amount_join_value",
                    "name": "Receive",
                    "id": 0,
                },
                "token_recv": {
                    "type": "amount_join_token",
                    "id": 0,
                },
                "with": {
                    "type": "raw",
                    "name": "With",
                },
                "expires": {"type": "datetime", "name": "Will Expire"},
            },
        },
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
                ],
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
            },
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
            },
        },
        "_permit",
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
                ],
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
            },
        },
        {
            "name": "Permit filtering",
            "tokens": [],
            "fields": {
                "value": {
                    "type": "amount_join_value",
                    "name": "Send",
                },
                "deadline": {
                    "type": "datetime",
                    "name": "Deadline",
                },
            },
        },
        "_permit_unknown_token",
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
                "token_big": "0x6b175474e89094c44da98b954eedeac495271d0f",
                "value_big": c_uint64(-1).value,
                "token_biggest": "0x6b175474e89094c44da98b954eedeac495271d0f",
                "value_biggest": int(web3.constants.MAX_INT, 0),
            },
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
                    "id": 0,
                },
                "value_big": {
                    "type": "amount_join_value",
                    "name": "Big",
                    "id": 0,
                },
                "token_biggest": {
                    "type": "amount_join_token",
                    "id": 0,
                },
                "value_biggest": {
                    "type": "amount_join_value",
                    "name": "Biggest",
                    "id": 0,
                },
            },
        },
        "_unlimited",
    ),
]


@pytest.fixture(name="data_set", params=ADVANCED_DATA_SETS)
def data_set_fixture(request) -> DataSet:
    return request.param


def test_eip712_advanced_filtering(scenario_navigator: NavigateWithScenario, data_set: DataSet, verbose_raw: bool):
    if verbose_raw and data_set.suffix:
        pytest.skip("Skipping Verbose mode for this data sets")

    snapshots_dirname = scenario_navigator.test_name + data_set.suffix
    if verbose_raw:
        settings_toggle(
            scenario_navigator.backend.device,
            scenario_navigator.navigator,
            [SettingID.DISPLAY_HASH],
        )
        snapshots_dirname += "-verbose"

    eip712_new_common(scenario_navigator, data_set.data, data_set.filters, snapshots_dirname)


def test_eip712_filtering_empty_array(scenario_navigator: NavigateWithScenario, simu_params: TxSimu | None = None):
    app_client = EthAppClient(scenario_navigator.backend)

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
        },
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
        },
    }

    nb_warnings = 0
    if simu_params is not None:
        set_wallet_addr(scenario_navigator.backend)
        smsg = encode_typed_data(full_message=data)
        simu_params.tx_hash = smsg.body
        simu_params.domain_hash = smsg.header
        response = app_client.provide_tx_simulation(simu_params)
        assert response.status == StatusWord.SWO_SUCCESS
        nb_warnings = 1

    eip712_new_common(
        scenario_navigator,
        data,
        filters,
        scenario_navigator.test_name,
        nb_warnings=nb_warnings,
    )


TOKENS = [
    [
        {
            "addr": "0x1111111111111111111111111111111111111111",
            "ticker": "SRC",
            "decimals": 18,
            "chain_id": 1,
        },
    ],
    [
        {
            "addr": "0x2222222222222222222222222222222222222222",
            "ticker": "DST",
            "decimals": 18,
            "chain_id": 1,
        },
    ],
]


@pytest.fixture(name="tokens", params=TOKENS)
def tokens_fixture(request) -> list[dict]:
    return request.param


def test_eip712_advanced_missing_token(scenario_navigator: NavigateWithScenario, tokens: list[dict]):
    test_name = scenario_navigator.test_name
    for token in tokens:
        test_name += f"-{token['ticker']}"
    data = {
        "types": {
            "EIP712Domain": [
                {"name": "name", "type": "string"},
                {"name": "version", "type": "string"},
                {"name": "chainId", "type": "uint256"},
                {"name": "verifyingContract", "type": "address"},
            ],
            "Root": [
                {"name": "token_from", "type": "address"},
                {"name": "value_from", "type": "uint256"},
                {"name": "token_to", "type": "address"},
                {"name": "value_to", "type": "uint256"},
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
            "token_from": "0x1111111111111111111111111111111111111111",
            "value_from": web3.Web3.to_wei(3.65, "ether"),
            "token_to": "0x2222222222222222222222222222222222222222",
            "value_to": web3.Web3.to_wei(15.47, "ether"),
        },
    }

    filters = {
        "name": "Token not in CAL test",
        "tokens": tokens,
        "fields": {
            "token_from": {
                "type": "amount_join_token",
                "id": 0,
            },
            "value_from": {
                "type": "amount_join_value",
                "name": "From",
                "id": 0,
            },
            "token_to": {
                "type": "amount_join_token",
                "id": 1,
            },
            "value_to": {
                "type": "amount_join_value",
                "name": "To",
                "id": 1,
            },
        },
    }

    eip712_new_common(scenario_navigator, data, filters, test_name)


TRUSTED_NAMES = [
    (TrustedNameType.CONTRACT, TrustedNameSource.CAL, "Validator contract"),
    (TrustedNameType.ACCOUNT, TrustedNameSource.ENS, "validator.eth"),
]

FILT_TN_TYPES = [
    [TrustedNameType.CONTRACT],
    [TrustedNameType.ACCOUNT],
    [TrustedNameType.CONTRACT, TrustedNameType.ACCOUNT],
    [TrustedNameType.ACCOUNT, TrustedNameType.CONTRACT],
]


@pytest.fixture(name="trusted_name", params=TRUSTED_NAMES)
def trusted_name_fixture(request) -> tuple:
    return request.param


@pytest.fixture(name="filt_tn_types", params=FILT_TN_TYPES)
def filt_tn_types_fixture(request) -> list[TrustedNameType]:
    return request.param


def test_eip712_advanced_trusted_name(
    scenario_navigator: NavigateWithScenario,
    trusted_name: tuple,
    filt_tn_types: list[TrustedNameType],
):
    test_name = f"{scenario_navigator.test_name}_{trusted_name[0].name.lower()}_with"
    for t in filt_tn_types:
        test_name += f"_{t.name.lower()}"

    app_client = EthAppClient(scenario_navigator.backend)

    data: dict[str, Any] = {
        "types": {
            "EIP712Domain": [
                {"name": "name", "type": "string"},
                {"name": "version", "type": "string"},
                {"name": "chainId", "type": "uint256"},
                {"name": "verifyingContract", "type": "address"},
            ],
            "Root": [
                {"name": "validator", "type": "address"},
                {"name": "enable", "type": "bool"},
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
            "validator": "0x1111111111111111111111111111111111111111",
            "enable": True,
        },
    }

    filters = {
        "name": "Trusted name test",
        "fields": {
            "validator": {
                "type": "trusted_name",
                "name": "Validator",
                "tn_type": filt_tn_types,
                "tn_source": [TrustedNameSource.CAL, TrustedNameSource.ENS],
            },
            "enable": {
                "type": "raw",
                "name": "State",
            },
        },
    }

    if trusted_name[0] is TrustedNameType.ACCOUNT:
        challenge = ResponseParser.challenge(app_client.get_challenge().data)
    else:
        challenge = None

    app_client.provide_trusted_name(
        TrustedName(
            2,
            bytes.fromhex(data["message"]["validator"][2:]),
            trusted_name[2],
            tn_type=trusted_name[0],
            tn_source=trusted_name[1],
            chain_id=data["domain"]["chainId"],
            challenge=challenge,
        )
    )
    eip712_new_common(scenario_navigator, data, filters, test_name)


def test_eip712_bs_not_activated_error(scenario_navigator: NavigateWithScenario):
    with pytest.raises(ExceptionRAPDU) as e:
        eip712_new_common(scenario_navigator, ADVANCED_DATA_SETS[0].data)
    assert e.value.status == StatusWord.SWO_INCORRECT_DATA


def test_eip712_proxy(scenario_navigator: NavigateWithScenario):
    app_client = EthAppClient(scenario_navigator.backend)

    input_file = input_files()[0]
    with open(input_file, encoding="utf-8") as file:
        data = json.load(file)
    with open(get_filter_file_from_data_file(Path(input_file)), encoding="utf-8") as file:
        filters = json.load(file)
    # change its name & set a different address than the one in verifyingContract
    filters["name"] = "Proxy test"
    filters["address"] = "0xaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"

    proxy_info = ProxyInfo(
        ResponseParser.challenge(app_client.get_challenge().data),
        bytes.fromhex(data["domain"]["verifyingContract"][2:]),
        int(data["domain"]["chainId"]),
        bytes.fromhex(filters["address"][2:]),
    )

    app_client.provide_proxy_info(proxy_info.serialize())

    eip712_new_common(scenario_navigator, data, filters)


def gcs_handler(app_client: EthAppClient, json_data: dict) -> None:
    fields = [
        Field(
            1,
            "Amount",
            ParamTokenAmount(
                1,
                Value(
                    1,
                    TypeFamily.UINT,
                    type_size=32,
                    data_path=DataPath(
                        1,
                        [
                            PathTuple(1),
                            PathLeaf(PathLeafType.STATIC),
                        ],
                    ),
                ),
                token=Value(
                    1,
                    TypeFamily.ADDRESS,
                    container_path=ContainerPath.TO,
                ),
            ),
        ),
    ]
    # compute instructions hash
    inst_hash = compute_inst_hash(fields)
    tx_info = TxInfo(
        1,
        json_data["domain"]["chainId"],
        bytes.fromhex(json_data["message"]["to"][2:]),
        get_selector_from_data(json_data["message"]["data"]),
        inst_hash,
        "Token transfer",
        contract_name="USDC",
    )
    assert tx_info.contract_name is not None
    app_client.provide_token_metadata(tx_info.contract_name, tx_info.contract_addr, 6, tx_info.chain_id)

    app_client.provide_transaction_info(tx_info.serialize())

    for field in fields:
        app_client.provide_transaction_field_desc(field.serialize())


def gcs_handler_batch(app_client: EthAppClient, json_data: dict) -> None:
    # Load EIP-712 JSON data
    with open(f"{eip712_json_path()}/safe_batch.json", encoding="utf-8") as file:
        data = json.load(file)

    # Define tokens
    tokens: list[dict[str, Any]] = [
        {
            "ticker": "USDC",
            "address": bytes.fromhex("3c499c542cef5e3811e1192ce70d8cc03d5c3359"),
            "decimals": 6,
        },
        {
            "ticker": "USDC",
            "address": bytes.fromhex("3c499c542cef5e3811e1192ce70d8cc03d5c3359"),
            "decimals": 6,
        },
    ]
    # Encode token transfer data
    with open(f"{ABIS_FOLDER}/erc20.json", encoding="utf-8") as f:
        contract = web3.Web3().eth.contract(abi=json.load(f), address=None)
    tokenData0 = contract.encode_abi(
        "transfer",
        [
            bytes.fromhex("B8C8EB8EFC68796E766F6AB320DB8C165C064949"),
            int(0.004 * pow(10, tokens[0]["decimals"])),
        ],
    )
    tokenData1 = contract.encode_abi(
        "transfer",
        [
            bytes.fromhex("4DDA64E1EC1A2C00D0766F25877F6A3BC77F717E"),
            int(0.008 * pow(10, tokens[1]["decimals"])),
        ],
    )

    # Encode batchExecute data using token transfer data
    with open(f"{ABIS_FOLDER}/batch.json", encoding="utf-8") as f:
        contract = web3.Web3().eth.contract(abi=json.load(f), address=tokens[1]["address"])
    batchData = contract.encode_abi(
        "batchExecute",
        [
            [
                (tokens[0]["address"], web3.Web3.to_wei(0, "ether"), tokenData0),
                (tokens[1]["address"], web3.Web3.to_wei(0, "ether"), tokenData1),
            ]
        ],
    )

    # Top level transaction fields definition
    param_paths = get_all_tuple_array_paths(f"{ABIS_FOLDER}/batch.json", "batchExecute", "calls")
    L0_fields = [
        Field(
            1,
            "Transaction",
            ParamCalldata(
                1,
                Value(
                    1,
                    TypeFamily.BYTES,
                    data_path=DataPath(1, param_paths["data"]),
                ),
                Value(
                    1,
                    TypeFamily.ADDRESS,
                    data_path=DataPath(1, param_paths["to"]),
                ),
                amount=Value(
                    1,
                    TypeFamily.UINT,
                    data_path=DataPath(1, param_paths["value"]),
                ),
            ),
        ),
    ]
    # compute instructions hash
    L0_hash = compute_inst_hash(L0_fields)

    # Define intermediate execTransaction transaction info
    L0_tx_info = TxInfo(
        1,
        data["domain"]["chainId"],
        bytes.fromhex(json_data["domain"]["verifyingContract"][2:]),
        get_selector_from_data(batchData),
        L0_hash,
        "Batch transactions",
        creator_name="Ledger",
        creator_legal_name="Ledger Multisig",
        creator_url="https://www.ledger.com",
        contract_name="BatchExecutor",
    )

    # Lower batchExecute transaction fields definition
    param_paths = get_all_paths(f"{ABIS_FOLDER}/erc20.json", "transfer")
    L1_fields = [
        Field(
            1,
            "Amount",
            ParamTokenAmount(
                1,
                Value(
                    1,
                    TypeFamily.UINT,
                    data_path=DataPath(1, param_paths["_value"]),
                    type_size=32,
                ),
                Value(
                    1,
                    TypeFamily.ADDRESS,
                    container_path=ContainerPath.TO,
                ),
            ),
        ),
        Field(
            1,
            "To",
            ParamRaw(
                1,
                Value(
                    1,
                    TypeFamily.ADDRESS,
                    data_path=DataPath(1, param_paths["_to"]),
                ),
            ),
        ),
    ]
    # compute instructions hash
    L1_hash = compute_inst_hash(L1_fields)

    # Define lower batchExecute transaction info
    L1_tx_info = [
        TxInfo(
            1,
            data["domain"]["chainId"],
            tokens[0]["address"],
            get_selector_from_data(tokenData0),
            L1_hash,
            "Send",
            contract_name="USD_Coin",
        ),
        TxInfo(
            1,
            data["domain"]["chainId"],
            tokens[1]["address"],
            get_selector_from_data(tokenData1),
            L1_hash,
            "Send",
            contract_name="USD_Coin",
        ),
    ]

    proxy_info = ProxyInfo(
        ResponseParser.challenge(app_client.get_challenge().data),
        bytes.fromhex(json_data["message"]["to"][2:]),
        L0_tx_info.chain_id,
        L0_tx_info.contract_addr,
    )

    # Send Proxy information
    app_client.provide_proxy_info(proxy_info.serialize())

    # Send intermediate execTransaction info description
    app_client.provide_transaction_info(L0_tx_info.serialize())
    for f0 in L0_fields:
        # Send intermediate execTransaction fields description
        app_client.provide_transaction_field_desc(f0.serialize())

    # Lower batchExecute description
    for idx, i1 in enumerate(L1_tx_info):
        # Send lower batchExecute info description
        app_client.provide_transaction_info(i1.serialize())
        app_client.provide_token_metadata(
            tokens[idx]["ticker"],
            tokens[idx]["address"],
            tokens[idx]["decimals"],
            data["domain"]["chainId"],
        )
        for f1 in L1_fields:
            # Send lower batchExecute fields description
            app_client.provide_transaction_field_desc(f1.serialize())


def gcs_handler_no_param(app_client: EthAppClient, json_data: dict) -> None:
    tx_info = TxInfo(
        1,
        json_data["domain"]["chainId"],
        bytes.fromhex(json_data["message"]["to"][2:]),
        get_selector_from_data(json_data["message"]["data"]),
        hashlib.sha3_256().digest(),
        "get total supply",
        creator_name="WETH",
        creator_legal_name="Wrapped Ether",
        creator_url="weth.io",
    )

    app_client.provide_transaction_info(tx_info.serialize())


def eip712_calldata_common(
    scenario_navigator: NavigateWithScenario,
    filename: str,
    handler: Callable | None = None,
):
    with open(f"{eip712_json_path()}/{filename}.json", encoding="utf-8") as file:
        data = json.load(file)

    filters = {
        "name": "Calldata test",
        "calldatas": [
            {
                "index": 0,
                "handler": handler,
                "value_flag": True,
                "callee_flag": EIP712CalldataParamPresence.PRESENT_FILTERED,
                "chain_id_flag": False,
                "selector_flag": False,
                "amount_flag": True,
                "spender_flag": EIP712CalldataParamPresence.NONE,
            },
        ],
        "fields": {
            "to": {
                "type": "calldata_callee",
                "index": 0,
            },
            "value": {
                "type": "calldata_amount",
                "index": 0,
            },
            "data": {
                "type": "calldata_value",
                "index": 0,
            },
        },
    }

    eip712_new_common(scenario_navigator, data, filters, scenario_navigator.test_name)


def test_eip712_calldata(scenario_navigator: NavigateWithScenario):
    eip712_calldata_common(scenario_navigator, "safe", gcs_handler)


def test_eip712_calldata_empty_send(scenario_navigator: NavigateWithScenario):
    app_client = EthAppClient(scenario_navigator.backend)
    filename = "safe_empty"

    with Path(f"{eip712_json_path()}/{filename}.json").open(encoding="utf-8") as file:
        json_data = json.load(file)

    app_client.provide_trusted_name(
        TrustedName(
            2,
            bytes.fromhex(json_data["message"]["to"][2:]),
            "MAB_addr",
            tn_type=TrustedNameType.ACCOUNT,
            tn_source=TrustedNameSource.MULTISIG_ADDRESS_BOOK,
            chain_id=json_data["domain"]["chainId"],
            challenge=ResponseParser.challenge(app_client.get_challenge().data),
            owner=DEVICE_ADDR,
            owner_deriv_path=BIP32_PATH,
        )
    )
    eip712_calldata_common(scenario_navigator, filename)


def test_eip712_calldata_no_param(scenario_navigator: NavigateWithScenario):
    eip712_calldata_common(scenario_navigator, "safe_calldata_no_param", gcs_handler_no_param)


def test_eip712_gondi(scenario_navigator: NavigateWithScenario):
    """Test a basic EIP712 payload signature"""

    # Enable blind signing for EIP712
    settings_toggle(
        scenario_navigator.backend.device,
        scenario_navigator.navigator,
        [SettingID.BLIND_SIGNING],
    )

    # Define a simple EIP712 payload
    data = {
        "types": {
            "EIP712Domain": [
                {"name": "name", "type": "string"},
                {"name": "version", "type": "string"},
                {"name": "chainId", "type": "uint256"},
                {"name": "verifyingContract", "type": "address"},
            ],
            "Root": [
                {"name": "child", "type": "Inner[]"},
            ],
            "Inner": [
                {"name": "child", "type": "Leaf"},
            ],
            "Leaf": [
                {"name": "value", "type": "uint256"},
            ],
        },
        "primaryType": "Root",
        "domain": {
            "name": "DOMAIN",
            "version": "3.1",
            "chainId": 31337,
            "verifyingContract": "0x95401dc811bb5740090279ba06cfa8fcf6113778",
        },
        "message": {
            "child": [
                {
                    "child": {
                        "value": 2,
                    },
                }
            ],
        },
    }
    eip712_new_common(scenario_navigator, data, nb_warnings=1)


def test_eip712_batch(scenario_navigator: NavigateWithScenario):
    with open(f"{eip712_json_path()}/safe_batch.json", encoding="utf-8") as file:
        data = json.load(file)

    filters = {
        "name": "Calldata test",
        "calldatas": [
            {
                "index": 0,
                "handler": gcs_handler_batch,
                "value_flag": True,
                "callee_flag": EIP712CalldataParamPresence.PRESENT_FILTERED,
                "chain_id_flag": False,
                "selector_flag": False,
                "amount_flag": True,
                "spender_flag": EIP712CalldataParamPresence.NONE,
            },
        ],
        "fields": {
            "to": {
                "type": "calldata_callee",
                "index": 0,
            },
            "value": {
                "type": "calldata_amount",
                "index": 0,
            },
            "data": {
                "type": "calldata_value",
                "index": 0,
            },
            "operation": {
                "type": "raw",
                "name": "Operation type",
            },
            "baseGas": {
                "type": "raw",
                "name": "Gas amount",
            },
            "gasPrice": {
                "type": "raw",
                "name": "Gas price",
            },
            "gasToken": {
                "type": "raw",
                "name": "Gas token",
            },
            "refundReceiver": {
                "type": "trusted_name",
                "name": "Gas receiver",
                "tn_type": [
                    TrustedNameType.ACCOUNT,
                    TrustedNameType.CONTRACT,
                    TrustedNameType.TOKEN,
                ],
                "tn_source": [
                    TrustedNameSource.CAL,
                    TrustedNameSource.ENS,
                    TrustedNameSource.UD,
                    TrustedNameSource.FN,
                ],
            },
        },
    }

    eip712_new_common(scenario_navigator, data, filters, scenario_navigator.test_name)


def test_eip712_v0_reject(scenario_navigator: NavigateWithScenario):
    """Declining the blind typed-message review must return an error, not a signature.

    Not compared against snapshots: the rejecting tap answers the APDU, and ragger
    surfaces that error from inside wait_for_screen_change(), racing the status screen
    the same tap draws.
    """
    app_client = EthAppClient(scenario_navigator.backend)

    settings_toggle(
        scenario_navigator.backend.device,
        scenario_navigator.navigator,
        [SettingID.BLIND_SIGNING],
    )
    with open(input_files()[0], encoding="utf-8") as file:
        data = json.load(file)
    smsg = encode_typed_data(full_message=data)

    with pytest.raises(ExceptionRAPDU) as err:
        with app_client.eip712_sign_legacy(BIP32_PATH, smsg.header, smsg.body):
            scenario_navigator.review_reject_with_warning(do_comparison=False)
    assert err.value.status == StatusWord.SWO_CONDITIONS_NOT_SATISFIED


def test_eip712_new_reject(scenario_navigator: NavigateWithScenario):
    """Declining a clear-signed typed message must return an error, not a signature."""
    app_client = EthAppClient(scenario_navigator.backend)

    data = {
        "types": {
            "EIP712Domain": [
                {"name": "name", "type": "string"},
                {"name": "version", "type": "string"},
                {"name": "verifyingContract", "type": "address"},
                {"name": "chainId", "type": "uint256"},
            ],
            "Message": [{"name": "text", "type": "string"}],
        },
        "primaryType": "Message",
        "domain": {
            "name": "test",
            "version": "1",
            "verifyingContract": "0x0000000000000000000000000000000000000000",
            "chainId": 1,
        },
        "message": {"text": "This is a test"},
    }
    filters = {
        "name": "Reject test",
        "fields": {"text": {"type": "raw", "name": "Text"}},
    }

    InputData.process_data(app_client, data, filters)
    with pytest.raises(ExceptionRAPDU) as err:
        with app_client.eip712_sign_new(BIP32_PATH):
            scenario_navigator.review_reject(do_comparison=False)
    assert err.value.status == StatusWord.SWO_CONDITIONS_NOT_SATISFIED
