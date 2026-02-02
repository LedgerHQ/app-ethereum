# pylint: disable=too-many-lines
# Large test file containing multiple GCS (Generic Clear Signing) integration tests
from typing import Optional
import json
import hashlib
from pathlib import Path

from web3 import Web3

from ragger.navigator.navigation_scenario import NavigateWithScenario

from dynamic_networks_cfg import get_network_config
from constants import ABIS_FOLDER
from fields_utils import get_all_tuple_array_paths, get_all_paths, get_all_tuple_paths

import client.response_parser as ResponseParser
from client.client import EthAppClient, SignMode, TrustedNameType, TrustedNameSource
from client.status_word import StatusWord
from client.utils import get_selector_from_data
from client.gcs import (
    Field, ParamType, ParamRaw, Value, TypeFamily, DataPath, ParamTrustedName,
    ParamNFT, ParamDatetime, DatetimeType, ParamTokenAmount, ParamToken, ParamCalldata,
    ParamAmount, ParamEnum, ContainerPath, TxInfo,
)
from client.enum_value import EnumValue
from client.tx_simu import TxSimu
from client.proxy_info import ProxyInfo
from client.dynamic_networks import DynamicNetwork


def compute_inst_hash(fields: list[Field]) -> bytes:
    inst_hash = hashlib.sha3_256()
    for field in fields:
        inst_hash.update(field.serialize())
    return inst_hash.digest()


