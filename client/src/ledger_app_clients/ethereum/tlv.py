from typing import Union
from enum import IntEnum


class FieldTag(IntEnum):
    STRUCT_TYPE = 0x01
    STRUCT_VERSION = 0x02
    NOT_VALID_AFTER = 0x10
    CHALLENGE = 0x12
    SIGNER_KEY_ID = 0x13
    SIGNER_ALGO = 0x14
    DER_SIGNATURE = 0x15
    TRUSTED_NAME = 0x20
    COIN_TYPE = 0x21
    ADDRESS = 0x22
    CHAIN_ID = 0x23
    TICKER = 0x24
    TX_HASH = 0x27
    DOMAIN_HASH = 0x28
    BLOCKCHAIN_FAMILY = 0x51
    NETWORK_NAME = 0x52
    NETWORK_ICON_HASH = 0x53
    TRUSTED_NAME_TYPE = 0x70
    TRUSTED_NAME_SOURCE = 0x71
    TRUSTED_NAME_NFT_ID = 0x72
    W3C_NORMALIZED_RISK = 0x80
    W3C_NORMALIZED_CATEGORY = 0x81
    W3C_PROVIDER_MSG = 0x82
    W3C_TINY_URL = 0x83
    W3C_SIMULATION_TYPE = 0x84


class TlvSerializable:
    def serialize(self) -> bytes:
        raise NotImplementedError

    @staticmethod
    def der_encode(value: int) -> bytes:
        # max() to have minimum length of 1
        value_bytes = value.to_bytes(max(1, (value.bit_length() + 7) // 8), 'big')
        if value >= 0x80:
            value_bytes = (0x80 | len(value_bytes)).to_bytes(1, 'big') + value_bytes
        return value_bytes

    @staticmethod
    def serialize_field(tag: int, value: Union[int, str, bytes, bytearray]) -> bytes:
        if isinstance(value, int):
            # max() to have minimum length of 1
            value = value.to_bytes(max(1, (value.bit_length() + 7) // 8), 'big')
        elif isinstance(value, str):
            value = value.encode()
        elif isinstance(value, bytearray):
            value = bytes(value)

        assert isinstance(value, bytes), f"Unhandled TLV formatting for type : {type(value)}"

        tlv = bytearray()
        tlv += TlvSerializable.der_encode(tag)
        tlv += TlvSerializable.der_encode(len(value))
        tlv += value
        return tlv


def format_tlv(tag: int, value: Union[int, str, bytes, bytearray]) -> bytes:
    return TlvSerializable.serialize_field(tag, value)
