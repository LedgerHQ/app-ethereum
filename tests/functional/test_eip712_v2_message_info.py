"""Functional tests of EIP712_MESSAGE_INFO, the signed description of an EIP-712 V2 message.

The struct is the TRANSACTION_INFO one, received through the same instruction with P2 = 02. It is
accepted once the values are complete and before the review starts, and bound to the message: its
domain separator and its primary type hash must be the message's, which covers the chain and the
contract of the domain. It changes nothing to the review yet, which stays a raw one behind the blind
signing warning.

Every refusal resets the signing flow, so each is followed by a valid message going through.
"""

import copy
import hashlib
from typing import Any

import client.response_parser as ResponseParser
import pytest
from client.client import EthAppClient
from client.command_builder import InsType, P2Type
from client.eip712 import InputData, json_to_tlv
from client.eip712.json_to_tlv import STRUCT_VERSION
from client.gcs import Eip712MessageInfo, TxInfo, TxInfoTag
from client.signing_partners import CALLDATA_PARTNER
from client.status_word import StatusWord
from eth_account import Account
from eth_account._utils.encode_typed_data.encoding_and_hashing import hash_struct, hash_type
from eth_utils import keccak
from ragger.error import ExceptionRAPDU
from ragger.navigator import NavigateWithScenario
from test_eip712_v2 import (
    BIP32_PATH,
    CLA,
    UNKNOWN_TAG,
    assert_recovers,
    enable_blind_signing,
    expect_status,
    get_wallet_addr,
    minimal_data,
    tlv,
)

CONTRACT = "0x" + "ab" * 20
OTHER_CONTRACT = "0x" + "cd" * 20
# no field descriptor: what a message info announcing none carries
NO_FIELDS_HASH = hashlib.sha3_256(b"").digest()
SOME_FIELDS_HASH = hashlib.sha3_256(b"a descriptor").digest()
P1_UNKNOWN = 0x02


def message(chain_id: int | str | bool | None = 1, verifying_contract: str | None = None, chain_type: str = "uint256") -> dict:
    """A message whose domain carries the chain and the contract asked for, if any."""
    domain_type = [{"name": "name", "type": "string"}]
    domain: dict[str, object] = {"name": "Test"}
    if chain_id is not None:
        domain_type.append({"name": "chainId", "type": chain_type})
        domain["chainId"] = chain_id
    if verifying_contract is not None:
        domain_type.append({"name": "verifyingContract", "type": "address"})
        domain["verifyingContract"] = verifying_contract
    data = minimal_data()
    data["types"]["EIP712Domain"] = domain_type
    data["domain"] = domain
    return data


def ethermint(verifying_contract: str = "cosmos") -> dict:
    """A message whose domain names its contract with a string, as the Ethermint chains do."""
    data = message(1)
    data["types"]["EIP712Domain"].append({"name": "verifyingContract", "type": "string"})
    data["domain"]["verifyingContract"] = verifying_contract
    return data


def type_hash(data: dict, primary_type: str | None = None) -> bytes:
    """The reference keccak256(encodeType(primaryType)), independent from the app's own."""
    return hash_type(primary_type or data["primaryType"], data["types"])


def domain_separator(data: dict) -> bytes:
    """The reference hashStruct(domain), with the types the message declares."""
    return hash_struct("EIP712Domain", data["types"], data["domain"])


def info_for(data: dict, **overrides) -> Eip712MessageInfo:
    arguments: dict[str, Any] = {
        "version": STRUCT_VERSION,
        "domain_separator": domain_separator(data),
        "primary_type_hash": type_hash(data),
        "fields_hash": NO_FIELDS_HASH,
        "operation_type": "Send mail",
    } | overrides
    return Eip712MessageInfo(**arguments)


def signed(*entries: tuple[int, int | str | bytes]) -> bytes:
    """A struct made of the given tags only, correctly signed: for the ones the class would not build."""
    payload = tlv(*entries)
    return payload + tlv((TxInfoTag.SIGNATURE, CALLDATA_PARTNER.sign(payload)))


