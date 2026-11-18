"""Functional tests of the FIELD descriptors of an EIP-712 V2 message.

After its EIP712_MESSAGE_INFO announces descriptors, the host sends them with INS 0x28 and P2 = 02.
They are the descriptors of a transaction, with their values read in the message through an
EIP712_PATH, and the message info's FIELDS_HASH attests them. A message whose descriptors are all
there gets a clear-signing review: no blind signing warning, and the blind signing setting does not
matter.

Each kind of review is compared with snapshots once. The other tests check that the descriptors are
accepted or refused, and that the approved review gives the signature of the message.

Every refusal resets the signing flow, so each is followed by a valid message going through.
"""

import hashlib
import json
from collections.abc import Callable
from typing import Any

import client.response_parser as ResponseParser
import pytest
import web3
from client.client import EthAppClient
from client.command_builder import InsType, P2Type
from client.eip712 import InputData, json_to_tlv
from client.enum_value import EnumValue
from client.gcs import (
    ContainerPath,
    DataPath,
    DatetimeType,
    Eip712Path,
    Eip712PathArray,
    Eip712PathField,
    Eip712PathLeafSlice,
    Field,
    GroupIterationType,
    MapRef,
    ParamAmount,
    ParamCalldata,
    ParamDatetime,
    ParamDuration,
    ParamEnum,
    ParamGroup,
    ParamNetwork,
    ParamRaw,
    ParamTokenAmount,
    ParamUnit,
    TxInfo,
    TypeFamily,
    Value,
    VisibleType,
    fields_hash,
)
from client.map_entry import MapEntry
from client.settings import SettingID, settings_toggle
from client.status_word import StatusWord
from client.utils import get_selector_from_data, recover_message
from constants import ABIS_FOLDER
from fields_utils import RawEip712PathArray, RawEip712PathLeafSlice, RawValue, get_all_paths
from ragger.navigator import NavigateWithScenario
from test_eip712_v2 import (
    BIP32_PATH,
    assert_recovers,
    enable_blind_signing,
    expect_status,
    get_wallet_addr,
)
from test_eip712_v2_message_info import NO_FIELDS_HASH, domain_separator, info_for, message, type_hash

CONTRACT = "0x" + "ab" * 20
NATIVE_CURRENCY = bytes.fromhex("EeeeeEeeeEeEeeEeEeEeeEEEeeeeEeeeeeEEEeeE")

# --- The message ---------------------------------------------------------------------------------

# Test { amount, label, ids, blob, deadline, period, recipient, mode, flag, item }
AMOUNT, LABEL, IDS, BLOB, DEADLINE, PERIOD, RECIPIENT, MODE, FLAG, ITEM = range(10)
# Item { price, token }
PRICE, TOKEN = range(2)

THREE_ETH = 3 * 10**18


def order(chain_id: int | None = 1, contract: str | None = CONTRACT, **values) -> dict:
    """A message with a domain carrying the chain and a contract, and a Test of every kind of value."""
    data = message(chain_id, contract)
    data["types"]["Item"] = [{"name": "price", "type": "uint256"}, {"name": "token", "type": "address"}]
    data["types"]["Test"] = [
        {"name": "amount", "type": "uint256"},
        {"name": "label", "type": "string"},
        {"name": "ids", "type": "uint8[]"},
        {"name": "blob", "type": "bytes"},
        {"name": "deadline", "type": "uint256"},
        {"name": "period", "type": "uint32"},
        {"name": "recipient", "type": "address"},
        {"name": "mode", "type": "uint8"},
        {"name": "flag", "type": "bool"},
        {"name": "item", "type": "Item"},
    ]
    data["message"] = {
        "amount": THREE_ETH,
        "label": "hello",
        "ids": [1, 2, 3],
        "blob": "0x0102030405",
        "deadline": 1700000000,
        "period": 3661,
        "recipient": "0x" + "11" * 20,
        "mode": 1,
        "flag": True,
        "item": {"price": 5, "token": "0x" + "22" * 20},
    } | values
    return data


# --- Descriptors ---------------------------------------------------------------------------------


def path(*steps) -> Eip712Path:
    return Eip712Path(1, list(steps))


def value(family: TypeFamily, size: int | None, *steps, **kwargs) -> Value:
    return Value(1, family, size, eip712_path=path(*steps), **kwargs)


def raw(name: str, family: TypeFamily, size: int | None, *steps, **kwargs) -> Field:
    return Field(1, name, ParamRaw(1, value(family, size, *steps)), **kwargs)


def container(name: str, family: TypeFamily, size: int | None, container_path: ContainerPath) -> Field:
    return Field(1, name, ParamRaw(1, Value(1, family, size, container_path=container_path)))


def field_at(index: int) -> Eip712PathField:
    return Eip712PathField(index)


def all_of() -> Eip712PathArray:
    return Eip712PathArray()


# --- The flow ------------------------------------------------------------------------------------


def start(app_client: EthAppClient, data: dict, fields: list[Field], **overrides) -> None:
    """Deliver the message, and its message info announcing the descriptors."""
    json_to_tlv.process_data(app_client, data, BIP32_PATH)
    info = info_for(data, fields_hash=fields_hash(fields), **overrides)
    assert app_client.eip712_v2_send_message_info(info.serialize()).status == StatusWord.SWO_SUCCESS


def describe(app_client: EthAppClient, fields: list[Field]) -> None:
    for field in fields:
        assert app_client.eip712_v2_send_field_desc(field.serialize()).status == StatusWord.SWO_SUCCESS


def prepare(app_client: EthAppClient, data: dict, fields: list[Field], **overrides) -> None:
    start(app_client, data, fields, **overrides)
    describe(app_client, fields)


def sign(scenario_navigator: NavigateWithScenario, app_client: EthAppClient, data: dict) -> None:
    """Approve the review, and check the signature."""
    with app_client.eip712_v2_sign():
        scenario_navigator.review_approve(do_comparison=False)
    vrs = ResponseParser.signature(app_client.response().data)
    assert get_wallet_addr(app_client) == recover_message(data, vrs)


def sign_compared(
    scenario_navigator: NavigateWithScenario, app_client: EthAppClient, data: dict, snapshots_dirname: str | None = None
) -> None:
    """Approve the review against its snapshots, and check the signature."""
    with app_client.eip712_v2_sign():
        scenario_navigator.review_approve(test_name=snapshots_dirname)
    vrs = ResponseParser.signature(app_client.response().data)
    assert get_wallet_addr(app_client) == recover_message(data, vrs)


def sign_attempt(app_client: EthAppClient) -> None:
    with app_client.eip712_v2_sign():
        pass


# --- Reviews that are compared -----------------------------------------------------------------


def described_fields() -> list[Field]:
    return [
        raw("Label", TypeFamily.STRING, None, field_at(LABEL)),
        Field(1, "Amount", ParamAmount(1, value(TypeFamily.UINT, 32, field_at(AMOUNT)))),
        raw("Id", TypeFamily.UINT, 1, field_at(IDS), all_of()),
        raw("Tail", TypeFamily.BYTES, None, field_at(BLOB), Eip712PathLeafSlice(1, 3)),
        container("Contract", TypeFamily.ADDRESS, 20, ContainerPath.TO),
    ]


def test_eip712_v2_fields_review(scenario_navigator: NavigateWithScenario):
    """Verified descriptors give a clear-signing review, which does not need the blind signing setting."""
    app_client = EthAppClient(scenario_navigator.backend)
    data = order()

    prepare(app_client, data, described_fields())
    sign_compared(scenario_navigator, app_client, data)


ENS = "0xC18360217D8F7Ab5e7c516566761Ea12Ce7F9D72"
# Permit { owner, spender, value, nonce, deadline }
PERMIT_VALUE, PERMIT_DEADLINE = 2, 4


