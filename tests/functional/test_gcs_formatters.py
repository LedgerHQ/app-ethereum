# GCS formatter-focused tests: param types, iteration behaviour, constraints.
# Large test file containing multiple GCS (Generic Clear Signing) integration tests
import hashlib
import json

import client.response_parser as ResponseParser
import pytest
from client.client import EthAppClient, SignMode
from client.dynamic_networks import DynamicNetwork
from client.gcs import (
    ContainerPath,
    DataPath,
    DatetimeType,
    Field,
    GroupIterationType,
    MapRef,
    ParamDatetime,
    ParamGroup,
    ParamNetwork,
    ParamRaw,
    ParamTokenAmount,
    ParamTrustedName,
    TrustedNameValueType,
    TxInfo,
    TypeFamily,
    Value,
    VisibleType,
)
from client.map_entry import MapEntry
from client.status_word import StatusWord
from client.trusted_name import TrustedName, TrustedNameSource, TrustedNameType
from client.utils import get_selector_from_data
from constants import ABIS_FOLDER
from dynamic_networks_cfg import get_network_config
from fields_utils import get_all_paths
from ragger.error import ExceptionRAPDU
from ragger.navigator.navigation_scenario import NavigateWithScenario
from web3 import Web3


def compute_inst_hash(fields: list[Field]) -> bytes:
    inst_hash = hashlib.sha3_256()
    for field in fields:
        inst_hash.update(field.serialize())
    return inst_hash.digest()


def test_gcs_map_entry(scenario_navigator: NavigateWithScenario):
    """Test MAP_ENTRY feature: maps a calldata uint256 (eventId) to a human-readable event name."""
    backend = scenario_navigator.backend
    app_client = EthAppClient(backend)

    with open(f"{ABIS_FOLDER}/poap.abi.json", encoding="utf-8") as file:
        contract = Web3().eth.contract(abi=json.load(file), address=None)
    eventId = 175676
    data = contract.encode_abi(
        "mintToken",
        [
            eventId,
            7163978,
            bytes.fromhex("Dad77910DbDFdE764fC21FCD4E74D71bBACA6D8D"),
            1730621615,
            bytes.fromhex(
                "8991da687cff5300959810a08c4ec183bb2a56dc82f5aac2b24f1106c2d983ac6f7a6b28700a236724d814000d0fd8c395fcf9f87c4424432ebf30c9479201d71c"
            ),
        ],
    )
    tx_params = {
        "nonce": 235,
        "maxFeePerGas": Web3.to_wei(100, "gwei"),
        "maxPriorityFeePerGas": Web3.to_wei(10, "gwei"),
        "gas": 44001,
        "to": bytes.fromhex("0bb4D3e88243F4A057Db77341e6916B0e449b158"),
        "data": data,
        "chainId": 1,
    }

    with app_client.sign("m/44'/60'/0'/0/0", tx_params, mode=SignMode.STORE):
        pass

    param_paths = get_all_paths(f"{ABIS_FOLDER}/poap.abi.json", "mintToken")

    # MAP_REF key: the eventId Value (uint256 from calldata)
    event_id_key_value = Value(
        1,
        TypeFamily.UINT,
        type_size=32,
        data_path=DataPath(1, param_paths["eventId"]),
    )

    fields = [
        Field(
            1,
            "Event name",
            ParamRaw(
                1,
                Value(
                    1,
                    TypeFamily.STRING,
                    map_ref=MapRef(version=1, id=0, key=event_id_key_value),
                ),
            ),
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
                    data_path=DataPath(1, param_paths["tokenId"]),
                ),
            ),
        ),
        Field(
            1,
            "Receiver",
            ParamRaw(
                1,
                Value(
                    1,
                    TypeFamily.ADDRESS,
                    data_path=DataPath(1, param_paths["receiver"]),
                ),
            ),
        ),
    ]

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
        deploy_date=1646305200,
    )

    app_client.provide_map_entry(
        MapEntry(
            version=1,
            chain_id=tx_params["chainId"],
            contract_addr=tx_params["to"],
            selector=get_selector_from_data(tx_params["data"]),
            id=0,
            key=eventId.to_bytes(32, "big"),  # 32-byte big-endian (ABI uint256 encoding)
            value=b"EthCC Paris",
        ).serialize()
    )

    app_client.provide_transaction_info(tx_info.serialize())

    for field in fields:
        app_client.provide_transaction_field_desc(field.serialize())

    with app_client.sign(mode=SignMode.START_FLOW):
        scenario_navigator.review_approve()


