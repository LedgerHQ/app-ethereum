from typing import Union

from ethereum_client.utils import (read, read_uint, read_varint,
                                      write_varint, UINT64_MAX)

class Plugin:
    def __init__(self, type: int, version: int, name: str, addr: Union[str, bytes], selector: int = -1, chainID: int = 1, keyID: int = 0, algorithm: int = 1, sign: bytes = b'') -> None:
        self.type: int = type
        self.version: int = version
        self.name: bytes = bytes(name, 'UTF-8')
        self.addr: bytes = bytes.fromhex(addr[2:]) if isinstance(addr, str) else addr
        self.selector: int = selector
        self.chainID: int = chainID
        self.keyID: int = keyID
        self.algorithm: int = algorithm
        self.sign: bytes = sign

    def serialize(self) -> bytes:
        return b"".join([
            self.type.to_bytes(1, byteorder="big"),

            self.version.to_bytes(1, byteorder="big"),

            write_varint(len(self.name)),
            self.name,

            self.addr,

            b'' if self.selector == -1 else self.selector.to_bytes(4, byteorder="big"),

            self.chainID.to_bytes(8, byteorder="big"),

            self.keyID.to_bytes(1, byteorder="big"),

            self.algorithm.to_bytes(1, byteorder="big"),

            write_varint(len(self.sign)),
            self.sign,

        ])