def permit() -> dict:
    """An ERC-2612 permit of 4.2 ENS, the message of the V1 advanced filtering test."""
    return {
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
        "domain": {"name": "ENS", "version": "1", "verifyingContract": ENS, "chainId": 1},
        "message": {
            "owner": "0xd8dA6BF26964aF9D7eEd9e03E53415D37aA96045",
            "spender": "0x5B38Da6a701c568545dCfcB03FcB875f56beddC4",
            "value": 4200000000000000000,
            "nonce": 0,
            "deadline": 1719756000,
        },
    }


def permit_fields() -> list[Field]:
    """The amount, in the token of the contract that is asked for the permit, and its deadline."""
    return [
        Field(
            1,
            "Send",
            ParamTokenAmount(
                1,
                value(TypeFamily.UINT, 32, field_at(PERMIT_VALUE)),
                token=Value(1, TypeFamily.ADDRESS, 20, container_path=ContainerPath.TO),
            ),
        ),
        Field(
            1,
            "Deadline",
            ParamDatetime(1, value(TypeFamily.UINT, 32, field_at(PERMIT_DEADLINE)), DatetimeType.DT_UNIX),
        ),
    ]


@pytest.mark.parametrize("known_token, suffix", [(True, ""), (False, "_unknown_token")], ids=["known_token", "unknown_token"])
def test_eip712_v2_advanced_filtering_permit(scenario_navigator: NavigateWithScenario, known_token: bool, suffix: str):
    """A permit is reviewed with its amount in the token of the contract, whether the app knows the token or not."""
    app_client = EthAppClient(scenario_navigator.backend)
    data = permit()
    fields = permit_fields()
    token = bytes.fromhex(ENS[2:])

    start(
        app_client,
        data,
        fields,
        operation_type="authorize spending",
        creator_name="ENS",
        creator_legal_name="Ethereum Name Service",
        creator_url="ens.domains",
        contract_name="ENS Token",
        deploy_date=1636329600,
    )
    if known_token:
        assert app_client.provide_token_metadata("ENS", token, 18, 1).status == StatusWord.SWO_SUCCESS
    describe(app_client, fields)
    sign_compared(scenario_navigator, app_client, data, scenario_navigator.test_name + suffix)


def test_eip712_v2_fields_review_signer_described(scenario_navigator: NavigateWithScenario):
    """The review adds no row for the signer when a descriptor shows it."""
    app_client = EthAppClient(scenario_navigator.backend)
    data = order()
    fields = [
        container("Signer", TypeFamily.ADDRESS, 20, ContainerPath.FROM),
        raw("Label", TypeFamily.STRING, None, field_at(LABEL)),
    ]

    prepare(app_client, data, fields)
    sign_compared(scenario_navigator, app_client, data)


def test_eip712_v2_fields_review_other_chain(scenario_navigator: NavigateWithScenario):
    """A message for another chain than the app's tells its network."""
    app_client = EthAppClient(scenario_navigator.backend)
    data = order(chain_id=10)

    prepare(app_client, data, described_fields())
    sign_compared(scenario_navigator, app_client, data)


# --- What each kind of descriptor shows --------------------------------------------------------


def token_amount() -> Field:
    return Field(
        1,
        "Token amount",
        ParamTokenAmount(
            1,
            value(TypeFamily.UINT, 32, field_at(AMOUNT)),
            token=value(TypeFamily.ADDRESS, 20, field_at(ITEM), field_at(TOKEN)),
            native_currency=[bytes.fromhex("22" * 20)],
        ),
    )


def formats() -> list[Field]:
    return [
        raw("Mode", TypeFamily.UINT, 1, field_at(MODE)),
        raw("Flag", TypeFamily.BOOL, 1, field_at(FLAG)),
        raw("Recipient", TypeFamily.ADDRESS, 20, field_at(RECIPIENT)),
        raw("Blob", TypeFamily.BYTES, None, field_at(BLOB)),
        raw("Label", TypeFamily.STRING, None, field_at(LABEL)),
        Field(1, "Version", ParamRaw(1, Value(1, TypeFamily.UINT, 1, constant=b"\x07"))),
        Field(1, "Amount", ParamAmount(1, value(TypeFamily.UINT, 32, field_at(AMOUNT)))),
        Field(1, "Deadline", ParamDatetime(1, value(TypeFamily.UINT, 32, field_at(DEADLINE)), DatetimeType.DT_UNIX)),
        Field(1, "Period", ParamDuration(1, value(TypeFamily.UINT, 4, field_at(PERIOD)))),
        Field(1, "Share", ParamUnit(1, value(TypeFamily.UINT, 4, field_at(PERIOD)), "pts", decimals=2)),
        Field(1, "Network", ParamNetwork(1, Value(1, TypeFamily.UINT, container_path=ContainerPath.CHAIN_ID))),
        # in the native currency
        token_amount(),
    ]


def test_eip712_v2_fields_formats(scenario_navigator: NavigateWithScenario):
    """Each kind of descriptor formats a value of the message the way it does for a transaction."""
    app_client = EthAppClient(scenario_navigator.backend)
    data = order()

    prepare(app_client, data, formats())
    sign_compared(scenario_navigator, app_client, data)


def unit_with_prefix(prefix: bool) -> Field:
    return Field(1, "Share", ParamUnit(1, value(TypeFamily.UINT, 4, field_at(PERIOD)), "pts", decimals=2, prefix=prefix))


def test_eip712_v2_fields_unit_without_prefix(scenario_navigator: NavigateWithScenario):
    """A unit that does not ask for the prefix is accepted, like one that says nothing about it."""
    app_client = EthAppClient(scenario_navigator.backend)
    data = order()

    prepare(app_client, data, [unit_with_prefix(False)])
    sign(scenario_navigator, app_client, data)


def test_eip712_v2_fields_unit_signed(scenario_navigator: NavigateWithScenario):
    """A unit on a signed integer shows a negative value with its sign."""
    app_client = EthAppClient(scenario_navigator.backend)
    data = order(period=-1234)
    data["types"]["Test"][PERIOD]["type"] = "int32"
    fields = [Field(1, "Share", ParamUnit(1, value(TypeFamily.INT, 4, field_at(PERIOD)), "pts", decimals=2))]

    prepare(app_client, data, fields)
    sign_compared(scenario_navigator, app_client, data)


def test_eip712_v2_fields_unit_with_prefix_refused(scenario_navigator: NavigateWithScenario):
    """The prefix is not applied to the value, so a descriptor that asks for it is refused."""
    app_client = EthAppClient(scenario_navigator.backend)
    field = unit_with_prefix(True)

    start(app_client, order(), [field])
    expect_status(StatusWord.SWO_INCORRECT_DATA, lambda: describe(app_client, [field]))
    assert_recovers(app_client)


def test_eip712_v2_fields_group(scenario_navigator: NavigateWithScenario):
    """A group shows its descriptors in order, a group in a group as well, as it does for a transaction."""
    app_client = EthAppClient(scenario_navigator.backend)
    data = order()
    inner = Field(
        1,
        "Inner",
        ParamGroup(1, GroupIterationType.SEQUENTIAL, [raw("Mode", TypeFamily.UINT, 1, field_at(MODE))]),
    )
    group = Field(
        1,
        "Details",
        ParamGroup(
            1,
            GroupIterationType.SEQUENTIAL,
            [
                raw("Price", TypeFamily.UINT, 32, field_at(ITEM), field_at(PRICE)),
                inner,
                raw("Label", TypeFamily.STRING, None, field_at(LABEL)),
            ],
        ),
    )

    prepare(app_client, data, [group, raw("Flag", TypeFamily.BOOL, 1, field_at(FLAG))])
    sign_compared(scenario_navigator, app_client, data)


# --- Which values are selected ---------------------------------------------------------------