def test_gcs_map_entry_chain_id_key(scenario_navigator: NavigateWithScenario):
    """Test MAP_ENTRY with ContainerPath.CHAIN_ID as key: maps chain_id to a network name."""
    backend = scenario_navigator.backend
    app_client = EthAppClient(backend)

    with open(f"{ABIS_FOLDER}/poap.abi.json", encoding="utf-8") as file:
        contract = Web3().eth.contract(abi=json.load(file), address=None)
    data = contract.encode_abi(
        "mintToken",
        [
            175676,
            7163978,
            bytes.fromhex("Dad77910DbDFdE764fC21FCD4E74D71bBACA6D8D"),
            1730621615,
            bytes.fromhex(
                "8991da687cff5300959810a08c4ec183bb2a56dc82f5aac2b24f1106c2d983ac6f7a6b28700a236724d814000d0fd8c395fcf9f87c4424432ebf30c9479201d71c"
            ),
        ],
    )
    tx_params = {
        "nonce": 235,
        "maxFeePerGas": Web3.to_wei(100, "gwei"),
        "maxPriorityFeePerGas": Web3.to_wei(10, "gwei"),
        "gas": 44001,
        "to": bytes.fromhex("0bb4D3e88243F4A057Db77341e6916B0e449b158"),
        "data": data,
        "chainId": 1,
    }

    with app_client.sign("m/44'/60'/0'/0/0", tx_params, mode=SignMode.STORE):
        pass

    param_paths = get_all_paths(f"{ABIS_FOLDER}/poap.abi.json", "mintToken")

    # MAP_REF key: chain_id from the RLP transaction context (ContainerPath.CHAIN_ID).
    # The C implementation exposes the chain_id as an 8-byte big-endian uint64, so the
    # MAP_ENTRY key must be encoded the same way: chain_id=1 → (1).to_bytes(8, "big").
    chain_id_key_value = Value(
        1,
        TypeFamily.UINT,
        container_path=ContainerPath.CHAIN_ID,
    )

    fields = [
        Field(
            1,
            "Network",
            ParamRaw(
                1,
                Value(
                    1,
                    TypeFamily.STRING,
                    map_ref=MapRef(version=1, id=1, key=chain_id_key_value),
                ),
            ),
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
                    data_path=DataPath(1, param_paths["tokenId"]),
                ),
            ),
        ),
    ]

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
        deploy_date=1646305200,
    )

    # MAP_ENTRY key is the 8-byte big-endian encoding of chain_id=1
    app_client.provide_map_entry(
        MapEntry(
            version=1,
            chain_id=tx_params["chainId"],
            contract_addr=tx_params["to"],
            selector=get_selector_from_data(tx_params["data"]),
            id=1,
            key=tx_params["chainId"].to_bytes(8, "big"),
            value=b"Ethereum",
        ).serialize()
    )

    app_client.provide_transaction_info(tx_info.serialize())

    for field in fields:
        app_client.provide_transaction_field_desc(field.serialize())

    with app_client.sign(mode=SignMode.START_FLOW):
        scenario_navigator.review_approve()


def test_gcs_group_sequential(scenario_navigator: NavigateWithScenario):
    """Test PARAM_GROUP with SEQUENTIAL iteration over scalar sub-fields.

    No visual difference vs individual flat fields is expected here: GROUP with
    SEQUENTIAL iteration and scalar sub-fields produces exactly the same display
    order as listing those fields individually (Event ID, then Token ID).
    Visual differentiation only arises with BUNDLED iteration (not yet implemented)
    or group separator UI elements. This test validates correct TLV parsing and
    round-trip for the GROUP param type.
    """
    backend = scenario_navigator.backend
    app_client = EthAppClient(backend)

    with open(f"{ABIS_FOLDER}/poap.abi.json", encoding="utf-8") as file:
        contract = Web3().eth.contract(abi=json.load(file), address=None)
    data = contract.encode_abi(
        "mintToken",
        [
            175676,
            7163978,
            bytes.fromhex("Dad77910DbDFdE764fC21FCD4E74D71bBACA6D8D"),
            1730621615,
            bytes.fromhex(
                "8991da687cff5300959810a08c4ec183bb2a56dc82f5aac2b24f1106c2d983ac6f7a6b28700a236724d814000d0fd8c395fcf9f87c4424432ebf30c9479201d71c"
            ),
        ],
    )
    tx_params = {
        "nonce": 235,
        "maxFeePerGas": Web3.to_wei(100, "gwei"),
        "maxPriorityFeePerGas": Web3.to_wei(10, "gwei"),
        "gas": 44001,
        # PoapBridge
        "to": bytes.fromhex("0bb4D3e88243F4A057Db77341e6916B0e449b158"),
        "data": data,
        "chainId": 1,
    }

    with app_client.sign("m/44'/60'/0'/0/0", tx_params, mode=SignMode.STORE):
        pass

    param_paths = get_all_paths(f"{ABIS_FOLDER}/poap.abi.json", "mintToken")

    # GROUP containing two sub-fields: Event ID and Token ID displayed in order.
    group_field = Field(
        1,
        "POAP Details",
        ParamGroup(
            version=1,
            iteration_type=GroupIterationType.SEQUENTIAL,
            fields=[
                Field(
                    1,
                    "Event ID",
                    ParamRaw(
                        1,
                        Value(
                            1,
                            TypeFamily.UINT,
                            type_size=32,
                            data_path=DataPath(1, param_paths["eventId"]),
                        ),
                    ),
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
                            data_path=DataPath(1, param_paths["tokenId"]),
                        ),
                    ),
                ),
            ],
        ),
    )

    receiver_field = Field(
        1,
        "Receiver",
        ParamRaw(
            1,
            Value(
                1,
                TypeFamily.ADDRESS,
                data_path=DataPath(1, param_paths["receiver"]),
            ),
        ),
    )

    fields = [group_field, receiver_field]

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
        deploy_date=1646305200,
    )

    app_client.provide_transaction_info(tx_info.serialize())

    for field in fields:
        app_client.provide_transaction_field_desc(field.serialize())

    with app_client.sign(mode=SignMode.START_FLOW):
        scenario_navigator.review_approve()


