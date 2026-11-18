from enum import IntEnum

from ragger.tlv import (
    EvmFunctionFieldTag,
    LedgerCommonFieldTag,
    TlvSerializable,
    TxSimulationFieldTag,
)

from .signing_partners import GATING_PARTNER
from .utils import TxType


class GatingFieldTag(IntEnum):
    """The gating tags that ragger does not name. 0x28 is its DOMAIN_HASH."""

    PRIMARY_TYPE_HASH = 0x2A


class Gating(TlvSerializable):
    """A gating descriptor. A transaction or a V1 message is named by its contract (address, chain ID and
    selector or schema hash), a V2 message by its domain separator and the hash of its primary type."""

    tx_type: TxType
    address: bytes | None
    intro_message: str
    tiny_url: str
    chain_id: int | None
    selector: bytes | None
    signature: bytes | None
    domain_separator: bytes | None
    primary_type_hash: bytes | None

    def __init__(
        self,
        tx_type: TxType,
        address: bytes | None,
        intro_message: str,
        tiny_url: str,
        chain_id: int | None = None,
        selector: bytes | None = None,
        signature: bytes | None = None,
        domain_separator: bytes | None = None,
        primary_type_hash: bytes | None = None,
    ) -> None:

        self.tx_type = tx_type
        self.address = address
        self.intro_message = intro_message
        self.tiny_url = tiny_url
        self.chain_id = chain_id
        self.selector = selector
        self.signature = signature
        self.domain_separator = domain_separator
        self.primary_type_hash = primary_type_hash

    def serialize(self) -> bytes:
        of_message = (self.domain_separator is not None) or (self.primary_type_hash is not None)
        assert self.tx_type is not None, "Transaction type is required"
        if not of_message:
            assert self.address is not None, "Address is required"
        if self.tx_type == TxType.TRANSACTION:
            assert self.chain_id is not None, "Chain ID is required"
        if self.tx_type == TxType.TYPED_DATA and not of_message:
            assert self.selector is not None, "Selector (schema_hash) is required"
        assert self.intro_message is not None, "Intro message is required"
        assert self.tiny_url is not None, "Tiny URL is required"

        # Construct the TLV payload. The TX_TYPE / PROVIDER_MESSAGE / TINY_URL tags
        # are shared on the wire with the TxSimulation feature (same tag IDs).
        payload: bytes = self.serialize_field(LedgerCommonFieldTag.STRUCTURE_TYPE, 0x0D)
        payload += self.serialize_field(LedgerCommonFieldTag.VERSION, 1)
        payload += self.serialize_field(TxSimulationFieldTag.SIMULATION_TYPE, self.tx_type)
        if self.address is not None:
            payload += self.serialize_field(LedgerCommonFieldTag.ADDRESS, self.address)
        if self.chain_id is not None:
            payload += self.serialize_field(LedgerCommonFieldTag.CHAIN_ID, self.chain_id.to_bytes(8, "big"))
        if self.domain_separator is not None:
            payload += self.serialize_field(LedgerCommonFieldTag.DOMAIN_HASH, self.domain_separator)
        if self.primary_type_hash is not None:
            payload += self.serialize_field(GatingFieldTag.PRIMARY_TYPE_HASH, self.primary_type_hash)
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