def test_eip712_v2_fields_selection(scenario_navigator: NavigateWithScenario):
    """Paths select elements of an array, a field of a struct and slices of a packed value. A hidden value,
    and one with no descriptor, are not shown."""
    app_client = EthAppClient(scenario_navigator.backend)
    data = order(ids=[1, 2, 3], blob="0x0102030405060708")
    fields = [
        raw("Last ids", TypeFamily.UINT, 1, field_at(IDS), Eip712PathArray(-2, None)),
        raw("Price", TypeFamily.UINT, 32, field_at(ITEM), field_at(PRICE)),
        raw("First", TypeFamily.UINT, 2, field_at(BLOB), Eip712PathLeafSlice(0, 2)),
        raw("Second", TypeFamily.UINT, 4, field_at(BLOB), Eip712PathLeafSlice(2, 6)),
        raw("Last", TypeFamily.UINT, 1, field_at(BLOB), Eip712PathLeafSlice(-1, None)),
        raw("Mode", TypeFamily.UINT, 1, field_at(MODE), visible=VisibleType.IF_NOT_IN, constraints=[b"\x01"]),
        raw("Flag", TypeFamily.BOOL, 1, field_at(FLAG)),
    ]

    prepare(app_client, data, fields)
    sign_compared(scenario_navigator, app_client, data)


ARRAYS = {
    "every_element": (None, None),
    "a_range": (1, 3),
    "from_the_start": (None, 2),
    "to_the_end": (2, None),
    "from_the_end": (-2, None),
    "up_to_the_last_but_one": (None, -1),
    "nothing": (3, 3),
}


@pytest.mark.parametrize("bounds", ARRAYS.values(), ids=ARRAYS.keys())
def test_eip712_v2_fields_array_slice(scenario_navigator: NavigateWithScenario, bounds: tuple):
    """A slice of an array selects its elements, and one that selects nothing is valid."""
    app_client = EthAppClient(scenario_navigator.backend)
    data = order(ids=[1, 2, 3])
    fields = [
        raw("Id", TypeFamily.UINT, 1, field_at(IDS), Eip712PathArray(*bounds)),
        raw("Mode", TypeFamily.UINT, 1, field_at(MODE)),
    ]

    prepare(app_client, data, fields)
    sign(scenario_navigator, app_client, data)


BOUND_LENGTHS = {
    "one_byte_start": {"start": b"\x01"},
    "one_byte_end": {"end": b"\x02"},
    "two_bytes": {"start": b"\x00\x01", "end": b"\x00\x03"},
    # one byte is 0 to 255, two bytes are two's complement
    "one_byte_ff_is_255": {"start": b"\xff"},
    "two_bytes_ffff_is_minus_one": {"start": b"\xff\xff"},
}


@pytest.mark.parametrize("bounds", BOUND_LENGTHS.values(), ids=BOUND_LENGTHS.keys())
def test_eip712_v2_fields_array_bound_lengths(scenario_navigator: NavigateWithScenario, bounds: dict):
    """A bound is one byte, 0 to 255, or two bytes, two's complement."""
    app_client = EthAppClient(scenario_navigator.backend)
    data = order(ids=[1, 2, 3])
    fields = [
        raw("Id", TypeFamily.UINT, 1, field_at(IDS), RawEip712PathArray(**bounds)),
        raw("Mode", TypeFamily.UINT, 1, field_at(MODE)),
    ]

    prepare(app_client, data, fields)
    sign(scenario_navigator, app_client, data)


def test_eip712_v2_fields_an_empty_array(scenario_navigator: NavigateWithScenario):
    """Every element of an array with none is no value, which is not an error."""
    app_client = EthAppClient(scenario_navigator.backend)
    data = order(ids=[])

    prepare(
        app_client,
        data,
        [raw("Id", TypeFamily.UINT, 1, field_at(IDS), all_of()), raw("Mode", TypeFamily.UINT, 1, field_at(MODE))],
    )
    sign(scenario_navigator, app_client, data)


@pytest.mark.parametrize("ids, accepted", [([], True), ([1], False)], ids=["no_element", "an_element"])
def test_eip712_v2_fields_steps_after_an_empty_selection_are_not_checked(
    scenario_navigator: NavigateWithScenario, ids: list[int], accepted: bool
):
    """With no element to follow, the rest of a path has nothing to be checked against, unlike with one."""
    app_client = EthAppClient(scenario_navigator.backend)
    data = order(ids=ids)
    # a step into a struct, below an array of integers
    fields = [raw("Id", TypeFamily.UINT, 1, field_at(IDS), all_of(), field_at(0))]

    start(app_client, data, fields)
    if accepted:
        describe(app_client, fields)
        sign(scenario_navigator, app_client, data)
    else:
        expect_status(StatusWord.SWO_INCORRECT_DATA, lambda: describe(app_client, fields))
        assert_recovers(app_client)


# --- What a transaction has, and a message has not -------------------------------------------


def test_eip712_v2_fields_the_chain_is_the_one_of_the_domain(scenario_navigator: NavigateWithScenario):
    """The descriptor reads the chain of the domain."""
    app_client = EthAppClient(scenario_navigator.backend)
    data = order(chain_id=137)

    prepare(app_client, data, [container("Chain", TypeFamily.UINT, 8, ContainerPath.CHAIN_ID)])
    sign(scenario_navigator, app_client, data)


# a domain that has none of what is asked for leaves the descriptor nothing to show
NOTHING_TO_SHOW = {
    "no_contract": (message(1), container("Contract", TypeFamily.ADDRESS, 20, ContainerPath.TO)),
    "no_chain": (message(None, CONTRACT), container("Chain", TypeFamily.UINT, 8, ContainerPath.CHAIN_ID)),
    # a message sends nothing: there is no amount to show
    "native_amount": (message(1, CONTRACT), container("Value", TypeFamily.UINT, 32, ContainerPath.VALUE)),
}


@pytest.mark.parametrize("data, field", NOTHING_TO_SHOW.values(), ids=NOTHING_TO_SHOW.keys())
def test_eip712_v2_fields_container_value_that_is_not_there_refused(
    scenario_navigator: NavigateWithScenario, data: dict, field: Field
):
    app_client = EthAppClient(scenario_navigator.backend)

    start(app_client, data, [field])
    expect_status(StatusWord.SWO_INCORRECT_DATA, lambda: describe(app_client, [field]))
    assert_recovers(app_client)


# --- Visibility --------------------------------------------------------------------------------


VISIBILITY = {
    # shown only when the value is not one of the constraints
    "hidden_when_in": (VisibleType.IF_NOT_IN, [b"\x01"]),
    "shown_when_not_in": (VisibleType.IF_NOT_IN, [b"\x02"]),
    # never shown, but the value has to be one of the constraints
    "must_be": (VisibleType.MUST_BE, [b"\x02", b"\x01"]),
}


@pytest.mark.parametrize("visible, constraints", VISIBILITY.values(), ids=VISIBILITY.keys())
def test_eip712_v2_fields_visibility(scenario_navigator: NavigateWithScenario, visible: VisibleType, constraints: list[bytes]):
    """A descriptor can be shown or hidden on the value it reads, as for a transaction."""
    app_client = EthAppClient(scenario_navigator.backend)
    data = order()
    fields = [
        raw("Mode", TypeFamily.UINT, 1, field_at(MODE), visible=visible, constraints=constraints),
        raw("Flag", TypeFamily.BOOL, 1, field_at(FLAG)),
    ]

    prepare(app_client, data, fields)
    sign(scenario_navigator, app_client, data)


def test_eip712_v2_fields_a_value_that_must_be_something_else_refused(scenario_navigator: NavigateWithScenario):
    """A value that has to be one of the constraints is not accepted when it is none of them."""
    app_client = EthAppClient(scenario_navigator.backend)
    fields = [raw("Mode", TypeFamily.UINT, 1, field_at(MODE), visible=VisibleType.MUST_BE, constraints=[b"\x02"])]

    start(app_client, order(), fields)
    expect_status(StatusWord.SWO_INCORRECT_DATA, lambda: describe(app_client, fields))
    assert_recovers(app_client)


# --- Refused: the path ---------------------------------------------------------------------------


def path_field(*steps, version: int = 1) -> Field:
    return Field(1, "Bad", ParamRaw(1, Value(1, TypeFamily.UINT, 32, eip712_path=Eip712Path(version, list(steps)))))