def test_gcs_nft(scenario_navigator: NavigateWithScenario):
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
            2,
            4,
            8,
            16,
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

    param_paths = get_all_paths(f"{ABIS_FOLDER}/erc1155.json", "safeBatchTransferFrom")
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
                            param_paths["_from"]
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
                            param_paths["_to"]
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
                           param_paths["_ids"]
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
                            param_paths["_values"]
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
                            param_paths["_data"]
                        ),
                    )
                )
            ),
    ]

    # compute instructions hash
    inst_hash = compute_inst_hash(fields)

    tx_info = TxInfo(
        1,
        tx_params["chainId"],
        tx_params["to"],
        get_selector_from_data(tx_params["data"]),
        inst_hash,
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
        scenario_navigator.review_approve()


def test_gcs_poap(scenario_navigator: NavigateWithScenario,
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

    param_paths = get_all_paths(f"{ABIS_FOLDER}/poap.abi.json", "mintToken")
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
                            param_paths["eventId"]
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
                            param_paths["tokenId"]
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
                            param_paths["receiver"]
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
                            param_paths["expirationTime"]
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
                            param_paths["signature"]
                        ),
                    )
                )
            ),
    ]

    # compute instructions hash
    inst_hash = compute_inst_hash(fields)

    tx_info = TxInfo(
        1,
        tx_params["chainId"],
        tx_params["to"],
        get_selector_from_data(tx_params["data"]),
        inst_hash,
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
            scenario_navigator.review_approve_with_warning()
        else:
            scenario_navigator.review_approve()


def test_gcs_1inch(scenario_navigator: NavigateWithScenario):
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

    param_paths = get_all_paths(f"{ABIS_FOLDER}/1inch.abi.json", "swap")
    param_tuple_paths = get_all_tuple_paths(f"{ABIS_FOLDER}/1inch.abi.json", "swap", "desc")
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
                            param_paths["executor"]
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
                            param_tuple_paths["amount"]
                        ),
                    ),
                    token=Value(
                        1,
                        TypeFamily.ADDRESS,
                        data_path=DataPath(
                            1,
                            param_tuple_paths["srcToken"]
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
                            param_tuple_paths["minReturnAmount"]
                        ),
                    ),
                    token=Value(
                        1,
                        TypeFamily.ADDRESS,
                        data_path=DataPath(
                            1,
                            param_tuple_paths["dstToken"]
                        ),
                    ),
                    native_currency=[
                        bytes.fromhex("EeeeeEeeeEeEeeEeEeEeeEEEeeeeEeeeeeeeEEeE"),
                    ],
                )
            ),
    ]

    # compute instructions hash
    inst_hash = compute_inst_hash(fields)

    tx_info = TxInfo(
        1,
        tx_params["chainId"],
        tx_params["to"],
        get_selector_from_data(tx_params["data"]),
        inst_hash,
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
        scenario_navigator.review_approve()


def test_gcs_proxy(scenario_navigator: NavigateWithScenario):
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

    param_paths = get_all_paths(f"{ABIS_FOLDER}/proxy_implem.abi.json", "transferOwnership")
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
                        param_paths["newOwner"]
                    ),
                ),
                [TrustedNameType.CONTRACT],
                [TrustedNameSource.CAL],
            )
        ),
    ]

    # compute instructions hash
    inst_hash = compute_inst_hash(fields)

    tx_info = TxInfo(
        1,
        tx_params["chainId"],
        # address of the implementation contract
        bytes.fromhex("1784be6401339fc0fedf7e9379409f5c1bfe9dda"),
        get_selector_from_data(tx_params["data"]),
        inst_hash,
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
        scenario_navigator.review_approve()


def test_gcs_4226(scenario_navigator: NavigateWithScenario):
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
    param_paths = get_all_paths(f"{ABIS_FOLDER}/rSWELL.abi.json", "deposit")
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
                            param_paths["assets"]
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
                            param_paths["receiver"]
                        ),
                    ),
                )
            ),
    ]

    # compute instructions hash
    inst_hash = compute_inst_hash(fields)

    tx_info = TxInfo(
        1,
        tx_params["chainId"],
        tx_params["to"],
        get_selector_from_data(tx_params["data"]),
        inst_hash,
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
        scenario_navigator.review_approve()


# https://etherscan.io/tx/0x07a80f1b359146129f3369af39e7eb2457581109c8300fc2ef81e997a07cf3f0
def test_gcs_nested_createProxyWithNonce(scenario_navigator: NavigateWithScenario):
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

    param_paths = get_all_paths(f"{ABIS_FOLDER}/safe_proxy_factory_1.4.1.abi.json", "createProxyWithNonce")
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
                            param_paths["_singleton"]
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
                            param_paths["initializer"]
                        ),
                    ),
                    Value(
                        1,
                        TypeFamily.BYTES,
                        data_path=DataPath(
                            1,
                            param_paths["_singleton"]
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
                            param_paths["saltNonce"]
                        ),
                    ),
                )
            ),
    ]

    # compute instructions hash
    inst_hash = compute_inst_hash(fields)

    tx_info = TxInfo(
        1,
        tx_params["chainId"],
        tx_params["to"],
        get_selector_from_data(tx_params["data"]),
        inst_hash,
        "create a Safe account",
        creator_name="Safe",
        creator_legal_name="Safe Ecosystem Foundation",
        creator_url="safe.global",
    )

    app_client.provide_transaction_info(tx_info.serialize())

    param_paths = get_all_paths(f"{ABIS_FOLDER}/safe_1.4.1.abi.json", "setup")
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
                            param_paths["_owners"]
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
                            param_paths["_threshold"]
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
                            param_paths["to"]
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
                            param_paths["data"]
                        ),
                    ),
                    Value(
                        1,
                        TypeFamily.ADDRESS,
                        data_path=DataPath(
                            1,
                            param_paths["to"]
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
                            param_paths["fallbackHandler"]
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
                            param_paths["paymentToken"]
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
                            param_paths["payment"]
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
                            param_paths["paymentReceiver"]
                        ),
                    ),
                )
            ),
    ]
    # compute instructions hash
    sub_inst_hash = compute_inst_hash(sub_fields)
    sub_tx_info = TxInfo(
        1,
        tx_params["chainId"],
        safe.address,
        get_selector_from_data(safe_data),
        sub_inst_hash,
        "setup",
    )

    param_paths = get_all_paths(f"{ABIS_FOLDER}/safe_l2_setup_1.4.1.abi.json", "setupToL2")
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
                            param_paths["l2Singleton"]
                        ),
                    ),
                )
            ),
    ]
    # compute instructions hash
    sub_sub_inst_hash = compute_inst_hash(sub_sub_fields)
    sub_sub_tx_info = TxInfo(
        1,
        tx_params["chainId"],
        safe_l2_setup.address,
        get_selector_from_data(safe_l2_setup_data),
        sub_sub_inst_hash,
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
        scenario_navigator.review_approve()


# https://etherscan.io/tx/0xc5545f13bfaf6f69ae937bc64337405060dc56ce7649ea7051d2bbc3b4316b79
def test_gcs_nested_execTransaction_send(scenario_navigator: NavigateWithScenario):
    backend = scenario_navigator.backend
    app_client = EthAppClient(backend)

    with open(f"{ABIS_FOLDER}/safe_1.4.1.abi.json", encoding="utf-8") as f:
        contract = Web3().eth.contract(
            abi=json.load(f),
            address=bytes.fromhex("23F8abfC2824C397cCB3DA89ae772984107dDB99")
        )
    # pylint: disable=line-too-long
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
    # pylint: enable=line-too-long

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

    param_paths = get_all_paths(f"{ABIS_FOLDER}/safe_1.4.1.abi.json", "execTransaction")
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
                        param_paths["data"]
                    ),
                ),
                Value(
                    1,
                    TypeFamily.ADDRESS,
                    data_path=DataPath(
                        1,
                        param_paths["to"]
                    ),
                ),
                amount=Value(
                    1,
                    TypeFamily.UINT,
                    type_size=32,
                    data_path=DataPath(
                        1,
                        param_paths["value"]
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
    inst_hash = compute_inst_hash(fields)

    tx_info = TxInfo(
        1,
        tx_params["chainId"],
        tx_params["to"],
        get_selector_from_data(tx_params["data"]),
        inst_hash,
        "execute a Safe action",
        creator_name="Safe",
        creator_legal_name="Safe Ecosystem Foundation",
        creator_url="safe.global",
    )

    app_client.provide_transaction_info(tx_info.serialize())

    for field in fields:
        app_client.provide_transaction_field_desc(field.serialize())

    with app_client.sign(mode=SignMode.START_FLOW):
        scenario_navigator.review_approve()


# https://etherscan.io/tx/0xbeafe22c9e3ddcf85b06f65a56cc3ea8f5b02c323cc433c93c103ad3526db88d
def test_gcs_nested_execTransaction_addOwnerWithThreshold(scenario_navigator: NavigateWithScenario):
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
    # pylint: disable=line-too-long
    data = contract.encode_abi("execTransaction", [
        contract.address,
        Web3.to_wei(0, "ether"),
        sub_data,
        1, # operation
        0,
        0,
        0,
        bytes.fromhex("0000000000000000000000000000000000000000"),
        bytes.fromhex("0000000000000000000000000000000000000000"),
        bytes.fromhex("c14660c23f715fc85c01326c7fa7f05ddeb71147fc7bad912eace6ee55c24a314f814262b3c8ca64fc77377ce6e65b20bdc902c34931888c433e23ab0069843d1bf3d2dfb18fd6bd807002bffec3326755c928e325981f30e1518e999b348a5f011446931b8bd9fbb152cdc00d945b7cd030c14e48c7826d31f9c09a1376f694de1b"),
    ])
    # pylint: enable=line-too-long

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

    param_paths = get_all_paths(f"{ABIS_FOLDER}/safe_1.4.1.abi.json", "execTransaction")
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
                            param_paths["to"]
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
                            param_paths["value"]
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
                            param_paths["data"]
                        ),
                    ),
                    Value(
                        1,
                        TypeFamily.ADDRESS,
                        data_path=DataPath(
                            1,
                            param_paths["to"]
                        ),
                    ),
                )
            ),
            Field(
                1,
                "Operation type",
                ParamEnum(
                    1,
                    0,
                    Value(
                        1,
                        TypeFamily.UINT,
                        type_size=1,
                        data_path=DataPath(
                            1,
                            param_paths["operation"],
                        ),
                    ),
                ),
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
                            param_paths["safeTxGas"]
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
                            param_paths["baseGas"]
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
                            param_paths["gasPrice"]
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
                            param_paths["gasToken"]
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
                            param_paths["refundReceiver"]
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
                            param_paths["signatures"]
                        ),
                    ),
                )
            ),
    ]

    # compute instructions hash
    inst_hash = compute_inst_hash(fields)

    tx_info = TxInfo(
        1,
        tx_params["chainId"],
        tx_params["to"],
        get_selector_from_data(tx_params["data"]),
        inst_hash,
        "execute a Safe action",
        creator_name="Safe",
        creator_legal_name="Safe Ecosystem Foundation",
        creator_url="safe.global",
    )

    app_client.provide_transaction_info(tx_info.serialize())

    param_paths = get_all_paths(f"{ABIS_FOLDER}/safe_1.4.1.abi.json", "addOwnerWithThreshold")
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
                            param_paths["owner"]
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
                            param_paths["_threshold"]
                        ),
                    ),
                )
            ),
    ]
    # compute instructions hash
    sub_inst_hash = compute_inst_hash(sub_fields)

    sub_tx_info = TxInfo(
        1,
        tx_params["chainId"],
        contract.address,
        get_selector_from_data(sub_data),
        sub_inst_hash,
        "add owner with threshold",
    )

    enum_values = [
        (0, "Call"),
        (1, "Delegate Call"),
        (2, "Unknown"),
    ]
    for enum_val in enum_values:
        app_client.provide_enum_value(EnumValue(1,
                                                tx_info.chain_id,
                                                tx_info.contract_addr,
                                                tx_info.selector,
                                                0,
                                                enum_val[0],
                                                enum_val[1]).serialize())

    for field in fields:
        app_client.provide_transaction_field_desc(field.serialize())
        if field.param.type == ParamType.CALLDATA:
            app_client.provide_transaction_info(sub_tx_info.serialize())
            for sub_field in sub_fields:
                app_client.provide_transaction_field_desc(sub_field.serialize())

    with app_client.sign(mode=SignMode.START_FLOW):
        scenario_navigator.review_approve()