def test_gcs_group_sequential_arrays(scenario_navigator: NavigateWithScenario):
    """Test PARAM_GROUP with SEQUENTIAL iteration over two array sub-fields.

    With SEQUENTIAL, all elements of the first sub-field (_ids) are displayed
    first (Token ID[0], Token ID[1]), then all elements of the second sub-field
    (_values) follow (Amount[0], Amount[1]).

    The future BUNDLED iteration (not yet implemented) would instead interleave
    by index: Token ID[0]+Amount[0], Token ID[1]+Amount[1].
    This test exercises GROUP with dynamic memory allocation for each array element
    in nested sub-fields.
    """
    backend = scenario_navigator.backend
    app_client = EthAppClient(backend)

    with open(f"{ABIS_FOLDER}/erc1155.json", encoding="utf-8") as file:
        contract = Web3().eth.contract(abi=json.load(file), address=None)

    data = contract.encode_abi(
        "safeBatchTransferFrom",
        [
            bytes.fromhex("1111111111111111111111111111111111111111"),
            bytes.fromhex("d8da6bf26964af9d7eed9e03e53415d37aa96045"),
            [1, 2],
            [100, 200],
            b"",
        ],
    )
    tx_params = {
        "nonce": 10,
        "maxFeePerGas": Web3.to_wei(50, "gwei"),
        "maxPriorityFeePerGas": Web3.to_wei(5, "gwei"),
        "gas": 80000,
        # OpenSea Shared Storefront
        "to": bytes.fromhex("495f947276749ce646f68ac8c248420045cb7b5e"),
        "data": data,
        "chainId": 1,
    }

    with app_client.sign("m/44'/60'/0'/0/0", tx_params, mode=SignMode.STORE):
        pass

    param_paths = get_all_paths(f"{ABIS_FOLDER}/erc1155.json", "safeBatchTransferFrom")

    # GROUP(SEQUENTIAL): all Token IDs displayed first, then all Amounts.
    # With BUNDLED (future): interleaved as Token ID[i] + Amount[i] per iteration.
    token_data_group = Field(
        1,
        "Token Data",
        ParamGroup(
            version=1,
            iteration_type=GroupIterationType.SEQUENTIAL,
            fields=[
                Field(
                    1,
                    "Token ID",
                    ParamRaw(
                        1,
                        Value(
                            1,
                            TypeFamily.UINT,
                            type_size=32,
                            data_path=DataPath(1, param_paths["_ids"]),
                        ),
                    ),
                ),
                Field(
                    1,
                    "Amount",
                    ParamRaw(
                        1,
                        Value(
                            1,
                            TypeFamily.UINT,
                            type_size=32,
                            data_path=DataPath(1, param_paths["_values"]),
                        ),
                    ),
                ),
            ],
        ),
    )

    to_field = Field(
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
    )

    fields = [to_field, token_data_group]

    inst_hash = compute_inst_hash(fields)

    tx_info = TxInfo(
        1,
        tx_params["chainId"],
        tx_params["to"],
        get_selector_from_data(tx_params["data"]),
        inst_hash,
        "Batch Transfer",
        creator_name="OpenSea",
        creator_legal_name="OpenSea Inc.",
        creator_url="opensea.io",
        contract_name="ERC1155",
        deploy_date=1646305200,
    )

    app_client.provide_transaction_info(tx_info.serialize())

    for field in fields:
        app_client.provide_transaction_field_desc(field.serialize())

    with app_client.sign(mode=SignMode.START_FLOW):
        scenario_navigator.review_approve()


