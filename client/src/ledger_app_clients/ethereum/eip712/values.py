from enum import IntEnum

from ragger.bip import pack_derivation_path
from ragger.tlv import TlvSerializable


class Eip712ValueSeqTag(IntEnum):
    LEAF = 0x00
    SEQ = 0x01


class Eip712ValueSeq(TlvSerializable):
    """An ordered sequence of values: a struct instance or an array instance.

    Each entry corresponds by position to a declared field (struct) or to an element
    (array); no index or count is carried. Entries are ``bytes`` for a leaf value, or a
    nested ``Eip712ValueSeq`` for an array dimension or a struct instance.

    Arrays and structs share the ``SEQ`` tag: which one a nested sequence opens is derived
    from the schema, so the wire never declares it.
    """

    entries: list["bytes | Eip712ValueSeq"]

    def __init__(self, entries: list["bytes | Eip712ValueSeq"]):
        self.entries = entries

    def serialize(self) -> bytes:
        payload = bytearray()
        for entry in self.entries:
            if isinstance(entry, Eip712ValueSeq):
                payload += self.serialize_field(Eip712ValueSeqTag.SEQ, entry.serialize())
            else:
                payload += self.serialize_field(Eip712ValueSeqTag.LEAF, entry)
        return bytes(payload)


class Eip712ValuesTag(IntEnum):
    VERSION = 0x00
    PRIMARY_TYPE = 0x01
    DERIVATION_PATH = 0x02
    DOMAIN = 0x03
    MESSAGE = 0x04


class Eip712Values(TlvSerializable):
    version: int
    primary_type: str
    derivation_path: str
    domain: Eip712ValueSeq
    message: Eip712ValueSeq

    def __init__(
        self,
        version: int,
        primary_type: str,
        derivation_path: str,
        domain: Eip712ValueSeq,
        message: Eip712ValueSeq,
    ):
        self.version = version
        self.primary_type = primary_type
        self.derivation_path = derivation_path
        self.domain = domain
        self.message = message

    def serialize(self) -> bytes:
        payload = bytearray()
        payload += self.serialize_field(Eip712ValuesTag.VERSION, self.version)
        payload += self.serialize_field(Eip712ValuesTag.PRIMARY_TYPE, self.primary_type)
        # the app derives the path length from the payload size, so drop the count byte
        # that pack_derivation_path prefixes
        payload += self.serialize_field(Eip712ValuesTag.DERIVATION_PATH, pack_derivation_path(self.derivation_path)[1:])
        payload += self.serialize_field(Eip712ValuesTag.DOMAIN, self.domain.serialize())
        payload += self.serialize_field(Eip712ValuesTag.MESSAGE, self.message.serialize())
        return bytes(payload)
