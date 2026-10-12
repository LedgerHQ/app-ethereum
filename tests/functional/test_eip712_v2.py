"""Functional tests for the EIP-712 V2 clear-signing protocol.

V2 delivers the whole typed message in two one-shot TLV payloads, EIP712_SCHEMA then
EIP712_VALUES, before a bare SIGN command triggers the review.

The signature check is the meaningful assertion here: the expected address is recovered from a
hash that eth_account computes independently from the same JSON, so it only matches if the
app's schema parsing, value tree and hashing all agree with the EIP-712 standard.

V2 has no field descriptors yet, so every message is displayed raw. That is blind signing, so
the setting has to be enabled and the review carries a warning page.
"""

import fnmatch
import json
import os
import struct
from collections.abc import Callable
from functools import partial
from pathlib import Path
from typing import cast

import client.response_parser as ResponseParser
import pytest
from client.client import EthAppClient
from client.command_builder import InsType, P1Type, P2Type
from client.eip712 import json_to_tlv
from client.eip712.json_to_tlv import STRUCT_VERSION
from client.eip712.schema import (
    ArrayDim,
    Eip712Field,
    Eip712FieldTag,
    Eip712Schema,
    Eip712SchemaTag,
    Eip712Struct,
    Eip712StructTag,
    SolType,
)
from client.eip712.values import Eip712ValueSeq, Eip712ValuesTag
from client.settings import SettingID, settings_toggle
from client.status_word import StatusWord
from client.utils import recover_message
from ragger.bip import pack_derivation_path
from ragger.error import ExceptionRAPDU
from ragger.navigator import NavigateWithScenario
from ragger.tlv import TlvSerializable

BIP32_PATH = "m/44'/60'/0'/0/0"
OTHER_BIP32_PATH = "m/44'/60'/1'/0/0"

CLA = 0xE0
P1_UNKNOWN = 0x02
# a tag that none of the structures of the protocol defines
UNKNOWN_TAG = 0x7F
HARDENED = 0x80000000
MAX_DEPTH = 16
MAX_DERIVATION_PATH_LENGTH = 10


def input_files() -> list[str]:
    json_path = f"{os.path.dirname(__file__)}/eip712_input_files"
    return sorted(file.path for file in os.scandir(json_path) if fnmatch.fnmatch(file.name, "*-data.json"))


def pytest_generate_tests(metafunc: pytest.Metafunc) -> None:
    if "input_file" in metafunc.fixturenames:
        metafunc.parametrize(
            "input_file",
            [pytest.param(Path(f), id="-".join(Path(f).stem.split("-")[:-1])) for f in input_files()],
        )


def load(input_file: Path) -> dict:
    with open(input_file, encoding="utf-8") as file:
        return json.load(file)


def get_wallet_addr(app_client: EthAppClient, bip32_path: str = BIP32_PATH) -> bytes:
    with app_client.get_public_addr(display=False, bip32_path=bip32_path):
        pass
    _, addr, _ = ResponseParser.pk_addr(app_client.response().data)
    return addr


def enable_blind_signing(scenario_navigator: NavigateWithScenario) -> None:
    settings_toggle(
        scenario_navigator.backend.device,
        scenario_navigator.navigator,
        [SettingID.BLIND_SIGNING],
    )


def sign_and_approve(
    scenario_navigator: NavigateWithScenario,
    data: dict,
    bip32_path: str = BIP32_PATH,
) -> tuple[int, int, int]:
    """Deliver the message, approve the review, and return the (v, r, s) signature."""
    app_client = EthAppClient(scenario_navigator.backend)

    json_to_tlv.process_data(app_client, data, bip32_path)
    with app_client.eip712_v2_sign():
        # a raw display always warns about blind signing
        scenario_navigator.review_approve_with_warning(do_comparison=False, nb_warnings=1)
    return ResponseParser.signature(app_client.response().data)


def test_eip712_v2_sign(scenario_navigator: NavigateWithScenario, input_file: Path):
    """The app must derive the reference EIP-712 hash for every known typed message."""
    enable_blind_signing(scenario_navigator)
    data = load(input_file)
    vrs = sign_and_approve(scenario_navigator, data)
    assert get_wallet_addr(EthAppClient(scenario_navigator.backend)) == recover_message(data, vrs)


def test_eip712_v2_derivation_path(scenario_navigator: NavigateWithScenario):
    """The signing key must come from the path carried by EIP712_VALUES, not a default one."""
    enable_blind_signing(scenario_navigator)
    data = load(Path(input_files()[0]))
    vrs = sign_and_approve(scenario_navigator, data, OTHER_BIP32_PATH)
    addr = get_wallet_addr(EthAppClient(scenario_navigator.backend), OTHER_BIP32_PATH)
    assert addr == recover_message(data, vrs)


def test_eip712_v2_reject(scenario_navigator: NavigateWithScenario):
    """Rejecting the review must not produce a signature."""
    enable_blind_signing(scenario_navigator)
    app_client = EthAppClient(scenario_navigator.backend)

    json_to_tlv.process_data(app_client, load(Path(input_files()[0])), BIP32_PATH)
    with pytest.raises(ExceptionRAPDU) as e:
        with app_client.eip712_v2_sign():
            scenario_navigator.review_reject_with_warning(do_comparison=False)
    assert e.value.status == StatusWord.SWO_CONDITIONS_NOT_SATISFIED


def test_eip712_v2_blind_signing_disabled(scenario_navigator: NavigateWithScenario):
    """Without the blind signing setting, a raw message must be refused rather than displayed."""
    app_client = EthAppClient(scenario_navigator.backend)

    json_to_tlv.process_data(app_client, load(Path(input_files()[0])), BIP32_PATH)
    with pytest.raises(ExceptionRAPDU) as e:
        with app_client.eip712_v2_sign():
            pass
    assert e.value.status == StatusWord.SWO_INCORRECT_DATA


def test_eip712_v2_values_without_schema(scenario_navigator: NavigateWithScenario):
    """Values sent before any schema must be refused."""
    app_client = EthAppClient(scenario_navigator.backend)

    payload = json_to_tlv.values_from_json(load(Path(input_files()[0])), BIP32_PATH).serialize()
    with pytest.raises(ExceptionRAPDU) as e:
        app_client.eip712_v2_send_values(payload)
    assert e.value.status == StatusWord.SWO_COMMAND_NOT_ALLOWED


def test_eip712_v2_sign_without_values(scenario_navigator: NavigateWithScenario):
    """Signing before a message has been delivered must be refused."""
    app_client = EthAppClient(scenario_navigator.backend)

    response = app_client.eip712_v2_send_schema(json_to_tlv.schema_from_json(load(Path(input_files()[0]))).serialize())
    assert response.status == StatusWord.SWO_SUCCESS

    with pytest.raises(ExceptionRAPDU) as e:
        with app_client.eip712_v2_sign():
            pass
    assert e.value.status == StatusWord.SWO_COMMAND_NOT_ALLOWED


