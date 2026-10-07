from hashlib import sha256

from ragger.tlv import CoinInfoFieldTag, LedgerCommonFieldTag, TlvSerializable

from .signing_partners import NETWORK_PARTNER


class DynamicNetwork(TlvSerializable):
    name: str
    ticker: str
    chain_id: int
    icon: bytes | None

    def __init__(self, name: str, ticker: str, chain_id: int, icon: bytes | None = None) -> None:
        self.name = name
        self.ticker = ticker
        self.chain_id = chain_id
        self.icon = icon

    def serialize(self) -> bytes:
        # Construct the TLV payload
        payload: bytes = self.serialize_field(LedgerCommonFieldTag.STRUCTURE_TYPE, 8)
        payload += self.serialize_field(LedgerCommonFieldTag.VERSION, 1)
        payload += self.serialize_field(CoinInfoFieldTag.BLOCKCHAIN_FAMILY, 1)
        payload += self.serialize_field(LedgerCommonFieldTag.CHAIN_ID, self.chain_id)
        payload += self.serialize_field(CoinInfoFieldTag.NETWORK_NAME, self.name.encode("utf-8"))
        payload += self.serialize_field(LedgerCommonFieldTag.TICKER, self.ticker.encode("utf-8"))
        if self.icon:
            # Network Icon
            payload += self.serialize_field(CoinInfoFieldTag.NETWORK_ICON_HASH, sha256(self.icon).digest())
        # Append the data Signature
        payload += self.serialize_field(LedgerCommonFieldTag.DER_SIGNATURE, NETWORK_PARTNER.sign(payload))
        return payload