# https://etherscan.io/tx/0x5047fedc98f46d2afd94d0a2813ddf0c8fe777ec0739ffd327586a91e1e5a89a
def test_gcs_nested_execTransaction_changeThreshold(scenario_navigator: NavigateWithScenario):
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
    # pylint: disable=line-too-long
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
    # pylint: enable=line-too-long

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

    param_paths = get_all_paths(f"{ABIS_FOLDER}/safe_1.4.1.abi.json", "execTransaction")
    fields = [
            Field(
                1,
                "to",
                ParamTrustedName(
                    1,
                    Value(
                        1,
                        TypeFamily.ADDRESS,
                        data_path=DataPath(
                            1,
                            param_paths["to"]
                        ),
                    ),
                    [TrustedNameType.ACCOUNT],
                    [TrustedNameSource.MULTISIG_ADDRESS_BOOK],
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
                            param_paths["value"]
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
                            param_paths["data"]
                        ),
                    ),
                    Value(
                        1,
                        TypeFamily.ADDRESS,
                        data_path=DataPath(
                            1,
                            param_paths["to"]
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
                            param_paths["operation"]
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
                            param_paths["safeTxGas"]
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
                            param_paths["baseGas"]
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
                            param_paths["gasPrice"]
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
                            param_paths["gasToken"]
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
                            param_paths["refundReceiver"]
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
                            param_paths["signatures"]
                        ),
                    ),
                )
            ),
    ]

    # compute instructions hash
    inst_hash = compute_inst_hash(fields)

    tx_info = TxInfo(
        1,
        tx_params["chainId"],
        tx_params["to"],
        get_selector_from_data(tx_params["data"]),
        inst_hash,
        "execute a Safe action",
        creator_name="Safe",
        creator_legal_name="Safe Ecosystem Foundation",
        creator_url="safe.global",
    )

    app_client.provide_transaction_info(tx_info.serialize())

    param_paths = get_all_paths(f"{ABIS_FOLDER}/safe_1.4.1.abi.json", "changeThreshold")
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
                            param_paths["_threshold"]
                        ),
                    ),
                )
            ),
    ]
    # compute instructions hash
    sub_inst_hash = compute_inst_hash(sub_fields)

    sub_tx_info = TxInfo(
        1,
        tx_params["chainId"],
        contract.address,
        get_selector_from_data(sub_data),
        sub_inst_hash,
        "change threshold",
    )

    app_client.provide_trusted_name_v2(contract.address,
                                       "My Safe",
                                       TrustedNameType.ACCOUNT,
                                       TrustedNameSource.MULTISIG_ADDRESS_BOOK,
                                       tx_info.chain_id,
                                       challenge=ResponseParser.challenge(app_client.get_challenge().data))

    for field in fields:
        app_client.provide_transaction_field_desc(field.serialize())
        if field.param.type == ParamType.CALLDATA:
            app_client.provide_transaction_info(sub_tx_info.serialize())
            for sub_field in sub_fields:
                app_client.provide_transaction_field_desc(sub_field.serialize())

    with app_client.sign(mode=SignMode.START_FLOW):
        scenario_navigator.review_approve()