def test_eip712_v2_sign_twice(scenario_navigator: NavigateWithScenario):
    """A second signature must not be obtainable from an already consumed message."""
    enable_blind_signing(scenario_navigator)
    sign_and_approve(scenario_navigator, load(Path(input_files()[0])))

    app_client = EthAppClient(scenario_navigator.backend)
    with pytest.raises(ExceptionRAPDU) as e:
        with app_client.eip712_v2_sign():
            pass
    assert e.value.status == StatusWord.SWO_COMMAND_NOT_ALLOWED


# --- Negative tests ---------------------------------------------------------------------------
#
# Every payload below is malformed or out of sequence, and the app must refuse it. A refusal
# resets the signing flow, so each rejection is followed by a valid message going through.


def minimal_data() -> dict:
    return {
        "types": {
            "EIP712Domain": [{"name": "name", "type": "string"}, {"name": "chainId", "type": "uint256"}],
            "Test": [{"name": "flag", "type": "bool"}],
        },
        "primaryType": "Test",
        "domain": {"name": "Test", "chainId": 1},
        "message": {"flag": True},
    }


def chain_data(levels: int) -> dict:
    """A message made of `levels` structs nested into each other."""
    types: dict = {"EIP712Domain": [{"name": "name", "type": "string"}]}
    for i in range(levels):
        types[f"L{i}"] = [{"name": "depth", "type": "uint256"}]
        if i + 1 < levels:
            types[f"L{i}"].append({"name": "nested", "type": f"L{i + 1}"})
    message: dict = {"depth": levels - 1}
    for i in reversed(range(levels - 1)):
        message = {"depth": i, "nested": message}
    return {"types": types, "primaryType": "L0", "domain": {"name": "Test"}, "message": message}


def expect_status(status: StatusWord, call: Callable[[], object]) -> None:
    with pytest.raises(ExceptionRAPDU) as e:
        call()
    assert e.value.status == status


def reset_app(app_client: EthAppClient) -> None:
    """Reset the app with a command it refuses, which frees the message the test left loaded."""
    expect_status(
        StatusWord.SWO_INCORRECT_P1_P2,
        lambda: app_client.send_raw(CLA, InsType.EIP712_SIGN, P1_UNKNOWN, P2Type.EIP712_V2_IMPLEM, b""),
    )


def assert_recovers(app_client: EthAppClient) -> None:
    """After a refusal, a valid message must be accepted again."""
    json_to_tlv.process_data(app_client, minimal_data(), BIP32_PATH)
    reset_app(app_client)


def send_first_chunk(
    app_client: EthAppClient,
    ins: InsType,
    payload: bytes,
    declared_size: int | None = None,
):
    """Send a first chunk, whose announced size may differ from the data it actually carries."""
    size = len(payload) if declared_size is None else declared_size
    return app_client.send_raw(CLA, ins, P1Type.FIRST_CHUNK, P2Type.EIP712_V2_IMPLEM, struct.pack(">H", size) + payload)


def tlv(*entries: tuple[int, int | str | bytes]) -> bytes:
    return b"".join(TlvSerializable.serialize_field(tag, value) for tag, value in entries)


def field(name: str, sol_type: SolType | int, **kwargs) -> Eip712Field:
    # a type the protocol does not define is sent as its raw number
    return Eip712Field(STRUCT_VERSION, name, cast(SolType, sol_type), **kwargs)


def struct_def(name: str, *fields: Eip712Field) -> Eip712Struct:
    return Eip712Struct(STRUCT_VERSION, name, list(fields))


def domain_struct() -> Eip712Struct:
    return struct_def("EIP712Domain", field("name", SolType.STRING), field("chainId", SolType.UINT, type_size=32))


def flag_struct() -> Eip712Struct:
    return struct_def("Test", field("flag", SolType.BOOL))


def schema_of(*structs: Eip712Struct) -> bytes:
    return Eip712Schema(STRUCT_VERSION, list(structs)).serialize()


def schema_with_field(message_field: Eip712Field) -> bytes:
    return schema_of(domain_struct(), struct_def("Test", message_field))


def schema_with_struct_named(name: str) -> bytes:
    return schema_of(domain_struct(), struct_def(name, field("flag", SolType.BOOL)))


def schema_with_raw_struct(raw: bytes) -> bytes:
    return tlv(
        (Eip712SchemaTag.VERSION, STRUCT_VERSION),
        (Eip712SchemaTag.STRUCT, domain_struct().serialize()),
        (Eip712SchemaTag.STRUCT, raw),
    )


def schema_with_raw_field(raw: bytes) -> bytes:
    return schema_with_raw_struct(
        tlv(
            (Eip712StructTag.VERSION, STRUCT_VERSION),
            (Eip712StructTag.NAME, "Test"),
            (Eip712StructTag.FIELD, raw),
        )
    )


# what a name must not hold is what would make the schema ambiguous: the encoded type string
# ('(' ')' ',' and, for a type, the space and the brackets), and a NUL, which cuts a name short.
# Everything else is a name, and how it looks is for the review to judge
BAD_NAMES = {
    "comma": "a,b",
    "open_paren": "a(b",
    "close_paren": "a)b",
    "nul": "a\x00b",
}
# a struct name, and a reference to one, also delimit a type: a field name may hold these, or be empty
BAD_STRUCT_NAMES = BAD_NAMES | {
    "empty": "",
    "space": "a b",
    "open_bracket": "a[0",
    "close_bracket": "a]",
    "brackets": "a[0]",
}

INVALID_FIELDS = {
    "int_without_size": field("x", SolType.INT),
    "uint_without_size": field("x", SolType.UINT),
    "bytes_fix_without_size": field("x", SolType.BYTES_FIX),
    "int_size_33": field("x", SolType.INT, type_size=33),
    "uint_size_33": field("x", SolType.UINT, type_size=33),
    "bytes_fix_size_0": field("x", SolType.BYTES_FIX, type_size=0),
    "bytes_fix_size_33": field("x", SolType.BYTES_FIX, type_size=33),
    "uint_size_0": field("x", SolType.UINT, type_size=0),
    "string_with_size": field("x", SolType.STRING, type_size=1),
    "address_with_size": field("x", SolType.ADDRESS, type_size=20),
    "bool_with_size": field("x", SolType.BOOL, type_size=1),
    "bytes_dyn_with_size": field("x", SolType.BYTES_DYN, type_size=1),
    "struct_with_size": field("x", SolType.STRUCT, type_size=1, struct_name="Test"),
    "struct_without_struct_name": field("x", SolType.STRUCT),
    "struct_name_on_scalar": field("x", SolType.BOOL, struct_name="Test"),
    "unknown_type": field("x", 0x08),
    "nine_array_dims": field("x", SolType.BOOL, array_dims=[ArrayDim() for _ in range(9)]),
    "array_dim_size_256": field("x", SolType.BOOL, array_dims=[ArrayDim(256)]),
}

