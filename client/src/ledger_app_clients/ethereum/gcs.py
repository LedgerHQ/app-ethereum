from typing import Optional
from enum import IntEnum
import struct

from .tlv import TlvSerializable
from .keychain import sign_data, Key
from .client import TrustedNameType, TrustedNameSource


class TxInfoTag(IntEnum):
    VERSION = 0x00
    CHAIN_ID = 0x01
    CONTRACT_ADDR = 0x02
    SELECTOR = 0x03
    FIELDS_HASH = 0x04
    OPERATION_TYPE = 0x05
    CREATOR_NAME = 0x06
    CREATOR_LEGAL_NAME = 0x07
    CREATOR_URL = 0x08
    CONTRACT_NAME = 0x09
    DEPLOY_DATE = 0x0a
    SIGNATURE = 0xff


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
        payload += self.serialize_field(TxInfoTag.VERSION, self.version)
        payload += self.serialize_field(TxInfoTag.CHAIN_ID, self.chain_id)
        payload += self.serialize_field(TxInfoTag.CONTRACT_ADDR, self.contract_addr)
        payload += self.serialize_field(TxInfoTag.SELECTOR, self.selector)
        payload += self.serialize_field(TxInfoTag.FIELDS_HASH, self.fields_hash)
        payload += self.serialize_field(TxInfoTag.OPERATION_TYPE, self.operation_type)
        if self.creator_name is not None:
            payload += self.serialize_field(TxInfoTag.CREATOR_NAME, self.creator_name)
        if self.creator_legal_name is not None:
            payload += self.serialize_field(TxInfoTag.CREATOR_LEGAL_NAME, self.creator_legal_name)
        if self.creator_url is not None:
            payload += self.serialize_field(TxInfoTag.CREATOR_URL, self.creator_url)
        if self.contract_name is not None:
            payload += self.serialize_field(TxInfoTag.CONTRACT_NAME, self.contract_name)
        if self.deploy_date is not None:
            payload += self.serialize_field(TxInfoTag.DEPLOY_DATE, self.deploy_date)
        signature = self.signature
        if signature is None:
            signature = sign_data(Key.CALLDATA, payload)
        payload += self.serialize_field(TxInfoTag.SIGNATURE, signature)
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


class FieldParam(TlvSerializable):
    type: ParamType


class ParamRaw(FieldParam):
    version: int
    value: Value

    def __init__(self, version: int, value: Value):
        self.type = ParamType.RAW
        self.version = version
        self.value = value

    def serialize(self) -> bytes:
        payload = bytearray()
        payload += self.serialize_field(0x00, self.version)
        payload += self.serialize_field(0x01, self.value.serialize())
        return payload


class ParamAmount(FieldParam):
    version: int
    value: Value

    def __init__(self, version: int, value: Value):
        self.type = ParamType.AMOUNT
        self.version = version
        self.value = value

    def serialize(self) -> bytes:
        payload = bytearray()
        payload += self.serialize_field(0x00, self.version)
        payload += self.serialize_field(0x01, self.value.serialize())
        return payload


class ParamTokenAmount(FieldParam):
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
        self.type = ParamType.TOKEN_AMOUNT
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


class ParamNFT(FieldParam):
    version: int
    id: Value
    collection: Value

    def __init__(self, version: int, id: Value, collection: Value):
        self.type = ParamType.NFT
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


class ParamDatetime(FieldParam):
    version: int
    value: Value
    dt_type: DatetimeType

    def __init__(self, version: int, value: Value, type: DatetimeType):
        self.type = ParamType.DATETIME
        self.version = version
        self.value = value
        self.dt_type = type

    def serialize(self) -> bytes:
        payload = bytearray()
        payload += self.serialize_field(0x00, self.version)
        payload += self.serialize_field(0x01, self.value.serialize())
        payload += self.serialize_field(0x02, self.dt_type)
        return payload


class ParamDuration(FieldParam):
    version: int
    value: Value

    def __init__(self, version: int, value: Value):
        self.type = ParamType.DURATION
        self.version = version
        self.value = value

    def serialize(self) -> bytes:
        payload = bytearray()
        payload += self.serialize_field(0x00, self.version)
        payload += self.serialize_field(0x01, self.value.serialize())
        return payload


class ParamUnit(FieldParam):
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
        self.type = ParamType.UNIT
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


class ParamTrustedName(FieldParam):
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
        self.type = ParamType.TRUSTED_NAME
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


class ParamEnum(FieldParam):
    version: int
    id: int
    value: Value

    def __init__(self, version: int, id: int, value: Value):
        self.type = ParamType.ENUM
        self.version = version
        self.id = id
        self.value = value

    def serialize(self) -> bytes:
        payload = bytearray()
        payload += self.serialize_field(0x00, self.version)
        payload += self.serialize_field(0x01, self.id)
        payload += self.serialize_field(0x02, self.value.serialize())
        return payload


class ParamCalldata(FieldParam):
    version: int
    calldata: Value
    contract_addr: Value
    chain_id: Optional[Value]
    selector: Optional[Value]
    amount: Optional[Value]
    spender: Optional[Value]

    def __init__(self,
                 version: int,
                 calldata: Value,
                 contract_addr: Value,
                 chain_id: Optional[Value] = None,
                 selector: Optional[Value] = None,
                 amount: Optional[Value] = None,
                 spender: Optional[Value] = None):
        self.type = ParamType.CALLDATA
        self.version = version
        self.calldata = calldata
        self.contract_addr = contract_addr
        self.chain_id = chain_id
        self.selector = selector
        self.amount = amount
        self.spender = spender

    def serialize(self) -> bytes:
        payload = bytearray()
        payload += self.serialize_field(0x00, self.version)
        payload += self.serialize_field(0x01, self.calldata.serialize())
        payload += self.serialize_field(0x02, self.contract_addr.serialize())
        if self.chain_id is not None:
            payload += self.serialize_field(0x03, self.chain_id.serialize())
        if self.selector is not None:
            payload += self.serialize_field(0x04, self.selector.serialize())
        if self.amount is not None:
            payload += self.serialize_field(0x05, self.amount.serialize())
        if self.spender is not None:
            payload += self.serialize_field(0x06, self.spender.serialize())
        return payload


class ParamToken(FieldParam):
    version: int
    addr: Value
    native_currency: Optional[list[bytes]]

    def __init__(self, version, addr: Value, native_currency: Optional[list[bytes]] = None):
        self.type = ParamType.TOKEN
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


class FieldTag(IntEnum):
    VERSION = 0x00
    NAME = 0x01
    PARAM_TYPE = 0x02
    PARAM = 0x03


class Field(TlvSerializable):
    version: int
    name: str
    param: FieldParam

    def __init__(self, version: int, name: str, param: FieldParam):
        self.version = version
        self.name = name
        self.param = param

    def serialize(self) -> bytes:
        payload = bytearray()
        payload += self.serialize_field(FieldTag.VERSION, self.version)
        payload += self.serialize_field(FieldTag.NAME, self.name)
        payload += self.serialize_field(FieldTag.PARAM_TYPE, self.param.type)
        payload += self.serialize_field(FieldTag.PARAM, self.param.serialize())
        return payload