def test_gcs_nested_no_param(scenario_navigator: NavigateWithScenario):
    backend = scenario_navigator.backend
    app_client = EthAppClient(backend)

    with open(f"{ABIS_FOLDER}/erc20.json", encoding="utf-8") as f:
        sub_contract = Web3().eth.contract(
            abi=json.load(f),
            address=bytes.fromhex("c02aaa39b223fe8d0a0e5c4f27ead9083c756cc2")
        )
    sub_data = sub_contract.encode_abi("totalSupply", [])

    with open(f"{ABIS_FOLDER}/safe_1.4.1.abi.json", encoding="utf-8") as f:
        contract = Web3().eth.contract(
            abi=json.load(f),
            address=bytes.fromhex("23F8abfC2824C397cCB3DA89ae772984107dDB99")
        )
    data = contract.encode_abi("execTransaction", [
        sub_contract.address,
        Web3.to_wei(0, "ether"),
        sub_data,
        0,
        0,
        0,
        0,
        bytes.fromhex("0000000000000000000000000000000000000000"),
        bytes.fromhex("0000000000000000000000000000000000000000"),
        bytes(),
    ])

    tx_params = {
        "nonce": 79,
        "maxFeePerGas": Web3.to_wei(4.8, "gwei"),
        "maxPriorityFeePerGas": Web3.to_wei(2, "gwei"),
        "gas": 5118,
        "to": contract.address,
        "data": data,
        "chainId": 1
    }

    with app_client.sign("m/44'/60'/0'/0/0", tx_params, mode=SignMode.STORE):
        pass

    param_paths = get_all_paths(f"{ABIS_FOLDER}/safe_1.4.1.abi.json", "execTransaction")
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
                        param_paths["data"]
                    ),
                ),
                Value(
                    1,
                    TypeFamily.ADDRESS,
                    data_path=DataPath(
                        1,
                        param_paths["to"]
                    ),
                ),
            )
        ),
    ]

    # compute instructions hash
    inst_hash = compute_inst_hash(fields)

    tx_info = TxInfo(
        1,
        tx_params["chainId"],
        tx_params["to"],
        get_selector_from_data(tx_params["data"]),
        inst_hash,
        "execute a Safe action",
        creator_name="Safe",
        creator_legal_name="Safe Ecosystem Foundation",
        creator_url="safe.global",
    )

    app_client.provide_transaction_info(tx_info.serialize())

    sub_tx_info = TxInfo(
        1,
        tx_params["chainId"],
        sub_contract.address,
        get_selector_from_data(sub_data),
        hashlib.sha3_256().digest(),
        "get total supply",
        creator_name="WETH",
        creator_legal_name="Wrapped Ether",
        creator_url="weth.io",
    )

    for field in fields:
        app_client.provide_transaction_field_desc(field.serialize())
        if field.param.type == ParamType.CALLDATA:
            app_client.provide_transaction_info(sub_tx_info.serialize())

    with app_client.sign(mode=SignMode.START_FLOW):
        scenario_navigator.review_approve()


def test_gcs_no_param(scenario_navigator: NavigateWithScenario):
    backend = scenario_navigator.backend
    app_client = EthAppClient(backend)

    with open(f"{ABIS_FOLDER}/erc20.json", encoding="utf-8") as f:
        contract = Web3().eth.contract(
            abi=json.load(f),
            address=bytes.fromhex("c02aaa39b223fe8d0a0e5c4f27ead9083c756cc2")
        )
    data = contract.encode_abi("totalSupply", [])

    tx_params = {
        "nonce": 79,
        "maxFeePerGas": Web3.to_wei(4.8, "gwei"),
        "maxPriorityFeePerGas": Web3.to_wei(2, "gwei"),
        "gas": 5118,
        "to": contract.address,
        "data": data,
        "chainId": 1
    }

    with app_client.sign("m/44'/60'/0'/0/0", tx_params, mode=SignMode.STORE):
        pass

    tx_info = TxInfo(
        1,
        tx_params["chainId"],
        contract.address,
        get_selector_from_data(data),
        hashlib.sha3_256().digest(),
        "get total supply",
        creator_name="WETH",
        creator_legal_name="Wrapped Ether",
        creator_url="weth.io",
    )

    app_client.provide_transaction_info(tx_info.serialize())

    with app_client.sign(mode=SignMode.START_FLOW):
        scenario_navigator.review_approve()