INVALID_SCHEMAS: dict[str, Callable[[], bytes]] = {
    "schema_version": lambda: Eip712Schema(2, [domain_struct(), flag_struct()]).serialize(),
    "struct_version": lambda: schema_of(domain_struct(), Eip712Struct(2, "Test", [field("flag", SolType.BOOL)])),
    "field_version": lambda: schema_of(domain_struct(), struct_def("Test", Eip712Field(2, "flag", SolType.BOOL))),
    "no_struct": lambda: schema_of(),
    "duplicate_struct_name": lambda: schema_of(domain_struct(), flag_struct(), flag_struct()),
    "duplicate_field_name": lambda: schema_of(
        domain_struct(), struct_def("Test", field("flag", SolType.BOOL), field("flag", SolType.UINT, type_size=1))
    ),
    "schema_without_version": lambda: tlv((Eip712SchemaTag.STRUCT, domain_struct().serialize())),
    "schema_duplicate_version": lambda: tlv(
        (Eip712SchemaTag.VERSION, STRUCT_VERSION),
        (Eip712SchemaTag.VERSION, STRUCT_VERSION),
        (Eip712SchemaTag.STRUCT, domain_struct().serialize()),
    ),
    "struct_without_version": lambda: schema_with_raw_struct(
        tlv((Eip712StructTag.NAME, "Test"), (Eip712StructTag.FIELD, field("flag", SolType.BOOL).serialize()))
    ),
    "struct_without_name": lambda: schema_with_raw_struct(
        tlv((Eip712StructTag.VERSION, STRUCT_VERSION), (Eip712StructTag.FIELD, field("flag", SolType.BOOL).serialize()))
    ),
    "struct_duplicate_name_tag": lambda: schema_with_raw_struct(
        tlv(
            (Eip712StructTag.VERSION, STRUCT_VERSION),
            (Eip712StructTag.NAME, "Test"),
            (Eip712StructTag.NAME, "Other"),
            (Eip712StructTag.FIELD, field("flag", SolType.BOOL).serialize()),
        )
    ),
    "field_without_version": lambda: schema_with_raw_field(
        tlv((Eip712FieldTag.NAME, "flag"), (Eip712FieldTag.TYPE, SolType.BOOL))
    ),
    "field_without_name": lambda: schema_with_raw_field(
        tlv((Eip712FieldTag.VERSION, STRUCT_VERSION), (Eip712FieldTag.TYPE, SolType.BOOL))
    ),
    "field_without_type": lambda: schema_with_raw_field(
        tlv((Eip712FieldTag.VERSION, STRUCT_VERSION), (Eip712FieldTag.NAME, "flag"))
    ),
    "field_duplicate_name_tag": lambda: schema_with_raw_field(
        tlv(
            (Eip712FieldTag.VERSION, STRUCT_VERSION),
            (Eip712FieldTag.NAME, "flag"),
            (Eip712FieldTag.NAME, "other"),
            (Eip712FieldTag.TYPE, SolType.BOOL),
        )
    ),
    # a tag that the structure does not define is refused, not skipped
    "schema_unknown_tag": lambda: schema_of(domain_struct(), flag_struct()) + tlv((UNKNOWN_TAG, 1)),
    "struct_unknown_tag": lambda: schema_with_raw_struct(
        tlv(
            (Eip712StructTag.VERSION, STRUCT_VERSION),
            (Eip712StructTag.NAME, "Test"),
            (Eip712StructTag.FIELD, field("flag", SolType.BOOL).serialize()),
            (UNKNOWN_TAG, 1),
        )
    ),
    "field_unknown_tag": lambda: schema_with_raw_field(
        tlv(
            (Eip712FieldTag.VERSION, STRUCT_VERSION),
            (Eip712FieldTag.NAME, "flag"),
            (Eip712FieldTag.TYPE, SolType.BOOL),
            (UNKNOWN_TAG, 1),
        )
    ),
}
INVALID_SCHEMAS |= {f"field_{label}": partial(schema_with_field, f) for label, f in INVALID_FIELDS.items()}
INVALID_SCHEMAS |= {f"struct_name_{label}": partial(schema_with_struct_named, name) for label, name in BAD_STRUCT_NAMES.items()}
INVALID_SCHEMAS |= {
    f"field_name_{label}": partial(schema_with_field, field(name, SolType.BOOL)) for label, name in BAD_NAMES.items()
}
INVALID_SCHEMAS |= {
    f"field_struct_name_{label}": partial(schema_with_field, field("x", SolType.STRUCT, struct_name=name))
    for label, name in BAD_STRUCT_NAMES.items()
}


@pytest.mark.parametrize("build", INVALID_SCHEMAS.values(), ids=INVALID_SCHEMAS.keys())
def test_eip712_v2_schema_rejected(scenario_navigator: NavigateWithScenario, build: Callable[[], bytes]):
    """A malformed schema must be refused, and must leave the app able to start over."""
    app_client = EthAppClient(scenario_navigator.backend)

    expect_status(StatusWord.SWO_INCORRECT_DATA, lambda: app_client.eip712_v2_send_schema(build()))
    assert_recovers(app_client)


def struct_without_field(where: str) -> dict:
    data = minimal_data()
    if where == "domain":
        data["types"]["EIP712Domain"] = []
        data["domain"] = {}
    else:
        data["types"]["Empty"] = []
        data["types"]["Test"].insert(0, {"name": "empty", "type": "Empty"})
        data["message"]["empty"] = {}
    return data


@pytest.mark.parametrize("where", ["domain", "member"])
def test_eip712_v2_struct_without_field(scenario_navigator: NavigateWithScenario, where: str):
    """A struct without member, which EIP-712 allows, is hashed as its type hash alone."""
    enable_blind_signing(scenario_navigator)
    data = struct_without_field(where)

    vrs = sign_and_approve(scenario_navigator, data)
    assert get_wallet_addr(EthAppClient(scenario_navigator.backend)) == recover_message(data, vrs)


def values_fields(data: dict | None = None, bip32_path: str = BIP32_PATH) -> list[tuple[int, int | str | bytes]]:
    """The top-level entries of an EIP712_VALUES payload, in the order the app expects them."""
    values = json_to_tlv.values_from_json(data or minimal_data(), bip32_path)
    return [
        (Eip712ValuesTag.VERSION, STRUCT_VERSION),
        (Eip712ValuesTag.PRIMARY_TYPE, values.primary_type),
        # the app derives the path length from the payload size, so the count byte is dropped
        (Eip712ValuesTag.DERIVATION_PATH, pack_derivation_path(bip32_path)[1:]),
        (Eip712ValuesTag.DOMAIN, values.domain.serialize()),
        (Eip712ValuesTag.MESSAGE, values.message.serialize()),
    ]


