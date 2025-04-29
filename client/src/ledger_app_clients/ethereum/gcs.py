from typing import Optional
from enum import IntEnum
import struct

from .tlv import TlvSerializable
from .keychain import sign_data, Key
from .client import TrustedNameType, TrustedNameSource


class TxInfo(TlvSerializable):
    version: int
    chain_id: int
    contract_addr: bytes
    selector: bytes
    fields_hash: bytes
    operation_type: str
    creator_name: Optional[str]
    creator_legal_name: Optional[str]
    creator_url: Optional[str]
    contract_name: Optional[str]
    deploy_date: Optional[int]
    signature: Optional[bytes]

    def __init__(self,
                 version: int,
                 chain_id: int,
                 contract_addr: bytes,
                 selector: bytes,
                 fields_hash: bytes,
                 operation_type: str,
                 creator_name: Optional[str] = None,
                 creator_legal_name: Optional[str] = None,
                 creator_url: Optional[str] = None,
                 contract_name: Optional[str] = None,
                 deploy_date: Optional[int] = None,
                 signature: Optional[bytes] = None):
        self.version = version
        self.chain_id = chain_id
        self.contract_addr = contract_addr
        self.selector = selector
        self.fields_hash = fields_hash
        self.operation_type = operation_type
        self.creator_name = creator_name
        self.creator_legal_name = creator_legal_name
        self.creator_url = creator_url
        self.contract_name = contract_name
        self.deploy_date = deploy_date
        self.signature = signature

    def serialize(self) -> bytes:
        payload = bytearray()
        payload += self.serialize_field(0x00, self.version)
        payload += self.serialize_field(0x01, self.chain_id)
        payload += self.serialize_field(0x02, self.contract_addr)
        payload += self.serialize_field(0x03, self.selector)
        payload += self.serialize_field(0x04, self.fields_hash)
        payload += self.serialize_field(0x05, self.operation_type)
        if self.creator_name is not None:
            payload += self.serialize_field(0x06, self.creator_name)
        if self.creator_legal_name is not None:
            payload += self.serialize_field(0x07, self.creator_legal_name)
        if self.creator_url is not None:
            payload += self.serialize_field(0x08, self.creator_url)
        if self.contract_name is not None:
            payload += self.serialize_field(0x09, self.contract_name)
        if self.deploy_date is not None:
            payload += self.serialize_field(0x0a, self.deploy_date)
        signature = self.signature
        if signature is None:
            signature = sign_data(Key.CALLDATA, payload)
        payload += self.serialize_field(0xff, signature)
        return payload


class ParamType(IntEnum):
    RAW = 0x00
    AMOUNT = 0x01
    TOKEN_AMOUNT = 0x02
    NFT = 0x03
    DATETIME = 0x04
    DURATION = 0x05
    UNIT = 0x06
    ENUM = 0x07
    TRUSTED_NAME = 0x08
    CALLDATA = 0x09
    TOKEN = 0x0a


class TypeFamily(IntEnum):
    UINT = 0x01
    INT = 0x02
    UFIXED = 0x03
    FIXED = 0x04
    ADDRESS = 0x05
    BOOL = 0x06
    BYTES = 0x07
    STRING = 0x08


class PathTuple(TlvSerializable):
    value: int

    def __init__(self, value: int):
        self.value = value

    def serialize(self) -> bytes:
        return struct.pack(">H", self.value)


class PathArray(TlvSerializable):
    weight: int
    start: Optional[int]
    end: Optional[int]

    def __init__(self,
                 weight: int = 1,
                 start: Optional[int] = None,
                 end: Optional[int] = None):
        self.weight = weight
        self.start = start
        self.end = end

    def serialize(self) -> bytes:
        payload = bytearray()
        payload += self.serialize_field(0x01, self.weight)
        if self.start is not None:
            payload += self.serialize_field(0x02, struct.pack(">h", self.start))
        if self.end is not None:
            payload += self.serialize_field(0x03, struct.pack(">h", self.end))
        return payload


class PathRef(TlvSerializable):
    def __init__(self):
        pass

    def serialize(self) -> bytes:
        return bytes()


class PathLeafType(IntEnum):
    ARRAY = 0x01
    TUPLE = 0x02
    STATIC = 0x03
    DYNAMIC = 0x04


class PathLeaf(TlvSerializable):
    type: PathLeafType

    def __init__(self, type: PathLeafType):
        self.type = type

    def serialize(self) -> bytes:
        return struct.pack("B", self.type)


