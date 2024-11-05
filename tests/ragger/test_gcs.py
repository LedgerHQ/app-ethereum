import struct
import json
import hashlib

from ragger.backend import BackendInterface
from ragger.firmware import Firmware
from ragger.navigator import Navigator
from ragger.navigator.navigation_scenario import NavigateWithScenario

import pytest
from web3 import Web3

import client.response_parser as ResponseParser
from client.client import EthAppClient, SignMode, TrustedNameType, TrustedNameSource
from client.utils import get_selector_from_data
from client.gcs import *

from constants import ABIS_FOLDER


def test_nft(firmware: Firmware,
             backend: BackendInterface,
             scenario_navigator: NavigateWithScenario,
             test_name: str):
    app_client = EthAppClient(backend)

    if firmware == Firmware.NANOS:
        pytest.skip("Not supported on LNS")

    with open(f"{ABIS_FOLDER}/erc1155.json", encoding="utf-8") as file:
        contract = Web3().eth.contract(
            abi=json.load(file),
            address=None
        )
    recipient_addr = bytes.fromhex("1111111111111111111111111111111111111111")
    data = contract.encode_abi("safeBatchTransferFrom", [
        bytes.fromhex("Dad77910DbDFdE764fC21FCD4E74D71bBACA6D8D"),
        recipient_addr,
        [
            0xff,
            0xffff,
            0xffffff,
            0xffffffff,
        ],
        [
            1,
            2,
            3,
            4,
        ],
        bytes.fromhex("deadbeef1337cafe"),
    ])
    tx_params = {
        "nonce": 235,
        "maxFeePerGas": Web3.to_wei(100, "gwei"),
        "maxPriorityFeePerGas": Web3.to_wei(10, "gwei"),
        "gas": 44001,
        # OpenSea Shared Storefront
        "to": bytes.fromhex("495f947276749ce646f68ac8c248420045cb7b5e"),
        "data": data,
        "chainId": 1
    }
    with app_client.sign("m/44'/60'/0'/0/0", tx_params, SignMode.STORE):
        pass

    fields = [
            Field(
                1,
                "From",
                ParamType.RAW,
                ParamRaw(
                    1,
                    Value(
                        1,
                        TypeFamily.ADDRESS,
                        data_path=DataPath(
                            1,
                            [
                                PathTuple(0),
                                PathLeaf(PathLeafType.STATIC),
                            ]
                        ),
                    )
                )
            ),
            Field(
                1,
                "To",
                ParamType.TRUSTED_NAME,
                ParamTrustedName(
                    1,
                    Value(
                        1,
                        TypeFamily.ADDRESS,
                        data_path=DataPath(
                            1,
                            [
                                PathTuple(1),
                                PathLeaf(PathLeafType.STATIC),
                            ]
                        ),
                    ),
                    [
                        TrustedNameType.ACCOUNT,
                    ],
                    [
                        TrustedNameSource.UD,
                        TrustedNameSource.ENS,
                        TrustedNameSource.FN,
                    ],
                )
            ),
            Field(
                1,
                "NFTs",
                ParamType.NFT,
                ParamNFT(
                    1,
                    Value(
                        1,
                        TypeFamily.UINT,
                        type_size=32,
                        data_path=DataPath(
                            1,
                            [
                                PathTuple(2),
                                PathRef(),
                                PathArray(1),
                                PathLeaf(PathLeafType.STATIC),
                            ]
                        ),
                    ),
                    Value(
                        1,
                        TypeFamily.ADDRESS,
                        container_path=ContainerPath.TO
                    )
                )
            ),
            Field(
                1,
                "Values",
                ParamType.RAW,
                ParamRaw(
                    1,
                    Value(
                        1,
                        TypeFamily.UINT,
                        type_size=32,
                        data_path=DataPath(
                            1,
                            [
                                PathTuple(3),
                                PathRef(),
                                PathArray(1),
                                PathLeaf(PathLeafType.STATIC),
                            ]
                        ),
                    )
                )
            ),
            Field(
                1,
                "Data",
                ParamType.RAW,
                ParamRaw(
                    1,
                    Value(
                        1,
                        TypeFamily.BYTES,
                        data_path=DataPath(
                            1,
                            [
                                PathTuple(4),
                                PathRef(),
                                PathLeaf(PathLeafType.DYNAMIC),
                            ]
                        ),
                    )
                )
            ),
    ]

    # compute instructions hash
    inst_hash = hashlib.sha3_256()
    for field in fields:
        inst_hash.update(field.serialize())

    tx_info = TxInfo(
        1,
        tx_params["chainId"],
        tx_params["to"],
        get_selector_from_data(tx_params["data"]),
        inst_hash.digest(),
        "batch transfer NFTs",
    )

    app_client.provide_transaction_info(tx_info.serialize())
    challenge = ResponseParser.challenge(app_client.get_challenge().data)
    app_client.provide_trusted_name_v2(recipient_addr,
                                       "gerard.eth",
                                       TrustedNameType.ACCOUNT,
                                       TrustedNameSource.ENS,
                                       tx_params["chainId"],
                                       challenge=challenge)
    app_client.provide_nft_metadata("OpenSea Shared Storefront", tx_params["to"], tx_params["chainId"])

    for field in fields:
        payload = field.serialize()
        app_client.send_raw(0xe0, 0x28, 0x01, 0x00, struct.pack(">H", len(payload)) + payload)

    with app_client.send_raw_async(0xe0, 0x04, 0x00, 0x02, bytes()):
        scenario_navigator.review_approve(test_name=test_name, custom_screen_text="Sign transaction")


