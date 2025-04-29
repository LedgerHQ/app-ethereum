from enum import IntEnum
from typing import Optional
from .tlv import TlvSerializable
from .keychain import sign_data, Key


class Tag(IntEnum):
    STRUCT_TYPE = 0x01
    STRUCT_VERSION = 0x02
    CHALLENGE = 0x12
    ADDRESS = 0x22
    CHAIN_ID = 0x23
    SELECTOR = 0x28
    IMPL_ADDRESS = 0x29
    SIGNATURE = 0x15


class ProxyInfo(TlvSerializable):
    challenge: int
    address: bytes
    chain_id: int
    selector: Optional[bytes]
    impl_address: bytes
    signature: Optional[bytes]

    def __init__(self,
                 challenge: int,
                 address: bytes,
                 chain_id: int,
                 impl_address: bytes,
                 selector: Optional[bytes] = None,
                 signature: Optional[bytes] = None):
        self.challenge = challenge
        self.address = address
        self.chain_id = chain_id
        self.selector = selector
        self.impl_address = impl_address
        self.signature = signature

    def serialize(self) -> bytes:
        payload = bytearray()
        payload += self.serialize_field(Tag.STRUCT_TYPE, 0x26)
        payload += self.serialize_field(Tag.STRUCT_VERSION, 1)
        payload += self.serialize_field(Tag.CHALLENGE, self.challenge)
        payload += self.serialize_field(Tag.ADDRESS, self.address)
        payload += self.serialize_field(Tag.CHAIN_ID, self.chain_id)
        if self.selector is not None:
            payload += self.serialize_field(Tag.SELECTOR, self.selector)
        payload += self.serialize_field(Tag.IMPL_ADDRESS, self.impl_address)
        sig = self.signature
        if sig is None:
            sig = sign_data(Key.CALLDATA, payload)
        payload += self.serialize_field(Tag.SIGNATURE, sig)
        return payload
