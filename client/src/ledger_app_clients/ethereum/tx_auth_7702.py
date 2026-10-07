from enum import IntEnum

from ragger.tlv import TlvSerializable


class FieldTag(IntEnum):
    STRUCT_VERSION = 0x00
    DELEGATE_ADDR = 0x01
    CHAIN_ID = 0x02
    NONCE = 0x03


# Sentinel for the on-device CHAIN_ID_ALL wildcard. The firmware treats
# chain_id == 0 as "authorization valid on every chain", which expands the
# replay surface of the resulting signature. Require callers to opt into
# that semantics explicitly instead of letting a missing chain_id default
# to it (Cerberus CWE-1188).
CHAIN_ID_ALL = 0


class TxAuth7702(TlvSerializable):
    delegate: bytes
    nonce: int
    chain_id: int

    def __init__(self, delegate: bytes, nonce: int, chain_id: int) -> None:
        # chain_id is required and must be an int. Pass
        # TxAuth7702.CHAIN_ID_ALL (0) to explicitly request an
        # all-chains authorization; do not pass None.
        self.delegate = delegate
        self.nonce = nonce
        self.chain_id = chain_id

    def serialize(self) -> bytes:
        payload: bytes = self.serialize_field(FieldTag.STRUCT_VERSION, 1)
        payload += self.serialize_field(FieldTag.DELEGATE_ADDR, self.delegate)
        payload += self.serialize_field(FieldTag.CHAIN_ID, self.chain_id)
        payload += self.serialize_field(FieldTag.NONCE, self.nonce)
        return payload
