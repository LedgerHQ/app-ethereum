import fnmatch
import os
from pathlib import Path
import json
from typing import Optional, Callable
from ctypes import c_uint64
import hashlib

import pytest
from eth_account.messages import encode_typed_data
import web3

from ragger.backend import BackendInterface
from ragger.navigator import NavigateWithScenario
from ragger.error import ExceptionRAPDU

import client.response_parser as ResponseParser
from client.utils import recover_message, get_selector_from_data
from client.client import EthAppClient, TrustedNameType, TrustedNameSource, EIP712CalldataParamPresence
from client.status_word import StatusWord
from client.eip712 import InputData
from client.settings import SettingID, settings_toggle
from client.tx_simu import TxSimu
from client.proxy_info import ProxyInfo

from client.gcs import (
    Field, ParamType, ParamRaw, Value, TypeFamily, DataPath, PathTuple, ParamTrustedName,
    ParamNFT, ParamDatetime, DatetimeType, ParamTokenAmount, ParamToken, ParamCalldata,
    ParamAmount, ContainerPath, PathLeaf, PathLeafType, PathRef, PathArray, TxInfo
)


BIP32_PATH = "m/44'/60'/0'/0/0"
DEVICE_ADDR: Optional[bytes] = None


def set_wallet_addr(backend: BackendInterface) -> bytes:
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
        if fnmatch.fnmatch(file, "*-data.json"):
            files.append(file.path)
    return sorted(files)


@pytest.fixture(name="input_file", params=input_files())
def input_file_fixture(request) -> Path:
    return Path(request.param)


@pytest.fixture(name="verbose_raw", params=[True, False])
def verbose_raw_fixture(request) -> bool:
    return request.param


@pytest.fixture(name="filtering", params=[False, True])
def filtering_fixture(request) -> bool:
    return request.param


def test_eip712_v0(scenario_navigator: NavigateWithScenario, simu_params: Optional[TxSimu] = None):
    app_client = EthAppClient(scenario_navigator.backend)

    settings_toggle(scenario_navigator.backend.device, scenario_navigator.navigator, [SettingID.BLIND_SIGNING])
    with open(input_files()[0], encoding="utf-8") as file:
        data = json.load(file)
    smsg = encode_typed_data(full_message=data)

    if simu_params is not None:
        set_wallet_addr(scenario_navigator.backend)
        simu_params.from_addr = DEVICE_ADDR
        simu_params.tx_hash = smsg.body
        simu_params.domain_hash = smsg.header
        response = app_client.provide_tx_simulation(simu_params)
        assert response.status == StatusWord.OK

    with app_client.eip712_sign_legacy(BIP32_PATH, smsg.header, smsg.body):
        scenario_navigator.review_approve_with_warning(do_comparison=False)

    vrs = ResponseParser.signature(app_client.response().data)
    assert DEVICE_ADDR == recover_message(data, vrs)


def eip712_new_common(scenario_navigator: NavigateWithScenario,
                      data: dict,
                      filters: Optional[dict] = None,
                      snapshots_dirname: Optional[str] = None,
                      with_warning: bool = False) -> bytes:
    app_client = EthAppClient(scenario_navigator.backend)

    InputData.process_data(app_client, data, filters)
    do_compare = snapshots_dirname is not None
    with app_client.eip712_sign_new(BIP32_PATH):
        if with_warning:
            # Warning screen
            scenario_navigator.review_approve_with_warning(test_name=snapshots_dirname, do_comparison=do_compare)
        else:
            scenario_navigator.review_approve(test_name=snapshots_dirname, do_comparison=do_compare)

    vrs = ResponseParser.signature(app_client.response().data)
    # verify signature
    assert DEVICE_ADDR == recover_message(data, vrs)


def get_filter_file_from_data_file(data_file: Path) -> Path:
    test_path = f"{data_file.parent}/{'-'.join(data_file.stem.split('-')[:-1])}"
    return Path(f"{test_path}-filter.json")