def test_gcs_trusted_name_token(scenario_navigator: NavigateWithScenario):
    backend = scenario_navigator.backend
    app_client = EthAppClient(backend)

    tokens = [
        {
            "name": "WETH",
            "address": bytes.fromhex("c02aaa39b223fe8d0a0e5c4f27ead9083c756cc2"),
        },
        {
            "name": "USDC",
            "address": bytes.fromhex("A0b86991c6218b36c1d19D4a2e9Eb0cE3606eB48"),
        },
    ]

    with open(f"{ABIS_FOLDER}/1inch.abi.json", encoding="utf-8") as file:
        contract = Web3().eth.contract(
            abi=json.load(file),
            address=None
        )
    data = contract.encode_abi("swap", [
        bytes.fromhex("F313B370D28760b98A2E935E56Be92Feb2c4EC04"),
        [
            tokens[0]["address"],
            tokens[1]["address"],
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

    param_paths = get_all_tuple_paths(f"{ABIS_FOLDER}/1inch.abi.json", "swap", "desc")
    fields = [
            Field(
                1,
                "Send token",
                ParamTrustedName(
                    1,
                    Value(
                        1,
                        TypeFamily.ADDRESS,
                        data_path=DataPath(
                            1,
                            param_paths["srcToken"]
                        ),
                    ),
                    [TrustedNameType.TOKEN],
                    [TrustedNameSource.CAL],
                )
            ),
            Field(
                1,
                "Receive token",
                ParamTrustedName(
                    1,
                    Value(
                        1,
                        TypeFamily.ADDRESS,
                        data_path=DataPath(
                            1,
                            param_paths["dstToken"]
                        ),
                    ),
                    [TrustedNameType.TOKEN],
                    [TrustedNameSource.CAL],
                )
            ),
    ]

    # compute instructions hash
    inst_hash = compute_inst_hash(fields)
    tx_info = TxInfo(
        1,
        tx_params["chainId"],
        tx_params["to"],
        get_selector_from_data(tx_params["data"]),
        inst_hash,
        "swap",
        creator_name="1inch",
        creator_legal_name="1inch Network",
        creator_url="1inch.io",
        contract_name="Aggregation Router V6",
        deploy_date=1707724800
    )

    app_client.provide_transaction_info(tx_info.serialize())

    for i, field in enumerate(fields):
        challenge = ResponseParser.challenge(app_client.get_challenge().data)
        app_client.provide_trusted_name_v2(tokens[i]["address"],
                                           tokens[i]["name"],
                                           TrustedNameType.TOKEN,
                                           TrustedNameSource.CAL,
                                           tx_params["chainId"],
                                           challenge=challenge)
        app_client.provide_transaction_field_desc(field.serialize())

    with app_client.sign(mode=SignMode.START_FLOW):
        scenario_navigator.review_approve()


def test_gcs_batch(scenario_navigator: NavigateWithScenario):
    backend = scenario_navigator.backend
    app_client = EthAppClient(backend)

    tokens = [
        {
            "ticker": "USDT",
            "address": bytes.fromhex("dac17f958d2ee523a2206206994597c13d831ec7"),
            "decimals": 6,
        },
        {
            "ticker": "WETH",
            "address": bytes.fromhex("c02aaa39b223fe8d0a0e5c4f27ead9083c756cc2"),
            "decimals": 18,
        },
    ]
    with open(f"{ABIS_FOLDER}/erc20.json", encoding="utf-8") as f:
        contract = Web3().eth.contract(
            abi=json.load(f),
            address=None
        )
    data0 = contract.encode_abi("transfer", [
        bytes.fromhex("0000000000000000000000000000000000000000"),
        int(500 * pow(10, tokens[0]["decimals"])),
    ])
    data1 = contract.encode_abi("transfer", [
        bytes.fromhex("1111111111111111111111111111111111111111"),
        int(0.25 * pow(10, tokens[1]["decimals"])),
    ])

    with open(f"{ABIS_FOLDER}/batch.json", encoding="utf-8") as f:
        contract = Web3().eth.contract(
            abi=json.load(f),
            address=tokens[1]["address"]
        )

    data = contract.encode_abi("batchExecute", [[
        (
            tokens[0]["address"],
            Web3.to_wei(0, "ether"),
            data0
        ),
        (
            tokens[1]["address"],
            Web3.to_wei(0, "ether"),
            data1
        ),
    ]])

    tx_params = {
        "nonce": 79,
        "maxFeePerGas": Web3.to_wei(4.8, "gwei"),
        "maxPriorityFeePerGas": Web3.to_wei(2, "gwei"),
        "gas": 5118,
        "to": contract.address,
        "data": data,
        "chainId": 1
    }

    with app_client.sign("m/44'/60'/0'/0/0", tx_params, mode=SignMode.STORE):
        pass

    param_paths = get_all_paths(f"{ABIS_FOLDER}/erc20.json", "transfer")
    sub_fields = [
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
                            param_paths["_to"]
                        ),
                    )
                )
            ),
            Field(
                1,
                "Amount",
                ParamTokenAmount(
                    1,
                    Value(
                        1,
                        TypeFamily.UINT,
                        32,
                        DataPath(
                            1,
                            param_paths["_value"]
                        ),
                    ),
                    Value(
                        1,
                        TypeFamily.ADDRESS,
                        container_path=ContainerPath.TO,
                    ),
                )
            ),
    ]

    param_paths = get_all_tuple_array_paths(f"{ABIS_FOLDER}/batch.json", "batchExecute", "calls")
    fields = [
            Field(
                1,
                "Destination",
                ParamCalldata(
                    1,
                    Value(
                        1,
                        TypeFamily.BYTES,
                        data_path=DataPath(
                            1,
                            param_paths["data"]
                        ),
                    ),
                    Value(
                        1,
                        TypeFamily.ADDRESS,
                        data_path=DataPath(
                            1,
                            param_paths["to"]
                        ),
                    ),
                    amount=Value(
                        1,
                        TypeFamily.UINT,
                        data_path=DataPath(
                            1,
                            param_paths["value"]
                        ),
                    ),
                )
            ),
    ]

    # compute instructions hash
    inst_hash = compute_inst_hash(fields)

    tx_info = TxInfo(
        1,
        tx_params["chainId"],
        contract.address,
        get_selector_from_data(data),
        inst_hash,
        "Batch transaction",
        creator_name="WETH",
        creator_legal_name="Wrapped Ether",
        creator_url="weth.io",
    )

    app_client.provide_transaction_info(tx_info.serialize())

    # compute instructions hash
    sub_inst_hash = compute_inst_hash(sub_fields)

    sub_tx_info = [
        TxInfo(
            1,
            tx_params["chainId"],
            tokens[0]["address"],
            get_selector_from_data(data0),
            sub_inst_hash,
            "Transfer token",
        ),
        TxInfo(
            1,
            tx_params["chainId"],
            tokens[1]["address"],
            get_selector_from_data(data1),
            sub_inst_hash,
            "Transfer token",
        )
    ]

    for field in fields:
        app_client.provide_transaction_field_desc(field.serialize())
        for idx, sub_info in enumerate(sub_tx_info):
            app_client.provide_token_metadata(tokens[idx]["ticker"],
                                              tokens[idx]["address"],
                                              tokens[idx]["decimals"],
                                              tx_params["chainId"])
            app_client.provide_transaction_info(sub_info.serialize())
            for sub_field in sub_fields:
                app_client.provide_transaction_field_desc(sub_field.serialize())

    with app_client.sign(mode=SignMode.START_FLOW):
        scenario_navigator.review_approve()