def replaced(fields: list[tuple[int, int | str | bytes]], tag: int, value: int | str | bytes):
    return [(t, value if t == tag else v) for t, v in fields]


def dropped(fields: list[tuple[int, int | str | bytes]], tag: int):
    return [(t, v) for t, v in fields if t != tag]


def derivation_path_of(length: int) -> bytes:
    return struct.pack(f">{length}I", *[HARDENED + i for i in range(length)])


LEAF_TRUE = b"\x01"

INVALID_VALUES: dict[str, Callable[[list], list]] = {
    "version": lambda f: replaced(f, Eip712ValuesTag.VERSION, 2),
    "duplicate_version": lambda f: [*f, (Eip712ValuesTag.VERSION, STRUCT_VERSION)],
    "unknown_tag": lambda f: [*f, (UNKNOWN_TAG, 1)],
    "primary_type_unknown": lambda f: replaced(f, Eip712ValuesTag.PRIMARY_TYPE, "Missing"),
    "primary_type_empty": lambda f: replaced(f, Eip712ValuesTag.PRIMARY_TYPE, ""),
    # the primary type is looked up by its whole name: a prefix, an extension or a NUL is another name
    "primary_type_prefix_of_a_struct": lambda f: replaced(f, Eip712ValuesTag.PRIMARY_TYPE, "Tes"),
    "primary_type_extending_a_struct": lambda f: replaced(f, Eip712ValuesTag.PRIMARY_TYPE, "Tests"),
    "primary_type_with_a_nul": lambda f: replaced(f, Eip712ValuesTag.PRIMARY_TYPE, "Test\x00"),
    "primary_type_with_a_nul_inside": lambda f: replaced(f, Eip712ValuesTag.PRIMARY_TYPE, "Te\x00st"),
    "message_before_primary_type": lambda f: sorted(f, key=lambda e: e[0] != Eip712ValuesTag.MESSAGE),
    "without_version": lambda f: dropped(f, Eip712ValuesTag.VERSION),
    "without_primary_type": lambda f: dropped(f, Eip712ValuesTag.PRIMARY_TYPE),
    "without_derivation_path": lambda f: dropped(f, Eip712ValuesTag.DERIVATION_PATH),
    "without_domain": lambda f: dropped(f, Eip712ValuesTag.DOMAIN),
    "without_message": lambda f: dropped(f, Eip712ValuesTag.MESSAGE),
    "derivation_path_empty": lambda f: replaced(f, Eip712ValuesTag.DERIVATION_PATH, b""),
    "derivation_path_unaligned": lambda f: replaced(f, Eip712ValuesTag.DERIVATION_PATH, derivation_path_of(2) + b"\x01"),
    "derivation_path_too_long": lambda f: replaced(
        f, Eip712ValuesTag.DERIVATION_PATH, derivation_path_of(MAX_DERIVATION_PATH_LENGTH + 1)
    ),
    # the entry count must not be truncated to a byte: 260 would otherwise be read as 4
    "derivation_path_count_wraps_a_byte": lambda f: replaced(f, Eip712ValuesTag.DERIVATION_PATH, derivation_path_of(256 + 4)),
    "domain_value_missing": lambda f: replaced(f, Eip712ValuesTag.DOMAIN, Eip712ValueSeq([b"Test"]).serialize()),
    "domain_value_extra": lambda f: replaced(
        f, Eip712ValuesTag.DOMAIN, Eip712ValueSeq([b"Test", LEAF_TRUE, LEAF_TRUE]).serialize()
    ),
    "message_value_missing": lambda f: replaced(f, Eip712ValuesTag.MESSAGE, Eip712ValueSeq([]).serialize()),
    "message_value_extra": lambda f: replaced(f, Eip712ValuesTag.MESSAGE, Eip712ValueSeq([LEAF_TRUE, LEAF_TRUE]).serialize()),
    "message_sequence_for_leaf": lambda f: replaced(
        f, Eip712ValuesTag.MESSAGE, Eip712ValueSeq([Eip712ValueSeq([LEAF_TRUE])]).serialize()
    ),
}


@pytest.mark.parametrize("mutate", INVALID_VALUES.values(), ids=INVALID_VALUES.keys())
def test_eip712_v2_values_rejected(scenario_navigator: NavigateWithScenario, mutate: Callable[[list], list]):
    """A malformed value payload must be refused, and must leave the app able to start over."""
    app_client = EthAppClient(scenario_navigator.backend)

    response = app_client.eip712_v2_send_schema(json_to_tlv.schema_from_json(minimal_data()).serialize())
    assert response.status == StatusWord.SWO_SUCCESS

    expect_status(StatusWord.SWO_INCORRECT_DATA, lambda: app_client.eip712_v2_send_values(tlv(*mutate(values_fields()))))
    assert_recovers(app_client)


# a value that fits each type, so that a message can be built before one of its leaves is replaced
LEAF_TYPES = {
    "uint8": 0,
    "uint256": 0,
    "int8": 0,
    "int16": 0,
    "int256": 0,
    "bytes4": "0x00000000",
    "address": "0x" + "00" * 20,
    "bool": False,
}

# leaves that no value of their type is encoded as: not minimal, too long, too short, or empty
INVALID_LEAVES = {
    "uint8_two_bytes": ("uint8", b"\x01\x00"),
    "uint8_empty": ("uint8", b""),
    "uint8_leading_zero": ("uint8", b"\x00\x01"),
    "uint8_zero_on_two_bytes": ("uint8", b"\x00\x00"),
    "uint256_leading_zero": ("uint256", b"\x00\x01"),
    "uint256_33_bytes": ("uint256", b"\xff" * 33),
    "int8_two_bytes": ("int8", b"\x01\x00"),
    "int8_empty": ("int8", b""),
    "int16_leading_zero": ("int16", b"\x00\x7f"),
    "int16_leading_zero_before_a_top_bit": ("int16", b"\x00\x80"),
    "int256_33_bytes": ("int256", b"\xff" * 33),
    "bytes4_short": ("bytes4", b"\x01\x02\x03"),
    "bytes4_long": ("bytes4", b"\x01\x02\x03\x04\x05"),
    "bytes4_empty": ("bytes4", b""),
    "address_short": ("address", b"\x00" * 19),
    "address_long": ("address", b"\x00" * 21),
    "address_empty": ("address", b""),
    "bool_two": ("bool", b"\x02"),
    "bool_two_bytes": ("bool", b"\x00\x01"),
    "bool_empty": ("bool", b""),
}


def data_with_field_of_type(sol_type: str) -> dict:
    data = minimal_data()
    data["types"]["Test"] = [{"name": "x", "type": sol_type}]
    data["message"] = {"x": LEAF_TYPES[sol_type]}
    return data