def test_gcs_separator(scenario_navigator: NavigateWithScenario):
    """Test FIELD-level SEPARATOR tag on an array field.

    A separator `"Token {index}"` is attached to the Token ID field (uint256[]).
    For each element in the array, the device should display the separator label
    (e.g. "Token 1", "Token 2") immediately before the corresponding value.
    """
    backend = scenario_navigator.backend
    app_client = EthAppClient(backend)

    with open(f"{ABIS_FOLDER}/erc1155.json", encoding="utf-8") as file:
        contract = Web3().eth.contract(abi=json.load(file), address=None)

    data = contract.encode_abi(
        "safeBatchTransferFrom",
        [
            bytes.fromhex("1111111111111111111111111111111111111111"),
            bytes.fromhex("d8da6bf26964af9d7eed9e03e53415d37aa96045"),
            [1, 2],
            [100, 200],
            b"",
        ],
    )
    tx_params = {
        "nonce": 11,
        "maxFeePerGas": Web3.to_wei(50, "gwei"),
        "maxPriorityFeePerGas": Web3.to_wei(5, "gwei"),
        "gas": 80000,
        # OpenSea Shared Storefront
        "to": bytes.fromhex("495f947276749ce646f68ac8c248420045cb7b5e"),
        "data": data,
        "chainId": 1,
    }

    with app_client.sign("m/44'/60'/0'/0/0", tx_params, mode=SignMode.STORE):
        pass

    param_paths = get_all_paths(f"{ABIS_FOLDER}/erc1155.json", "safeBatchTransferFrom")

    fields = [
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
        Field(
            1,
            "Token ID",
            ParamRaw(
                1,
                Value(
                    1,
                    TypeFamily.UINT,
                    type_size=32,
                    data_path=DataPath(1, param_paths["_ids"]),
                ),
            ),
            separator="Token {index}",
        ),
    ]

    inst_hash = compute_inst_hash(fields)

    tx_info = TxInfo(
        1,
        tx_params["chainId"],
        tx_params["to"],
        get_selector_from_data(tx_params["data"]),
        inst_hash,
        "Batch Transfer",
        creator_name="OpenSea",
        creator_legal_name="OpenSea Inc.",
        creator_url="opensea.io",
        contract_name="ERC1155",
        deploy_date=1646305200,
    )

    app_client.provide_transaction_info(tx_info.serialize())

    for field in fields:
        app_client.provide_transaction_field_desc(field.serialize())

    with app_client.sign(mode=SignMode.START_FLOW):
        scenario_navigator.review_approve()


