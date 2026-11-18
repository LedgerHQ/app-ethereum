from enum import IntEnum

from ragger.tlv import TlvSerializable

from .signing_partners import CALLDATA_PARTNER


class Tag(IntEnum):
    VERSION = 0x00
    CHAIN_ID = 0x01
    CONTRACT_ADDR = 0x02
    SELECTOR = 0x03
    ID = 0x04
    VALUE = 0x05
    NAME = 0x06
    DOMAIN_SEPARATOR = 0x07
    PRIMARY_TYPE_HASH = 0x08
    SIGNATURE = 0xFF


class EnumValue(TlvSerializable):
    """ENUM_VALUE, to be sent before the field that uses it.

    It is attached to a function of a contract for a transaction (or for a call a typed message
    names): `chain_id`, `contract_addr` and `selector`. Or to a primary type of a domain for a typed
    message: `domain_separator` and `primary_type_hash`, the others being `None`.
    """

    version: int
    chain_id: int | None
    contract_addr: bytes | None
    selector: bytes | None
    domain_separator: bytes | None
    primary_type_hash: bytes | None
    id: int
    value: int
    name: str
    signature: bytes | None = None

    def __init__(
        self,
        version: int,
        chain_id: int | None,
        contract_addr: bytes | None,
        selector: bytes | None,
        id: int,
        value: int,
        name: str,
        signature: bytes | None = None,
        domain_separator: bytes | None = None,
        primary_type_hash: bytes | None = None,
    ):
        self.version = version
        self.chain_id = chain_id
        self.contract_addr = contract_addr
        self.selector = selector
        self.domain_separator = domain_separator
        self.primary_type_hash = primary_type_hash
        self.id = id
        self.value = value
        self.name = name
        self.signature = signature

    def serialize(self) -> bytes:
        payload = bytearray()
        payload += self.serialize_field(Tag.VERSION, self.version)
        if self.chain_id is not None:
            payload += self.serialize_field(Tag.CHAIN_ID, self.chain_id)
        if self.contract_addr is not None:
            payload += self.serialize_field(Tag.CONTRACT_ADDR, self.contract_addr)
        if self.selector is not None:
            payload += self.serialize_field(Tag.SELECTOR, self.selector)
        payload += self.serialize_field(Tag.ID, self.id)
        payload += self.serialize_field(Tag.VALUE, self.value)
        payload += self.serialize_field(Tag.NAME, self.name)
        if self.domain_separator is not None:
            payload += self.serialize_field(Tag.DOMAIN_SEPARATOR, self.domain_separator)
        if self.primary_type_hash is not None:
            payload += self.serialize_field(Tag.PRIMARY_TYPE_HASH, self.primary_type_hash)
        sig = self.signature
        if sig is None:
            sig = CALLDATA_PARTNER.sign(bytes(payload))
        payload += self.serialize_field(Tag.SIGNATURE, sig)
        return bytes(payload)