def required(data: dict) -> dict[str, tuple[int, int | str | bytes]]:
    return {
        "version": (TxInfoTag.VERSION, STRUCT_VERSION),
        "domain_separator": (TxInfoTag.DOMAIN_SEPARATOR, domain_separator(data)),
        "type_hash": (TxInfoTag.PRIMARY_TYPE_HASH, type_hash(data)),
        "fields_hash": (TxInfoTag.FIELDS_HASH, NO_FIELDS_HASH),
        "operation_type": (TxInfoTag.OPERATION_TYPE, "Send mail"),
    }


def sign_and_approve(scenario_navigator: NavigateWithScenario, app_client: EthAppClient, data: dict):
    """Approve the review, unchanged by the message info, and return the (v, r, s) signature."""
    with app_client.eip712_v2_sign():
        scenario_navigator.review_approve_with_warning(do_comparison=False, nb_warnings=1)
    return ResponseParser.signature(app_client.response().data)


def recover_declared_types(data: dict, vrs: tuple[int, int, int]) -> bytes:
    """The signer of a message, from hashes that eth_account computes with the types the message declares.

    `recover_message` hashes the domain with the standard type of each field (a uint256 chainId),
    which is not the hash of a domain that declares another one.
    """
    types = data["types"]
    digest = keccak(
        b"\x19\x01"
        + hash_struct("EIP712Domain", types, data["domain"])
        + hash_struct(data["primaryType"], types, data["message"])
    )
    return bytes.fromhex(Account._recover_hash(digest, vrs=vrs)[2:])


def check_signed(scenario_navigator: NavigateWithScenario, app_client: EthAppClient, data: dict) -> None:
    vrs = sign_and_approve(scenario_navigator, app_client, data)
    assert get_wallet_addr(app_client) == recover_declared_types(data, vrs)


# --- Accepted ----------------------------------------------------------------------------------

ACCEPTED = {
    # the chain and the contract are the ones of the domain, which the domain separator covers
    "chain_and_contract": (message(1, CONTRACT), {}),
    "chain_only": (message(137), {}),
    "contract_only": (message(None, CONTRACT), {}),
    "domain_with_neither": (message(None), {}),
    # the standard types chainId as uint256, and any size of unsigned integer is a chain
    "chain_of_uint8": (message(10, chain_type="uint8"), {}),
    "chain_of_uint64": (message(137, chain_type="uint64"), {}),
    # a chainId that is not an unsigned integer is no chain: the message has none
    "chain_of_text": (message("1", chain_type="string"), {}),
    # the Ethermint chains name no contract, with a placeholder that is accepted for one
    "ethermint_placeholder": (ethermint(), {}),
    "metadata": (
        message(1),
        {
            "creator_name": "Ledger",
            "creator_legal_name": "Ledger SAS",
            "creator_url": "ledger.com",
            "contract_name": "Mailbox",
            "deploy_date": 1700000000,
        },
    ),
}


@pytest.mark.parametrize("data, overrides", ACCEPTED.values(), ids=ACCEPTED.keys())
def test_eip712_v2_message_info_accepted(scenario_navigator: NavigateWithScenario, data: dict, overrides: dict):
    """A message info matching its message is accepted, and the message is signed as before."""
    enable_blind_signing(scenario_navigator)
    app_client = EthAppClient(scenario_navigator.backend)

    json_to_tlv.process_data(app_client, data, BIP32_PATH)
    response = app_client.eip712_v2_send_message_info(info_for(data, **overrides).serialize())
    assert response.status == StatusWord.SWO_SUCCESS
    check_signed(scenario_navigator, app_client, data)


def test_eip712_v2_message_info_is_optional(scenario_navigator: NavigateWithScenario):
    """A message without a message info is signed as before."""
    enable_blind_signing(scenario_navigator)
    app_client = EthAppClient(scenario_navigator.backend)
    data = message(1)

    json_to_tlv.process_data(app_client, data, BIP32_PATH)
    check_signed(scenario_navigator, app_client, data)


