from typing import Optional
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
    ParamNFT, ParamDatetime, DatetimeType, ParamTokenAmount, ParamToken, ParamCalldata,
    ParamAmount, ContainerPath, PathLeaf, PathLeafType, PathRef, PathArray, TxInfo
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
        app_client.provide_transaction_field_desc(field.serialize())

    with app_client.sign(mode=SignMode.START_FLOW):
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
        app_client.provide_transaction_field_desc(field.serialize())

    with app_client.sign(mode=SignMode.START_FLOW):
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
        app_client.provide_transaction_field_desc(field.serialize())

    with app_client.sign(mode=SignMode.START_FLOW):
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
        tx_params["to"],
        tx_info.chain_id,
        tx_info.contract_addr,
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
        app_client.provide_transaction_field_desc(field.serialize())

    with app_client.sign(mode=SignMode.START_FLOW):
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
        app_client.provide_transaction_field_desc(field.serialize())

    with app_client.sign(mode=SignMode.START_FLOW):
        scenario_navigator.review_approve(test_name=test_name)


# https://etherscan.io/tx/0x07a80f1b359146129f3369af39e7eb2457581109c8300fc2ef81e997a07cf3f0
def test_gcs_nested_createProxyWithNonce(scenario_navigator: NavigateWithScenario, test_name: str):
    backend = scenario_navigator.backend
    app_client = EthAppClient(backend)

    with open(f"{ABIS_FOLDER}/safe_l2_setup_1.4.1.abi.json", encoding="utf-8") as f:
        safe_l2_setup = Web3().eth.contract(
            abi=json.load(f),
            address=bytes.fromhex("BD89A1CE4DDe368FFAB0eC35506eEcE0b1fFdc54")
        )
    safe_l2_setup_data = safe_l2_setup.encode_abi("setupToL2", [
        bytes.fromhex("29fcB43b46531BcA003ddC8FCB67FFE91900C762")
    ])
    with open(f"{ABIS_FOLDER}/safe_1.4.1.abi.json", encoding="utf-8") as f:
        safe = Web3().eth.contract(
            abi=json.load(f),
            address=bytes.fromhex("41675C099F32341bf84BFc5382aF534df5C7461a")
        )
    safe_data = safe.encode_abi("setup", [
        [
            bytes.fromhex("6535d5F76F021FE65E2ac73D086dF4b4Bd7ee5D9"),
            bytes.fromhex("3fB2C8699C3D0Cedde210F383435C537C86D91B8")
        ],
        2,
        safe_l2_setup.address,
        safe_l2_setup_data,
        bytes.fromhex("fd0732Dc9E303f09fCEf3a7388Ad10A83459Ec99"),
        bytes.fromhex("0000000000000000000000000000000000000000"),
        0,
        bytes.fromhex("5afe7A11E7000000000000000000000000000000")
    ])
    with open(f"{ABIS_FOLDER}/safe_proxy_factory_1.4.1.abi.json", encoding="utf-8") as f:
        safe_proxy_factory = Web3().eth.contract(
            abi=json.load(f),
            address=bytes.fromhex("4e1DCf7AD4e460CfD30791CCC4F9c8a4f820ec67")
        )
    data = safe_proxy_factory.encode_abi("createProxyWithNonce", [
        safe.address,
        safe_data,
        0
    ])
    tx_params = {
        "nonce": 75,
        "maxFeePerGas": Web3.to_wei(4.2, "gwei"),
        "maxPriorityFeePerGas": Web3.to_wei(2, "gwei"),
        "gas": 291314,
        "to": safe_proxy_factory.address,
        "data": data,
        "chainId": 1
    }

    with app_client.sign("m/44'/60'/0'/0/0", tx_params, mode=SignMode.STORE):
        pass

    fields = [
            Field(
                1,
                "_singleton",
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
                    ),
                )
            ),
            Field(
                1,
                "initializer",
                ParamCalldata(
                    1,
                    Value(
                        1,
                        TypeFamily.BYTES,
                        data_path=DataPath(
                            1,
                            [
                                PathTuple(1),
                                PathRef(),
                                PathLeaf(PathLeafType.DYNAMIC),
                            ]
                        ),
                    ),
                    Value(
                        1,
                        TypeFamily.BYTES,
                        data_path=DataPath(
                            1,
                            [
                                PathTuple(0),
                                PathLeaf(PathLeafType.STATIC),
                            ]
                        ),
                    ),
                )
            ),
            Field(
                1,
                "saltNonce",
                ParamRaw(
                    1,
                    Value(
                        1,
                        TypeFamily.UINT,
                        type_size=32,
                        data_path=DataPath(
                            1,
                            [
                                PathTuple(2),
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
        "create a Safe account",
        creator_name="Safe",
        creator_legal_name="Safe Ecosystem Foundation",
        creator_url="safe.global",
    )

    app_client.provide_transaction_info(tx_info.serialize())

    sub_fields = [
            Field(
                1,
                "_owners",
                ParamRaw(
                    1,
                    Value(
                        1,
                        TypeFamily.ADDRESS,
                        data_path=DataPath(
                            1,
                            [
                                PathTuple(0),
                                PathRef(),
                                PathArray(1),
                                PathLeaf(PathLeafType.STATIC),
                            ]
                        ),
                    ),
                )
            ),
            Field(
                1,
                "_threshold",
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
                    ),
                )
            ),
            Field(
                1,
                "to",
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
                    ),
                )
            ),
            Field(
                1,
                "data",
                ParamCalldata(
                    1,
                    Value(
                        1,
                        TypeFamily.BYTES,
                        data_path=DataPath(
                            1,
                            [
                                PathTuple(3),
                                PathRef(),
                                PathLeaf(PathLeafType.DYNAMIC),
                            ]
                        ),
                    ),
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
                    ),
                )
            ),
            Field(
                1,
                "fallbackHandler",
                ParamRaw(
                    1,
                    Value(
                        1,
                        TypeFamily.ADDRESS,
                        data_path=DataPath(
                            1,
                            [
                                PathTuple(4),
                                PathLeaf(PathLeafType.STATIC),
                            ]
                        ),
                    ),
                )
            ),
            Field(
                1,
                "paymentToken",
                ParamRaw(
                    1,
                    Value(
                        1,
                        TypeFamily.ADDRESS,
                        data_path=DataPath(
                            1,
                            [
                                PathTuple(5),
                                PathLeaf(PathLeafType.STATIC),
                            ]
                        ),
                    ),
                )
            ),
            Field(
                1,
                "payment",
                ParamRaw(
                    1,
                    Value(
                        1,
                        TypeFamily.UINT,
                        type_size=32,
                        data_path=DataPath(
                            1,
                            [
                                PathTuple(6),
                                PathLeaf(PathLeafType.STATIC),
                            ]
                        ),
                    ),
                )
            ),
            Field(
                1,
                "paymentReceiver",
                ParamRaw(
                    1,
                    Value(
                        1,
                        TypeFamily.ADDRESS,
                        data_path=DataPath(
                            1,
                            [
                                PathTuple(7),
                                PathLeaf(PathLeafType.STATIC),
                            ]
                        ),
                    ),
                )
            ),
    ]
    # compute instructions hash
    sub_inst_hash = hashlib.sha3_256()
    for sub_field in sub_fields:
        sub_inst_hash.update(sub_field.serialize())

    sub_tx_info = TxInfo(
        1,
        tx_params["chainId"],
        safe.address,
        get_selector_from_data(safe_data),
        sub_inst_hash.digest(),
        "setup",
    )

    sub_sub_fields = [
            Field(
                1,
                "l2Singleton",
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
                    ),
                )
            ),
    ]
    # compute instructions hash
    sub_sub_inst_hash = hashlib.sha3_256()
    for sub_sub_field in sub_sub_fields:
        sub_sub_inst_hash.update(sub_sub_field.serialize())
    sub_sub_tx_info = TxInfo(
        1,
        tx_params["chainId"],
        safe_l2_setup.address,
        get_selector_from_data(safe_l2_setup_data),
        sub_sub_inst_hash.digest(),
        "L2 setup",
    )

    for field in fields:
        app_client.provide_transaction_field_desc(field.serialize())
        if field.param.type == ParamType.CALLDATA:
            app_client.provide_transaction_info(sub_tx_info.serialize())
            for sub_field in sub_fields:
                app_client.provide_transaction_field_desc(sub_field.serialize())
                if sub_field.param.type == ParamType.CALLDATA:
                    app_client.provide_transaction_info(sub_sub_tx_info.serialize())
                    for sub_sub_field in sub_sub_fields:
                        app_client.provide_transaction_field_desc(sub_sub_field.serialize())

    with app_client.sign(mode=SignMode.START_FLOW):
        scenario_navigator.review_approve(test_name=test_name)


