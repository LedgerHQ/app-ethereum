from typing import Optional
from enum import IntEnum

from .tlv import format_tlv, FieldTag
from .keychain import sign_data, Key


class SimuType(IntEnum):
    TRANSACTION = 0x00
    TYPED_DATA = 0x01
    PERSONAL_MESSAGE = 0x02


class TxSimu:
    simu_type: SimuType
    risk: int
    category: int
    tiny_url: str
    from_addr: Optional[bytes] = None
    tx_hash: Optional[bytes] = None
    chain_id: Optional[int] = None
    domain_hash: Optional[bytes] = None
    provider_message:  Optional[str]

    def __init__(self,
                 simu_type: SimuType,
                 risk: int,
                 category: int,
                 tiny_url: str,
                 from_addr: Optional[bytes] = None,
                 tx_hash: Optional[bytes] = None,
                 chain_id: Optional[int] = None,
                 domain_hash: Optional[bytes] = None,
                 provider_message:  Optional[str] = None) -> None:
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
        payload: bytes = format_tlv(FieldTag.STRUCT_TYPE, 9)
        payload += format_tlv(FieldTag.STRUCT_VERSION, 1)
        payload += format_tlv(FieldTag.W3C_SIMULATION_TYPE, self.simu_type)
        payload += format_tlv(FieldTag.ADDRESS, self.from_addr)
        payload += format_tlv(FieldTag.TX_HASH, self.tx_hash)
        payload += format_tlv(FieldTag.W3C_NORMALIZED_RISK, self.risk)
        payload += format_tlv(FieldTag.W3C_NORMALIZED_CATEGORY, self.category)
        payload += format_tlv(FieldTag.W3C_TINY_URL, self.tiny_url.encode('utf-8'))
        if self.chain_id:
            payload += format_tlv(FieldTag.CHAIN_ID, self.chain_id.to_bytes(8, 'big'))
        if self.domain_hash:
            payload += format_tlv(FieldTag.DOMAIN_HASH, self.domain_hash)
        if self.provider_message:
            payload += format_tlv(FieldTag.W3C_PROVIDER_MSG, self.provider_message.encode('utf-8'))

        # Append the data Signature
        payload += format_tlv(FieldTag.DER_SIGNATURE, sign_data(Key.WEB3_CHECK, payload))
        return payload
