from .keychain import Key, sign_data
from .tlv import TlvSerializable


class EthTUID(TlvSerializable):
    chain_id: int
    address: bytes

    def __init__(self, chain_id: int, address: bytes) -> None:
        self.chain_id = chain_id
        self.address = address

    def serialize(self) -> bytes:
        payload = bytearray()
        payload += self.serialize_field(0x23, self.chain_id)
        payload += self.serialize_field(0x22, self.address)
        return payload


class TokenInfo(TlvSerializable):
    version: int
    coin_type: int
    application_name: str
    ticker: str
    decimals: int
    tuid: EthTUID
    signature: bytes | None

    def __init__(self,
                 version: int,
                 application_name: str,
                 ticker: str,
                 decimals: int,
                 tuid: EthTUID,
                 coin_type: int = 60,
                 signature: bytes | None = None) -> None:
        self.version = version
        self.coin_type = coin_type
        self.application_name = application_name
        self.ticker = ticker
        self.decimals = decimals
        self.tuid = tuid
        self.signature = signature

    def serialize(self) -> bytes:
        payload = bytearray()
        payload += self.serialize_field(0x01, 0x90)
        payload += self.serialize_field(0x02, self.version)
        payload += self.serialize_field(0x03, self.coin_type)
        payload += self.serialize_field(0x04, self.application_name)
        payload += self.serialize_field(0x05, self.ticker)
        payload += self.serialize_field(0x06, self.decimals)
        payload += self.serialize_field(0x07, self.tuid.serialize())
        sig = self.signature
        if sig is None:
            sig = sign_data(Key.CAL, payload)
        payload += self.serialize_field(0x08, sig)
        return payload