@pytest.mark.parametrize(
    "test_config",
    ["chain_id", "network"],
)
def test_gcs_formatter(scenario_navigator: NavigateWithScenario, test_config: str):
    app_client = EthAppClient(scenario_navigator.backend)

    with open(f"{ABIS_FOLDER}/poap.abi.json", encoding="utf-8") as file:
        contract = Web3().eth.contract(abi=json.load(file), address=None)
    data = contract.encode_abi(
        "mintToken",
        [
            175676,
            7163978,
            bytes.fromhex("Dad77910DbDFdE764fC21FCD4E74D71bBACA6D8D"),
            1730621615,
            bytes.fromhex(
                "8991da687cff5300959810a08c4ec183bb2a56dc82f5aac2b24f1106c2d983ac6f7a6b28700a236724d814000d0fd8c395fcf9f87c4424432ebf30c9479201d71c"
            ),
        ],
    )
    tx_params = {
        "nonce": 235,
        "maxFeePerGas": Web3.to_wei(100, "gwei"),
        "maxPriorityFeePerGas": Web3.to_wei(10, "gwei"),
        "gas": 44001,
        # PoapBridge
        "to": bytes.fromhex("0bb4D3e88243F4A057Db77341e6916B0e449b158"),
        "data": data,
        "chainId": 5 if test_config == "network" else 1,
    }

    with app_client.sign("m/44'/60'/0'/0/0", tx_params, mode=SignMode.STORE):
        pass

    param_paths = get_all_paths(f"{ABIS_FOLDER}/poap.abi.json", "mintToken")
    fields = [
        Field(
            1,
            "Token ID",
            ParamRaw(
                1,
                Value(
                    1,
                    TypeFamily.UINT,
                    type_size=32,
                    data_path=DataPath(1, param_paths["tokenId"]),
                ),
            ),
        ),
        Field(
            1,
            "Receiver",
            ParamRaw(
                1,
                Value(
                    1,
                    TypeFamily.ADDRESS,
                    data_path=DataPath(1, param_paths["receiver"]),
                ),
            ),
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
                    data_path=DataPath(1, param_paths["expirationTime"]),
                ),
                DatetimeType.DT_UNIX,
            ),
        ),
    ]
    if test_config == "chain_id":
        fields += [
            Field(
                1,
                "Chain ID",
                ParamRaw(
                    1,
                    Value(
                        1,
                        TypeFamily.UINT,
                        container_path=ContainerPath.CHAIN_ID,
                    ),
                ),
            ),
        ]
    else:
        fields += [
            Field(
                1,
                "Custom Network",
                ParamNetwork(
                    1,
                    Value(
                        1,
                        TypeFamily.UINT,
                        container_path=ContainerPath.CHAIN_ID,
                    ),
                ),
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
        deploy_date=1646305200,
    )

    if test_config == "network":
        # Send Network information (name, ticker, icon)
        name, ticker, icon = get_network_config(scenario_navigator.backend.device.type, tx_params["chainId"])
        if name and ticker:
            app_client.provide_network_information(DynamicNetwork(name, ticker, tx_params["chainId"], icon))

    app_client.provide_transaction_info(tx_info.serialize())

    for field in fields:
        app_client.provide_transaction_field_desc(field.serialize())

    with app_client.sign(mode=SignMode.START_FLOW):
        scenario_navigator.review_approve(test_name=scenario_navigator.test_name + f"_{test_config}")


@pytest.mark.parametrize(
    "test_config, visible, constraints",
    [
        (
            "if_not_0",
            VisibleType.IF_NOT_IN,
            [bytes.fromhex("0000000000000000000000000000000000000000")],
        ),
        (
            "if_not_addr",
            VisibleType.IF_NOT_IN,
            [bytes.fromhex("Dad77910DbDFdE764fC21FCD4E74D71bBACA6D8D")],
        ),
        (
            "must_be_addr",
            VisibleType.MUST_BE,
            [bytes.fromhex("Dad77910DbDFdE764fC21FCD4E74D71bBACA6D8D")],
        ),
        (
            "must_be_0",
            VisibleType.MUST_BE,
            [bytes.fromhex("00"), bytes.fromhex("01"), bytes.fromhex("02")],
        ),
    ],
)
def test_gcs_constraints(
    scenario_navigator: NavigateWithScenario,
    test_config: str,
    visible: VisibleType,
    constraints: list[bytes],
):
    app_client = EthAppClient(scenario_navigator.backend)

    with open(f"{ABIS_FOLDER}/poap.abi.json", encoding="utf-8") as file:
        contract = Web3().eth.contract(abi=json.load(file), address=None)
    data = contract.encode_abi(
        "mintToken",
        [
            175676,
            7163978,
            bytes.fromhex("Dad77910DbDFdE764fC21FCD4E74D71bBACA6D8D"),
            1730621615,
            bytes.fromhex(
                "8991da687cff5300959810a08c4ec183bb2a56dc82f5aac2b24f1106c2d983ac6f7a6b28700a236724d814000d0fd8c395fcf9f87c4424432ebf30c9479201d71c"
            ),
        ],
    )
    tx_params = {
        "nonce": 235,
        "maxFeePerGas": Web3.to_wei(100, "gwei"),
        "maxPriorityFeePerGas": Web3.to_wei(10, "gwei"),
        "gas": 44001,
        # PoapBridge
        "to": bytes.fromhex("0bb4D3e88243F4A057Db77341e6916B0e449b158"),
        "data": data,
        "chainId": 1,
    }

    with app_client.sign("m/44'/60'/0'/0/0", tx_params, mode=SignMode.STORE):
        pass

    param_paths = get_all_paths(f"{ABIS_FOLDER}/poap.abi.json", "mintToken")
    fields = [
        Field(
            1,
            "Token ID",
            ParamRaw(
                1,
                Value(
                    1,
                    TypeFamily.UINT,
                    type_size=32,
                    data_path=DataPath(1, param_paths["tokenId"]),
                ),
            ),
        ),
        Field(
            1,
            "Receiver",
            ParamTrustedName(
                1,
                Value(
                    1,
                    TypeFamily.ADDRESS,
                    data_path=DataPath(1, param_paths["receiver"]),
                ),
                [
                    TrustedNameType.ACCOUNT,
                    TrustedNameType.WALLET,
                ],
                [
                    TrustedNameSource.UD,
                    TrustedNameSource.ENS,
                    TrustedNameSource.FN,
                ],
            ),
            visible,
            constraints,
        ),
        Field(
            1,
            "Receiver uint",
            ParamRaw(
                1,
                Value(
                    1,
                    TypeFamily.UINT,
                    type_size=32,
                    data_path=DataPath(1, param_paths["receiver"]),
                ),
            ),
            visible,
            constraints,
        ),
        Field(
            1,
            "Receiver addr",
            ParamRaw(
                1,
                Value(
                    1,
                    TypeFamily.ADDRESS,
                    type_size=32,
                    data_path=DataPath(1, param_paths["receiver"]),
                ),
            ),
            visible,
            constraints,
        ),
        Field(
            # Same receiver re-rendered as a signed integer so the INT
            # constraint path (MUST_BE / IF_NOT_IN on TF_INT) is actually
            # exercised end-to-end. The data_path returns the full 32-byte
            # calldata chunk, so type_size must be 32 (format_signed_int_be
            # rejects length > type_size). The high byte is the zero
            # padding of the address slot, so the value is a positive
            # int256 and the address-shaped constraints match by canonical
            # decimal-string equality.
            1,
            "Receiver int",
            ParamRaw(
                1,
                Value(
                    1,
                    TypeFamily.INT,
                    type_size=32,
                    data_path=DataPath(1, param_paths["receiver"]),
                ),
            ),
            visible,
            constraints,
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
                    data_path=DataPath(1, param_paths["expirationTime"]),
                ),
                DatetimeType.DT_UNIX,
            ),
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
        deploy_date=1646305200,
    )

    app_client.provide_transaction_info(tx_info.serialize())

    if test_config == "must_be_0":
        with pytest.raises(ExceptionRAPDU) as err:
            for field in fields:
                app_client.provide_transaction_field_desc(field.serialize())
            assert err.value.status == StatusWord.SWO_CONDITIONS_NOT_SATISFIED
    else:
        for field in fields:
            app_client.provide_transaction_field_desc(field.serialize())

        with app_client.sign(mode=SignMode.START_FLOW):
            scenario_navigator.review_approve(test_name=scenario_navigator.test_name + f"_{test_config}")


