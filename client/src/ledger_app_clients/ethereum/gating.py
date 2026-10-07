from ragger.tlv import (
    EvmFunctionFieldTag,
    LedgerCommonFieldTag,
    TlvSerializable,
    TxSimulationFieldTag,
)

from .signing_partners import GATING_PARTNER
from .utils import TxType


class Gating(TlvSerializable):
    tx_type: TxType
    address: bytes
    intro_message: str
    tiny_url: str
    chain_id: int | None
    selector: bytes | None
    signature: bytes | None

    def __init__(
        self,
        tx_type: TxType,
        address: bytes,
        intro_message: str,
        tiny_url: str,
        chain_id: int | None = None,
        selector: bytes | None = None,
        signature: bytes | None = None,
    ) -> None:

        self.tx_type = tx_type
        self.address = address
        self.intro_message = intro_message
        self.tiny_url = tiny_url
        self.chain_id = chain_id
        self.selector = selector
        self.signature = signature

    def serialize(self) -> bytes:
        assert self.tx_type is not None, "Transaction type is required"
        assert self.address is not None, "Address is required"
        if self.tx_type == TxType.TRANSACTION:
            assert self.chain_id is not None, "Chain ID is required"
        if self.tx_type == TxType.TYPED_DATA:
            assert self.selector is not None, "Selector (schema_hash) is required"
        assert self.intro_message is not None, "Intro message is required"
        assert self.tiny_url is not None, "Tiny URL is required"

        # Construct the TLV payload. The TX_TYPE / PROVIDER_MESSAGE / TINY_URL tags
        # are shared on the wire with the TxSimulation feature (same tag IDs).
        payload: bytes = self.serialize_field(LedgerCommonFieldTag.STRUCTURE_TYPE, 0x0D)
        payload += self.serialize_field(LedgerCommonFieldTag.VERSION, 1)
        payload += self.serialize_field(TxSimulationFieldTag.SIMULATION_TYPE, self.tx_type)
        payload += self.serialize_field(LedgerCommonFieldTag.ADDRESS, self.address)
        if self.chain_id is not None:
            payload += self.serialize_field(LedgerCommonFieldTag.CHAIN_ID, self.chain_id.to_bytes(8, "big"))
        payload += self.serialize_field(TxSimulationFieldTag.PROVIDER_MESSAGE, self.intro_message.encode("utf-8"))
        payload += self.serialize_field(TxSimulationFieldTag.TINY_URL, self.tiny_url.encode("utf-8"))
        if self.selector:
            payload += self.serialize_field(EvmFunctionFieldTag.EVM_FUNCTION_SELECTOR, self.selector)
        # Append the data Signature
        sig = self.signature
        if sig is None:
            sig = GATING_PARTNER.sign(payload)
        payload += self.serialize_field(LedgerCommonFieldTag.DER_SIGNATURE, sig)
        return payload