def test_gcs_batch_2(scenario_navigator: NavigateWithScenario):
    backend = scenario_navigator.backend
    app_client = EthAppClient(backend)

    # Define tokens
    tokens = [
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
        contract = Web3().eth.contract(
            abi=json.load(f),
            address=None
        )
    tokenData0 = contract.encode_abi("transfer", [
        bytes.fromhex("B8C8EB8EFC68796E766F6AB320DB8C165C064949"),
        int(0.004 * pow(10, tokens[0]["decimals"])),
    ])
    tokenData1 = contract.encode_abi("transfer", [
        bytes.fromhex("4DDA64E1EC1A2C00D0766F25877F6A3BC77F717E"),
        int(0.008 * pow(10, tokens[1]["decimals"])),
    ])

    # Encode batchExecute data using token transfer data
    with open(f"{ABIS_FOLDER}/batch.json", encoding="utf-8") as f:
        contract = Web3().eth.contract(
            abi=json.load(f),
            address=tokens[1]["address"]
        )
    batchData = contract.encode_abi("batchExecute", [[
        (
            tokens[0]["address"],
            Web3.to_wei(0, "ether"),
            tokenData0
        ),
        (
            tokens[1]["address"],
            Web3.to_wei(0, "ether"),
            tokenData1
        ),
    ]])

    # pylint: disable=line-too-long
    # Encode execTransaction data using batchExecute data
    with open(f"{ABIS_FOLDER}/safe_1.4.1.abi.json", encoding="utf-8") as f:
        contract = Web3().eth.contract(
            abi=json.load(f),
            address=bytes.fromhex("60aa01971a2adc1d6b2b59b972fb47b2fec095fc")
        )
    execDataSignature = "93a3e6ff4d0798d51ba53f5d8287326adbe3e22dd0dc28bdbfab825be357ce8c76a13b8128f5d91530af675925220ede099e0f0a51af3a65760060d4b37db9281c"
    execTxData = contract.encode_abi("execTransaction", [
        contract.address,
        0,
        batchData,
        1,
        0,
        0,
        0,
        bytes.fromhex("0000000000000000000000000000000000000000"),
        bytes.fromhex("0000000000000000000000000000000000000000"),
        bytes.fromhex(execDataSignature),
    ])
    # pylint: enable=line-too-long

    # Define top level transaction parameters
    tx_params = {
        "nonce": 0,
        "maxFeePerGas": Web3.to_wei(4.41, "gwei"),
        "maxPriorityFeePerGas": Web3.to_wei(2.03, "gwei"),
        "gas": 109012,
        "to": bytes.fromhex("19a4d6928cd3b32Fa4Eb3962bfF1Abca91EB7C52"),
        "data": execTxData,
        "chainId": 137
    }

    # Store transaction parameters on the device
    with app_client.sign("m/44'/60'/0'/0/0", tx_params, mode=SignMode.STORE):
        pass

    print(f"{'~' * 20} DEBUG L0 {'~' * 20}")
    param_paths = get_all_paths(f"{ABIS_FOLDER}/safe_1.4.1.abi.json", "execTransaction")
    # Top level transaction fields definition
    L0_fields = [
        Field(
            1,
            "From Safe",
            ParamTrustedName(
                1,
                Value(
                    1,
                    TypeFamily.ADDRESS,
                    container_path=ContainerPath.TO,
                ),
                [TrustedNameType.CONTRACT],
                [TrustedNameSource.CAL],
            )
        ),
        Field(
            1,
            "Execution signer",
            ParamTrustedName(
                1,
                Value(
                    1,
                    TypeFamily.ADDRESS,
                    container_path=ContainerPath.FROM,
                ),
                [TrustedNameType.ACCOUNT],
                [TrustedNameSource.ENS, TrustedNameSource.UD, TrustedNameSource.FN],
            )
        ),
        Field(
            1,
            "Transaction",
            ParamCalldata(
                1,
                Value(
                    1,
                    TypeFamily.BYTES,
                    data_path=DataPath(
                        1,
                        param_paths["data"]
                    ),
                ),
                Value(
                    1,
                    TypeFamily.ADDRESS,
                    data_path=DataPath(
                        1,
                        param_paths["to"]
                    ),
                ),
                amount=Value(
                    1,
                    TypeFamily.UINT,
                    type_size=32,
                    data_path=DataPath(
                        1,
                        param_paths["value"]
                    ),
                ),
                spender=Value(
                    1,
                    TypeFamily.ADDRESS,
                    container_path=ContainerPath.TO,
                ),
            )
        ),
        Field(
            1,
            "Gas amount",
            ParamRaw(
                1,
                Value(
                    1,
                    TypeFamily.UINT,
                    type_size=32,
                    data_path=DataPath(
                        1,
                        param_paths["safeTxGas"]
                    ),
                ),
            )
        ),
        Field(
            1,
            "Gas price",
            ParamTokenAmount(
                1,
                Value(
                    1,
                    TypeFamily.UINT,
                    type_size=32,
                    data_path=DataPath(
                        1,
                        param_paths["baseGas"]
                    ),
                ),
                Value(
                    1,
                    TypeFamily.ADDRESS,
                    data_path=DataPath(
                        1,
                        param_paths["gasPrice"]
                    ),
                ),
                [bytes.fromhex("0000000000000000000000000000000000000000")]
            )
        ),
        Field(
            1,
            "Gas receiver",
            ParamTrustedName(
                1,
                Value(
                    1,
                    TypeFamily.ADDRESS,
                    data_path=DataPath(
                        1,
                        param_paths["refundReceiver"]
                    ),
                ),
                [TrustedNameType.ACCOUNT, TrustedNameType.CONTRACT, TrustedNameType.TOKEN],
                [TrustedNameSource.CAL, TrustedNameSource.ENS, TrustedNameSource.UD, TrustedNameSource.FN],
            )
        ),
    ]
    # compute instructions hash
    L0_hash = compute_inst_hash(L0_fields)

    # Define top level transaction info
    L0_tx_info = TxInfo(
        1,
        tx_params["chainId"],
        bytes.fromhex("29fcb43b46531bca003ddc8fcb67ffe91900c762"),
        get_selector_from_data(tx_params["data"]),
        L0_hash,
        "sign multisig operation",
        creator_name="Safe",
        creator_legal_name="Safe{Wallet}",
        creator_url="https://app.safe.global/welcome",
        contract_name="SafeL2",
    )

    print(f"{'~' * 20} DEBUG L1 {'~' * 20}")
    param_paths = get_all_tuple_array_paths(f"{ABIS_FOLDER}/batch.json", "batchExecute", "calls")
    # Intermediate execTransaction transaction fields definition
    L1_fields = [
            Field(
                1,
                "Transaction",
                ParamCalldata(
                    1,
                    Value(
                        1,
                        TypeFamily.BYTES,
                        data_path=DataPath(
                            1,
                            param_paths["data"]
                        ),
                    ),
                    Value(
                        1,
                        TypeFamily.ADDRESS,
                        data_path=DataPath(
                            1,
                            param_paths["to"]
                        ),
                    ),
                    amount=Value(
                        1,
                        TypeFamily.UINT,
                        data_path=DataPath(
                            1,
                            param_paths["value"]
                        ),
                    ),
                )
            ),
    ]
    # compute instructions hash
    L1_hash = compute_inst_hash(L1_fields)

    # Define intermediate execTransaction transaction info
    L1_tx_info = [
        TxInfo(
            1,
            tx_params["chainId"],
            contract.address,
            get_selector_from_data(batchData),
            L1_hash,
            "Batch transactions",
            creator_name="Ledger",
            creator_legal_name="Ledger Multisig",
            creator_url="https://www.ledger.com",
            contract_name="BatchExecutor",
        ),
    ]

    print(f"{'~' * 20} DEBUG L2 {'~' * 20}")
    param_paths = get_all_paths(f"{ABIS_FOLDER}/erc20.json", "transfer")
    # Lower batchExecute transaction fields definition
    L2_fields = [
            Field(
                1,
                "Amount",
                ParamTokenAmount(
                    1,
                    Value(
                        1,
                        TypeFamily.UINT,
                        data_path=DataPath(
                            1,
                            param_paths["_value"]
                        ),
                        type_size=32,
                    ),
                    Value(
                        1,
                        TypeFamily.ADDRESS,
                        container_path=ContainerPath.TO,
                    ),
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
                            param_paths["_to"]
                        ),
                    )
                )
            ),
    ]
    # compute instructions hash
    L2_hash = compute_inst_hash(L2_fields)

    # Define lower batchExecute transaction info
    L2_tx_info = [
        TxInfo(
            1,
            tx_params["chainId"],
            tokens[0]["address"],
            get_selector_from_data(tokenData0),
            L2_hash,
            "Send",
            contract_name="USD_Coin",
        ),
        TxInfo(
            1,
            tx_params["chainId"],
            tokens[1]["address"],
            get_selector_from_data(tokenData1),
            L2_hash,
            "Send",
            contract_name="USD_Coin",
        )
    ]

    proxy_info = ProxyInfo(
        ResponseParser.challenge(app_client.get_challenge().data),
        tx_params["to"],
        L0_tx_info.chain_id,
        L0_tx_info.contract_addr,
    )

    # Send Proxy information
    app_client.provide_proxy_info(proxy_info.serialize())

    # Send Network information (name, ticker, icon)
    name, ticker, icon = get_network_config(backend.device.type, tx_params["chainId"])
    if name and ticker:
        app_client.provide_network_information(DynamicNetwork(name, ticker, tx_params["chainId"], icon))

    # Send top level transaction info
    app_client.provide_transaction_info(L0_tx_info.serialize())

    # Send all fields and subfields info
    for f0 in L0_fields:
        # Send top level fields description
        app_client.provide_transaction_field_desc(f0.serialize())
        if f0.param.type == ParamType.CALLDATA:
            # Intermediate execTransaction description
            for _, i1 in enumerate(L1_tx_info):
                # Send intermediate execTransaction info description
                app_client.provide_transaction_info(i1.serialize())
                for f1 in L1_fields:
                    # Send intermediate execTransaction fields description
                    app_client.provide_transaction_field_desc(f1.serialize())

                # Lower batchExecute description
                for idx, i2 in enumerate(L2_tx_info):
                    # Send lower batchExecute info description
                    app_client.provide_transaction_info(i2.serialize())
                    app_client.provide_token_metadata(tokens[idx]["ticker"],
                                                      tokens[idx]["address"],
                                                      tokens[idx]["decimals"],
                                                      tx_params["chainId"])
                    for f2 in L2_fields:
                        # Send lower batchExecute fields description
                        app_client.provide_transaction_field_desc(f2.serialize())

    # Send the full transaction
    with app_client.sign(mode=SignMode.START_FLOW):
        scenario_navigator.review_approve()


