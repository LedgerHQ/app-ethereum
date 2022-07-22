import string
from typing import Union

from ethereum_client.utils import write_varint

class ERC20Information:
    def __init__(self, erc20_ticker: string , addr: Union[str, bytes], nb_decimals: int, chainID: int, sign: str) -> None:
        self.erc20_ticker: bytes = bytes.fromhex(erc20_ticker)
        self.addr: bytes = bytes.fromhex(addr[2:]) if isinstance(addr, str) else addr
        self.nb_decimals: int = nb_decimals
        self.chainID: int = chainID
        self.sign: bytes = bytes.fromhex(sign)
    
    def serialize(self) -> bytes:
        return b"".join([
            write_varint(len(self.erc20_ticker)),
            self.erc20_ticker,

            self.addr,

            self.nb_decimals.to_bytes(4, byteorder="big"),

            self.chainID.to_bytes(4, byteorder="big"),

            self.sign,
        ])

class Plugin:
    """Plugin class
    Allows to generate an apdu of the SET_PLUGIN command or PROVIDE_NFT_INFORMATION

    PROVIDE_NFT_INFORMATION
    ----
     do not define a selector

    """
    def __init__(self, type: int, version: int, name: str, addr: Union[str, bytes], selector: int = -1, chainID: int = 1, keyID: int = 0, algorithm: int = 1, sign: str = "") -> None:
        self.type: int = type
        self.version: int = version
        self.name: bytes = bytes(name, 'UTF-8')
        self.addr: bytes = bytes.fromhex(addr[2:]) if isinstance(addr, str) else addr
        self.selector: int = selector
        self.chainID: int = chainID
        self.keyID: int = keyID
        self.algorithm: int = algorithm
        self.sign: bytes = bytes.fromhex(sign)

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