@pytest.mark.parametrize("sol_type, leaf", INVALID_LEAVES.values(), ids=INVALID_LEAVES.keys())
def test_eip712_v2_leaf_rejected(scenario_navigator: NavigateWithScenario, sol_type: str, leaf: bytes):
    """A leaf that is not the one encoding of a value of its type must be refused."""
    app_client = EthAppClient(scenario_navigator.backend)
    data = data_with_field_of_type(sol_type)

    response = app_client.eip712_v2_send_schema(json_to_tlv.schema_from_json(data).serialize())
    assert response.status == StatusWord.SWO_SUCCESS

    fields = replaced(values_fields(data), Eip712ValuesTag.MESSAGE, Eip712ValueSeq([leaf]).serialize())
    expect_status(StatusWord.SWO_INCORRECT_DATA, lambda: app_client.eip712_v2_send_values(tlv(*fields)))
    assert_recovers(app_client)


def test_eip712_v2_schema_without_domain(scenario_navigator: NavigateWithScenario):
    """Values cannot be built without the domain struct, so the message must be refused."""
    app_client = EthAppClient(scenario_navigator.backend)

    app_client.eip712_v2_send_schema(schema_of(flag_struct()))
    expect_status(StatusWord.SWO_INCORRECT_DATA, lambda: app_client.eip712_v2_send_values(tlv(*values_fields())))
    assert_recovers(app_client)


# a field that refers to a struct, with a value of it, or none in an empty array
UNRESOLVED_REFERENCES = {
    "a_value": ([], Eip712ValueSeq([LEAF_TRUE])),
    "an_empty_array": ([ArrayDim()], Eip712ValueSeq([])),
}


@pytest.mark.parametrize("array_dims, value", UNRESOLVED_REFERENCES.values(), ids=UNRESOLVED_REFERENCES.keys())
def test_eip712_v2_unresolved_struct_reference(
    scenario_navigator: NavigateWithScenario, array_dims: list[ArrayDim], value: Eip712ValueSeq
):
    """A field referring to a struct the schema never defines is refused with the values, even when they hold none."""
    app_client = EthAppClient(scenario_navigator.backend)

    app_client.eip712_v2_send_schema(schema_with_field(field("x", SolType.STRUCT, struct_name="Missing", array_dims=array_dims)))
    message = Eip712ValueSeq([value]).serialize()
    fields = replaced(values_fields(), Eip712ValuesTag.MESSAGE, message)
    expect_status(StatusWord.SWO_INCORRECT_DATA, lambda: app_client.eip712_v2_send_values(tlv(*fields)))
    assert_recovers(app_client)


@pytest.mark.parametrize("count, accepted", [(1, False), (2, True), (3, False)])
def test_eip712_v2_fixed_array_cardinality(scenario_navigator: NavigateWithScenario, count: int, accepted: bool):
    """A fixed-size array must hold exactly the number of elements its schema declares."""
    app_client = EthAppClient(scenario_navigator.backend)
    data = minimal_data()
    data["types"]["Test"] = [{"name": "values", "type": "uint8[2]"}]

    app_client.eip712_v2_send_schema(json_to_tlv.schema_from_json(data).serialize())
    message = Eip712ValueSeq([Eip712ValueSeq([LEAF_TRUE] * count)]).serialize()
    fields = replaced(values_fields(), Eip712ValuesTag.MESSAGE, message)
    if accepted:
        assert app_client.eip712_v2_send_values(tlv(*fields)).status == StatusWord.SWO_SUCCESS
        reset_app(app_client)
    else:
        expect_status(StatusWord.SWO_INCORRECT_DATA, lambda: app_client.eip712_v2_send_values(tlv(*fields)))
        assert_recovers(app_client)


@pytest.mark.parametrize("levels, accepted", [(MAX_DEPTH, True), (MAX_DEPTH + 1, False)])
def test_eip712_v2_depth_limit(scenario_navigator: NavigateWithScenario, levels: int, accepted: bool):
    """Nesting is bounded by the stack, so a message deeper than the limit must be refused."""
    app_client = EthAppClient(scenario_navigator.backend)
    data = chain_data(levels)

    app_client.eip712_v2_send_schema(json_to_tlv.schema_from_json(data).serialize())
    payload = json_to_tlv.values_from_json(data, BIP32_PATH).serialize()
    if accepted:
        assert app_client.eip712_v2_send_values(payload).status == StatusWord.SWO_SUCCESS
        reset_app(app_client)
    else:
        expect_status(StatusWord.SWO_INCORRECT_DATA, lambda: app_client.eip712_v2_send_values(payload))
        assert_recovers(app_client)


def test_eip712_v2_derivation_path_max_length(scenario_navigator: NavigateWithScenario):
    """The longest supported derivation path must still be accepted."""
    app_client = EthAppClient(scenario_navigator.backend)

    app_client.eip712_v2_send_schema(json_to_tlv.schema_from_json(minimal_data()).serialize())
    fields = replaced(values_fields(), Eip712ValuesTag.DERIVATION_PATH, derivation_path_of(MAX_DERIVATION_PATH_LENGTH))
    assert app_client.eip712_v2_send_values(tlv(*fields)).status == StatusWord.SWO_SUCCESS
    reset_app(app_client)


def test_eip712_v2_schema_resent(scenario_navigator: NavigateWithScenario):
    """A second schema must not replace the first one once it is complete."""
    app_client = EthAppClient(scenario_navigator.backend)
    payload = json_to_tlv.schema_from_json(minimal_data()).serialize()

    assert app_client.eip712_v2_send_schema(payload).status == StatusWord.SWO_SUCCESS
    expect_status(StatusWord.SWO_COMMAND_NOT_ALLOWED, lambda: app_client.eip712_v2_send_schema(payload))
    assert_recovers(app_client)


def test_eip712_v2_schema_chunk_after_complete_schema(scenario_navigator: NavigateWithScenario):
    """No chunk may extend a schema that is already complete."""
    app_client = EthAppClient(scenario_navigator.backend)

    app_client.eip712_v2_send_schema(json_to_tlv.schema_from_json(minimal_data()).serialize())
    expect_status(
        StatusWord.SWO_COMMAND_NOT_ALLOWED,
        lambda: app_client.send_raw(
            CLA, InsType.EIP712_SEND_STRUCT_DEF, P1Type.FOLLOWING_CHUNK, P2Type.EIP712_V2_IMPLEM, b"\x00"
        ),
    )
    assert_recovers(app_client)


def test_eip712_v2_following_chunk_without_first(scenario_navigator: NavigateWithScenario):
    """A continuation chunk with nothing to continue must be refused."""
    app_client = EthAppClient(scenario_navigator.backend)

    expect_status(
        StatusWord.SWO_COMMAND_NOT_ALLOWED,
        lambda: app_client.send_raw(
            CLA, InsType.EIP712_SEND_STRUCT_DEF, P1Type.FOLLOWING_CHUNK, P2Type.EIP712_V2_IMPLEM, b"\x00"
        ),
    )
    assert_recovers(app_client)


