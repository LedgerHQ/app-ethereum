"""Build the EIP-712 V2 TLV payloads from a standard EIP-712 JSON document.

The input is the JSON structure defined by the EIP-712 standard itself, as loaded by
``json.load``: a ``types`` type dictionary, a ``primaryType`` name, and the ``domain`` and
``message`` value trees.
"""

import re
from typing import Any

from ..client import EthAppClient
from ..response_parser import signature
from ..status_word import StatusWord
from .schema import ArrayDim, Eip712Field, Eip712Schema, Eip712Struct, SolType
from .values import Eip712Values, Eip712ValueSeq

STRUCT_VERSION = 1
DOMAIN_STRUCT_NAME = "EIP712Domain"

# a native type may carry a bit-size suffix; anything else names a struct
_TYPE_SIZE_RE = re.compile(r"^(\w+?)(\d*)$")
_ARRAY_DIM_RE = re.compile(r"(.*)\[([0-9]*)\]$")


def _split_array_dims(typename: str) -> tuple[str, list[ArrayDim]]:
    """Strip the array suffixes off a type name.

    Solidity reads array dimensions outermost-last, while ``ARRAY_DIM`` entries are declared
    innermost-first, which is the order this returns them in.

    Input  = ``"uint8[2][]"``                      | ``"bool"``
    Output = ``('uint8', [ArrayDim(2), ArrayDim()])`` | ``('bool', [])``
    """
    dims: list[ArrayDim] = []
    while True:
        result = _ARRAY_DIM_RE.search(typename)
        if not result:
            return (typename, dims)
        typename = result.group(1)
        size = result.group(2)
        dims.insert(0, ArrayDim(int(size) if size else None))