def test_poap(firmware: Firmware,
              backend: BackendInterface,
              scenario_navigator: NavigateWithScenario,
              test_name: str):
    app_client = EthAppClient(backend)

    if firmware == Firmware.NANOS:
        pytest.skip("Not supported on LNS")

    with open(f"{ABIS_FOLDER}/poap.abi.json", encoding="utf-8") as file:
        contract = Web3().eth.contract(
            abi=json.load(file),
            address=None
        )
    data = contract.encode_abi("mintToken", [
        175676,
        7163978,
        bytes.fromhex("Dad77910DbDFdE764fC21FCD4E74D71bBACA6D8D"),
        1730621615,
        bytes.fromhex("8991da687cff5300959810a08c4ec183bb2a56dc82f5aac2b24f1106c2d983ac6f7a6b28700a236724d814000d0fd8c395fcf9f87c4424432ebf30c9479201d71c")
    ])
    tx_params = {
        "nonce": 235,
        "maxFeePerGas": Web3.to_wei(100, "gwei"),
        "maxPriorityFeePerGas": Web3.to_wei(10, "gwei"),
        "gas": 44001,
        # PoapBridge
        "to": bytes.fromhex("0bb4D3e88243F4A057Db77341e6916B0e449b158"),
        "data": data,
        "chainId": 1
    }
    with app_client.sign("m/44'/60'/0'/0/0", tx_params, SignMode.STORE):
        pass

    fields = [
            Field(
                1,
                "Event ID",
                ParamType.RAW,
                ParamRaw(
                    1,
                    Value(
                        1,
                        TypeFamily.UINT,
                        type_size=32,
                        data_path=DataPath(
                            1,
                            [
                                PathTuple(0),
                                PathLeaf(PathLeafType.STATIC),
                            ]
                        ),
                    )
                )
            ),
            Field(
                1,
                "Token ID",
                ParamType.RAW,
                ParamRaw(
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
                    )
                )
            ),
            Field(
                1,
                "Receiver",
                ParamType.RAW,
                ParamRaw(
                    1,
                    Value(
                        1,
                        TypeFamily.ADDRESS,
                        data_path=DataPath(
                            1,
                            [
                                PathTuple(2),
                                PathLeaf(PathLeafType.STATIC),
                            ]
                        ),
                    )
                )
            ),
            Field(
                1,
                "Expiration time",
                ParamType.DATETIME,
                ParamDatetime(
                    1,
                    Value(
                        1,
                        TypeFamily.UINT,
                        type_size=32,
                        data_path=DataPath(
                            1,
                            [
                                PathTuple(3),
                                PathLeaf(PathLeafType.STATIC),
                            ]
                        ),
                    ),
                    DatetimeType.DT_UNIX
                )
            ),
            Field(
                1,
                "Signature",
                ParamType.RAW,
                ParamRaw(
                    1,
                    Value(
                        1,
                        TypeFamily.BYTES,
                        data_path=DataPath(
                            1,
                            [
                                PathTuple(4),
                                PathRef(),
                                PathLeaf(PathLeafType.DYNAMIC),
                            ]
                        ),
                    )
                )
            ),
    ]

    # compute instructions hash
    inst_hash = hashlib.sha3_256()
    for field in fields:
        inst_hash.update(field.serialize())

    tx_info = TxInfo(
        1,
        tx_params["chainId"],
        tx_params["to"],
        get_selector_from_data(tx_params["data"]),
        inst_hash.digest(),
        "mint POAP",
        creator_name="POAP",
        creator_legal_name="Proof of Attendance Protocol",
        creator_url="poap.xyz",
        contract_name="PoapBridge",
        deploy_date=1646305200
    )

    app_client.provide_transaction_info(tx_info.serialize())

    for field in fields:
        payload = field.serialize()
        app_client.send_raw(0xe0, 0x28, 0x01, 0x00, struct.pack(">H", len(payload)) + payload)

    with app_client.send_raw_async(0xe0, 0x04, 0x00, 0x02, bytes()):
        scenario_navigator.review_approve(test_name=test_name, custom_screen_text="Sign transaction")