def test_eip712_v2_message_info_announcing_descriptors_is_kept(scenario_navigator: NavigateWithScenario):
    """A message info announcing descriptors is accepted, and signing needs them to have come."""
    app_client = EthAppClient(scenario_navigator.backend)
    data = message(1)

    json_to_tlv.process_data(app_client, data, BIP32_PATH)
    response = app_client.eip712_v2_send_message_info(info_for(data, fields_hash=SOME_FIELDS_HASH).serialize())
    assert response.status == StatusWord.SWO_SUCCESS

    def sign():
        with app_client.eip712_v2_sign():
            pass

    expect_status(StatusWord.SWO_INCORRECT_DATA, sign)
    assert_recovers(app_client)


def test_eip712_v2_message_info_multiple_chunks(scenario_navigator: NavigateWithScenario):
    """A struct longer than one APDU, with every text at its length limit, is received in chunks."""
    enable_blind_signing(scenario_navigator)
    app_client = EthAppClient(scenario_navigator.backend)
    data = message(1, CONTRACT)
    info = info_for(
        data,
        operation_type="O" * 30,
        creator_name="N" * 22,
        creator_legal_name="L" * 30,
        creator_url="U" * 26,
        contract_name="C" * 30,
        deploy_date=1700000000,
    )
    payload = info.serialize()
    assert len(payload) > 255

    json_to_tlv.process_data(app_client, data, BIP32_PATH)
    assert app_client.eip712_v2_send_message_info(payload).status == StatusWord.SWO_SUCCESS
    check_signed(scenario_navigator, app_client, data)


# --- Refused: the struct ------------------------------------------------------------------------


def rejected(data: dict, **overrides) -> tuple[dict, bytes]:
    """A message, and a message info for it that the app must refuse."""
    return data, info_for(data, **overrides).serialize()


def without(data: dict, *names: str) -> tuple[dict, bytes]:
    """A correctly signed message info that lacks the required tags named."""
    return data, signed(*(tag for name, tag in required(data).items() if name not in names))


DATA = message(1)
DATA_WITHOUT_CHAIN = message(None)
DATA_WITH_CONTRACT = message(1, CONTRACT)


def other_domain(data: dict, **fields: str) -> dict:
    """The message, with a domain of another name or with more fields: of the same chain and contract."""
    other = copy.deepcopy(data)
    for name, value in fields.items():
        if name not in other["domain"]:
            other["types"]["EIP712Domain"].append({"name": name, "type": "string"})
        other["domain"][name] = value
    return other


INVALID_INFOS: dict[str, tuple[dict, bytes]] = {
    "other_primary_type": rejected(DATA, primary_type_hash=type_hash(DATA, "EIP712Domain")),
    # the domain separator covers every field of the domain and its type, its chain and its contract
    # included
    "other_domain_name": rejected(DATA, domain_separator=domain_separator(other_domain(DATA, name="Other"))),
    "other_domain_fields": rejected(DATA, domain_separator=domain_separator(other_domain(DATA, version="1"))),
    "other_chain": rejected(DATA, domain_separator=domain_separator(message(137))),
    "domain_without_chain": rejected(DATA, domain_separator=domain_separator(DATA_WITHOUT_CHAIN)),
    "other_contract": rejected(DATA_WITH_CONTRACT, domain_separator=domain_separator(message(1, OTHER_CONTRACT))),
    "domain_without_contract": rejected(DATA_WITH_CONTRACT, domain_separator=domain_separator(DATA)),
    "domain_separator_one_byte_short": rejected(DATA, domain_separator=domain_separator(DATA)[:-1]),
    # the chain and the contract are the ones of the domain: a message info has no tag for them
    "a_chain": (DATA, signed(*required(DATA).values(), (TxInfoTag.CHAIN_ID, 1))),
    "a_contract": (
        DATA_WITH_CONTRACT,
        signed(*required(DATA_WITH_CONTRACT).values(), (TxInfoTag.CONTRACT_ADDR, bytes.fromhex(CONTRACT[2:]))),
    ),
    "bad_signature": rejected(DATA, signature=bytes.fromhex("3006020101020101")),
    "unsupported_version": rejected(DATA, version=2),
    "version_zero": rejected(DATA, version=0),
    # a selector-sized type hash would be zero-padded if the size were not exact
    "selector_sized_type_hash": rejected(DATA, primary_type_hash=bytes.fromhex("deadbeef")),
    "type_hash_one_byte_short": rejected(DATA, primary_type_hash=type_hash(DATA)[:-1]),
    "type_hash_one_byte_long": rejected(DATA, primary_type_hash=type_hash(DATA) + b"\x00"),
    "transaction_info": (
        DATA,
        TxInfo(STRUCT_VERSION, 1, bytes(20), bytes.fromhex("deadbeef"), NO_FIELDS_HASH, "Send mail").serialize(),
    ),
    # the selector identifies a transaction, never a message
    "a_selector": (DATA, signed(*required(DATA).values(), (TxInfoTag.SELECTOR, bytes.fromhex("deadbeef")))),
    "no_domain_separator": without(DATA, "domain_separator"),
    "no_type_hash": without(DATA, "type_hash"),
    "no_fields_hash": without(DATA, "fields_hash"),
    "no_operation_type": without(DATA, "operation_type"),
    "no_version": without(DATA, "version"),
    "type_hash_twice": (DATA, signed(*required(DATA).values(), required(DATA)["type_hash"])),
    "unknown_tag": (DATA, signed(*required(DATA).values(), (UNKNOWN_TAG, 1))),
    "no_signature": (DATA, tlv(*required(DATA).values())),
}