def test_gcs_batch_empty_tx(scenario_navigator: NavigateWithScenario) -> None:
    backend = scenario_navigator.backend
    app_client = EthAppClient(backend)

    with Path(f"{ABIS_FOLDER}/batch.json").open() as f:
        contract = Web3().eth.contract(
            abi=json.load(f),
            address=bytes.fromhex("2cc8475177918e8C4d840150b68815A4b6f0f5f3"),
        )

    data = contract.encode_abi("batchExecute", [[
        (
            bytes.fromhex("d8dA6BF26964aF9D7eEd9e03E53415D37aA96045"),
            Web3.to_wei(0.0, "ether"),
            b"",
        ),
    ]])
    tx_params = {
        "nonce": 79,
        "maxFeePerGas": Web3.to_wei(4.8, "gwei"),
        "maxPriorityFeePerGas": Web3.to_wei(2, "gwei"),
        "gas": 2000,
        "to": contract.address,
        "data": data,
        "chainId": 1,
    }
    with app_client.sign("m/44'/60'/0'/0/0", tx_params, mode=SignMode.STORE):
        pass
    param_paths = get_all_tuple_array_paths(f"{ABIS_FOLDER}/batch.json", "batchExecute", "calls")
    fields = [
            Field(
                1,
                "Destination",
                ParamCalldata(
                    1,
                    Value(
                        1,
                        TypeFamily.BYTES,
                        data_path=DataPath(
                            1,
                            param_paths["data"],
                        ),
                    ),
                    Value(
                        1,
                        TypeFamily.ADDRESS,
                        data_path=DataPath(
                            1,
                            param_paths["to"],
                        ),
                    ),
                ),
            ),
    ]

    # compute instructions hash
    inst_hash = compute_inst_hash(fields)

    tx_info = TxInfo(
        1,
        tx_params["chainId"],
        contract.address,
        get_selector_from_data(data),
        inst_hash,
        "Batch transaction",
        creator_name="Ledger Multisig",
        creator_legal_name="Ledger",
    )
    app_client.provide_transaction_info(tx_info.serialize())
    for field in fields:
        app_client.provide_transaction_field_desc(field.serialize())

    with app_client.sign(mode=SignMode.START_FLOW):
        scenario_navigator.review_approve()