@pytest.mark.parametrize("test_config", ["named", "raw"])
def test_gcs_interoperable_address(scenario_navigator: NavigateWithScenario, test_config: str):
    app_client = EthAppClient(scenario_navigator.backend)

    # EIP-7930 binary encoding for Ethereum mainnet (chain_id=1):
    # 1 byte chain_id + 20 bytes EVM address = 21 bytes
    interop_chain_id = 1
    interop_addr = bytes.fromhex("Dad77910DbDFdE764fC21FCD4E74D71bBACA6D8D")
    eip7930_bytes = bytes([interop_chain_id]) + interop_addr

    with open(f"{ABIS_FOLDER}/poap.abi.json", encoding="utf-8") as file:
        contract = Web3().eth.contract(abi=json.load(file), address=None)
    data = contract.encode_abi(
        "mintToken",
        [
            175676,
            7163978,
            bytes.fromhex("0000000000000000000000000000000000000000"),
            1730621615,
            eip7930_bytes,  # signature field carries the EIP-7930 interoperable address
        ],
    )
    tx_params = {
        "nonce": 235,
        "maxFeePerGas": Web3.to_wei(100, "gwei"),
        "maxPriorityFeePerGas": Web3.to_wei(10, "gwei"),
        "gas": 44001,
        # PoapBridge
        "to": bytes.fromhex("0bb4D3e88243F4A057Db77341e6916B0e449b158"),
        "data": data,
        "chainId": 1,
    }

    with app_client.sign("m/44'/60'/0'/0/0", tx_params, mode=SignMode.STORE):
        pass

    param_paths = get_all_paths(f"{ABIS_FOLDER}/poap.abi.json", "mintToken")
    fields = [
        Field(
            1,
            "Destination",
            ParamTrustedName(
                1,
                Value(
                    1,
                    TypeFamily.BYTES,
                    data_path=DataPath(1, param_paths["signature"]),
                ),
                [TrustedNameType.ACCOUNT],
                [TrustedNameSource.ENS],
                value_type=TrustedNameValueType.INTEROPERABLE,
            ),
        ),
    ]

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
        deploy_date=1646305200,
    )

    if test_config == "named":
        challenge = ResponseParser.challenge(app_client.get_challenge().data)
        app_client.provide_trusted_name(
            TrustedName(
                2,
                interop_addr,
                "ledger.eth",
                tn_type=TrustedNameType.ACCOUNT,
                tn_source=TrustedNameSource.ENS,
                chain_id=interop_chain_id,
                challenge=challenge,
            )
        )

    app_client.provide_transaction_info(tx_info.serialize())
    for field in fields:
        app_client.provide_transaction_field_desc(field.serialize())

    with app_client.sign(mode=SignMode.START_FLOW):
        scenario_navigator.review_approve(test_name=scenario_navigator.test_name + f"_{test_config}")