class PathSlice(TlvSerializable):
    start: Optional[int]
    end: Optional[int]

    def __init__(self, start: Optional[int] = None, end: Optional[int] = None):
        self.start = start
        self.end = end

    def serialize(self) -> bytes:
        payload = bytearray()
        if self.start is not None:
            payload += self.serialize_field(0x01, struct.pack(">h", self.start))
        if self.end is not None:
            payload += self.serialize_field(0x02, struct.pack(">h", self.end))
        return payload


class DataPath(TlvSerializable):
    version: int
    path: list[TlvSerializable]

    def __init__(self, version: int, path: list[TlvSerializable]):
        self.version = version
        self.path = path

    def serialize(self) -> bytes:
        payload = bytearray()
        payload += self.serialize_field(0x00, self.version)
        for node in self.path:
            if isinstance(node, PathTuple):
                tag = 0x01
            elif isinstance(node, PathArray):
                tag = 0x02
            elif isinstance(node, PathRef):
                tag = 0x03
            elif isinstance(node, PathLeaf):
                tag = 0x04
            elif isinstance(node, PathSlice):
                tag = 0x05
            else:
                assert False, f"Unknown path node type : {type(node)}"
            payload += self.serialize_field(tag, node.serialize())
        return payload


class ContainerPath(IntEnum):
    FROM = 0x00
    TO = 0x01
    VALUE = 0x02


class Value(TlvSerializable):
    version: int
    type_family: TypeFamily
    type_size: Optional[int]
    data_path: Optional[DataPath]
    container_path: Optional[ContainerPath]
    constant: Optional[bytes]

    def __init__(self,
                 version: int,
                 type_family: TypeFamily,
                 type_size: Optional[int] = None,
                 data_path: Optional[DataPath] = None,
                 container_path: Optional[ContainerPath] = None,
                 constant: Optional[bytes] = None):
        self.version = version
        self.type_family = type_family
        self.type_size = type_size
        self.data_path = data_path
        self.container_path = container_path
        self.constant = constant

    def serialize(self) -> bytes:
        payload = bytearray()
        payload += self.serialize_field(0x00, self.version)
        payload += self.serialize_field(0x01, self.type_family)
        if self.type_size is not None:
            payload += self.serialize_field(0x02, self.type_size)
        if self.data_path is not None:
            payload += self.serialize_field(0x03, self.data_path.serialize())
        if self.container_path is not None:
            payload += self.serialize_field(0x04, self.container_path)
        if self.constant is not None:
            payload += self.serialize_field(0x05, self.constant)
        return payload


class ParamRaw(TlvSerializable):
    version: int
    value: Value

    def __init__(self, version: int, value: Value):
        self.version = version
        self.value = value

    def serialize(self) -> bytes:
        payload = bytearray()
        payload += self.serialize_field(0x00, self.version)
        payload += self.serialize_field(0x01, self.value.serialize())
        return payload


class ParamAmount(TlvSerializable):
    version: int
    value: Value

    def __init__(self, version: int, value: Value):
        self.version = version
        self.value = value

    def serialize(self) -> bytes:
        payload = bytearray()
        payload += self.serialize_field(0x00, self.version)
        payload += self.serialize_field(0x01, self.value.serialize())
        return payload


class ParamTokenAmount(TlvSerializable):
    version: int
    value: Value
    token: Optional[Value]
    native_currency: Optional[list[bytes]]
    threshold: Optional[int]
    above_threshold_msg: Optional[str]

    def __init__(self,
                 version: int,
                 value: Value,
                 token: Optional[Value] = None,
                 native_currency: Optional[list[bytes]] = None,
                 threshold: Optional[int] = None,
                 above_threshold_msg: Optional[str] = None):
        self.version = version
        self.value = value
        self.token = token
        self.native_currency = native_currency
        self.threshold = threshold
        self.above_threshold_msg = above_threshold_msg

    def serialize(self) -> bytes:
        payload = bytearray()
        payload += self.serialize_field(0x00, self.version)
        payload += self.serialize_field(0x01, self.value.serialize())
        if self.token is not None:
            payload += self.serialize_field(0x02, self.token.serialize())
        if self.native_currency is not None:
            for nat_cur in self.native_currency:
                payload += self.serialize_field(0x03, nat_cur)
        if self.threshold is not None:
            payload += self.serialize_field(0x04, self.threshold)
        if self.above_threshold_msg is not None:
            payload += self.serialize_field(0x05, self.above_threshold_msg)
        return payload


class ParamNFT(TlvSerializable):
    version: int
    id: Value
    collection: Value

    def __init__(self, version: int, id: Value, collection: Value):
        self.version = version
        self.id = id
        self.collection = collection

    def serialize(self) -> bytes:
        payload = bytearray()
        payload += self.serialize_field(0x00, self.version)
        payload += self.serialize_field(0x01, self.id.serialize())
        payload += self.serialize_field(0x02, self.collection.serialize())
        return payload


