from io import BytesIO
from typing import List, Optional, Literal


UINT64_MAX: int = 18446744073709551615
UINT32_MAX: int = 4294967295
UINT16_MAX: int = 65535


def bip32_path_from_string(path: str) -> List[bytes]:
    splitted_path: List[str] = path.split("/")

    if not splitted_path:
        raise Exception(f"BIP32 path format error: '{path}'")

    if "m" in splitted_path and splitted_path[0] == "m":
        splitted_path = splitted_path[1:]

    return [int(p).to_bytes(4, byteorder="big") if "'" not in p
            else (0x80000000 | int(p[:-1])).to_bytes(4, byteorder="big")
            for p in splitted_path]


def write_varint(n: int) -> bytes:
    if n < 0xFC:
        return n.to_bytes(1, byteorder="little")

    if n <= UINT16_MAX:
        return b"\xFD" + n.to_bytes(2, byteorder="little")

    if n <= UINT32_MAX:
        return b"\xFE" + n.to_bytes(4, byteorder="little")

    if n <= UINT64_MAX:
        return b"\xFF" + n.to_bytes(8, byteorder="little")

    raise ValueError(f"Can't write to varint: '{n}'!")


def read_varint(buf: BytesIO,
                prefix: Optional[bytes] = None) -> int:
    b: bytes = prefix if prefix else buf.read(1)

    if not b:
        raise ValueError(f"Can't read prefix: '{b}'!")

    n: int = {b"\xfd": 2, b"\xfe": 4, b"\xff": 8}.get(b, 1)  # default to 1

    b = buf.read(n) if n > 1 else b

    if len(b) != n:
        raise ValueError("Can't read varint!")

    return int.from_bytes(b, byteorder="little")


def read(buf: BytesIO, size: int) -> bytes:
    b: bytes = buf.read(size)

    if len(b) < size:
        raise ValueError(f"Cant read {size} bytes in buffer!")

    return b


def read_uint(buf: BytesIO,
              bit_len: int,
              byteorder: Literal['big', 'little'] = 'little') -> int:
    size: int = bit_len // 8
    b: bytes = buf.read(size)

    if len(b) < size:
        raise ValueError(f"Can't read u{bit_len} in buffer!")

    return int.from_bytes(b, byteorder)
