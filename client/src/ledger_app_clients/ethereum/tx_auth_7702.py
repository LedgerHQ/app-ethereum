from typing import Optional
from enum import IntEnum

from bip_utils import Bip32Utils

from .tlv import TlvSerializable


class FieldTag(IntEnum):
    STRUCT_VERSION = 0x00
    DERIVATION_IDX = 0x01
    DELEGATE_ADDR = 0x02
    CHAIN_ID = 0x03
    NONCE = 0x04


class TxAuth7702(TlvSerializable):
    bip32_path: str
    delegate: bytes
    nonce: int
    chain_id: int

    def __init__(self,
                 bip32_path: str,
                 delegate: bytes,
                 nonce: int,
                 chain_id: Optional[int]) -> None:
        self.bip32_path = bip32_path
        self.delegate = delegate
        self.nonce = nonce
        if chain_id is None:
            self.chain_id = 0
        else:
            self.chain_id = chain_id

    def serialize(self) -> bytes:
        payload: bytes = self.serialize_field(FieldTag.STRUCT_VERSION, 1)
        split = self.bip32_path.split("/")
        if split[0] != "m":
            raise ValueError("Error master expected in bip32 path")
        for value in split[1:]:
            if value == "":
                raise ValueError(f'Error missing value in split bip 32 path list "{split}"')
            if value.endswith('\''):
                payload += self.serialize_field(FieldTag.DERIVATION_IDX,
                                                Bip32Utils.HardenIndex(int(value[:-1])).to_bytes(4, byteorder='big'))
            else:
                payload += self.serialize_field(FieldTag.DERIVATION_IDX,
                                                int(value).to_bytes(4, byteorder='big'))
        payload += self.serialize_field(FieldTag.DELEGATE_ADDR, self.delegate)
        payload += self.serialize_field(FieldTag.NONCE, self.nonce.to_bytes(8, 'big'))
        payload += self.serialize_field(FieldTag.CHAIN_ID, self.chain_id.to_bytes(8, 'big'))
        return payload