@pytest.mark.parametrize("data, payload", INVALID_INFOS.values(), ids=INVALID_INFOS.keys())
def test_eip712_v2_message_info_rejected(scenario_navigator: NavigateWithScenario, data: dict, payload: bytes):
    """A message info that is malformed, mismatching the message, or badly signed is refused."""
    app_client = EthAppClient(scenario_navigator.backend)

    json_to_tlv.process_data(app_client, data, BIP32_PATH)
    expect_status(StatusWord.SWO_INCORRECT_DATA, lambda: app_client.eip712_v2_send_message_info(payload))
    assert_recovers(app_client)


# --- Refused: the command -----------------------------------------------------------------------


def test_eip712_v2_message_info_unknown_p1(scenario_navigator: NavigateWithScenario):
    """P1 is a chunk flag, anything but 00 and 01 is refused."""
    app_client = EthAppClient(scenario_navigator.backend)

    json_to_tlv.process_data(app_client, message(1), BIP32_PATH)
    expect_status(
        StatusWord.SWO_INCORRECT_P1_P2,
        lambda: app_client.send_raw(CLA, InsType.PROVIDE_TRANSACTION_INFO, P1_UNKNOWN, P2Type.EIP712_V2_IMPLEM, b""),
    )
    assert_recovers(app_client)


@pytest.mark.parametrize("p2", [0x01, 0x03, 0x0F, 0xFF])
def test_eip712_v2_transaction_info_other_p2(scenario_navigator: NavigateWithScenario, p2: int):
    """TRANSACTION INFO is P2 = 00 for a transaction and P2 = 02 for a message: nothing else."""
    app_client = EthAppClient(scenario_navigator.backend)

    expect_status(
        StatusWord.SWO_INCORRECT_P1_P2,
        lambda: app_client.send_raw(CLA, InsType.PROVIDE_TRANSACTION_INFO, 0x01, p2, b""),
    )


def test_eip712_v2_message_info_without_message(scenario_navigator: NavigateWithScenario):
    """Nothing is bound to before the values: not even a schema is enough."""
    app_client = EthAppClient(scenario_navigator.backend)
    data = message(1)
    payload = info_for(data).serialize()

    expect_status(StatusWord.SWO_COMMAND_NOT_ALLOWED, lambda: app_client.eip712_v2_send_message_info(payload))

    response = app_client.eip712_v2_send_schema(json_to_tlv.schema_from_json(data).serialize())
    assert response.status == StatusWord.SWO_SUCCESS
    expect_status(StatusWord.SWO_COMMAND_NOT_ALLOWED, lambda: app_client.eip712_v2_send_message_info(payload))
    assert_recovers(app_client)


