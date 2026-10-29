"""Unit tests for the Ethereum client command builder.

Pure Python: these check the bytes the builder puts on the wire, with no device.
"""

import hashlib

import pytest
from client.client import EIP712CalldataParamPresence
from client.command_builder import CommandBuilder, InsType, P1Type, P2Type
from client.eip712.struct import EIP712FieldType
from client.gcs import (
    DataPath,
    Eip712Path,
    Eip712PathArray,
    Eip712PathField,
    Eip712PathLeafSlice,
    Field,
    ParamRaw,
    TypeFamily,
    Value,
    fields_hash,
)


def test_eip712_calldata_info_flags_are_not_narrowed():
    """The callee and spender flags are three-valued, and the signature covers them as
    bytes (InputData.send_filtering_calldata_info appends int(flag)). Packing them as
    booleans in the APDU made PRESENT_VERIFYING_CONTRACT arrive as PRESENT_FILTERED,
    which no longer matched the signed payload."""
    verifying_contract = EIP712CalldataParamPresence.PRESENT_VERIFYING_CONTRACT
    apdu = CommandBuilder().eip712_filtering_calldata_info(
        index=0,
        value_filter_flag=True,
        callee_filter_flag=verifying_contract,
        chain_id_filter_flag=False,
        selector_filter_flag=False,
        amount_filter_flag=True,
        spender_filter_flag=verifying_contract,
        sig=b"",
    )
    # 5-byte APDU header, then index, value, callee, chain_id, selector, amount, spender
    cdata = apdu[5:]
    assert cdata[2] == verifying_contract, "callee flag narrowed"
    assert cdata[6] == verifying_contract, "spender flag narrowed"


def test_eip712_struct_field_rejects_non_ascii():
    with pytest.raises(UnicodeEncodeError):
        CommandBuilder().eip712_send_struct_def_struct_field(EIP712FieldType.STRING, "", None, [], "prénom")


def test_eip712_discarded_path_rejects_non_ascii():
    with pytest.raises(UnicodeEncodeError):
        CommandBuilder().eip712_filtering_discarded_path("données.montant")


def test_eip712_filtering_display_name_rejects_non_ascii():
    with pytest.raises(UnicodeEncodeError):
        CommandBuilder().eip712_filtering_datetime("Échéance", b"", False)


def test_eip712_v2_field_desc_uses_the_field_instruction_with_the_v2_p2():
    """The descriptors of a typed message are the FIELD instruction with P2 = 02, in every chunk."""
    payload = bytes(range(256)) * 2
    chunks = CommandBuilder().eip712_v2_field_desc(payload)
    assert len(chunks) > 1
    for position, chunk in enumerate(chunks):
        _cla, ins, p1, p2, lc = chunk[:5]
        assert ins == InsType.PROVIDE_TRANSACTION_FIELD_DESC
        assert p1 == (P1Type.FIRST_CHUNK if position == 0 else P1Type.FOLLOWING_CHUNK)
        assert p2 == P2Type.EIP712_V2_IMPLEM
        assert lc == len(chunk) - 5
    # the length of the TLV payload comes first, then the payload itself
    assert b"".join(chunk[5:] for chunk in chunks) == len(payload).to_bytes(2, "big") + payload


def test_transaction_field_desc_keeps_the_transaction_p2():
    """The calls a typed message names are described as transactions, with P2 = 00."""
    chunk = CommandBuilder().provide_transaction_field_desc(b"\x00")[0]
    assert chunk[3] == 0x00


def test_eip712_path_serialization():
    path = Eip712Path(
        1,
        [
            Eip712PathField(3),
            Eip712PathArray(),
            Eip712PathArray(1, -1),
            Eip712PathField(0),
            Eip712PathLeafSlice(2, 4),
        ],
    )
    expected_parts = (
        "000101",  # VERSION
        "010103",  # STRUCT_FIELD 3
        "0200",  # ARRAY_SLICE, all the elements
        "0208" + "01020001" + "0202ffff",  # ARRAY_SLICE 1..-1
        "010100",  # STRUCT_FIELD 0
        "0308" + "01020002" + "02020004",  # LEAF_SLICE 2..4
    )
    assert path.serialize() == bytes.fromhex("".join(expected_parts))


def test_eip712_path_is_the_tag_03_of_a_value():
    path = Eip712Path(1, [Eip712PathField(0)])
    value = Value(1, TypeFamily.UINT, 32, eip712_path=path)
    assert value.serialize().hex() == (
        "000101"  # VERSION
        "010101"  # TYPE_FAMILY
        "020120"  # TYPE_SIZE
        "0306" + path.serialize().hex()  # the path
    )


def test_a_value_has_one_path():
    path = Eip712Path(1, [Eip712PathField(0)])
    with pytest.raises(AssertionError):
        Value(1, TypeFamily.UINT, 32, data_path=DataPath(1, []), eip712_path=path)


def test_fields_hash_is_the_sha3_of_the_descriptors():
    fields = [
        Field(1, "One", ParamRaw(1, Value(1, TypeFamily.UINT, 32, constant=b"\x01"))),
        Field(1, "Two", ParamRaw(1, Value(1, TypeFamily.UINT, 32, constant=b"\x02"))),
    ]
    expected = hashlib.sha3_256(fields[0].serialize() + fields[1].serialize()).digest()
    assert fields_hash(fields) == expected
    assert fields_hash([]) == hashlib.sha3_256().digest()
    assert fields_hash(fields[::-1]) != expected
