from typing import Optional
import struct
import json
import hashlib
from web3 import Web3

from ragger.navigator.navigation_scenario import NavigateWithScenario


from constants import ABIS_FOLDER

import client.response_parser as ResponseParser
from client.client import EthAppClient, SignMode, TrustedNameType, TrustedNameSource
from client.status_word import StatusWord
from client.utils import get_selector_from_data
from client.gcs import (
    Field, ParamType, ParamRaw, Value, TypeFamily, DataPath, PathTuple, ParamTrustedName,
    ParamNFT, ParamDatetime, DatetimeType, ParamTokenAmount, ParamToken, ContainerPath,
    PathLeaf, PathLeafType, PathRef, PathArray, TxInfo
)
from client.tx_simu import TxSimu
from client.proxy_info import ProxyInfo


def test_gcs_nft(scenario_navigator: NavigateWithScenario, test_name: str):
    backend = scenario_navigator.backend
    app_client = EthAppClient(backend)

    with open(f"{ABIS_FOLDER}/erc1155.json", encoding="utf-8") as file:
        contract = Web3().eth.contract(
            abi=json.load(file),
            address=None
        )
    with app_client.get_public_addr(bip32_path="m/44'/60'/0'/0/0", display=False):
        pass
    _, device_addr, _ = ResponseParser.pk_addr(app_client.response().data)
    data = contract.encode_abi("safeBatchTransferFrom", [
        bytes.fromhex("1111111111111111111111111111111111111111"),
        bytes.fromhex("d8da6bf26964af9d7eed9e03e53415d37aa96045"),
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
    with app_client.sign("m/44'/60'/0'/0/0", tx_params, mode=SignMode.STORE):
        pass

    fields = [
            Field(
                1,
                "From",
                ParamType.TRUSTED_NAME,
                ParamTrustedName(
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
                    ),
                    [
                        TrustedNameType.ACCOUNT,
                    ],
                    [
                        TrustedNameSource.UD,
                        TrustedNameSource.ENS,
                        TrustedNameSource.FN,
                    ],
                    [
                        bytes.fromhex("0000000000000000000000000000000000000000"),
                        bytes.fromhex("1111111111111111111111111111111111111111"),
                        bytes.fromhex("2222222222222222222222222222222222222222"),
                    ],
                )
            ),
            Field(
                1,
                "To",
                ParamType.RAW,
                ParamRaw(
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
                    )
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
    app_client.provide_trusted_name_v2(device_addr,
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
        scenario_navigator.review_approve(test_name=test_name)


def test_gcs_poap(scenario_navigator: NavigateWithScenario,
                  test_name: str,
                  simu_params: Optional[TxSimu] = None):
    backend = scenario_navigator.backend
    app_client = EthAppClient(backend)

    with open(f"{ABIS_FOLDER}/poap.abi.json", encoding="utf-8") as file:
        contract = Web3().eth.contract(
            abi=json.load(file),
            address=None
        )
    # pylint: disable=line-too-long
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
    # pylint: enable=line-too-long

    if simu_params is not None:
        _, tx_hash = app_client.serialize_tx(tx_params)
        simu_params.tx_hash = tx_hash
        simu_params.chain_id = tx_params["chainId"]
        response = app_client.provide_tx_simulation(simu_params)
        assert response.status == StatusWord.OK

    with app_client.sign("m/44'/60'/0'/0/0", tx_params, mode=SignMode.STORE):
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
        if simu_params is not None:
            scenario_navigator.review_approve_with_warning(test_name=test_name)
        else:
            scenario_navigator.review_approve(test_name=test_name)


def test_gcs_1inch(scenario_navigator: NavigateWithScenario, test_name: str):
    backend = scenario_navigator.backend
    app_client = EthAppClient(backend)

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
    with app_client.sign("m/44'/60'/0'/0/0", tx_params, mode=SignMode.STORE):
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
        scenario_navigator.review_approve(test_name=test_name)


def test_gcs_proxy(scenario_navigator: NavigateWithScenario, test_name: str):
    backend = scenario_navigator.backend
    app_client = EthAppClient(backend)

    new_owner = bytes.fromhex("2222222222222222222222222222222222222222")

    with open(f"{ABIS_FOLDER}/proxy_implem.abi.json", encoding="utf-8") as file:
        contract = Web3().eth.contract(
            abi=json.load(file),
            address=None
        )
    data = contract.encode_abi("transferOwnership", [
        new_owner,
    ])
    tx_params = {
        "nonce": 1,
        "maxFeePerGas": Web3.to_wei(100, "gwei"),
        "maxPriorityFeePerGas": Web3.to_wei(10, "gwei"),
        "gas": 230,
        # address of the proxy contract
        "to": bytes.fromhex("39053d51b77dc0d36036fc1fcc8cb819df8ef37a"),
        "data": data,
        "chainId": 1
    }
    with app_client.sign("m/44'/60'/0'/0/0", tx_params, mode=SignMode.STORE):
        pass

    fields = [
        Field(
            1,
            "New owner",
            ParamType.TRUSTED_NAME,
            ParamTrustedName(
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
                ),
                [TrustedNameType.CONTRACT],
                [TrustedNameSource.CAL],
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
        # address of the implementation contract
        bytes.fromhex("1784be6401339fc0fedf7e9379409f5c1bfe9dda"),
        get_selector_from_data(tx_params["data"]),
        inst_hash.digest(),
        "transfer ownership",
        creator_name="EigenLayer",
        creator_legal_name="Eigen Labs",
        creator_url="https://eigenlayer.xyz",
        contract_name="Delegation Manager",
        deploy_date=1711098731,
    )

    proxy_info = ProxyInfo(
        ResponseParser.challenge(app_client.get_challenge().data),
        tx_info.contract_addr,
        tx_info.chain_id,
        tx_params["to"],
        selector=tx_info.selector,
    )

    app_client.provide_proxy_info(proxy_info.serialize())

    app_client.provide_transaction_info(tx_info.serialize())

    # also test proxy trusted names
    impl_contract = bytes.fromhex("1111111111111111111111111111111111111111")
    proxy_info = ProxyInfo(
        ResponseParser.challenge(app_client.get_challenge().data),
        new_owner,
        tx_info.chain_id,
        impl_contract,
    )

    app_client.provide_proxy_info(proxy_info.serialize())

    app_client.provide_trusted_name_v2(impl_contract,
                                       "some contract",
                                       TrustedNameType.CONTRACT,
                                       TrustedNameSource.CAL,
                                       tx_info.chain_id,
                                       challenge=ResponseParser.challenge(app_client.get_challenge().data))

    for field in fields:
        payload = field.serialize()
        app_client.send_raw(0xe0, 0x28, 0x01, 0x00, struct.pack(">H", len(payload)) + payload)

    with app_client.send_raw_async(0xe0, 0x04, 0x00, 0x02, bytes()):
        scenario_navigator.review_approve(test_name=test_name)


def test_gcs_4226(scenario_navigator: NavigateWithScenario, test_name: str):
    backend = scenario_navigator.backend
    app_client = EthAppClient(backend)

    with open(f"{ABIS_FOLDER}/rSWELL.abi.json", encoding="utf-8") as file:
        contract = Web3().eth.contract(
            abi=json.load(file),
            address=None
        )
    data = contract.encode_abi("deposit", [
        Web3.to_wei(4.20, "ether"),
        bytes.fromhex("Dad77910DbDFdE764fC21FCD4E74D71bBACA6D8D"),
    ])
    tx_params = {
        "nonce": 235,
        "maxFeePerGas": Web3.to_wei(3, "gwei"),
        "maxPriorityFeePerGas": Web3.to_wei(1, "gwei"),
        "gas": 128872,
        "to": bytes.fromhex("358d94b5b2F147D741088803d932Acb566acB7B6"),
        "data": data,
        "chainId": 1
    }
    with app_client.sign("m/44'/60'/0'/0/0", tx_params, mode=SignMode.STORE):
        pass

    swell_token_addr = bytes.fromhex("0a6e7ba5042b38349e437ec6db6214aec7b35676")
    fields = [
            Field(
                1,
                "Deposit asset",
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
                                PathTuple(0),
                                PathLeaf(PathLeafType.STATIC),
                            ]
                        ),
                    ),
                    token=Value(
                        1,
                        TypeFamily.ADDRESS,
                        constant=swell_token_addr,
                    ),
                )
            ),
            Field(
                1,
                "Receive shares",
                ParamType.TOKEN,
                ParamToken(
                    1,
                    Value(
                        1,
                        TypeFamily.ADDRESS,
                        container_path=ContainerPath.TO,
                    ),
                )
            ),
            Field(
                1,
                "Send shares to",
                ParamType.RAW,
                ParamToken(
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
        "deposit",
        creator_name="Swell",
        creator_legal_name="Swell Network",
        creator_url="www.swellnetwork.io",
        contract_name="rSWELL Token",
        deploy_date=1726817291
    )

    app_client.provide_transaction_info(tx_info.serialize())

    app_client.provide_token_metadata("rSWELL", tx_params["to"], 18, 1)
    app_client.provide_token_metadata("SWELL", swell_token_addr, 18, 1)

    for field in fields:
        payload = field.serialize()
        app_client.send_raw(0xe0, 0x28, 0x01, 0x00, struct.pack(">H", len(payload)) + payload)

    with app_client.send_raw_async(0xe0, 0x04, 0x00, 0x02, bytes()):
        scenario_navigator.review_approve(test_name=test_name)
