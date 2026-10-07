import struct
from enum import IntEnum

from ragger.tlv import TlvSerializable

from .signing_partners import CALLDATA_PARTNER
from .trusted_name import TrustedNameSource, TrustedNameType


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
    DEPLOY_DATE = 0x0A
    SIGNATURE = 0xFF


class TxInfo(TlvSerializable):
    version: int
    chain_id: int
    contract_addr: bytes
    selector: bytes
    fields_hash: bytes
    operation_type: str
    creator_name: str | None
    creator_legal_name: str | None
    creator_url: str | None
    contract_name: str | None
    deploy_date: int | None
    signature: bytes | None

    def __init__(
        self,
        version: int,
        chain_id: int,
        contract_addr: bytes,
        selector: bytes,
        fields_hash: bytes,
        operation_type: str,
        creator_name: str | None = None,
        creator_legal_name: str | None = None,
        creator_url: str | None = None,
        contract_name: str | None = None,
        deploy_date: int | None = None,
        signature: bytes | None = None,
    ):
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
            signature = CALLDATA_PARTNER.sign(bytes(payload))
        payload += self.serialize_field(TxInfoTag.SIGNATURE, signature)
        return bytes(payload)


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
    TOKEN = 0x0A
    NETWORK = 0x0B
    GROUP = 0x0C


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
    start: int | None
    end: int | None

    def __init__(self, weight: int = 1, start: int | None = None, end: int | None = None):
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
        return bytes(payload)


class PathRef(TlvSerializable):
    def __init__(self):
        pass

    def serialize(self) -> bytes:
        return b""


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
    start: int | None
    end: int | None

    def __init__(self, start: int | None = None, end: int | None = None):
        self.start = start
        self.end = end

    def serialize(self) -> bytes:
        payload = bytearray()
        if self.start is not None:
            payload += self.serialize_field(0x01, struct.pack(">h", self.start))
        if self.end is not None:
            payload += self.serialize_field(0x02, struct.pack(">h", self.end))
        return bytes(payload)


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
                raise AssertionError(f"Unknown path node type : {type(node)}")
            payload += self.serialize_field(tag, node.serialize())
        return bytes(payload)


class ContainerPath(IntEnum):
    FROM = 0x00
    TO = 0x01
    VALUE = 0x02
    CHAIN_ID = 0x03


class MapRef(TlvSerializable):
    version: int
    id: int
    key: "Value"

    def __init__(self, version: int, id: int, key: "Value"):
        self.version = version
        self.id = id
        self.key = key

    def serialize(self) -> bytes:
        payload = bytearray()
        payload += self.serialize_field(0x00, self.version)
        payload += self.serialize_field(0x01, self.id)
        payload += self.serialize_field(0x02, self.key.serialize())
        return bytes(payload)


class Value(TlvSerializable):
    version: int
    type_family: TypeFamily
    type_size: int | None
    data_path: DataPath | None
    container_path: ContainerPath | None
    constant: bytes | None
    map_ref: MapRef | None

    def __init__(
        self,
        version: int,
        type_family: TypeFamily,
        type_size: int | None = None,
        data_path: DataPath | None = None,
        container_path: ContainerPath | None = None,
        constant: bytes | None = None,
        map_ref: MapRef | None = None,
    ):
        self.version = version
        self.type_family = type_family
        self.type_size = type_size
        self.data_path = data_path
        self.container_path = container_path
        self.constant = constant
        self.map_ref = map_ref

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
        if self.map_ref is not None:
            payload += self.serialize_field(0x06, self.map_ref.serialize())
        return bytes(payload)


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
        return bytes(payload)


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
        return bytes(payload)


class ParamTokenAmount(FieldParam):
    version: int
    value: Value
    token: Value | None
    native_currency: list[bytes] | None
    threshold: int | None
    above_threshold_msg: str | None

    def __init__(
        self,
        version: int,
        value: Value,
        token: Value | None = None,
        native_currency: list[bytes] | None = None,
        threshold: int | None = None,
        above_threshold_msg: str | None = None,
    ):
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
        return bytes(payload)


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
        return bytes(payload)


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
        return bytes(payload)


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
        return bytes(payload)


class ParamUnit(FieldParam):
    version: int
    value: Value
    base: str
    decimals: int | None
    prefix: bool | None

    def __init__(
        self,
        version: int,
        value: Value,
        base: str,
        decimals: int | None = None,
        prefix: bool | None = None,
    ):
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
        return bytes(payload)