def test_eip712_v2_values_twice(scenario_navigator: NavigateWithScenario):
    """A second set of values must not replace the first one."""
    app_client = EthAppClient(scenario_navigator.backend)
    json_to_tlv.process_data(app_client, minimal_data(), BIP32_PATH)

    payload = json_to_tlv.values_from_json(minimal_data(), BIP32_PATH).serialize()
    expect_status(StatusWord.SWO_COMMAND_NOT_ALLOWED, lambda: app_client.eip712_v2_send_values(payload))
    assert_recovers(app_client)


def send_v1_struct_definition(app_client: EthAppClient) -> None:
    with app_client.eip712_send_struct_def_struct_name("Test"):
        pass


def send_v1_struct_implementation(app_client: EthAppClient) -> None:
    with app_client.eip712_send_struct_impl_root_struct("Test"):
        pass


def send_v1_filtering(app_client: EthAppClient) -> None:
    with app_client.eip712_filtering_activate():
        pass


def send_v1_sign(app_client: EthAppClient) -> None:
    with app_client.eip712_sign_new(BIP32_PATH):
        pass


V1_COMMANDS = {
    "struct_definition": send_v1_struct_definition,
    "struct_implementation": send_v1_struct_implementation,
    "filtering": send_v1_filtering,
    "sign": send_v1_sign,
}


@pytest.mark.parametrize("values_sent", [False, True])
@pytest.mark.parametrize("send_v1_command", V1_COMMANDS.values(), ids=V1_COMMANDS.keys())
def test_eip712_v2_session_refuses_v1_commands(
    scenario_navigator: NavigateWithScenario, send_v1_command: Callable[[EthAppClient], None], values_sent: bool
):
    """A V1 command must not go on in a V2 session, with the schema only or with the values too."""
    app_client = EthAppClient(scenario_navigator.backend)
    data = minimal_data()
    app_client.eip712_v2_send_schema(json_to_tlv.schema_from_json(data).serialize())
    if values_sent:
        app_client.eip712_v2_send_values(json_to_tlv.values_from_json(data, BIP32_PATH).serialize())

    expect_status(StatusWord.SWO_COMMAND_NOT_ALLOWED, lambda: send_v1_command(app_client))
    assert_recovers(app_client)


@pytest.mark.parametrize("ins", [InsType.EIP712_SEND_STRUCT_DEF, InsType.EIP712_SEND_STRUCT_IMPL, InsType.EIP712_SIGN])
def test_eip712_v2_unknown_p1(scenario_navigator: NavigateWithScenario, ins: InsType):
    """Only the documented P1 values are accepted, whichever the command."""
    app_client = EthAppClient(scenario_navigator.backend)

    # values and signing are only reachable once a schema is in place
    if ins != InsType.EIP712_SEND_STRUCT_DEF:
        app_client.eip712_v2_send_schema(json_to_tlv.schema_from_json(minimal_data()).serialize())
    expect_status(
        StatusWord.SWO_INCORRECT_P1_P2,
        lambda: app_client.send_raw(CLA, ins, P1_UNKNOWN, P2Type.EIP712_V2_IMPLEM, b""),
    )
    assert_recovers(app_client)


def test_eip712_v2_sign_with_data(scenario_navigator: NavigateWithScenario):
    """The sign command carries no data, so any must be refused."""
    enable_blind_signing(scenario_navigator)
    app_client = EthAppClient(scenario_navigator.backend)
    json_to_tlv.process_data(app_client, minimal_data(), BIP32_PATH)

    expect_status(
        StatusWord.SWO_INCORRECT_DATA,
        lambda: app_client.send_raw(CLA, InsType.EIP712_SIGN, P1Type.COMPLETE_SEND, P2Type.EIP712_V2_IMPLEM, b"\x00"),
    )
    assert_recovers(app_client)


def test_eip712_v2_schema_truncated(scenario_navigator: NavigateWithScenario):
    """A schema that never completes must not allow values to be sent."""
    app_client = EthAppClient(scenario_navigator.backend)
    schema = json_to_tlv.schema_from_json(minimal_data()).serialize()

    response = send_first_chunk(app_client, InsType.EIP712_SEND_STRUCT_DEF, schema[:-1], declared_size=len(schema))
    assert response.status == StatusWord.SWO_SUCCESS
    payload = json_to_tlv.values_from_json(minimal_data(), BIP32_PATH).serialize()
    expect_status(StatusWord.SWO_COMMAND_NOT_ALLOWED, lambda: app_client.eip712_v2_send_values(payload))


def test_eip712_v2_values_truncated(scenario_navigator: NavigateWithScenario):
    """Values that never complete must not allow a signature."""
    enable_blind_signing(scenario_navigator)
    app_client = EthAppClient(scenario_navigator.backend)
    app_client.eip712_v2_send_schema(json_to_tlv.schema_from_json(minimal_data()).serialize())
    payload = json_to_tlv.values_from_json(minimal_data(), BIP32_PATH).serialize()

    response = send_first_chunk(app_client, InsType.EIP712_SEND_STRUCT_IMPL, payload[:-1], declared_size=len(payload))
    assert response.status == StatusWord.SWO_SUCCESS

    def sign():
        with app_client.eip712_v2_sign():
            pass

    expect_status(StatusWord.SWO_COMMAND_NOT_ALLOWED, sign)


def test_eip712_v2_values_oversized(scenario_navigator: NavigateWithScenario):
    """A payload longer than announced must be refused."""
    app_client = EthAppClient(scenario_navigator.backend)
    app_client.eip712_v2_send_schema(json_to_tlv.schema_from_json(minimal_data()).serialize())
    payload = json_to_tlv.values_from_json(minimal_data(), BIP32_PATH).serialize()

    expect_status(
        StatusWord.SWO_INCORRECT_DATA,
        lambda: send_first_chunk(app_client, InsType.EIP712_SEND_STRUCT_IMPL, payload, declared_size=len(payload) - 1),
    )
    assert_recovers(app_client)


@pytest.mark.parametrize("length", [200, 2000])
def test_eip712_v2_long_struct_names(scenario_navigator: NavigateWithScenario, length: int):
    """A struct name has no length of its own: the primary type, a referenced type and its reference are hashed like any other."""
    enable_blind_signing(scenario_navigator)
    data = minimal_data()
    primary = "A" * length
    nested = "B" * length
    data["types"].pop("Test")
    data["types"][primary] = [{"name": "inner", "type": nested}, {"name": "flag", "type": "bool"}]
    data["types"][nested] = [{"name": "flag", "type": "bool"}]
    data["primaryType"] = primary
    data["message"] = {"inner": {"flag": False}, "flag": True}

    vrs = sign_and_approve(scenario_navigator, data)
    assert get_wallet_addr(EthAppClient(scenario_navigator.backend)) == recover_message(data, vrs)


