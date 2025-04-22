from typing import Optional
from enum import IntEnum
from .tlv import TlvSerializable


class FieldTag(IntEnum):
    STRUCT_VERSION = 0x00
    DELEGATE_ADDR = 0x01
    CHAIN_ID = 0x02
    NONCE = 0x03


class TxAuth7702(TlvSerializable):
    delegate: bytes
    nonce: int
    chain_id: int

    def __init__(self,
                 delegate: bytes,
                 nonce: int,
                 chain_id: Optional[int]) -> None:
        self.delegate = delegate
        self.nonce = nonce
        if chain_id is None:
            self.chain_id = 0
        else:
            self.chain_id = chain_id

    def serialize(self) -> bytes:
        payload: bytes = self.serialize_field(FieldTag.STRUCT_VERSION, 1)
        payload += self.serialize_field(FieldTag.DELEGATE_ADDR, self.delegate)
        payload += self.serialize_field(FieldTag.CHAIN_ID, self.chain_id)
        payload += self.serialize_field(FieldTag.NONCE, self.nonce)
        return payload
