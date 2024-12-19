from enum import IntEnum
from typing import Optional
from .tlv import format_tlv
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


class EnumValue:
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
        payload += format_tlv(Tag.VERSION, self.version)
        payload += format_tlv(Tag.CHAIN_ID, self.chain_id)
        payload += format_tlv(Tag.CONTRACT_ADDR, self.contract_addr)
        payload += format_tlv(Tag.SELECTOR, self.selector)
        payload += format_tlv(Tag.ID, self.id)
        payload += format_tlv(Tag.VALUE, self.value)
        payload += format_tlv(Tag.NAME, self.name)
        sig = self.signature
        if sig is None:
            sig = sign_data(Key.CAL, payload)
        payload += format_tlv(Tag.SIGNATURE, sig)
        return payload