def test_eip712_v2_struct_names_larger_than_the_heap_are_refused(scenario_navigator: NavigateWithScenario):
    """Only the memory bounds a name: one that does not fit is refused, and the next message goes through."""
    app_client = EthAppClient(scenario_navigator.backend)
    name = "A" * 6000
    payload = schema_of(
        domain_struct(),
        struct_def("Test", field("inner", SolType.STRUCT, struct_name=name)),
        struct_def(name, field("flag", SolType.BOOL)),
    )

    expect_status(StatusWord.SWO_INCORRECT_DATA, lambda: app_client.eip712_v2_send_schema(payload))
    assert_recovers(app_client)


def test_eip712_v2_names_are_not_identifiers(scenario_navigator: NavigateWithScenario):
    """The standard puts no condition on a field name, and hosts send struct names that are no identifier."""
    enable_blind_signing(scenario_navigator)
    data = minimal_data()
    data["types"].pop("Test")
    # the primary type of a Hyperliquid message
    data["types"]["HyperliquidTransaction:UsdSend"] = [
        {"name": "from-address", "type": "bool"},
        {"name": "1st", "type": "uint8"},
        {"name": "Order Details", "type": "uint8"},
        {"name": "a.b", "type": "uint8"},
        {"name": "a[0]", "type": "uint8"},
        {"name": "_$", "type": "uint8"},
        {"name": 'q"uote', "type": "uint8"},
        {"name": "back\\slash", "type": "uint8"},
    ]
    data["primaryType"] = "HyperliquidTransaction:UsdSend"
    data["message"] = {
        "from-address": True,
        "1st": 1,
        "Order Details": 2,
        "a.b": 3,
        "a[0]": 4,
        "_$": 5,
        'q"uote': 6,
        "back\\slash": 7,
    }

    vrs = sign_and_approve(scenario_navigator, data)
    assert get_wallet_addr(EthAppClient(scenario_navigator.backend)) == recover_message(data, vrs)


def test_eip712_v2_struct_that_refers_to_itself(scenario_navigator: NavigateWithScenario):
    """A struct can hold an array of its own type: the type hash lists it once, and the values nest as deep as they go."""
    enable_blind_signing(scenario_navigator)
    data = minimal_data()
    data["types"].pop("Test")
    data["types"]["Node"] = [{"name": "v", "type": "uint8"}, {"name": "children", "type": "Node[]"}]
    data["primaryType"] = "Node"
    data["message"] = {
        "v": 1,
        "children": [{"v": 2, "children": [{"v": 3, "children": []}]}, {"v": 4, "children": []}],
    }

    vrs = sign_and_approve(scenario_navigator, data)
    assert get_wallet_addr(EthAppClient(scenario_navigator.backend)) == recover_message(data, vrs)


def test_eip712_v2_names_that_are_not_shown_can_be_anything(scenario_navigator: NavigateWithScenario):
    """Only the key of a value the review shows has to be displayable: a struct name or a field holding a struct are not."""
    enable_blind_signing(scenario_navigator)
    data = minimal_data()
    data["types"].pop("Test")
    data["types"]["Caf\u00e9"] = [{"name": "flag", "type": "bool"}]
    data["types"]["Test"] = [{"name": "caf\u00e9\x01", "type": "Caf\u00e9"}, {"name": "", "type": "Caf\u00e9"}]
    data["primaryType"] = "Test"
    data["message"] = {"caf\u00e9\x01": {"flag": True}, "": {"flag": False}}

    vrs = sign_and_approve(scenario_navigator, data)
    assert get_wallet_addr(EthAppClient(scenario_navigator.backend)) == recover_message(data, vrs)


# a key the screen cannot show: not ASCII, a control character, or nothing visible
NOT_DISPLAYABLE_KEYS = {
    "empty": "",
    "non_ascii": "caf\u00e9",
    "control": "a\x01b",
    "delete": "a\x7fb",
    "only_spaces": "  ",
}


@pytest.mark.parametrize("key", NOT_DISPLAYABLE_KEYS.values(), ids=NOT_DISPLAYABLE_KEYS.keys())
def test_eip712_v2_key_that_cannot_be_shown_refuses_the_review(scenario_navigator: NavigateWithScenario, key: str):
    """The key of a displayed value is part of the review: a message that cannot be shown whole must not be signed."""
    enable_blind_signing(scenario_navigator)
    app_client = EthAppClient(scenario_navigator.backend)
    data = minimal_data()
    data["types"]["Test"] = [{"name": key, "type": "bool"}]
    data["message"] = {key: True}

    json_to_tlv.process_data(app_client, data, BIP32_PATH)
    with pytest.raises(ExceptionRAPDU) as e:
        with app_client.eip712_v2_sign():
            pass
    assert e.value.status == StatusWord.SWO_INCORRECT_DATA
    assert_recovers(app_client)


@pytest.mark.parametrize("where", ["domain", "message"])
def test_eip712_v2_string_with_a_nul_refuses_the_review(scenario_navigator: NavigateWithScenario, where: str):
    """A NUL would end the text the review shows while the whole string is signed."""
    enable_blind_signing(scenario_navigator)
    app_client = EthAppClient(scenario_navigator.backend)
    data = minimal_data()
    data["types"]["Test"] = [{"name": "text", "type": "string"}]
    data["message"] = {"text": "ab"}
    if where == "domain":
        data["domain"]["name"] = "a\x00b"
    else:
        data["message"]["text"] = "a\x00b"

    json_to_tlv.process_data(app_client, data, BIP32_PATH)
    with pytest.raises(ExceptionRAPDU) as e:
        with app_client.eip712_v2_sign():
            pass
    assert e.value.status == StatusWord.SWO_INCORRECT_DATA
    assert_recovers(app_client)


# values at the edge of their type, each one sent as its one encoding: the integers are minimal, a
# negative one has the full width of its type, and the top bit of a shorter value does not make it one
BOUNDARY_VALUES = [
    ("uint8", 0),
    ("uint8", 255),
    ("uint256", 0),
    ("uint256", 256),
    ("uint256", 2**256 - 1),
    ("int8", 0),
    ("int8", 127),
    ("int8", -1),
    ("int8", -128),
    ("int16", 128),
    ("int16", 255),
    ("int16", 32767),
    ("int16", -1),
    ("int16", -32768),
    ("int256", 2**255 - 1),
    ("int256", -(2**255)),
    ("bytes4", "0x00000000"),
    ("bytes32", "0x" + "ff" * 32),
    ("address", "0x" + "ff" * 20),
    ("bool", False),
    ("bool", True),
    ("string", ""),
    ("bytes", "0x"),
]