BAD_PATHS = {
    "field_out_of_range": path_field(field_at(20)),
    "ends_on_an_array": path_field(field_at(IDS)),
    "ends_on_a_struct": path_field(field_at(ITEM)),
    "array_slice_past_the_end": path_field(field_at(IDS), Eip712PathArray(None, 4)),
    "array_slice_before_the_start": path_field(field_at(IDS), Eip712PathArray(-4, None)),
    "array_step_on_a_leaf": path_field(field_at(MODE), all_of()),
    "field_step_on_a_leaf": path_field(field_at(MODE), field_at(0)),
    "field_step_on_an_array": path_field(field_at(IDS), field_at(0)),
    "leaf_slice_on_a_number": path_field(field_at(MODE), Eip712PathLeafSlice(0, 1)),
    "leaf_slice_past_the_value": path_field(field_at(BLOB), Eip712PathLeafSlice(0, 99)),
    "leaf_slice_selecting_nothing": path_field(field_at(BLOB), Eip712PathLeafSlice(2, 2)),
    "unknown_version": path_field(field_at(MODE), version=2),
    "no_step": path_field(),
    "starting_with_an_array": path_field(all_of(), field_at(MODE)),
    "starting_with_a_leaf_slice": path_field(Eip712PathLeafSlice(0, 1), field_at(MODE)),
    "step_after_the_leaf_slice": path_field(field_at(BLOB), Eip712PathLeafSlice(0, 1), field_at(0)),
    "empty_array_bound": path_field(field_at(IDS), RawEip712PathArray(start=b"")),
    "three_byte_array_bound": path_field(field_at(IDS), RawEip712PathArray(end=b"\x00\x00\x02")),
    "four_byte_array_bound": path_field(field_at(IDS), RawEip712PathArray(start=b"\x00\x00\x00\x01")),
    "four_byte_leaf_bound": path_field(field_at(BLOB), RawEip712PathLeafSlice(start=b"\x00\x00\x00\x01")),
}


@pytest.mark.parametrize("field", BAD_PATHS.values(), ids=BAD_PATHS.keys())
def test_eip712_v2_fields_bad_path_refused(scenario_navigator: NavigateWithScenario, field: Field):
    """A path that does not lead to values of the message is refused, rather than shown as nothing."""
    app_client = EthAppClient(scenario_navigator.backend)

    start(app_client, order(), [field])
    expect_status(StatusWord.SWO_INCORRECT_DATA, lambda: describe(app_client, [field]))
    assert_recovers(app_client)


def test_eip712_v2_fields_more_matches_than_the_app_keeps_refused(scenario_navigator: NavigateWithScenario):
    """A descriptor selects at most 16 values."""
    app_client = EthAppClient(scenario_navigator.backend)
    field = raw("Id", TypeFamily.UINT, 1, field_at(IDS), all_of())

    start(app_client, order(ids=list(range(17))), [field])
    expect_status(StatusWord.SWO_INCORRECT_DATA, lambda: describe(app_client, [field]))
    assert_recovers(app_client)


def test_eip712_v2_fields_as_many_matches_as_the_app_keeps(scenario_navigator: NavigateWithScenario):
    app_client = EthAppClient(scenario_navigator.backend)
    data = order(ids=list(range(16)))

    prepare(app_client, data, [raw("Id", TypeFamily.UINT, 1, field_at(IDS), all_of())])
    sign(scenario_navigator, app_client, data)


def test_eip712_v2_fields_a_data_path_in_a_message_refused(scenario_navigator: NavigateWithScenario):
    """A path in calldata is not one in a message."""
    app_client = EthAppClient(scenario_navigator.backend)
    paths = get_all_paths(f"{ABIS_FOLDER}/erc20.json", "transfer")
    field = Field(1, "Bad", ParamRaw(1, Value(1, TypeFamily.UINT, 32, data_path=DataPath(1, paths["_value"]))))

    start(app_client, order(), [field])
    expect_status(StatusWord.SWO_INCORRECT_DATA, lambda: describe(app_client, [field]))
    assert_recovers(app_client)


BAD_VALUES = {
    "no_source": Value(1, TypeFamily.UINT, 1),
    "path_and_container_path": value(TypeFamily.UINT, 1, field_at(MODE), container_path=ContainerPath.FROM),
    "path_and_constant": value(TypeFamily.UINT, 1, field_at(MODE), constant=b"\x01"),
    "container_path_and_constant": Value(1, TypeFamily.UINT, 1, container_path=ContainerPath.FROM, constant=b"\x01"),
    "missing_version": RawValue(Value.serialize_field(0x01, TypeFamily.UINT) + Value.serialize_field(0x05, b"\x01")),
    "missing_type_family": RawValue(Value.serialize_field(0x00, 1) + Value.serialize_field(0x05, b"\x01")),
}


@pytest.mark.parametrize("bad", BAD_VALUES.values(), ids=BAD_VALUES.keys())
def test_eip712_v2_fields_bad_value_refused(scenario_navigator: NavigateWithScenario, bad: Value):
    """A VALUE needs its VERSION and TYPE_FAMILY and exactly one source, as for a transaction."""
    app_client = EthAppClient(scenario_navigator.backend)
    field = Field(1, "Bad", ParamAmount(1, bad))

    start(app_client, order(), [field])
    expect_status(StatusWord.SWO_INCORRECT_DATA, lambda: describe(app_client, [field]))
    assert_recovers(app_client)


@pytest.mark.parametrize("version", [0, 2])
@pytest.mark.parametrize("struct", ["param", "value"])
def test_eip712_v2_fields_struct_version_other_than_one_refused(
    scenario_navigator: NavigateWithScenario, struct: str, version: int
):
    """The only version of the PARAM and VALUE structures is 1."""
    app_client = EthAppClient(scenario_navigator.backend)
    versioned_value = Value(version if struct == "value" else 1, TypeFamily.UINT, 1, eip712_path=path(field_at(MODE)))
    field = Field(1, "Bad", ParamAmount(version if struct == "param" else 1, versioned_value))

    start(app_client, order(), [field])
    expect_status(StatusWord.SWO_INCORRECT_DATA, lambda: describe(app_client, [field]))
    assert_recovers(app_client)


# --- Refused: the descriptors are not the announced ones -----------------------------------------


def announced() -> list[Field]:
    return [
        raw("Label", TypeFamily.STRING, None, field_at(LABEL)),
        raw("Mode", TypeFamily.UINT, 1, field_at(MODE)),
        raw("Flag", TypeFamily.BOOL, 1, field_at(FLAG)),
    ]


def altered() -> list[Field]:
    return [raw("Other name", TypeFamily.STRING, None, field_at(LABEL)), *announced()[1:]]


OTHER_DESCRIPTORS = {
    "missing_the_last": announced()[:-1],
    "none": [],
    "one_too_many": [*announced(), announced()[0]],
    "the_last_twice": [*announced(), announced()[-1]],
    "reordered": [announced()[1], announced()[0], announced()[2]],
    "a_name_altered": altered(),
    "another_path": [announced()[0], raw("Mode", TypeFamily.UINT, 1, field_at(FLAG)), announced()[2]],
}


@pytest.mark.parametrize("sent", OTHER_DESCRIPTORS.values(), ids=OTHER_DESCRIPTORS.keys())
def test_eip712_v2_fields_other_descriptors_than_announced_refused(scenario_navigator: NavigateWithScenario, sent: list[Field]):
    """The message info attests the descriptors: no review is shown for any other set."""
    app_client = EthAppClient(scenario_navigator.backend)

    start(app_client, order(), announced())
    describe(app_client, sent)
    expect_status(StatusWord.SWO_INCORRECT_DATA, lambda: sign_attempt(app_client))
    assert_recovers(app_client)


def test_eip712_v2_fields_a_wrong_hash_does_not_depend_on_blind_signing(scenario_navigator: NavigateWithScenario):
    """With the blind signing setting on, a message that is not the described one is still not shown raw."""
    enable_blind_signing(scenario_navigator)
    app_client = EthAppClient(scenario_navigator.backend)

    start(app_client, order(), announced())
    describe(app_client, announced()[:-1])
    expect_status(StatusWord.SWO_INCORRECT_DATA, lambda: sign_attempt(app_client))
    assert_recovers(app_client)


