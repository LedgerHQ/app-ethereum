from ragger.tlv import LedgerCommonFieldTag, TlvSerializable, TxSimulationFieldTag

from .signing_partners import TX_SIMU_PARTNER
from .utils import TxType


class TxSimu(TlvSerializable):
    simu_type: TxType
    risk: int
    category: int
    tiny_url: str
    from_addr: bytes | None = None
    tx_hash: bytes | None = None
    chain_id: int | None = None
    domain_hash: bytes | None = None
    provider_message: str | None

    def __init__(
        self,
        simu_type: TxType,
        risk: int,
        category: int,
        tiny_url: str,
        from_addr: bytes | None = None,
        tx_hash: bytes | None = None,
        chain_id: int | None = None,
        domain_hash: bytes | None = None,
        provider_message: str | None = None,
    ) -> None:
        self.simu_type = simu_type
        self.from_addr = from_addr
        self.tx_hash = tx_hash
        self.risk = risk
        self.category = category
        self.tiny_url = tiny_url
        self.chain_id = chain_id
        self.domain_hash = domain_hash
        self.provider_message = provider_message

    def serialize(self) -> bytes:
        assert self.from_addr is not None, "From address is required"
        assert self.tx_hash is not None, "Transaction hash is required"
        # Construct the TLV payload
        payload: bytes = self.serialize_field(LedgerCommonFieldTag.STRUCTURE_TYPE, 9)
        payload += self.serialize_field(LedgerCommonFieldTag.VERSION, 1)
        payload += self.serialize_field(TxSimulationFieldTag.SIMULATION_TYPE, self.simu_type)
        payload += self.serialize_field(LedgerCommonFieldTag.ADDRESS, self.from_addr)
        payload += self.serialize_field(LedgerCommonFieldTag.TX_HASH, self.tx_hash)
        payload += self.serialize_field(TxSimulationFieldTag.NORMALIZED_RISK, self.risk)
        payload += self.serialize_field(TxSimulationFieldTag.NORMALIZED_CATEGORY, self.category)
        payload += self.serialize_field(TxSimulationFieldTag.TINY_URL, self.tiny_url.encode("utf-8"))
        if self.chain_id:
            payload += self.serialize_field(LedgerCommonFieldTag.CHAIN_ID, self.chain_id.to_bytes(8, "big"))
        if self.domain_hash:
            payload += self.serialize_field(LedgerCommonFieldTag.DOMAIN_HASH, self.domain_hash)
        if self.provider_message:
            payload += self.serialize_field(
                TxSimulationFieldTag.PROVIDER_MESSAGE,
                self.provider_message.encode("utf-8"),
            )

        # Append the data Signature
        payload += self.serialize_field(LedgerCommonFieldTag.DER_SIGNATURE, TX_SIMU_PARTNER.sign(payload))
        return payload