def test_1inch(firmware: Firmware,
               backend: BackendInterface,
               scenario_navigator: NavigateWithScenario,
               test_name: str):
    app_client = EthAppClient(backend)

    if firmware == Firmware.NANOS:
        pytest.skip("Not supported on LNS")

    with open(f"{ABIS_FOLDER}/1inch.abi.json", encoding="utf-8") as file:
        contract = Web3().eth.contract(
            abi=json.load(file),
            address=None
        )
    data = contract.encode_abi("swap", [
        bytes.fromhex("F313B370D28760b98A2E935E56Be92Feb2c4EC04"),
        [
            bytes.fromhex("EeeeeEeeeEeEeeEeEeEeeEEEeeeeEeeeeeeeEEeE"),
            bytes.fromhex("A0b86991c6218b36c1d19D4a2e9Eb0cE3606eB48"),
            bytes.fromhex("F313B370D28760b98A2E935E56Be92Feb2c4EC04"),
            bytes.fromhex("Dad77910DbDFdE764fC21FCD4E74D71bBACA6D8D"),
            Web3.to_wei(0.22, "ether"),
            682119805,
            0,
        ],
        bytes(),
    ])
    tx_params = {
        "nonce": 235,
        "maxFeePerGas": Web3.to_wei(100, "gwei"),
        "maxPriorityFeePerGas": Web3.to_wei(10, "gwei"),
        "gas": 44001,
        # Aggregation Router V6
        "to": bytes.fromhex("111111125421cA6dc452d289314280a0f8842A65"),
        "data": data,
        "chainId": 1
    }
    with app_client.sign("m/44'/60'/0'/0/0", tx_params, SignMode.STORE):
        pass

    fields = [
            Field(
                1,
                "Executor",
                ParamType.RAW,
                ParamRaw(
                    1,
                    Value(
                        1,
                        TypeFamily.ADDRESS,
                        data_path=DataPath(
                            1,
                            [
                                PathTuple(0),
                                PathLeaf(PathLeafType.STATIC),
                            ]
                        ),
                    )
                )
            ),
            Field(
                1,
                "Send",
                ParamType.TOKEN_AMOUNT,
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
                                PathTuple(4),
                                PathLeaf(PathLeafType.STATIC),
                            ]
                        ),
                    ),
                    token=Value(
                        1,
                        TypeFamily.ADDRESS,
                        data_path=DataPath(
                            1,
                            [
                                PathTuple(1),
                                PathTuple(0),
                                PathLeaf(PathLeafType.STATIC),
                            ]
                        ),
                    ),
                    native_currency=[
                        bytes.fromhex("EeeeeEeeeEeEeeEeEeEeeEEEeeeeEeeeeeeeEEeE"),
                    ],
                )
            ),
            Field(
                1,
                "Receive",
                ParamType.TOKEN_AMOUNT,
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
                                PathTuple(5),
                                PathLeaf(PathLeafType.STATIC),
                            ]
                        ),
                    ),
                    token=Value(
                        1,
                        TypeFamily.ADDRESS,
                        data_path=DataPath(
                            1,
                            [
                                PathTuple(1),
                                PathTuple(1),
                                PathLeaf(PathLeafType.STATIC),
                            ]
                        ),
                    ),
                    native_currency=[
                        bytes.fromhex("EeeeeEeeeEeEeeEeEeEeeEEEeeeeEeeeeeeeEEeE"),
                    ],
                )
            ),
    ]

    # compute instructions hash
    inst_hash = hashlib.sha3_256()
    for field in fields:
        inst_hash.update(field.serialize())

    tx_info = TxInfo(
        1,
        tx_params["chainId"],
        tx_params["to"],
        get_selector_from_data(tx_params["data"]),
        inst_hash.digest(),
        "swap",
        creator_name="1inch",
        creator_legal_name="1inch Network",
        creator_url="1inch.io",
        contract_name="Aggregation Router V6",
        deploy_date=1707724800
    )

    app_client.provide_transaction_info(tx_info.serialize())

    app_client.provide_token_metadata("USDC", bytes.fromhex("A0b86991c6218b36c1d19D4a2e9Eb0cE3606eB48"), 6, 1)

    for field in fields:
        payload = field.serialize()
        app_client.send_raw(0xe0, 0x28, 0x01, 0x00, struct.pack(">H", len(payload)) + payload)

    with app_client.send_raw_async(0xe0, 0x04, 0x00, 0x02, bytes()):
        scenario_navigator.review_approve(test_name=test_name, custom_screen_text="Sign transaction")