class TrustedNameValueType(IntEnum):
    STANDARD = 0x00
    INTEROPERABLE = 0x01


class ParamTrustedName(FieldParam):
    version: int
    value: Value
    types: list[TrustedNameType]
    sources: list[TrustedNameSource]
    sender_addrs: list[bytes] | None
    value_type: TrustedNameValueType | None

    def __init__(
        self,
        version: int,
        value: Value,
        types: list[TrustedNameType],
        sources: list[TrustedNameSource],
        sender_addrs: list[bytes] | None = None,
        value_type: TrustedNameValueType | None = None,
    ):
        self.type = ParamType.TRUSTED_NAME
        self.version = version
        self.value = value
        self.types = types
        self.sources = sources
        self.sender_addrs = sender_addrs
        self.value_type = value_type

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
        if self.value_type is not None:
            payload += self.serialize_field(0x05, self.value_type)
        return bytes(payload)


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
        return bytes(payload)


class ParamCalldata(FieldParam):
    version: int
    calldata: Value
    contract_addr: Value
    chain_id: Value | None
    selector: Value | None
    amount: Value | None
    spender: Value | None

    def __init__(
        self,
        version: int,
        calldata: Value,
        contract_addr: Value,
        chain_id: Value | None = None,
        selector: Value | None = None,
        amount: Value | None = None,
        spender: Value | None = None,
    ):
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
        return bytes(payload)


class ParamToken(FieldParam):
    version: int
    addr: Value
    native_currency: list[bytes] | None

    def __init__(self, version, addr: Value, native_currency: list[bytes] | None = None):
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
        return bytes(payload)


class ParamNetwork(FieldParam):
    version: int
    value: Value

    def __init__(self, version: int, value: Value):
        self.type = ParamType.NETWORK
        self.version = version
        self.value = value

    def serialize(self) -> bytes:
        payload = bytearray()
        payload += self.serialize_field(0x00, self.version)
        payload += self.serialize_field(0x01, self.value.serialize())
        return bytes(payload)


class FieldTag(IntEnum):
    VERSION = 0x00
    NAME = 0x01
    PARAM_TYPE = 0x02
    PARAM = 0x03
    VISIBLE = 0x04
    CONSTRAINT = 0x05
    SEPARATOR = 0x06


class VisibleType(IntEnum):
    ALWAYS = 0x00
    MUST_BE = 0x01
    IF_NOT_IN = 0x02


class GroupIterationType(IntEnum):
    BUNDLED = 0x00
    SEQUENTIAL = 0x01


class Field(TlvSerializable):
    version: int
    name: str
    param: FieldParam
    visible: VisibleType | None
    constraints: list[bytes] | None
    separator: str | None

    def __init__(
        self,
        version: int,
        name: str,
        param: FieldParam,
        visible: VisibleType | None = None,
        constraints: list[bytes] | None = None,
        separator: str | None = None,
    ):
        self.version = version
        self.name = name
        self.param = param
        self.visible = visible
        self.constraints = constraints
        self.separator = separator

    def serialize(self) -> bytes:
        payload = bytearray()
        payload += self.serialize_field(FieldTag.VERSION, self.version)
        payload += self.serialize_field(FieldTag.NAME, self.name)
        payload += self.serialize_field(FieldTag.PARAM_TYPE, self.param.type)
        payload += self.serialize_field(FieldTag.PARAM, self.param.serialize())
        if self.visible is not None:
            payload += self.serialize_field(FieldTag.VISIBLE, self.visible)
        if self.constraints is not None:
            for constraint in self.constraints:
                payload += self.serialize_field(FieldTag.CONSTRAINT, constraint)
        if self.separator is not None:
            payload += self.serialize_field(FieldTag.SEPARATOR, self.separator)
        return bytes(payload)


class ParamGroup(FieldParam):
    """PARAM_GROUP: a collection of sub-fields with controlled iteration."""

    version: int
    iteration_type: GroupIterationType
    fields: list[Field]

    def __init__(self, version: int, iteration_type: GroupIterationType, fields: list[Field]):
        self.type = ParamType.GROUP
        self.version = version
        self.iteration_type = iteration_type
        self.fields = fields

    def serialize(self) -> bytes:
        payload = bytearray()
        payload += self.serialize_field(0x00, self.version)
        payload += self.serialize_field(0x01, self.iteration_type)
        for field in self.fields:
            payload += self.serialize_field(0x02, field.serialize())
        return bytes(payload)
