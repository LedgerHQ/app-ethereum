from typing import Any

def format_tlv(tag: int, value: Any) -> bytes:
    if isinstance(value, int):
        value = value.to_bytes((value.bit_length() + 7) // 8, 'big')
    elif isinstance(value, str):
        value = value.encode()

    if not isinstance(value, bytes):
        print("Unhandled TLV formatting for type : %s" % (type(value)))
        return None

    tlv = bytearray()
    tlv.append(tag)
    tlv.append(len(value))
    tlv += value
    return tlv