def test_gcs_iteration_broadcast(scenario_navigator: NavigateWithScenario):
    """Test §3.1.8 iteration broadcast: secondary array of size 1 is repeated across
    all iterations of the primary array.

    batchTransferSameToken(address token, address[] recipients, uint256[] amounts)
    is called with 1 token address but 2 amounts.  The ParamTokenAmount field
    descriptor has value→amounts (size 2) and token→token (size 1).  The device
    must broadcast the single token entry and display both amounts labelled with
    the USDC ticker, rather than rejecting the size mismatch.
    """
    backend = scenario_navigator.backend
    app_client = EthAppClient(backend)

    usdc_address = bytes.fromhex("A0b86991c6218b36c1d19D4a2e9Eb0cE3606eB48")
    recipient0 = bytes.fromhex("1111111111111111111111111111111111111111")
    recipient1 = bytes.fromhex("2222222222222222222222222222222222222222")
    usdc_decimals = 6
    amount0 = int(100 * 10**usdc_decimals)  # 100 USDC
    amount1 = int(250 * 10**usdc_decimals)  # 250 USDC

    with open(f"{ABIS_FOLDER}/broadcast.json", encoding="utf-8") as f:
        contract = Web3().eth.contract(abi=json.load(f), address=None)

    data = contract.encode_abi(
        "batchTransferSameToken",
        [
            usdc_address,
            [recipient0, recipient1],
            [amount0, amount1],
        ],
    )

    tx_params = {
        "nonce": 1,
        "maxFeePerGas": Web3.to_wei(20, "gwei"),
        "maxPriorityFeePerGas": Web3.to_wei(2, "gwei"),
        "gas": 60000,
        "to": bytes.fromhex("deadbeefdeadbeefdeadbeefdeadbeefdeadbeef"),
        "data": data,
        "chainId": 1,
    }

    with app_client.sign("m/44'/60'/0'/0/0", tx_params, mode=SignMode.STORE):
        pass

    param_paths = get_all_paths(f"{ABIS_FOLDER}/broadcast.json", "batchTransferSameToken")

    # value → amounts[] (2 elements), token → token (1 element — broadcast)
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
                    data_path=DataPath(1, param_paths["amounts"]),
                ),
                token=Value(
                    1,
                    TypeFamily.ADDRESS,
                    data_path=DataPath(1, param_paths["token"]),
                ),
            ),
        ),
    ]

    inst_hash = compute_inst_hash(fields)

    tx_info = TxInfo(
        1,
        tx_params["chainId"],
        tx_params["to"],
        get_selector_from_data(tx_params["data"]),
        inst_hash,
        "Batch transfer same token",
        creator_name="Acme",
        creator_legal_name="Acme Protocol Inc.",
        creator_url="acme.finance",
        contract_name="Batch Distributor",
        deploy_date=1704067200,
    )

    app_client.provide_transaction_info(tx_info.serialize())
    app_client.provide_token_metadata("USDC", usdc_address, usdc_decimals, tx_params["chainId"])

    for field in fields:
        app_client.provide_transaction_field_desc(field.serialize())

    with app_client.sign(mode=SignMode.START_FLOW):
        scenario_navigator.review_approve()


def test_gcs_raw_bytes_oversize_rejected(scenario_navigator: NavigateWithScenario):
    """A PARAM_TYPE_RAW + TypeFamily.BYTES field whose value exceeds the
    on-device hex-display buffer (~188 bytes of raw payload) must be refused
    at field-description time instead of rendered truncated. Without that
    rejection the user would sign a hash computed over the full payload while
    only a prefix is shown."""
    app_client = EthAppClient(scenario_navigator.backend)

    # mintToken accepts a free-form `bytes signature` parameter; we abuse it
    # as the oversize bytes carrier. 200 raw bytes => "0x" + 400 hex chars +
    # NUL = 403 chars, well above the 380-byte display buffer.
    oversize_signature = bytes(range(200))

    with open(f"{ABIS_FOLDER}/poap.abi.json", encoding="utf-8") as file:
        contract = Web3().eth.contract(abi=json.load(file), address=None)
    data = contract.encode_abi(
        "mintToken",
        [
            175676,
            7163978,
            bytes.fromhex("Dad77910DbDFdE764fC21FCD4E74D71bBACA6D8D"),
            1730621615,
            oversize_signature,
        ],
    )
    tx_params = {
        "nonce": 235,
        "maxFeePerGas": Web3.to_wei(100, "gwei"),
        "maxPriorityFeePerGas": Web3.to_wei(10, "gwei"),
        "gas": 44001,
        "to": bytes.fromhex("0bb4D3e88243F4A057Db77341e6916B0e449b158"),
        "data": data,
        "chainId": 1,
    }

    with app_client.sign("m/44'/60'/0'/0/0", tx_params, mode=SignMode.STORE):
        pass

    param_paths = get_all_paths(f"{ABIS_FOLDER}/poap.abi.json", "mintToken")
    fields = [
        Field(
            1,
            "Signature",
            ParamRaw(
                1,
                Value(
                    1,
                    TypeFamily.BYTES,
                    data_path=DataPath(1, param_paths["signature"]),
                ),
            ),
        ),
    ]

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
        deploy_date=1646305200,
    )

    app_client.provide_transaction_info(tx_info.serialize())

    # The single oversize Field must be rejected when its description APDU
    # reaches format_field() -> format_bytes(), which now returns false
    # rather than truncating the hex string.
    with pytest.raises(ExceptionRAPDU) as err:
        for field in fields:
            app_client.provide_transaction_field_desc(field.serialize())
    assert err.value.status == StatusWord.SWO_INCORRECT_DATA