def test_eip712_v2_fields_a_message_is_described_whatever_the_blind_signing_setting(
    scenario_navigator: NavigateWithScenario,
):
    """The review of described fields is not blind signing: the setting changes nothing, and it shows no warning."""
    enable_blind_signing(scenario_navigator)
    app_client = EthAppClient(scenario_navigator.backend)
    data = order()

    prepare(app_client, data, announced())
    sign(scenario_navigator, app_client, data)


def test_eip712_v2_fields_review_hashes(scenario_navigator: NavigateWithScenario):
    """The review ends with the hashes the signature covers when the setting asks for them, as the standard one."""
    settings_toggle(scenario_navigator.backend.device, scenario_navigator.navigator, [SettingID.DISPLAY_HASH])
    app_client = EthAppClient(scenario_navigator.backend)
    data = order()

    prepare(app_client, data, announced())
    sign_compared(scenario_navigator, app_client, data)


# what is told about the contract: its creator, or only that there is a contract, or nothing
INTERACTIONS = {
    "creator": (CONTRACT, {"creator_name": "Acme"}),
    "contract_only": (CONTRACT, {}),
    "creator_without_contract": (None, {"creator_name": "Acme"}),
    # the signed information about the contract is shown even when it names no creator nor address
    "information_without_creator_nor_contract": (
        None,
        {"creator_legal_name": "Acme Inc", "contract_name": "Thing", "deploy_date": 1636329600},
    ),
    "nothing_to_tell": (None, {}),
}


@pytest.mark.parametrize("contract, overrides", INTERACTIONS.values(), ids=INTERACTIONS.keys())
def test_eip712_v2_fields_review_interaction(scenario_navigator: NavigateWithScenario, contract: str | None, overrides: dict):
    """The row about the contract is shown when the info or the domain has something to tell about it."""
    app_client = EthAppClient(scenario_navigator.backend)
    data = order(contract=contract)

    prepare(app_client, data, announced(), **overrides)
    sign(scenario_navigator, app_client, data)


# more than the heap of the device holds, if each review kept a part of what it took
REVIEWS_IN_A_ROW = 40


def test_eip712_v2_fields_reviews_in_a_row(scenario_navigator: NavigateWithScenario):
    """A described review gives back the memory it took: many in a row are all signed."""
    app_client = EthAppClient(scenario_navigator.backend)
    data = order()

    for _ in range(REVIEWS_IN_A_ROW):
        prepare(app_client, data, announced(), creator_name="Acme", creator_legal_name="Acme Inc", contract_name="Thing")
        with app_client.eip712_v2_sign():
            scenario_navigator.review_approve(do_comparison=False)
        vrs = ResponseParser.signature(app_client.response().data)
        assert get_wallet_addr(app_client) == recover_message(data, vrs)


# --- Refused: the command ------------------------------------------------------------------------


def send_field(app_client: EthAppClient, field: Field) -> None:
    app_client.eip712_v2_send_field_desc(field.serialize())


def test_eip712_v2_fields_with_no_message_info_refused(scenario_navigator: NavigateWithScenario):
    app_client = EthAppClient(scenario_navigator.backend)

    json_to_tlv.process_data(app_client, order(), BIP32_PATH)
    expect_status(StatusWord.SWO_COMMAND_NOT_ALLOWED, lambda: send_field(app_client, announced()[0]))
    assert_recovers(app_client)


def test_eip712_v2_fields_with_no_message_refused(scenario_navigator: NavigateWithScenario):
    app_client = EthAppClient(scenario_navigator.backend)

    expect_status(StatusWord.SWO_COMMAND_NOT_ALLOWED, lambda: send_field(app_client, announced()[0]))
    assert_recovers(app_client)


def test_eip712_v2_fields_when_none_are_announced_refused(scenario_navigator: NavigateWithScenario):
    """A message info that announces no descriptor leaves no room for one."""
    app_client = EthAppClient(scenario_navigator.backend)
    data = order()

    json_to_tlv.process_data(app_client, data, BIP32_PATH)
    assert app_client.eip712_v2_send_message_info(info_for(data, fields_hash=NO_FIELDS_HASH).serialize()).status == (
        StatusWord.SWO_SUCCESS
    )
    expect_status(StatusWord.SWO_COMMAND_NOT_ALLOWED, lambda: send_field(app_client, announced()[0]))
    assert_recovers(app_client)


def test_eip712_v2_fields_after_the_signature_refused(scenario_navigator: NavigateWithScenario):
    """The descriptors are the ones of a message, and the flow is over once it is signed."""
    app_client = EthAppClient(scenario_navigator.backend)
    data = order()

    prepare(app_client, data, announced())
    sign(scenario_navigator, app_client, data)
    expect_status(StatusWord.SWO_COMMAND_NOT_ALLOWED, lambda: send_field(app_client, announced()[0]))
    assert_recovers(app_client)


def test_eip712_v2_fields_in_a_v1_session_refused(scenario_navigator: NavigateWithScenario):
    """V1 and V2 never share a session."""
    app_client = EthAppClient(scenario_navigator.backend)

    InputData.process_data(app_client, message(1))
    expect_status(StatusWord.SWO_COMMAND_NOT_ALLOWED, lambda: send_field(app_client, announced()[0]))
    assert_recovers(app_client)


@pytest.mark.parametrize("p2", [0x01, 0x03, 0x04, 0xFF])
def test_eip712_v2_fields_unknown_p2_refused(scenario_navigator: NavigateWithScenario, p2: int):
    """The instruction is the one of the transaction descriptors (P2 = 00) or of a message (P2 = 02)."""
    app_client = EthAppClient(scenario_navigator.backend)
    data = order()

    start(app_client, data, announced())
    chunk = app_client._cmd_builder.common_tlv_serialize(
        InsType.PROVIDE_OPERATION_FIELD_DESC, announced()[0].serialize(), p2l=[p2]
    )[0]
    expect_status(StatusWord.SWO_INCORRECT_P1_P2, lambda: app_client._exchange(chunk))
    assert_recovers(app_client)


def test_eip712_v2_fields_unknown_p1_refused(scenario_navigator: NavigateWithScenario):
    app_client = EthAppClient(scenario_navigator.backend)

    start(app_client, order(), announced())
    expect_status(
        StatusWord.SWO_INCORRECT_P1_P2,
        lambda: app_client.send_raw(0xE0, InsType.PROVIDE_OPERATION_FIELD_DESC, 0x02, P2Type.EIP712_V2_IMPLEM, b"\x00\x00"),
    )
    assert_recovers(app_client)


def test_eip712_v2_fields_a_refusal_is_followed_by_a_message_that_is_signed(
    scenario_navigator: NavigateWithScenario,
):
    """After a refused descriptor, the next message is described and signed normally."""
    app_client = EthAppClient(scenario_navigator.backend)
    bad = BAD_PATHS["field_out_of_range"]

    start(app_client, order(), [bad])
    expect_status(StatusWord.SWO_INCORRECT_DATA, lambda: describe(app_client, [bad]))
    data = order(label="second")
    prepare(app_client, data, announced())
    sign(scenario_navigator, app_client, data)


def test_eip712_v2_fields_reject_the_review(scenario_navigator: NavigateWithScenario):
    """Rejecting the review gives no signature, and the next message goes through."""
    app_client = EthAppClient(scenario_navigator.backend)
    data = order()

    prepare(app_client, data, announced())
    expect_status(
        StatusWord.SWO_CONDITIONS_NOT_SATISFIED,
        lambda: _reject(scenario_navigator, app_client),
    )
    prepare(app_client, data, announced())
    sign(scenario_navigator, app_client, data)


def _reject(scenario_navigator: NavigateWithScenario, app_client: EthAppClient) -> None:
    with app_client.eip712_v2_sign():
        scenario_navigator.review_reject(do_comparison=False)


# --- The calls a message makes -------------------------------------------------------------------
#
# A descriptor of calldata names a call, which is described as a transaction: its own
# TRANSACTION_INFO and FIELDs (P2 = 00) follow the descriptors of the message that name it.