def test_eip712_new(scenario_navigator: NavigateWithScenario,
                    input_file: Path,
                    verbose_raw: bool,
                    filtering: bool):
    settings_to_toggle: list[SettingID] = []

    filters = None
    if filtering:
        try:
            filterfile = get_filter_file_from_data_file(input_file)
            with open(filterfile, encoding="utf-8") as f:
                filters = json.load(f)
        except (IOError, json.decoder.JSONDecodeError) as e:
            pytest.skip(f"{filterfile.name}: {e.strerror}")
    else:
        settings_to_toggle.append(SettingID.BLIND_SIGNING)

    if verbose_raw:
        settings_to_toggle.append(SettingID.VERBOSE_EIP712)

    if len(settings_to_toggle) > 0:
        settings_toggle(scenario_navigator.backend.device, scenario_navigator.navigator, settings_to_toggle)

    with open(input_file, encoding="utf-8") as file:
        data = json.load(file)
        eip712_new_common(scenario_navigator, data, filters, with_warning=bool(not filters or verbose_raw))


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
                    "token": 1,
                },
                "token_send": {
                    "type": "amount_join_token",
                    "token": 1,
                },
                "value_recv": {
                    "type": "amount_join_value",
                    "name": "Receive",
                    "token": 0,
                },
                "token_recv": {
                    "type": "amount_join_token",
                    "token": 0,
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


def test_eip712_advanced_filtering(scenario_navigator: NavigateWithScenario,
                                   test_name: str,
                                   data_set: DataSet,
                                   verbose_raw: bool):
    if verbose_raw and data_set.suffix:
        pytest.skip("Skipping Verbose mode for this data sets")

    snapshots_dirname = test_name + data_set.suffix
    if verbose_raw:
        settings_toggle(scenario_navigator.backend.device, scenario_navigator.navigator, [SettingID.DISPLAY_HASH])
        snapshots_dirname += "-verbose"

    eip712_new_common(scenario_navigator, data_set.data, data_set.filters, snapshots_dirname)


def test_eip712_filtering_empty_array(scenario_navigator: NavigateWithScenario,
                                      test_name: str,
                                      simu_params: Optional[TxSimu] = None):
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

    if simu_params is not None:
        set_wallet_addr(scenario_navigator.backend)
        smsg = encode_typed_data(full_message=data)
        simu_params.tx_hash = smsg.body
        simu_params.domain_hash = smsg.header
        response = app_client.provide_tx_simulation(simu_params)
        assert response.status == StatusWord.OK

    eip712_new_common(scenario_navigator, data, filters, test_name, with_warning=bool(simu_params is not None))


TOKENS = [
    [
        {
            "addr": "0x1111111111111111111111111111111111111111",
            "ticker": "SRC",
            "decimals": 18,
            "chain_id": 1,
        },
        {},
    ],
    [
        {},
        {
            "addr": "0x2222222222222222222222222222222222222222",
            "ticker": "DST",
            "decimals": 18,
            "chain_id": 1,
        },
    ]
]


@pytest.fixture(name="tokens", params=TOKENS)
def tokens_fixture(request) -> list[dict]:
    return request.param


def test_eip712_advanced_missing_token(scenario_navigator: NavigateWithScenario,
                                       test_name: str,
                                       tokens: list[dict]):
    test_name += f"-{len(tokens[0]) == 0}-{len(tokens[1]) == 0}"

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
            "token_from": "0x1111111111111111111111111111111111111111",
            "value_from": web3.Web3.to_wei(3.65, "ether"),
            "token_to": "0x2222222222222222222222222222222222222222",
            "value_to": web3.Web3.to_wei(15.47, "ether"),
        }
    }

    filters = {
        "name": "Token not in CAL test",
        "tokens": tokens,
        "fields": {
            "token_from": {
                "type": "amount_join_token",
                "token": 0,
            },
            "value_from": {
                "type": "amount_join_value",
                "name": "From",
                "token": 0,
            },
            "token_to": {
                "type": "amount_join_token",
                "token": 1,
            },
            "value_to": {
                "type": "amount_join_value",
                "name": "To",
                "token": 1,
            },
        }
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


def test_eip712_advanced_trusted_name(scenario_navigator: NavigateWithScenario,
                                      test_name: str,
                                      trusted_name: tuple,
                                      filt_tn_types: list[TrustedNameType]):
    test_name += f"_{trusted_name[0].name.lower()}_with"
    for t in filt_tn_types:
        test_name += f"_{t.name.lower()}"

    app_client = EthAppClient(scenario_navigator.backend)

    data = {
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
            "validator": "0x1111111111111111111111111111111111111111",
            "enable": True,
        }
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
        }
    }

    if trusted_name[0] is TrustedNameType.ACCOUNT:
        challenge = ResponseParser.challenge(app_client.get_challenge().data)
    else:
        challenge = None

    app_client.provide_trusted_name_v2(bytes.fromhex(data["message"]["validator"][2:]),
                                       trusted_name[2],
                                       trusted_name[0],
                                       trusted_name[1],
                                       data["domain"]["chainId"],
                                       challenge=challenge)
    eip712_new_common(scenario_navigator, data, filters, test_name)


