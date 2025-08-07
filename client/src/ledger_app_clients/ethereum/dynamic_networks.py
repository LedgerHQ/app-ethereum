from typing import Optional
from hashlib import sha256

from .tlv import TlvSerializable, FieldTag
from .keychain import sign_data, Key


class DynamicNetwork(TlvSerializable):
    name: str
    ticker: str
    chain_id: int
    icon: Optional[bytes]

    def __init__(self,
                 name: str,
                 ticker: str,
                 chain_id: int,
                 icon: Optional[bytes] = None) -> None:
        self.name = name
        self.ticker = ticker
        self.chain_id = chain_id
        self.icon = icon

    def serialize(self) -> bytes:
        # Construct the TLV payload
        payload: bytes = self.serialize_field(FieldTag.STRUCT_TYPE, 8)
        payload += self.serialize_field(FieldTag.STRUCT_VERSION, 1)
        payload += self.serialize_field(FieldTag.BLOCKCHAIN_FAMILY, 1)
        payload += self.serialize_field(FieldTag.CHAIN_ID, self.chain_id)
        payload += self.serialize_field(FieldTag.NETWORK_NAME, self.name.encode('utf-8'))
        payload += self.serialize_field(FieldTag.TICKER, self.ticker.encode('utf-8'))
        if self.icon:
            # Network Icon
            payload += self.serialize_field(FieldTag.NETWORK_ICON_HASH, sha256(self.icon).digest())
        # Append the data Signature
        payload += self.serialize_field(FieldTag.DER_SIGNATURE, sign_data(Key.NETWORK, payload))
        return payload