USDC = "0x3c499c542cef5e3811e1192ce70d8cc03d5c3359"
TRANSFER_RECIPIENT = bytes.fromhex("B8C8EB8EFC68796E766F6AB320DB8C165C064949")
TRANSFERRED = 4000
EMPTY_CALLDATA = "0x"
SOMEONE = "0x" + "11" * 20


def transfer_calldata() -> str:
    with open(f"{ABIS_FOLDER}/erc20.json", encoding="utf-8") as file:
        contract = web3.Web3().eth.contract(abi=json.load(file), address=None)
    return contract.encode_abi("transfer", [TRANSFER_RECIPIENT, TRANSFERRED])


def transfer_message(chain_id: int = 1) -> dict:
    """A message that calls USDC to transfer."""
    data = message(chain_id, CONTRACT)
    data["types"]["Test"] = [{"name": "to", "type": "address"}, {"name": "data", "type": "bytes"}]
    data["message"] = {"to": USDC, "data": transfer_calldata()}
    return data


def batch_message(calls: list[tuple[str, int, str]]) -> dict:
    """A message that makes calls, each to an address with an amount and calldata."""
    data = message(1, CONTRACT)
    data["types"]["Call"] = [
        {"name": "to", "type": "address"},
        {"name": "value", "type": "uint256"},
        {"name": "data", "type": "bytes"},
    ]
    data["types"]["Batch"] = [{"name": "calls", "type": "Call[]"}]
    data["primaryType"] = "Batch"
    del data["types"]["Test"]
    data["message"] = {"calls": [{"to": to, "value": amount, "data": calldata} for to, amount, calldata in calls]}
    return data


def call_field() -> Field:
    """The descriptor of the message that names the call it makes."""
    return Field(
        1,
        "Transaction",
        ParamCalldata(
            1,
            value(TypeFamily.BYTES, None, field_at(1)),
            value(TypeFamily.ADDRESS, 20, field_at(0)),
        ),
    )


def batch_field() -> Field:
    """The descriptor of the message that names every call it makes."""
    each = all_of()
    return Field(
        1,
        "Transactions",
        ParamCalldata(
            1,
            value(TypeFamily.BYTES, None, field_at(0), each, field_at(2)),
            value(TypeFamily.ADDRESS, 20, field_at(0), each, field_at(0)),
            amount=value(TypeFamily.UINT, 32, field_at(0), each, field_at(1)),
        ),
    )


def transfer_fields() -> list[Field]:
    paths = get_all_paths(f"{ABIS_FOLDER}/erc20.json", "transfer")
    return [
        Field(1, "Recipient", ParamRaw(1, Value(1, TypeFamily.ADDRESS, 20, data_path=DataPath(1, paths["_to"])))),
        Field(1, "Amount", ParamRaw(1, Value(1, TypeFamily.UINT, 32, data_path=DataPath(1, paths["_value"])))),
    ]


def transfer_info(fields: list[Field], **overrides) -> TxInfo:
    arguments: dict[str, Any] = {
        "version": 1,
        "chain_id": 1,
        "contract_addr": bytes.fromhex(USDC[2:]),
        "selector": get_selector_from_data(transfer_calldata()),
        "fields_hash": fields_hash(fields),
        "operation_type": "Token transfer",
    } | overrides
    return TxInfo(**arguments)


def describe_call(app_client: EthAppClient, fields: list[Field] | None = None, **overrides) -> None:
    """Describe a call: its TRANSACTION_INFO, then its FIELDs."""
    fields = transfer_fields() if fields is None else fields
    assert app_client.provide_operation_info(transfer_info(fields, **overrides).serialize()).status == (StatusWord.SWO_SUCCESS)
    for field in fields:
        assert app_client.provide_operation_field_desc(field.serialize()).status == StatusWord.SWO_SUCCESS


def test_eip712_v2_fields_call(scenario_navigator: NavigateWithScenario):
    """A call a message makes is reviewed as a transaction."""
    app_client = EthAppClient(scenario_navigator.backend)
    data = transfer_message()

    prepare(app_client, data, [call_field()])
    describe_call(app_client)
    sign_compared(scenario_navigator, app_client, data)


def test_eip712_v2_fields_calls(scenario_navigator: NavigateWithScenario):
    """Several calls are reviewed one after the other, each on its own page."""
    app_client = EthAppClient(scenario_navigator.backend)
    data = batch_message([(USDC, 0, transfer_calldata()), (USDC, 0, transfer_calldata())])

    prepare(app_client, data, [batch_field()])
    describe_call(app_client)
    describe_call(app_client)
    sign_compared(scenario_navigator, app_client, data)


@pytest.mark.parametrize("call_first", [True, False], ids=["call_where_it_is_named", "call_after_the_message"])
def test_eip712_v2_fields_when_a_call_is_described(scenario_navigator: NavigateWithScenario, call_first: bool):
    """The host describes a call right after the descriptor that names it, or after the whole message."""
    app_client = EthAppClient(scenario_navigator.backend)
    data = transfer_message()
    after = container("Contract", TypeFamily.ADDRESS, 20, ContainerPath.TO)

    start(app_client, data, [call_field(), after])
    describe(app_client, [call_field()])
    if call_first:
        describe_call(app_client)
        describe(app_client, [after])
    else:
        describe(app_client, [after])
        describe_call(app_client)
    sign(scenario_navigator, app_client, data)


def test_eip712_v2_fields_call_without_calldata(scenario_navigator: NavigateWithScenario):
    """A call with no calldata only sends an amount: the app describes it, as it has the address and the amount."""
    app_client = EthAppClient(scenario_navigator.backend)
    data = batch_message([(USDC, 0, transfer_calldata()), (SOMEONE, 1000, EMPTY_CALLDATA)])

    prepare(app_client, data, [batch_field()])
    describe_call(app_client)
    sign_compared(scenario_navigator, app_client, data)


# the calls that are not described are the ones that only send an amount
BATCHES = {
    "empty_send_then_transfer": ([(SOMEONE, 1000, EMPTY_CALLDATA), (USDC, 0, transfer_calldata())], 1),
    "only_empty_sends": ([(SOMEONE, 1000, EMPTY_CALLDATA), ("0x" + "22" * 20, 5, EMPTY_CALLDATA)], 0),
}


@pytest.mark.parametrize("calls, described", BATCHES.values(), ids=BATCHES.keys())
def test_eip712_v2_fields_calls_with_and_without_calldata(
    scenario_navigator: NavigateWithScenario, calls: list[tuple[str, int, str]], described: int
):
    app_client = EthAppClient(scenario_navigator.backend)
    data = batch_message(calls)

    prepare(app_client, data, [batch_field()])
    for _ in range(described):
        describe_call(app_client)
    sign(scenario_navigator, app_client, data)


def test_eip712_v2_fields_a_call_starts_from_the_signer_on_the_chain_of_the_message(
    scenario_navigator: NavigateWithScenario,
):
    """What a call does not say is the message's: its sender is the signer, its chain the message's."""
    app_client = EthAppClient(scenario_navigator.backend)
    data = transfer_message()
    fields = [
        container("Sender", TypeFamily.ADDRESS, 20, ContainerPath.FROM),
        container("Chain", TypeFamily.UINT, 8, ContainerPath.CHAIN_ID),
        *transfer_fields()[1:],
    ]

    prepare(app_client, data, [call_field()])
    describe_call(app_client, fields)
    sign(scenario_navigator, app_client, data)


# --- Refused: the calls --------------------------------------------------------------------------


def test_eip712_v2_fields_a_call_info_when_no_call_is_named_refused(scenario_navigator: NavigateWithScenario):
    app_client = EthAppClient(scenario_navigator.backend)
    data = order()

    prepare(app_client, data, announced())
    expect_status(
        StatusWord.SWO_COMMAND_NOT_ALLOWED,
        lambda: app_client.provide_operation_info(transfer_info(transfer_fields()).serialize()),
    )
    assert_recovers(app_client)