class DatetimeType(IntEnum):
    DT_UNIX = 0x00
    DT_BLOCKHEIGHT = 0x01


class ParamDatetime(TlvSerializable):
    version: int
    value: Value
    type: DatetimeType

    def __init__(self, version: int, value: Value, type: DatetimeType):
        self.version = version
        self.value = value
        self.type = type

    def serialize(self) -> bytes:
        payload = bytearray()
        payload += self.serialize_field(0x00, self.version)
        payload += self.serialize_field(0x01, self.value.serialize())
        payload += self.serialize_field(0x02, self.type)
        return payload


class ParamDuration(TlvSerializable):
    version: int
    value: Value

    def __init__(self, version: int, value: Value):
        self.version = version
        self.value = value

    def serialize(self) -> bytes:
        payload = bytearray()
        payload += self.serialize_field(0x00, self.version)
        payload += self.serialize_field(0x01, self.value.serialize())
        return payload


class ParamUnit(TlvSerializable):
    version: int
    value: Value
    base: str
    decimals: Optional[int]
    prefix: Optional[bool]

    def __init__(self,
                 version: int,
                 value: Value,
                 base: str,
                 decimals: Optional[int] = None,
                 prefix: Optional[bool] = None):
        self.version = version
        self.value = value
        self.base = base
        self.decimals = decimals
        self.prefix = prefix

    def serialize(self) -> bytes:
        payload = bytearray()
        payload += self.serialize_field(0x00, self.version)
        payload += self.serialize_field(0x01, self.value.serialize())
        payload += self.serialize_field(0x02, self.base)
        if self.decimals is not None:
            payload += self.serialize_field(0x03, self.decimals)
        if self.prefix is not None:
            payload += self.serialize_field(0x04, self.prefix)
        return payload


class ParamTrustedName(TlvSerializable):
    version: int
    value: Value
    types: list[TrustedNameType]
    sources: list[TrustedNameSource]
    sender_addrs: Optional[list[bytes]]

    def __init__(self,
                 version: int,
                 value: Value,
                 types: list[TrustedNameType],
                 sources: list[TrustedNameSource],
                 sender_addrs: Optional[list[bytes]] = None):
        self.version = version
        self.value = value
        self.types = types
        self.sources = sources
        self.sender_addrs = sender_addrs

    def serialize(self) -> bytes:
        payload = bytearray()
        payload += self.serialize_field(0x00, self.version)
        payload += self.serialize_field(0x01, self.value.serialize())
        types = bytearray()
        for type in self.types:
            types.append(type)
        payload += self.serialize_field(0x02, types)
        sources = bytearray()
        for source in self.sources:
            sources.append(source)
        payload += self.serialize_field(0x03, sources)
        if self.sender_addrs is not None:
            for addr in self.sender_addrs:
                payload += self.serialize_field(0x04, addr)
        return payload


class ParamEnum(TlvSerializable):
    version: int
    id: int
    value: Value

    def __init__(self, version: int, id: int, value: Value):
        self.version = version
        self.id = id
        self.value = value

    def serialize(self) -> bytes:
        payload = bytearray()
        payload += self.serialize_field(0x00, self.version)
        payload += self.serialize_field(0x01, self.id)
        payload += self.serialize_field(0x02, self.value.serialize())
        return payload


class ParamToken(TlvSerializable):
    version: int
    addr: Value
    native_currency: Optional[list[bytes]]

    def __init__(self, version, addr: Value, native_currency: Optional[list[bytes]] = None):
        self.version = version
        self.addr = addr
        self.native_currency = native_currency

    def serialize(self) -> bytes:
        payload = bytearray()
        payload += self.serialize_field(0x00, self.version)
        payload += self.serialize_field(0x01, self.addr.serialize())
        if self.native_currency is not None:
            for nat_cur in self.native_currency:
                payload += self.serialize_field(0x02, nat_cur)
        return payload


class Field(TlvSerializable):
    version: int
    name: str
    param_type: ParamType
    param: TlvSerializable

    def __init__(self, version: int, name: str, param_type: ParamType, param: TlvSerializable):
        self.version = version
        self.name = name
        self.param_type = param_type
        self.param = param

    def serialize(self) -> bytes:
        payload = bytearray()
        payload += self.serialize_field(0x00, self.version)
        payload += self.serialize_field(0x01, self.name)
        payload += self.serialize_field(0x02, self.param_type)
        payload += self.serialize_field(0x03, self.param.serialize())
        return payload