def test_eip712_v2_values_at_the_edge_of_their_type(scenario_navigator: NavigateWithScenario):
    """Each edge value is accepted and hashed as the standard says."""
    enable_blind_signing(scenario_navigator)
    data = minimal_data()
    data["types"]["Test"] = [{"name": f"f{i}", "type": sol_type} for i, (sol_type, _) in enumerate(BOUNDARY_VALUES)]
    data["message"] = {f"f{i}": value for i, (_, value) in enumerate(BOUNDARY_VALUES)}

    vrs = sign_and_approve(scenario_navigator, data)
    assert get_wallet_addr(EthAppClient(scenario_navigator.backend)) == recover_message(data, vrs)


# hex values the JSON may hold in a shorter form: a bytesN value shorter than its type is
# left-aligned, and an odd number of digits gets a leading zero
SHORT_HEX_VALUES = [
    ("bytes4", "0x1234"),
    ("bytes4", "0x123"),
    ("bytes", "0x123"),
]


def test_eip712_v2_short_hex_values(scenario_navigator: NavigateWithScenario):
    """The client encodes a short hex value as the standard does, so the signature is the one of the JSON."""
    enable_blind_signing(scenario_navigator)
    data = minimal_data()
    data["types"]["Test"] = [{"name": f"f{i}", "type": sol_type} for i, (sol_type, _) in enumerate(SHORT_HEX_VALUES)]
    data["message"] = {f"f{i}": value for i, (_, value) in enumerate(SHORT_HEX_VALUES)}

    vrs = sign_and_approve(scenario_navigator, data)
    assert get_wallet_addr(EthAppClient(scenario_navigator.backend)) == recover_message(data, vrs)


def test_eip712_v2_verbose_eip712_allows_raw_display(scenario_navigator: NavigateWithScenario):
    """Verbose EIP-712 alone is enough to display a raw message, with blind signing left off."""
    settings_toggle(
        scenario_navigator.backend.device,
        scenario_navigator.navigator,
        [SettingID.VERBOSE_EIP712],
    )
    data = minimal_data()
    vrs = sign_and_approve(scenario_navigator, data)
    assert get_wallet_addr(EthAppClient(scenario_navigator.backend)) == recover_message(data, vrs)


def strings_data(count: int) -> dict:
    """A message of `count` strings, each told apart by its key and its value."""
    keys = [f"k{i:02d}" for i in range(count)]
    return {
        "types": {
            "EIP712Domain": [{"name": "name", "type": "string"}],
            "Test": [{"name": key, "type": "string"} for key in keys],
        },
        "primaryType": "Test",
        "domain": {"name": "Test"},
        "message": {key: f"v{i:02d}" for i, key in enumerate(keys)},
    }


def test_eip712_v2_review(scenario_navigator: NavigateWithScenario):
    """A raw review shows the signer, every value past the slots the values are formatted in, then the hashes."""
    settings_toggle(
        scenario_navigator.backend.device,
        scenario_navigator.navigator,
        [SettingID.BLIND_SIGNING, SettingID.DISPLAY_HASH],
    )
    # the domain and the message have more values than a touch screen has slots, and a Nano 2
    data = strings_data(10)
    app_client = EthAppClient(scenario_navigator.backend)

    json_to_tlv.process_data(app_client, data, BIP32_PATH)
    with app_client.eip712_v2_sign():
        scenario_navigator.review_approve_with_warning(nb_warnings=1)
    vrs = ResponseParser.signature(app_client.response().data)
    assert get_wallet_addr(app_client) == recover_message(data, vrs)


def max_review_rows(scenario_navigator: NavigateWithScenario) -> int:
    """The rows a review can have: 255, and 252 on a Nano, whose title, approve and reject pages count."""
    return 252 if scenario_navigator.backend.device.is_nano else 255


def rows_data(rows: int) -> dict:
    """A message of the given number of rows: the signer gives 1, the domain 2 and each element of
    its array one."""
    data = minimal_data()
    data["types"]["Test"] = [{"name": "ids", "type": "uint8[]"}]
    data["message"] = {"ids": [i % 256 for i in range(rows - 3)]}
    return data


def test_eip712_v2_review_hashes_count_among_the_rows(scenario_navigator: NavigateWithScenario):
    """The two rows of the hashes are rows of the review, which has a limit."""
    settings_toggle(
        scenario_navigator.backend.device,
        scenario_navigator.navigator,
        [SettingID.BLIND_SIGNING, SettingID.DISPLAY_HASH],
    )
    app_client = EthAppClient(scenario_navigator.backend)
    # a row short of the limit, which the rows of the hashes go over
    data = rows_data(max_review_rows(scenario_navigator) - 1)

    json_to_tlv.process_data(app_client, data, BIP32_PATH)
    with pytest.raises(ExceptionRAPDU) as e:
        with app_client.eip712_v2_sign():
            pass
    assert e.value.status == StatusWord.SWO_INCORRECT_DATA
    assert_recovers(app_client)


def test_eip712_v2_review_rows_up_to_the_limit(scenario_navigator: NavigateWithScenario):
    """A message of as many rows as the review can have is signed."""
    enable_blind_signing(scenario_navigator)
    data = rows_data(max_review_rows(scenario_navigator))
    vrs = sign_and_approve(scenario_navigator, data)
    assert get_wallet_addr(EthAppClient(scenario_navigator.backend)) == recover_message(data, vrs)


def test_eip712_v2_review_rows_over_the_limit(scenario_navigator: NavigateWithScenario):
    """A message of a row more than the review can have is refused before the review."""
    enable_blind_signing(scenario_navigator)
    app_client = EthAppClient(scenario_navigator.backend)
    data = rows_data(max_review_rows(scenario_navigator) + 1)

    json_to_tlv.process_data(app_client, data, BIP32_PATH)
    with pytest.raises(ExceptionRAPDU) as e:
        with app_client.eip712_v2_sign():
            pass
    assert e.value.status == StatusWord.SWO_INCORRECT_DATA
    assert_recovers(app_client)


def test_eip712_v2_review_too_big_for_memory_refused(scenario_navigator: NavigateWithScenario):
    """A message whose review text would not fit in memory is refused before the review.

    The elements of an array share their field name, which every slot of the review holds next to a
    value: 8 slots of a 7,936-character name and a 255-character string take more than 64 KiB.
    """
    enable_blind_signing(scenario_navigator)
    app_client = EthAppClient(scenario_navigator.backend)
    name = "a" * 7936
    data = minimal_data()
    data["types"]["EIP712Domain"] = [{"name": name, "type": "string[]"}]
    data["domain"] = {name: ["x" * 255] * 8}

    json_to_tlv.process_data(app_client, data, BIP32_PATH)
    with pytest.raises(ExceptionRAPDU) as e:
        with app_client.eip712_v2_sign():
            pass
    assert e.value.status == StatusWord.SWO_INCORRECT_DATA
    assert_recovers(app_client)