def test_eip712_v2_fields_a_call_field_without_its_info_refused(scenario_navigator: NavigateWithScenario):
    app_client = EthAppClient(scenario_navigator.backend)

    prepare(app_client, transfer_message(), [call_field()])
    expect_status(
        StatusWord.SWO_COMMAND_NOT_ALLOWED,
        lambda: app_client.provide_operation_field_desc(transfer_fields()[0].serialize()),
    )
    assert_recovers(app_client)


def test_eip712_v2_fields_a_message_descriptor_while_a_call_is_described_refused(
    scenario_navigator: NavigateWithScenario,
):
    """The descriptors of the message resume once the call is described."""
    app_client = EthAppClient(scenario_navigator.backend)

    start(app_client, transfer_message(), [call_field(), call_field()])
    describe(app_client, [call_field()])
    assert app_client.provide_operation_info(transfer_info(transfer_fields()).serialize()).status == (StatusWord.SWO_SUCCESS)
    expect_status(StatusWord.SWO_COMMAND_NOT_ALLOWED, lambda: send_field(app_client, call_field()))
    assert_recovers(app_client)


CALL_INFOS = {
    "other_selector": {"selector": bytes.fromhex("deadbeef")},
    "other_chain": {"chain_id": 10},
    "other_contract": {"contract_addr": bytes.fromhex("cd" * 20)},
}


@pytest.mark.parametrize("overrides", CALL_INFOS.values(), ids=CALL_INFOS.keys())
def test_eip712_v2_fields_a_call_info_for_another_call_refused(scenario_navigator: NavigateWithScenario, overrides: dict):
    """The call a message names is told by its contract, its selector and its chain."""
    app_client = EthAppClient(scenario_navigator.backend)

    prepare(app_client, transfer_message(), [call_field()])
    expect_status(
        StatusWord.SWO_INCORRECT_DATA,
        lambda: app_client.provide_operation_info(transfer_info(transfer_fields(), **overrides).serialize()),
    )
    assert_recovers(app_client)


def test_eip712_v2_fields_a_call_that_is_not_described_refused(scenario_navigator: NavigateWithScenario):
    app_client = EthAppClient(scenario_navigator.backend)

    prepare(app_client, transfer_message(), [call_field()])
    expect_status(StatusWord.SWO_REFERENCED_DATA_NOT_FOUND, lambda: sign_attempt(app_client))
    assert_recovers(app_client)


def test_eip712_v2_fields_a_call_that_is_half_described_refused(scenario_navigator: NavigateWithScenario):
    app_client = EthAppClient(scenario_navigator.backend)
    fields = transfer_fields()

    prepare(app_client, transfer_message(), [call_field()])
    assert app_client.provide_operation_info(transfer_info(fields).serialize()).status == StatusWord.SWO_SUCCESS
    assert app_client.provide_operation_field_desc(fields[0].serialize()).status == StatusWord.SWO_SUCCESS
    expect_status(StatusWord.SWO_REFERENCED_DATA_NOT_FOUND, lambda: sign_attempt(app_client))
    assert_recovers(app_client)


def test_eip712_v2_fields_a_call_with_other_descriptors_than_announced_refused(
    scenario_navigator: NavigateWithScenario,
):
    """The info of the call attests its descriptors: with others, the call is never complete."""
    app_client = EthAppClient(scenario_navigator.backend)
    fields = transfer_fields()
    other = transfer_info(fields, fields_hash=hashlib.sha3_256(b"other").digest())

    prepare(app_client, transfer_message(), [call_field()])
    assert app_client.provide_operation_info(other.serialize()).status == StatusWord.SWO_SUCCESS
    for field in fields:
        assert app_client.provide_operation_field_desc(field.serialize()).status == StatusWord.SWO_SUCCESS
    expect_status(StatusWord.SWO_REFERENCED_DATA_NOT_FOUND, lambda: sign_attempt(app_client))
    assert_recovers(app_client)


def test_eip712_v2_fields_a_call_with_more_descriptors_than_announced_refused(
    scenario_navigator: NavigateWithScenario,
):
    """The call is complete as soon as its descriptors give the announced hash: no other one follows."""
    app_client = EthAppClient(scenario_navigator.backend)
    fields = transfer_fields()

    prepare(app_client, transfer_message(), [call_field()])
    assert (
        app_client.provide_operation_info(transfer_info(fields, fields_hash=fields_hash(fields[:1])).serialize()).status
        == StatusWord.SWO_SUCCESS
    )
    assert app_client.provide_operation_field_desc(fields[0].serialize()).status == StatusWord.SWO_SUCCESS
    expect_status(StatusWord.SWO_COMMAND_NOT_ALLOWED, lambda: app_client.provide_operation_field_desc(fields[1].serialize()))
    assert_recovers(app_client)


def test_eip712_v2_fields_a_call_that_is_missing_refused(scenario_navigator: NavigateWithScenario):
    """Two calls are named, and one is described."""
    app_client = EthAppClient(scenario_navigator.backend)
    data = batch_message([(USDC, 0, transfer_calldata()), (USDC, 0, transfer_calldata())])

    prepare(app_client, data, [batch_field()])
    describe_call(app_client)
    expect_status(StatusWord.SWO_REFERENCED_DATA_NOT_FOUND, lambda: sign_attempt(app_client))
    assert_recovers(app_client)


# --- Enum values and map entries -----------------------------------------------------------------
#
# They are attached to the operation being described: the hash of the primary type for a message, the
# selector for a call, and they follow the info of that operation.


def mode_and_tier() -> tuple[dict, list[Field]]:
    """A message with a mode that is an enum, and a tier whose name is a map entry."""
    data = message(1, CONTRACT)
    data["types"]["Test"] = [{"name": "mode", "type": "uint8"}, {"name": "tier", "type": "uint256"}]
    data["message"] = {"mode": 1, "tier": 7}
    fields = [
        Field(1, "Mode", ParamEnum(1, 0, value(TypeFamily.UINT, 1, field_at(0)))),
        Field(
            1,
            "Tier name",
            ParamRaw(
                1,
                Value(1, TypeFamily.STRING, map_ref=MapRef(1, 0, value(TypeFamily.UINT, 32, field_at(1)))),
            ),
        ),
    ]
    return data, fields


def enum_card(data: dict, **overrides) -> EnumValue:
    arguments: dict[str, Any] = {
        "version": 1,
        "chain_id": None,
        "contract_addr": None,
        "selector": None,
        "domain_separator": domain_separator(data),
        "primary_type_hash": type_hash(data),
        "id": 0,
        "value": 1,
        "name": "Delegate call",
    } | overrides
    return EnumValue(**arguments)


def map_card(data: dict, **overrides) -> MapEntry:
    arguments: dict[str, Any] = {
        "version": 1,
        "chain_id": None,
        "contract_addr": None,
        "selector": None,
        "domain_separator": domain_separator(data),
        "primary_type_hash": type_hash(data),
        "id": 0,
        # an integer of a message is the shortest big-endian form of the number
        "key": b"\x07",
        "value": b"Gold",
    } | overrides
    return MapEntry(**arguments)


def give_cards(app_client: EthAppClient, data: dict, **overrides) -> None:
    assert app_client.provide_enum_value(enum_card(data, **overrides).serialize()).status == StatusWord.SWO_SUCCESS
    assert app_client.provide_map_entry(map_card(data, **overrides).serialize()).status == StatusWord.SWO_SUCCESS


def test_eip712_v2_fields_enum_and_map_entry(scenario_navigator: NavigateWithScenario):
    """A value is shown by the name of its enum value, or by its map entry."""
    app_client = EthAppClient(scenario_navigator.backend)
    data, fields = mode_and_tier()

    start(app_client, data, fields)
    give_cards(app_client, data)
    describe(app_client, fields)
    sign_compared(scenario_navigator, app_client, data)