def _parse_type(typename: str) -> tuple[SolType, int | None, str | None]:
    """Map a Solidity type name onto its TLV representation.

    Returns the base type, its size in bytes, and the referenced struct name.
    """
    result = _TYPE_SIZE_RE.search(typename)
    basename = result.group(1) if result else typename
    suffix = result.group(2) if result else ""

    if basename == "bytes":
        # a size suffix distinguishes the fixed-size type from the dynamic one
        if suffix:
            return (SolType.BYTES_FIX, int(suffix), None)
        return (SolType.BYTES_DYN, None, None)
    if basename in ("int", "uint"):
        assert suffix, f"Type {typename} must carry an explicit bit size"
        bits = int(suffix)
        assert bits % 8 == 0, f"Type {typename} has a bit size that is not a byte multiple"
        sol_type = SolType.INT if basename == "int" else SolType.UINT
        return (sol_type, bits // 8, None)
    if basename == "address" and not suffix:
        return (SolType.ADDRESS, None, None)
    if basename == "bool" and not suffix:
        return (SolType.BOOL, None, None)
    if basename == "string" and not suffix:
        return (SolType.STRING, None, None)
    # splitting a struct name on its digits would truncate it, so keep it whole
    return (SolType.STRUCT, None, typename)


def schema_from_json(data: dict) -> Eip712Schema:
    """Build the ``EIP712_SCHEMA`` payload from the JSON ``types`` dictionary."""
    types = data["types"]
    assert DOMAIN_STRUCT_NAME in types, f"Missing {DOMAIN_STRUCT_NAME} in types definition"

    structs = []
    for struct_name, struct_fields in types.items():
        fields = []
        for field in struct_fields:
            typename, array_dims = _split_array_dims(field["type"])
            sol_type, type_size, ref_name = _parse_type(typename)
            fields.append(
                Eip712Field(
                    STRUCT_VERSION,
                    field["name"],
                    sol_type,
                    type_size=type_size,
                    array_dims=array_dims if array_dims else None,
                    struct_name=ref_name,
                )
            )
        structs.append(Eip712Struct(STRUCT_VERSION, struct_name, fields))
    return Eip712Schema(STRUCT_VERSION, structs)


def _encode_integer(value: str | int, type_size: int, signed: bool) -> bytes:
    if isinstance(value, str):
        value = int(value, 0)
    if value == 0:
        return b"\x00"
    bits = type_size * 8
    lo, hi = (-(1 << (bits - 1)), (1 << (bits - 1)) - 1) if signed else (0, (1 << bits) - 1)
    assert lo <= value <= hi, f"Value {value} does not fit in a {bits}-bit {'int' if signed else 'uint'}"
    if value < 0:
        # a negative always keeps its full width, its sign living in the leading byte
        return value.to_bytes(type_size, "big", signed=True)
    # the app left-pads to 32 bytes on its side, so only the significant bytes are sent
    return value.to_bytes(type_size, "big").lstrip(b"\x00")


def _encode_hex_string(value: str, size: int) -> bytes:
    assert value.startswith("0x"), f"Expected a 0x-prefixed hex string, got {value!r}"
    digits = value[2:].rjust(size * 2, "0")
    assert len(digits) == (size * 2), f"Value {value!r} is wider than its {size}-byte type"
    return bytes.fromhex(digits)


def _decode_hex_bytes(value: str) -> bytes:
    assert value.startswith("0x"), f"Expected a 0x-prefixed hex string, got {value!r}"
    digits = value[2:]
    # an odd number of digits gets a leading zero, as eth_account does
    if len(digits) % 2:
        digits = "0" + digits
    return bytes.fromhex(digits)


def _encode_leaf(value: Any, sol_type: SolType, type_size: int | None) -> bytes:
    """Encode one scalar JSON value into the bytes carried by a ``LEAF`` entry.

    The app accepts one encoding per value only, see "Leaf encoding" in doc/tlv_structs.md.
    """
    match sol_type:
        case SolType.INT | SolType.UINT:
            assert type_size is not None
            return _encode_integer(value, type_size, sol_type == SolType.INT)
        case SolType.ADDRESS:
            return _encode_hex_string(value, 20)
        case SolType.BOOL:
            return _encode_integer(value, 1, False)
        case SolType.STRING:
            return value.encode()
        case SolType.BYTES_FIX:
            assert type_size is not None
            data = _decode_hex_bytes(value)
            assert len(data) <= type_size, f"Value {value!r} is wider than its {type_size}-byte type"
            # a shorter value is left-aligned, as the standard encodes it
            return data.ljust(type_size, b"\x00")
        case SolType.BYTES_DYN:
            return _decode_hex_bytes(value)
        case SolType.STRUCT:
            raise AssertionError("A struct is not a scalar value")


def _encode_value(
    value: Any,
    dims: list[ArrayDim],
    sol_type: SolType,
    type_size: int | None,
    struct_name: str | None,
    types: dict,
) -> Eip712ValueSeq | bytes:
    """Encode one JSON value into the entry the schema expects at its position."""
    if dims:
        # dimensions are declared innermost-first, so the outermost one is opened first
        outermost = dims[-1]
        assert isinstance(value, list), f"Expected a list for an array value, got {value!r}"
        if outermost.size is not None:
            assert len(value) == outermost.size, f"Array holds {len(value)} elements where the schema declares {outermost.size}"
        return Eip712ValueSeq([_encode_value(el, dims[:-1], sol_type, type_size, struct_name, types) for el in value])
    if sol_type == SolType.STRUCT:
        assert struct_name is not None
        return _struct_value_from_json(value, struct_name, types)
    return _encode_leaf(value, sol_type, type_size)


def _struct_value_from_json(values: dict, struct_name: str, types: dict) -> Eip712ValueSeq:
    """Build one struct instance, in the field order the schema declares.

    Entries are addressed by position, so the declaration order is what matters here — not the
    order the keys happen to appear in within the JSON object.
    """
    assert struct_name in types, f"Unknown struct {struct_name} in types definition"

    entries: list = []
    for field in types[struct_name]:
        assert field["name"] in values, f"Missing value for {struct_name}.{field['name']}"
        typename, dims = _split_array_dims(field["type"])
        sol_type, type_size, ref_name = _parse_type(typename)
        entries.append(_encode_value(values[field["name"]], dims, sol_type, type_size, ref_name, types))
    return Eip712ValueSeq(entries)


def values_from_json(data: dict, bip32_path: str) -> Eip712Values:
    """Build the ``EIP712_VALUES`` payload from the JSON domain and message trees."""
    types = data["types"]
    primary_type = data["primaryType"]
    return Eip712Values(
        STRUCT_VERSION,
        primary_type,
        bip32_path,
        _struct_value_from_json(data["domain"], DOMAIN_STRUCT_NAME, types),
        _struct_value_from_json(data["message"], primary_type, types),
    )


def process_data(app_client: EthAppClient, data: dict, bip32_path: str) -> None:
    """Send a whole EIP-712 JSON document to the app as the schema and values payloads."""
    response = app_client.eip712_v2_send_schema(schema_from_json(data).serialize())
    assert response.status == StatusWord.SWO_SUCCESS, f"Error sending schema: {response.status:#x}"

    response = app_client.eip712_v2_send_values(values_from_json(data, bip32_path).serialize())
    assert response.status == StatusWord.SWO_SUCCESS, f"Error sending values: {response.status:#x}"


def sign(app_client: EthAppClient, data: dict, bip32_path: str) -> tuple[int, int, int]:
    """Send a whole EIP-712 JSON document and sign it, returning the (v, r, s) signature."""
    process_data(app_client, data, bip32_path)

    # the review is not navigated here, so this only returns when the backend approves it by
    # itself; a test that navigates the review uses process_data() and eip712_v2_sign() instead
    with app_client.eip712_v2_sign():
        pass
    response = app_client.response()
    assert response.status == StatusWord.SWO_SUCCESS, f"Error signing: {response.status:#x}"
    return signature(response.data)