def test_eip712_bs_not_activated_error(scenario_navigator: NavigateWithScenario):
    with pytest.raises(ExceptionRAPDU) as e:
        eip712_new_common(scenario_navigator, ADVANCED_DATA_SETS[0].data)
    assert e.value.status == StatusWord.INVALID_DATA


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
                        ]
                    ),
                ),
                token=Value(
                    1,
                    TypeFamily.ADDRESS,
                    container_path=ContainerPath.TO,
                ),
            )
        ),
    ]
    # compute instructions hash
    inst_hash = hashlib.sha3_256()
    for field in fields:
        inst_hash.update(field.serialize())
    tx_info = TxInfo(
        1,
        json_data["domain"]["chainId"],
        bytes.fromhex(json_data["message"]["to"][2:]),
        get_selector_from_data(json_data["message"]["data"]),
        inst_hash.digest(),
        "Token transfer",
        contract_name="USDC",
    )
    app_client.provide_token_metadata(tx_info.contract_name, tx_info.contract_addr, 6, tx_info.chain_id)

    app_client.provide_transaction_info(tx_info.serialize())

    for field in fields:
        app_client.provide_transaction_field_desc(field.serialize())


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


def eip712_calldata_common(scenario_navigator: NavigateWithScenario,
                           test_name: str,
                           filename: str,
                           handler: Optional[Callable] = None):
    with open("%s/%s.json" % (eip712_json_path(), filename)) as file:
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
        }
    }

    eip712_new_common(scenario_navigator, data, filters, test_name)


def test_eip712_calldata(scenario_navigator: NavigateWithScenario, test_name: str):
    eip712_calldata_common(scenario_navigator, test_name, "safe", gcs_handler)


def test_eip712_calldata_empty_send(scenario_navigator: NavigateWithScenario, test_name: str):
    eip712_calldata_common(scenario_navigator, test_name, "safe_empty")


def test_eip712_calldata_no_param(scenario_navigator: NavigateWithScenario, test_name: str):
    eip712_calldata_common(scenario_navigator, test_name, "safe_calldata_no_param", gcs_handler_no_param)


def test_eip712_payload(scenario_navigator: NavigateWithScenario):
    """Test a basic EIP712 payload signature"""
    app_client = EthAppClient(scenario_navigator.backend)
    
    # Enable blind signing for EIP712
    settings_toggle(scenario_navigator.backend.device, scenario_navigator.navigator, [SettingID.BLIND_SIGNING])
    
    # Define a simple EIP712 payload
    data = {
        "types": {
            "ExecutionData": [
                {
                    "name": "offerExecution",
                    "type": "OfferExecution[]"
                },
            ],
            "EIP712Domain": [
                {
                    "name": "name",
                    "type": "string"
                },
                {
                    "name": "version",
                    "type": "string"
                },
                {
                    "name": "chainId",
                    "type": "uint256"
                },
                {
                    "name": "verifyingContract",
                    "type": "address"
                }
            ],
            "LoanOffer": [
                {
                    "name": "offerId",
                    "type": "uint256"
                },
            ],
            "OfferExecution": [
                {
                    "name": "offer",
                    "type": "LoanOffer"
                },
            ],
        },
        "primaryType": "ExecutionData",
        "domain": {
            "name": "MY_BEAUTIFUL_DOMAIN",
            "version": "3.1",
            "chainId": 31337,
            "verifyingContract": "0x95401dc811bb5740090279ba06cfa8fcf6113778"
        },
        "message": {
            "offerExecution": [
                {
                    "offer": {
                        "offerId": 2,
                    },
                }
            ],
        }
    }
    eip712_new_common(scenario_navigator, data, None, None, with_warning=True)
    