def test_gcs_raw_string_oversize_rejected(scenario_navigator: NavigateWithScenario):
    """A PARAM_TYPE_RAW + TypeFamily.STRING field whose value fills the on-device
    display buffer (380 bytes) must be refused — no room for the NUL terminator —
    instead of silently truncated. Without this rejection the user would sign a
    hash computed over the full value while only a prefix is shown on screen."""
    app_client = EthAppClient(scenario_navigator.backend)

    # 380 bytes → length + 1 = 381 > 380-byte display buffer, triggers rejection.
    oversize_string = bytes(range(256)) + bytes(range(124))

    with open(f"{ABIS_FOLDER}/poap.abi.json", encoding="utf-8") as file:
        contract = Web3().eth.contract(abi=json.load(file), address=None)
    data = contract.encode_abi(
        "mintToken",
        [
            175676,
            7163978,
            bytes.fromhex("Dad77910DbDFdE764fC21FCD4E74D71bBACA6D8D"),
            1730621615,
            oversize_string,
        ],
    )
    tx_params = {
        "nonce": 235,
        "maxFeePerGas": Web3.to_wei(100, "gwei"),
        "maxPriorityFeePerGas": Web3.to_wei(10, "gwei"),
        "gas": 44001,
        "to": bytes.fromhex("0bb4D3e88243F4A057Db77341e6916B0e449b158"),
        "data": data,
        "chainId": 1,
    }

    with app_client.sign("m/44'/60'/0'/0/0", tx_params, mode=SignMode.STORE):
        pass

    param_paths = get_all_paths(f"{ABIS_FOLDER}/poap.abi.json", "mintToken")
    fields = [
        Field(
            1,
            "Memo",
            ParamRaw(
                1,
                Value(
                    1,
                    TypeFamily.STRING,
                    data_path=DataPath(1, param_paths["signature"]),
                ),
            ),
        ),
    ]

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
        deploy_date=1646305200,
    )

    app_client.provide_transaction_info(tx_info.serialize())

    with pytest.raises(ExceptionRAPDU) as err:
        for field in fields:
            app_client.provide_transaction_field_desc(field.serialize())
    assert err.value.status == StatusWord.SWO_INCORRECT_DATA


def test_gcs_raw_string_embedded_nul_rejected(scenario_navigator: NavigateWithScenario):
    """A PARAM_TYPE_RAW + TypeFamily.STRING field containing an embedded NUL byte
    must be refused. An embedded NUL would silently truncate the on-screen display
    at the NUL while the full byte sequence remains in the instruction hash,
    allowing a host to hide arbitrary trailing content from the review screen."""
    app_client = EthAppClient(scenario_navigator.backend)

    # b"hello\x00world": the NUL at index 5 would hide " world" from the screen.
    nul_string = b"hello\x00world"

    with open(f"{ABIS_FOLDER}/poap.abi.json", encoding="utf-8") as file:
        contract = Web3().eth.contract(abi=json.load(file), address=None)
    data = contract.encode_abi(
        "mintToken",
        [
            175676,
            7163978,
            bytes.fromhex("Dad77910DbDFdE764fC21FCD4E74D71bBACA6D8D"),
            1730621615,
            nul_string,
        ],
    )
    tx_params = {
        "nonce": 235,
        "maxFeePerGas": Web3.to_wei(100, "gwei"),
        "maxPriorityFeePerGas": Web3.to_wei(10, "gwei"),
        "gas": 44001,
        "to": bytes.fromhex("0bb4D3e88243F4A057Db77341e6916B0e449b158"),
        "data": data,
        "chainId": 1,
    }

    with app_client.sign("m/44'/60'/0'/0/0", tx_params, mode=SignMode.STORE):
        pass

    param_paths = get_all_paths(f"{ABIS_FOLDER}/poap.abi.json", "mintToken")
    fields = [
        Field(
            1,
            "Memo",
            ParamRaw(
                1,
                Value(
                    1,
                    TypeFamily.STRING,
                    data_path=DataPath(1, param_paths["signature"]),
                ),
            ),
        ),
    ]

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
        deploy_date=1646305200,
    )

    app_client.provide_transaction_info(tx_info.serialize())

    with pytest.raises(ExceptionRAPDU) as err:
        for field in fields:
            app_client.provide_transaction_field_desc(field.serialize())
    assert err.value.status == StatusWord.SWO_INCORRECT_DATA