def test_eip712_v2_fields_enum_and_map_entry_before_the_message_info(scenario_navigator: NavigateWithScenario):
    """The tag of their key tells what they are for: they can come before the info of the message."""
    app_client = EthAppClient(scenario_navigator.backend)
    data, fields = mode_and_tier()

    json_to_tlv.process_data(app_client, data, BIP32_PATH)
    give_cards(app_client, data)
    info = info_for(data, fields_hash=fields_hash(fields))
    assert app_client.eip712_v2_send_message_info(info.serialize()).status == StatusWord.SWO_SUCCESS
    describe(app_client, fields)
    sign(scenario_navigator, app_client, data)


OTHER_CARDS = {
    "other_type": {"primary_type_hash": hashlib.sha3_256(b"other").digest()},
    "other_domain": {"domain_separator": hashlib.sha3_256(b"other").digest()},
    # a card of a function of the contract of the domain is for a transaction
    "of_a_function": {
        "chain_id": 1,
        "contract_addr": bytes.fromhex(CONTRACT[2:]),
        "selector": bytes.fromhex("12345678"),
        "domain_separator": None,
        "primary_type_hash": None,
    },
}


@pytest.mark.parametrize("overrides", OTHER_CARDS.values(), ids=OTHER_CARDS.keys())
def test_eip712_v2_fields_cards_for_something_else_do_not_apply(scenario_navigator: NavigateWithScenario, overrides: dict):
    """What is looked up is the type and the domain of the message: no label is guessed."""
    app_client = EthAppClient(scenario_navigator.backend)
    data, fields = mode_and_tier()

    start(app_client, data, fields)
    give_cards(app_client, data, **overrides)
    expect_status(StatusWord.SWO_INCORRECT_DATA, lambda: describe(app_client, fields))
    assert_recovers(app_client)


# a card is for a function of a contract or for a primary type of a domain: it has all the tags of
# one, and none of the other
NOT_ONE_KEY = {
    "a_chain_too": {"chain_id": 1},
    "a_contract_too": {"contract_addr": bytes.fromhex(CONTRACT[2:])},
    "a_selector_too": {"selector": bytes.fromhex("12345678")},
    "no_domain_separator": {"domain_separator": None},
    "no_type_hash": {"primary_type_hash": None},
}


@pytest.mark.parametrize("card", ["enum_value", "map_entry"])
@pytest.mark.parametrize("overrides", NOT_ONE_KEY.values(), ids=NOT_ONE_KEY.keys())
def test_eip712_v2_fields_a_card_not_of_one_operation_refused(
    scenario_navigator: NavigateWithScenario, card: str, overrides: dict
):
    app_client = EthAppClient(scenario_navigator.backend)
    data, fields = mode_and_tier()

    start(app_client, data, fields)
    if card == "enum_value":
        expect_status(
            StatusWord.SWO_INCORRECT_DATA, lambda: app_client.provide_enum_value(enum_card(data, **overrides).serialize())
        )
    else:
        expect_status(
            StatusWord.SWO_INCORRECT_DATA, lambda: app_client.provide_map_entry(map_card(data, **overrides).serialize())
        )
    assert_recovers(app_client)


def test_eip712_v2_fields_no_card_refuses_the_field(scenario_navigator: NavigateWithScenario):
    app_client = EthAppClient(scenario_navigator.backend)
    data, fields = mode_and_tier()

    start(app_client, data, fields)
    expect_status(StatusWord.SWO_INCORRECT_DATA, lambda: describe(app_client, fields))
    assert_recovers(app_client)


def without_domain_field(data: dict, name: str) -> dict:
    data["types"]["EIP712Domain"] = [f for f in data["types"]["EIP712Domain"] if f["name"] != name]
    del data["domain"][name]
    return data


def amount_of_the_message() -> tuple[dict, list[Field]]:
    return order(), [Field(1, "Amount", ParamAmount(1, value(TypeFamily.UINT, 32, field_at(AMOUNT))))]


def token_amount_of_the_message() -> tuple[dict, list[Field]]:
    return order(), [token_amount()]


def enum_of_the_message() -> tuple[dict, list[Field]]:
    data, fields = mode_and_tier()
    return data, fields[:1]


def map_entry_of_the_message() -> tuple[dict, list[Field]]:
    data, fields = mode_and_tier()
    return data, fields[1:]


# what a descriptor needs of the domain, and whether the message has cards to be found: a card is
# for the domain whatever it has
NEEDS_OF_THE_DOMAIN = {
    "amount": (amount_of_the_message, {"chainId"}, False),
    "token_amount": (token_amount_of_the_message, {"chainId"}, False),
    "enum": (enum_of_the_message, set(), True),
    "map_entry": (map_entry_of_the_message, set(), True),
}


@pytest.mark.parametrize("missing", ["chainId", "verifyingContract"])
@pytest.mark.parametrize("build, needs, has_cards", NEEDS_OF_THE_DOMAIN.values(), ids=NEEDS_OF_THE_DOMAIN.keys())
def test_eip712_v2_fields_what_the_domain_has_to_give(
    scenario_navigator: NavigateWithScenario,
    build: Callable[[], tuple[dict, list[Field]]],
    needs: set[str],
    has_cards: bool,
    missing: str,
):
    """A formatter that needs the chain or the contract of the message refuses a domain that has none."""
    app_client = EthAppClient(scenario_navigator.backend)
    data, fields = build()
    without_domain_field(data, missing)

    start(app_client, data, fields)
    if has_cards:
        give_cards(app_client, data)
    if missing in needs:
        expect_status(StatusWord.SWO_INCORRECT_DATA, lambda: describe(app_client, fields))
        assert_recovers(app_client)
    else:
        describe(app_client, fields)
        sign(scenario_navigator, app_client, data)


def test_eip712_v2_fields_a_map_key_is_the_value_as_it_is_sent(scenario_navigator: NavigateWithScenario):
    """The key has to be the bytes of the value, whose length counts: 7 is not a word of 32 bytes."""
    app_client = EthAppClient(scenario_navigator.backend)
    data, fields = mode_and_tier()

    start(app_client, data, fields)
    assert app_client.provide_enum_value(enum_card(data).serialize()).status == StatusWord.SWO_SUCCESS
    assert app_client.provide_map_entry(map_card(data, key=(7).to_bytes(32, "big")).serialize()).status == StatusWord.SWO_SUCCESS
    expect_status(StatusWord.SWO_INCORRECT_DATA, lambda: describe(app_client, fields))
    assert_recovers(app_client)


def transfer_amount_label() -> list[Field]:
    paths = get_all_paths(f"{ABIS_FOLDER}/erc20.json", "transfer")
    key = Value(1, TypeFamily.UINT, 32, data_path=DataPath(1, paths["_value"]))
    return [
        transfer_fields()[0],
        Field(1, "Amount label", ParamRaw(1, Value(1, TypeFamily.STRING, map_ref=MapRef(1, 0, key)))),
    ]


def call_card() -> MapEntry:
    return MapEntry(
        version=1,
        chain_id=1,
        contract_addr=bytes.fromhex(USDC[2:]),
        selector=get_selector_from_data(transfer_calldata()),
        id=0,
        key=TRANSFERRED.to_bytes(32, "big"),
        value=b"Four thousand",
    )


@pytest.mark.parametrize("entry_first", [False, True], ids=["entry_after_the_info", "entry_before_the_info"])
def test_eip712_v2_fields_a_call_map_entry(scenario_navigator: NavigateWithScenario, entry_first: bool):
    """The call a message makes is a transaction: its entries are keyed by its selector, before or after its info."""
    app_client = EthAppClient(scenario_navigator.backend)
    data = transfer_message()
    fields = transfer_amount_label()

    prepare(app_client, data, [call_field()])
    if entry_first:
        assert app_client.provide_map_entry(call_card().serialize()).status == StatusWord.SWO_SUCCESS
    assert app_client.provide_operation_info(transfer_info(fields).serialize()).status == StatusWord.SWO_SUCCESS
    if not entry_first:
        assert app_client.provide_map_entry(call_card().serialize()).status == StatusWord.SWO_SUCCESS
    for field in fields:
        assert app_client.provide_operation_field_desc(field.serialize()).status == StatusWord.SWO_SUCCESS
    sign(scenario_navigator, app_client, data)
