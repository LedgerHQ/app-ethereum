from typing import Union


def der_encode(value: int) -> bytes:
    # max() to have minimum length of 1
    value_bytes = value.to_bytes(max(1, (value.bit_length() + 7) // 8), 'big')
    if value >= 0x80:
        value_bytes = (0x80 | len(value_bytes)).to_bytes(1, 'big') + value_bytes
    return value_bytes


def format_tlv(tag: int, value: Union[int, str, bytes]) -> bytes:
    if isinstance(value, int):
        # max() to have minimum length of 1
        value = value.to_bytes(max(1, (value.bit_length() + 7) // 8), 'big')
    elif isinstance(value, str):
        value = value.encode()

    assert isinstance(value, bytes), f"Unhandled TLV formatting for type : {type(value)}"

    tlv = bytearray()
    tlv += der_encode(tag)
    tlv += der_encode(len(value))
    tlv += value
    return tlv
