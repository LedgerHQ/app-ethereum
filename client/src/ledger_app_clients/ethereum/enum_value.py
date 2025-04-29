from enum import IntEnum
from typing import Optional
from .tlv import TlvSerializable
from .keychain import sign_data, Key


class Tag(IntEnum):
    VERSION = 0x00
    CHAIN_ID = 0x01
    CONTRACT_ADDR = 0x02
    SELECTOR = 0x03
    ID = 0x04
    VALUE = 0x05
    NAME = 0x06
    SIGNATURE = 0xff


class EnumValue(TlvSerializable):
    version: int
    chain_id: int
    contract_addr: bytes
    selector: bytes
    id: int
    value: int
    name: str
    signature: Optional[bytes] = None

    def __init__(self,
                 version: int,
                 chain_id: int,
                 contract_addr: bytes,
                 selector: bytes,
                 id: int,
                 value: int,
                 name: str,
                 signature: Optional[bytes] = None):
        self.version = version
        self.chain_id = chain_id
        self.contract_addr = contract_addr
        self.selector = selector
        self.id = id
        self.value = value
        self.name = name
        self.signature = signature

    def serialize(self) -> bytes:
        payload = bytearray()
        payload += self.serialize_field(Tag.VERSION, self.version)
        payload += self.serialize_field(Tag.CHAIN_ID, self.chain_id)
        payload += self.serialize_field(Tag.CONTRACT_ADDR, self.contract_addr)
        payload += self.serialize_field(Tag.SELECTOR, self.selector)
        payload += self.serialize_field(Tag.ID, self.id)
        payload += self.serialize_field(Tag.VALUE, self.value)
        payload += self.serialize_field(Tag.NAME, self.name)
        sig = self.signature
        if sig is None:
            sig = sign_data(Key.CALLDATA, payload)
        payload += self.serialize_field(Tag.SIGNATURE, sig)
        return payload