def test_eip712_v2_message_info_twice(scenario_navigator: NavigateWithScenario):
    """A message has one message info."""
    app_client = EthAppClient(scenario_navigator.backend)
    data = message(1)
    payload = info_for(data).serialize()

    json_to_tlv.process_data(app_client, data, BIP32_PATH)
    assert app_client.eip712_v2_send_message_info(payload).status == StatusWord.SWO_SUCCESS
    expect_status(StatusWord.SWO_COMMAND_NOT_ALLOWED, lambda: app_client.eip712_v2_send_message_info(payload))
    assert_recovers(app_client)


def test_eip712_v2_message_info_does_not_outlive_the_flow(scenario_navigator: NavigateWithScenario):
    """The message info of a flow is not the one of the next: a mismatching one must not stay."""
    enable_blind_signing(scenario_navigator)
    app_client = EthAppClient(scenario_navigator.backend)
    first = message(1)
    second = message(137)

    json_to_tlv.process_data(app_client, first, BIP32_PATH)
    assert app_client.eip712_v2_send_message_info(info_for(first).serialize()).status == StatusWord.SWO_SUCCESS
    check_signed(scenario_navigator, app_client, first)

    # a new flow may have its own, and is not satisfied by the previous one
    json_to_tlv.process_data(app_client, second, BIP32_PATH)
    assert app_client.eip712_v2_send_message_info(info_for(second).serialize()).status == StatusWord.SWO_SUCCESS
    check_signed(scenario_navigator, app_client, second)


# --- V1 and V2 never share a session -------------------------------------------------------------
#
# These would be refused for lack of a message if the V1 stream were not complete, so the V1 data
# is sent whole: what refuses them is the session, not the state of the typed data.


def test_eip712_v2_message_info_in_v1_session(scenario_navigator: NavigateWithScenario):
    """A V1 message is not one a V2 message info can be bound to."""
    app_client = EthAppClient(scenario_navigator.backend)
    data = message(1)

    InputData.process_data(app_client, data)
    expect_status(
        StatusWord.SWO_COMMAND_NOT_ALLOWED,
        lambda: app_client.eip712_v2_send_message_info(info_for(data).serialize()),
    )
    assert_recovers(app_client)


def test_eip712_v2_sign_in_v1_session(scenario_navigator: NavigateWithScenario):
    """The V2 sign does not sign a V1 session's message."""
    app_client = EthAppClient(scenario_navigator.backend)

    InputData.process_data(app_client, message(1))
    with pytest.raises(ExceptionRAPDU) as e:
        with app_client.eip712_v2_sign():
            pass
    assert e.value.status == StatusWord.SWO_COMMAND_NOT_ALLOWED
    assert_recovers(app_client)


def send_v1_struct_definitions(app_client: EthAppClient, data: dict) -> None:
    """Open a V1 session and define the types of a message in it, without any value."""
    InputData.app_client = app_client
    for name, fields in data["types"].items():
        with app_client.eip712_send_struct_def_struct_name(name):
            pass
        for field in fields:
            InputData.send_struct_def_field(field["type"], field["name"])


def test_eip712_v2_schema_in_v1_session(scenario_navigator: NavigateWithScenario):
    """A V2 schema does not replace the types a V1 session has defined."""
    app_client = EthAppClient(scenario_navigator.backend)
    data = message(1)

    send_v1_struct_definitions(app_client, data)
    expect_status(
        StatusWord.SWO_COMMAND_NOT_ALLOWED,
        lambda: app_client.eip712_v2_send_schema(json_to_tlv.schema_from_json(data).serialize()),
    )
    assert_recovers(app_client)


def test_eip712_v2_values_in_v1_session(scenario_navigator: NavigateWithScenario):
    """V2 values do not give a value to the types a V1 session has defined."""
    app_client = EthAppClient(scenario_navigator.backend)
    data = message(1)

    send_v1_struct_definitions(app_client, data)
    expect_status(
        StatusWord.SWO_COMMAND_NOT_ALLOWED,
        lambda: app_client.eip712_v2_send_values(json_to_tlv.values_from_json(data, BIP32_PATH).serialize()),
    )
    assert_recovers(app_client)