def test_gcs_batch_complex(scenario_navigator: NavigateWithScenario) -> None:
    backend = scenario_navigator.backend
    app_client = EthAppClient(backend)

    tokens = [
        {
            "ticker": "USDT",
            "address": bytes.fromhex("dac17f958d2ee523a2206206994597c13d831ec7"),
            "decimals": 6,
        },
        {
            "ticker": "WETH",
            "address": bytes.fromhex("c02aaa39b223fe8d0a0e5c4f27ead9083c756cc2"),
            "decimals": 18,
        },
    ]
    with Path(f"{ABIS_FOLDER}/erc20.json").open() as f:
        contract = Web3().eth.contract(
            abi=json.load(f),
            address=None,
        )
    data0 = contract.encode_abi("transfer", [
        bytes.fromhex("1111111111111111111111111111111111111111"),
        int(1.1 * pow(10, tokens[0]["decimals"])),
    ])
    data1 = contract.encode_abi("transfer", [
        bytes.fromhex("3333333333333333333333333333333333333333"),
        int(3.3 * pow(10, tokens[1]["decimals"])),
    ])

    with Path(f"{ABIS_FOLDER}/batch.json").open() as f:
        contract = Web3().eth.contract(
            abi=json.load(f),
            address=bytes.fromhex("2cc8475177918e8C4d840150b68815A4b6f0f5f3"),
        )

    data = contract.encode_abi("batchExecute", [[
        (
            bytes.fromhex("0000000000000000000000000000000000000000"),
            Web3.to_wei(0.0, "ether"),
            b"",
        ),
        (
            tokens[0]["address"],
            Web3.to_wei(0, "ether"),
            data0,
        ),
        (
            bytes.fromhex("2222222222222222222222222222222222222222"),
            Web3.to_wei(2.2, "ether"),
            b"",
        ),
        (
            tokens[1]["address"],
            Web3.to_wei(0, "ether"),
            data1,
        ),
        (
            bytes.fromhex("4444444444444444444444444444444444444444"),
            Web3.to_wei(4.4, "ether"),
            b"",
        ),
    ]])

    tx_params = {
        "nonce": 79,
        "maxFeePerGas": Web3.to_wei(4.8, "gwei"),
        "maxPriorityFeePerGas": Web3.to_wei(2, "gwei"),
        "gas": 10120,
        "to": contract.address,
        "data": data,
        "chainId": 1,
    }

    with app_client.sign("m/44'/60'/0'/0/0", tx_params, mode=SignMode.STORE):
        pass

    param_paths = get_all_paths(f"{ABIS_FOLDER}/erc20.json", "transfer")
    sub_fields = [
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
                            param_paths["_to"],
                        ),
                    ),
                ),
            ),
            Field(
                1,
                "Amount",
                ParamTokenAmount(
                    1,
                    Value(
                        1,
                        TypeFamily.UINT,
                        32,
                        DataPath(
                            1,
                            param_paths["_value"],
                        ),
                    ),
                    Value(
                        1,
                        TypeFamily.ADDRESS,
                        container_path=ContainerPath.TO,
                    ),
                ),
            ),
    ]

    param_paths = get_all_tuple_array_paths(f"{ABIS_FOLDER}/batch.json", "batchExecute", "calls")
    fields = [
            Field(
                1,
                "Destination",
                ParamCalldata(
                    1,
                    Value(
                        1,
                        TypeFamily.BYTES,
                        data_path=DataPath(
                            1,
                            param_paths["data"],
                        ),
                    ),
                    Value(
                        1,
                        TypeFamily.ADDRESS,
                        data_path=DataPath(
                            1,
                            param_paths["to"],
                        ),
                    ),
                    amount=Value(
                        1,
                        TypeFamily.UINT,
                        data_path=DataPath(
                            1,
                            param_paths["value"],
                        ),
                    ),
                ),
            ),
    ]

    # compute instructions hash
    inst_hash = compute_inst_hash(fields)

    tx_info = TxInfo(
        1,
        tx_params["chainId"],
        contract.address,
        get_selector_from_data(data),
        inst_hash,
        "Batch transaction",
        creator_name="Ledger Multisig",
        creator_legal_name="Ledger",
    )

    app_client.provide_transaction_info(tx_info.serialize())

    app_client.provide_trusted_name_v2(b"\x00" * 20,
                                       "null.eth",
                                       TrustedNameType.ACCOUNT,
                                       TrustedNameSource.ENS,
                                       tx_params["chainId"],
                                       challenge=ResponseParser.challenge(app_client.get_challenge().data))

    app_client.provide_trusted_name_v2(b"\x44" * 20,
                                        "FOUR",
                                        TrustedNameType.ACCOUNT,
                                        TrustedNameSource.MULTISIG_ADDRESS_BOOK,
                                        tx_params["chainId"],
                                        challenge=ResponseParser.challenge(app_client.get_challenge().data))

    # compute instructions hash
    sub_inst_hash = compute_inst_hash(sub_fields)

    sub_tx_info = [
        TxInfo(
            1,
            tx_params["chainId"],
            tokens[0]["address"],
            get_selector_from_data(data0),
            sub_inst_hash,
            "Transfer token",
        ),
        TxInfo(
            1,
            tx_params["chainId"],
            tokens[1]["address"],
            get_selector_from_data(data1),
            sub_inst_hash,
            "Transfer token",
        ),
    ]

    for field in fields:
        app_client.provide_transaction_field_desc(field.serialize())
        for idx, sub_info in enumerate(sub_tx_info):
            app_client.provide_token_metadata(tokens[idx]["ticker"],
                                              tokens[idx]["address"],
                                              tokens[idx]["decimals"],
                                              tx_params["chainId"])
            app_client.provide_transaction_info(sub_info.serialize())
            for sub_field in sub_fields:
                app_client.provide_transaction_field_desc(sub_field.serialize())

    with app_client.sign(mode=SignMode.START_FLOW):
        scenario_navigator.review_approve()