# https://etherscan.io/tx/0xc5545f13bfaf6f69ae937bc64337405060dc56ce7649ea7051d2bbc3b4316b79
def test_gcs_nested_execTransaction_send(scenario_navigator: NavigateWithScenario, test_name: str):
    backend = scenario_navigator.backend
    app_client = EthAppClient(backend)

    with open(f"{ABIS_FOLDER}/safe_1.4.1.abi.json", encoding="utf-8") as f:
        contract = Web3().eth.contract(
            abi=json.load(f),
            address=bytes.fromhex("23F8abfC2824C397cCB3DA89ae772984107dDB99")
        )
    data = contract.encode_abi("execTransaction", [
        contract.address,
        Web3.to_wei(0.0042, "ether"),
        bytes(),
        0,
        0,
        0,
        0,
        bytes.fromhex("0000000000000000000000000000000000000000"),
        bytes.fromhex("0000000000000000000000000000000000000000"),
        bytes.fromhex("a974345670d8e06c52eeb7bfe59b1ed0fc879223ff0938c859c3852110c8c58016ec4bf0c68e84d3a40e3ac519f0a0db6954e7c4107fc6985de7dc683603f62a1b"),
    ])

    tx_params = {
        "nonce": 77,
        "maxFeePerGas": Web3.to_wei(4.8, "gwei"),
        "maxPriorityFeePerGas": Web3.to_wei(2, "gwei"),
        "gas": 95118,
        "to": bytes.fromhex("C1897a9Acbdd54028dA5f7b76B5833A91553AaF6"),
        "data": data,
        "chainId": 1
    }

    with app_client.sign("m/44'/60'/0'/0/0", tx_params, mode=SignMode.STORE):
        pass

    fields = [
        Field(
            1,
            "data",
            ParamCalldata(
                1,
                Value(
                    1,
                    TypeFamily.BYTES,
                    data_path=DataPath(
                        1,
                        [
                            PathTuple(2),
                            PathRef(),
                            PathLeaf(PathLeafType.DYNAMIC),
                        ]
                    ),
                ),
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
                amount=Value(
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
                spender=Value(
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
        tx_params["chainId"],
        tx_params["to"],
        get_selector_from_data(tx_params["data"]),
        inst_hash.digest(),
        "execute a Safe action",
        creator_name="Safe",
        creator_legal_name="Safe Ecosystem Foundation",
        creator_url="safe.global",
    )

    app_client.provide_transaction_info(tx_info.serialize())

    for field in fields:
        app_client.provide_transaction_field_desc(field.serialize())

    with app_client.sign(mode=SignMode.START_FLOW):
        scenario_navigator.review_approve(test_name=test_name)


# https://etherscan.io/tx/0xbeafe22c9e3ddcf85b06f65a56cc3ea8f5b02c323cc433c93c103ad3526db88d
def test_gcs_nested_execTransaction_addOwnerWithThreshold(scenario_navigator: NavigateWithScenario, test_name: str):
    backend = scenario_navigator.backend
    app_client = EthAppClient(backend)

    with open(f"{ABIS_FOLDER}/safe_1.4.1.abi.json", encoding="utf-8") as f:
        contract = Web3().eth.contract(
            abi=json.load(f),
            address=bytes.fromhex("23F8abfC2824C397cCB3DA89ae772984107dDB99")
        )
    sub_data = contract.encode_abi("addOwnerWithThreshold", [
        bytes.fromhex("FD6765Ad4eE64668701356a16aB28B123B3A4170"),
        2
    ])
    data = contract.encode_abi("execTransaction", [
        contract.address,
        Web3.to_wei(0, "ether"),
        sub_data,
        0,
        0,
        0,
        0,
        bytes.fromhex("0000000000000000000000000000000000000000"),
        bytes.fromhex("0000000000000000000000000000000000000000"),
        bytes.fromhex("c14660c23f715fc85c01326c7fa7f05ddeb71147fc7bad912eace6ee55c24a314f814262b3c8ca64fc77377ce6e65b20bdc902c34931888c433e23ab0069843d1bf3d2dfb18fd6bd807002bffec3326755c928e325981f30e1518e999b348a5f011446931b8bd9fbb152cdc00d945b7cd030c14e48c7826d31f9c09a1376f694de1b"),
    ])

    tx_params = {
        "nonce": 78,
        "maxFeePerGas": Web3.to_wei(8.9, "gwei"),
        "maxPriorityFeePerGas": Web3.to_wei(2, "gwei"),
        "gas": 168740,
        "to": contract.address,
        "data": data,
        "chainId": 1
    }

    with app_client.sign("m/44'/60'/0'/0/0", tx_params, mode=SignMode.STORE):
        pass

    fields = [
            Field(
                1,
                "to",
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
                    ),
                )
            ),
            Field(
                1,
                "value",
                ParamAmount(
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
                )
            ),
            Field(
                1,
                "data",
                ParamCalldata(
                    1,
                    Value(
                        1,
                        TypeFamily.BYTES,
                        data_path=DataPath(
                            1,
                            [
                                PathTuple(2),
                                PathRef(),
                                PathLeaf(PathLeafType.DYNAMIC),
                            ]
                        ),
                    ),
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
                )
            ),
            Field(
                1,
                "operation",
                ParamRaw(
                    1,
                    Value(
                        1,
                        TypeFamily.UINT,
                        type_size=1,
                        data_path=DataPath(
                            1,
                            [
                                PathTuple(3),
                                PathLeaf(PathLeafType.STATIC),
                            ]
                        ),
                    ),
                )
            ),
            Field(
                1,
                "safeTxGas",
                ParamRaw(
                    1,
                    Value(
                        1,
                        TypeFamily.UINT,
                        type_size=32,
                        data_path=DataPath(
                            1,
                            [
                                PathTuple(4),
                                PathLeaf(PathLeafType.STATIC),
                            ]
                        ),
                    ),
                )
            ),
            Field(
                1,
                "dataGas",
                ParamRaw(
                    1,
                    Value(
                        1,
                        TypeFamily.UINT,
                        type_size=32,
                        data_path=DataPath(
                            1,
                            [
                                PathTuple(5),
                                PathLeaf(PathLeafType.STATIC),
                            ]
                        ),
                    ),
                )
            ),
            Field(
                1,
                "gasPrice",
                ParamRaw(
                    1,
                    Value(
                        1,
                        TypeFamily.UINT,
                        type_size=32,
                        data_path=DataPath(
                            1,
                            [
                                PathTuple(6),
                                PathLeaf(PathLeafType.STATIC),
                            ]
                        ),
                    ),
                )
            ),
            Field(
                1,
                "gasToken",
                ParamRaw(
                    1,
                    Value(
                        1,
                        TypeFamily.ADDRESS,
                        data_path=DataPath(
                            1,
                            [
                                PathTuple(7),
                                PathLeaf(PathLeafType.STATIC),
                            ]
                        ),
                    ),
                )
            ),
            Field(
                1,
                "refundReceiver",
                ParamRaw(
                    1,
                    Value(
                        1,
                        TypeFamily.ADDRESS,
                        data_path=DataPath(
                            1,
                            [
                                PathTuple(8),
                                PathLeaf(PathLeafType.STATIC),
                            ]
                        ),
                    ),
                )
            ),
            Field(
                1,
                "signatures",
                ParamRaw(
                    1,
                    Value(
                        1,
                        TypeFamily.BYTES,
                        data_path=DataPath(
                            1,
                            [
                                PathTuple(9),
                                PathRef(),
                                PathLeaf(PathLeafType.DYNAMIC),
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
        "execute a Safe action",
        creator_name="Safe",
        creator_legal_name="Safe Ecosystem Foundation",
        creator_url="safe.global",
    )

    app_client.provide_transaction_info(tx_info.serialize())

    sub_fields = [
            Field(
                1,
                "owner",
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
                    ),
                )
            ),
            Field(
                1,
                "_threshold",
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
                    ),
                )
            ),
    ]
    # compute instructions hash
    sub_inst_hash = hashlib.sha3_256()
    for sub_field in sub_fields:
        sub_inst_hash.update(sub_field.serialize())

    sub_tx_info = TxInfo(
        1,
        tx_params["chainId"],
        contract.address,
        get_selector_from_data(sub_data),
        sub_inst_hash.digest(),
        "add owner with threshold",
    )

    for field in fields:
        app_client.provide_transaction_field_desc(field.serialize())
        if field.param.type == ParamType.CALLDATA:
            app_client.provide_transaction_info(sub_tx_info.serialize())
            for sub_field in sub_fields:
                app_client.provide_transaction_field_desc(sub_field.serialize())

    with app_client.sign(mode=SignMode.START_FLOW):
        scenario_navigator.review_approve(test_name=test_name)


# https://etherscan.io/tx/0x5047fedc98f46d2afd94d0a2813ddf0c8fe777ec0739ffd327586a91e1e5a89a
def test_gcs_nested_execTransaction_changeThreshold(scenario_navigator: NavigateWithScenario, test_name: str):
    backend = scenario_navigator.backend
    app_client = EthAppClient(backend)

    with open(f"{ABIS_FOLDER}/safe_1.4.1.abi.json", encoding="utf-8") as f:
        contract = Web3().eth.contract(
            abi=json.load(f),
            address=bytes.fromhex("23F8abfC2824C397cCB3DA89ae772984107dDB99")
        )
    sub_data = contract.encode_abi("changeThreshold", [
        3
    ])
    data = contract.encode_abi("execTransaction", [
        contract.address,
        Web3.to_wei(0, "ether"),
        sub_data,
        0,
        0,
        0,
        0,
        bytes.fromhex("0000000000000000000000000000000000000000"),
        bytes.fromhex("0000000000000000000000000000000000000000"),
        bytes.fromhex("d3a6ddfb9dffe883d609129d9e87dda928a4a9b9d5d2f4a93879d03ccb0d32b12df7dcf9acd9c5f73443c82b0e01183794436a381148cf2fb928f7df776a01701b2fc9ebbc15bfdae0f5ef1b6f4ad1389d31f1dc137e51e7a184e255fd0ed065911ad684bd97ee43892013b4eebdaec528020ed657b92b90562f4df5a18540e4b91b"),
    ])

    tx_params = {
        "nonce": 83,
        "maxFeePerGas": Web3.to_wei(5.7, "gwei"),
        "maxPriorityFeePerGas": Web3.to_wei(2, "gwei"),
        "gas": 98318,
        "to": contract.address,
        "data": data,
        "chainId": 1
    }

    with app_client.sign("m/44'/60'/0'/0/0", tx_params, mode=SignMode.STORE):
        pass

    fields = [
            Field(
                1,
                "to",
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
                    ),
                )
            ),
            Field(
                1,
                "value",
                ParamAmount(
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
                )
            ),
            Field(
                1,
                "data",
                ParamCalldata(
                    1,
                    Value(
                        1,
                        TypeFamily.BYTES,
                        data_path=DataPath(
                            1,
                            [
                                PathTuple(2),
                                PathRef(),
                                PathLeaf(PathLeafType.DYNAMIC),
                            ]
                        ),
                    ),
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
                )
            ),
            Field(
                1,
                "operation",
                ParamRaw(
                    1,
                    Value(
                        1,
                        TypeFamily.UINT,
                        type_size=1,
                        data_path=DataPath(
                            1,
                            [
                                PathTuple(3),
                                PathLeaf(PathLeafType.STATIC),
                            ]
                        ),
                    ),
                )
            ),
            Field(
                1,
                "safeTxGas",
                ParamRaw(
                    1,
                    Value(
                        1,
                        TypeFamily.UINT,
                        type_size=32,
                        data_path=DataPath(
                            1,
                            [
                                PathTuple(4),
                                PathLeaf(PathLeafType.STATIC),
                            ]
                        ),
                    ),
                )
            ),
            Field(
                1,
                "dataGas",
                ParamRaw(
                    1,
                    Value(
                        1,
                        TypeFamily.UINT,
                        type_size=32,
                        data_path=DataPath(
                            1,
                            [
                                PathTuple(5),
                                PathLeaf(PathLeafType.STATIC),
                            ]
                        ),
                    ),
                )
            ),
            Field(
                1,
                "gasPrice",
                ParamRaw(
                    1,
                    Value(
                        1,
                        TypeFamily.UINT,
                        type_size=32,
                        data_path=DataPath(
                            1,
                            [
                                PathTuple(6),
                                PathLeaf(PathLeafType.STATIC),
                            ]
                        ),
                    ),
                )
            ),
            Field(
                1,
                "gasToken",
                ParamRaw(
                    1,
                    Value(
                        1,
                        TypeFamily.ADDRESS,
                        data_path=DataPath(
                            1,
                            [
                                PathTuple(7),
                                PathLeaf(PathLeafType.STATIC),
                            ]
                        ),
                    ),
                )
            ),
            Field(
                1,
                "refundReceiver",
                ParamRaw(
                    1,
                    Value(
                        1,
                        TypeFamily.ADDRESS,
                        data_path=DataPath(
                            1,
                            [
                                PathTuple(8),
                                PathLeaf(PathLeafType.STATIC),
                            ]
                        ),
                    ),
                )
            ),
            Field(
                1,
                "signatures",
                ParamRaw(
                    1,
                    Value(
                        1,
                        TypeFamily.BYTES,
                        data_path=DataPath(
                            1,
                            [
                                PathTuple(9),
                                PathRef(),
                                PathLeaf(PathLeafType.DYNAMIC),
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
        "execute a Safe action",
        creator_name="Safe",
        creator_legal_name="Safe Ecosystem Foundation",
        creator_url="safe.global",
    )

    app_client.provide_transaction_info(tx_info.serialize())

    sub_fields = [
            Field(
                1,
                "newThreshold",
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
                    ),
                )
            ),
    ]
    # compute instructions hash
    sub_inst_hash = hashlib.sha3_256()
    for sub_field in sub_fields:
        sub_inst_hash.update(sub_field.serialize())

    sub_tx_info = TxInfo(
        1,
        tx_params["chainId"],
        contract.address,
        get_selector_from_data(sub_data),
        sub_inst_hash.digest(),
        "change threshold",
    )

    for field in fields:
        app_client.provide_transaction_field_desc(field.serialize())
        if field.param.type == ParamType.CALLDATA:
            app_client.provide_transaction_info(sub_tx_info.serialize())
            for sub_field in sub_fields:
                app_client.provide_transaction_field_desc(sub_field.serialize())

    with app_client.sign(mode=SignMode.START_FLOW):
        scenario_navigator.review_approve(test_name=test_name)
