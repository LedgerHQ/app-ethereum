import struct
from enum import IntEnum

from .keychain import Key, sign_data
from .tlv import TlvSerializable


class TrustedNameType(IntEnum):
    ACCOUNT = 0x01
    CONTRACT = 0x02
    NFT = 0x03
    TOKEN = 0x04
    WALLET = 0x05
    CONTEXT_ADDRESS = 0x06


class TrustedNameSource(IntEnum):
    LAB = 0x00
    CAL = 0x01
    ENS = 0x02
    UD = 0x03
    FN = 0x04
    DNS = 0x05
    DYN_RESOLVER = 0x06
    MULTISIG_ADDRESS_BOOK = 0x07


class Tag(IntEnum):
    STRUCT_TYPE = 0x01
    STRUCT_VERSION = 0x02
    NOT_VALID_AFTER = 0x10
    CHALLENGE = 0x12
    SIG_KEY_ID = 0x13
    SIG_ALGO = 0x14
    SIGNATURE = 0x15
    NAME = 0x20
    COIN_TYPE = 0x21
    ADDRESS = 0x22
    CHAIN_ID = 0x23
    TYPE = 0x70
    SOURCE = 0x71
    NFT_ID = 0x72
    OWNER = 0x74


class TrustedName(TlvSerializable):
    struct_version: int
    coin_type: int | None
    not_valid_after: tuple[int, int, int] | None
    tn_type: TrustedNameType | None
    tn_source: TrustedNameSource | None
    name: str
    chain_id: int | None
    address: bytes
    challenge: bytes | None
    nft_id: int | None
    owner: bytes | None

    def __init__(
        self,
        version: int,
        address: bytes,
        name: str,
        coin_type: int | None = None,
        not_valid_after: tuple[int, int, int] | None = None,
        challenge: bytes | None = None,
        tn_type: TrustedNameType | None = None,
        tn_source: TrustedNameSource | None = None,
        chain_id: int | None = None,
        nft_id: int | None = None,
        owner: bytes | None = None,
        signature: bytes | None = None,
    ) -> None:
        self.version = version
        self.coin_type = coin_type
        self.not_valid_after = not_valid_after
        self.tn_type = tn_type
        self.tn_source = tn_source
        self.name = name
        self.chain_id = chain_id
        self.address = address
        self.challenge = challenge
        self.owner = owner
        self.nft_id = nft_id
        self.signature = signature

    def serialize(self) -> bytes:
        payload = bytearray()
        payload += self.serialize_field(Tag.STRUCT_TYPE, 0x03)
        payload += self.serialize_field(Tag.STRUCT_VERSION, self.version)
        if self.coin_type is not None:
            payload += self.serialize_field(Tag.COIN_TYPE, self.coin_type)
        if self.not_valid_after is not None:
            payload += self.serialize_field(Tag.NOT_VALID_AFTER, struct.pack("BBB", *self.not_valid_after))
        if self.tn_type is not None:
            payload += self.serialize_field(Tag.TYPE, self.tn_type)
        if self.tn_source is not None:
            payload += self.serialize_field(Tag.SOURCE, self.tn_source)
        payload += self.serialize_field(Tag.NAME, self.name)
        if self.chain_id is not None:
            payload += self.serialize_field(Tag.CHAIN_ID, self.chain_id)
        payload += self.serialize_field(Tag.ADDRESS, self.address)
        if self.challenge is not None:
            payload += self.serialize_field(Tag.CHALLENGE, self.challenge)
        if self.nft_id is not None:
            payload += self.serialize_field(Tag.NFT_ID, self.nft_id)
        if self.owner is not None:
            payload += self.serialize_field(Tag.OWNER, self.owner)
        sig = self.signature
        if self.tn_source == TrustedNameSource.CAL:
            key_id = 9
            key = Key.CAL
        else:
            key_id = 7
            key = Key.TRUSTED_NAME
        payload += self.serialize_field(Tag.SIG_KEY_ID, key_id)
        payload += self.serialize_field(Tag.SIG_ALGO, 1)
        if sig is None:
            sig = sign_data(key, payload)
        payload += self.serialize_field(Tag.SIGNATURE, sig)
        return payload
