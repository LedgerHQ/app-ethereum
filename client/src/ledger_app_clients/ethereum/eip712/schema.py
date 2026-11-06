from enum import IntEnum

from ragger.tlv import TlvSerializable


class SolType(IntEnum):
    STRUCT = 0x00
    INT = 0x01
    UINT = 0x02
    ADDRESS = 0x03
    BOOL = 0x04
    STRING = 0x05
    BYTES_FIX = 0x06
    BYTES_DYN = 0x07


class ArrayDim:
    """One array dimension: a fixed size, or None for a dynamic dimension."""

    size: int | None

    def __init__(self, size: int | None = None):
        self.size = size

    def serialize_value(self) -> bytes | int:
        # An empty payload marks the dimension as dynamic; its presence marks it as fixed.
        return b"" if self.size is None else self.size


class Eip712FieldTag(IntEnum):
    VERSION = 0x00
    NAME = 0x01
    TYPE = 0x02
    TYPE_SIZE = 0x03
    ARRAY_DIM = 0x04
    STRUCT_NAME = 0x05


class Eip712Field(TlvSerializable):
    version: int
    name: str
    type: SolType
    type_size: int | None
    array_dims: list[ArrayDim] | None
    struct_name: str | None

    def __init__(
        self,
        version: int,
        name: str,
        type: SolType,
        type_size: int | None = None,
        array_dims: list[ArrayDim] | None = None,
        struct_name: str | None = None,
    ):
        self.version = version
        self.name = name
        self.type = type
        self.type_size = type_size
        self.array_dims = array_dims
        self.struct_name = struct_name

    def serialize(self) -> bytes:
        payload = bytearray()
        payload += self.serialize_field(Eip712FieldTag.VERSION, self.version)
        payload += self.serialize_field(Eip712FieldTag.NAME, self.name)
        payload += self.serialize_field(Eip712FieldTag.TYPE, self.type)
        if self.type_size is not None:
            payload += self.serialize_field(Eip712FieldTag.TYPE_SIZE, self.type_size)
        if self.array_dims is not None:
            for dim in self.array_dims:
                payload += self.serialize_field(Eip712FieldTag.ARRAY_DIM, dim.serialize_value())
        if self.struct_name is not None:
            payload += self.serialize_field(Eip712FieldTag.STRUCT_NAME, self.struct_name)
        return bytes(payload)


class Eip712StructTag(IntEnum):
    VERSION = 0x00
    NAME = 0x01
    FIELD = 0x02


class Eip712Struct(TlvSerializable):
    version: int
    name: str
    fields: list[Eip712Field]

    def __init__(self, version: int, name: str, fields: list[Eip712Field]):
        self.version = version
        self.name = name
        self.fields = fields

    def serialize(self) -> bytes:
        payload = bytearray()
        payload += self.serialize_field(Eip712StructTag.VERSION, self.version)
        payload += self.serialize_field(Eip712StructTag.NAME, self.name)
        for field in self.fields:
            payload += self.serialize_field(Eip712StructTag.FIELD, field.serialize())
        return bytes(payload)


class Eip712SchemaTag(IntEnum):
    VERSION = 0x00
    STRUCT = 0x01


class Eip712Schema(TlvSerializable):
    version: int
    structs: list[Eip712Struct]

    def __init__(self, version: int, structs: list[Eip712Struct]):
        self.version = version
        self.structs = structs

    def serialize(self) -> bytes:
        payload = bytearray()
        payload += self.serialize_field(Eip712SchemaTag.VERSION, self.version)
        for struct_ in self.structs:
            payload += self.serialize_field(Eip712SchemaTag.STRUCT, struct_.serialize())
        return bytes(payload)
