import enum
import logging
import struct
from typing import List, Tuple, Union, Iterator, cast

import rlp

from boilerplate_client.transaction import Transaction
from boilerplate_client.utils import bip32_path_from_string

MAX_APDU_LEN: int = 255


def chunkify(data: bytes, chunk_len: int) -> Iterator[Tuple[bool, bytes]]:
    size: int = len(data)

    if size <= chunk_len:
        yield True, data
        return

    chunk: int = size // chunk_len
    remaining: int = size % chunk_len
    offset: int = 0

    for i in range(chunk):
        yield False, data[offset:offset + chunk_len]
        offset += chunk_len

    if remaining:
        yield True, data[offset:]


class InsType(enum.IntEnum):
    INS_GET_PUBLIC_KEY          = 0x02
    INS_SIGN_TX                 = 0x04
    INS_GET_CONFIGURATION       = 0x06
    INS_SIGN_PERSONAL_TX        = 0x08
    INS_PROVIDE_ERC20           = 0x0A
    INS_SIGN_EIP712             = 0x0c
    INS_ETH2_GET_PUBLIC_KEY     = 0x0E
    INS_SET_ETH2_WITHDRAWAL     = 0x10
    INS_SET_EXTERNAL_PLUGIN     = 0x12
    INS_PROVIDE_NFT_INFORMATION = 0x14
    INS_SET_PLUGIN              = 0x16


class BoilerplateCommandBuilder:
    """APDU command builder for the Boilerplate application.

    Parameters
    ----------
    debug: bool
        Whether you want to see logging or not.

    Attributes
    ----------
    debug: bool
        Whether you want to see logging or not.

    """
    CLA: int = 0xE0

    def __init__(self, debug: bool = False):
        """Init constructor."""
        self.debug = debug

    def serialize(self,
                  cla: int,
                  ins: Union[int, enum.IntEnum],
                  p1: int = 0,
                  p2: int = 0,
                  cdata: bytes = b"") -> bytes:
        """Serialize the whole APDU command (header + data).

        Parameters
        ----------
        cla : int
            Instruction class: CLA (1 byte)
        ins : Union[int, IntEnum]
            Instruction code: INS (1 byte)
        p1 : int
            Instruction parameter 1: P1 (1 byte).
        p2 : int
            Instruction parameter 2: P2 (1 byte).
        cdata : bytes
            Bytes of command data.

        Returns
        -------
        bytes
            Bytes of a complete APDU command.

        """
        ins = cast(int, ins.value) if isinstance(ins, enum.IntEnum) else cast(int, ins)

        header: bytes = struct.pack("BBBBB",
                                    cla,
                                    ins,
                                    p1,
                                    p2,
                                    len(cdata))  # add Lc to APDU header

        if self.debug:
            logging.info("header: %s", header.hex())
            logging.info("cdata:  %s", cdata.hex())

        return header + cdata

    def get_configuration(self) -> bytes:
        """Command builder for GET_CONFIGURATON

        Returns
        -------
        bytes
            APDU command for GET_CONFIGURATON

        """
        return self.serialize(cla=self.CLA,
                              ins=InsType.INS_GET_CONFIGURATION,
                              p1=0x00,
                              p2=0x00,
                              cdata=b"")

    def get_public_key(self, bip32_path: str, display: bool = False) -> bytes:
        """Command builder for GET_PUBLIC_KEY.

        Parameters
        ----------
        bip32_path: str
            String representation of BIP32 path.
        display : bool
            Whether you want to display the address on the device.

        Returns
        -------
        bytes
            APDU command for GET_PUBLIC_KEY.

        """
        bip32_paths: List[bytes] = bip32_path_from_string(bip32_path)
        
        cdata: bytes = b"".join([
            len(bip32_paths).to_bytes(1, byteorder="big"),
            *bip32_paths
        ])

        return self.serialize(cla=self.CLA,
                              ins=InsType.INS_GET_PUBLIC_KEY,
                              p1=0x01 if display else 0x00,
                              p2=0x01,
                              cdata=cdata)

    def sign_tx(self, bip32_path: str, transaction: Transaction) -> Iterator[Tuple[bool, bytes]]:
        """Command builder for INS_SIGN_TX.

        Parameters
        ----------
        bip32_path : str
            String representation of BIP32 path.
        transaction : Transaction
            Representation of the transaction to be signed.

        Yields
        -------
        bytes
            APDU command chunk for INS_SIGN_TX.

        """
        bip32_paths: List[bytes] = bip32_path_from_string(bip32_path)

        cdata: bytes = b"".join([
            len(bip32_paths).to_bytes(1, byteorder="big"),
            *bip32_paths
        ])

        yield False, self.serialize(cla=self.CLA,
                                    ins=InsType.INS_SIGN_TX,
                                    p1=0x00,
                                    p2=0x00,
                                    cdata=cdata)

        tx: bytes = transaction.serialize()

        for i, (is_last, chunk) in enumerate(chunkify(tx, MAX_APDU_LEN)):
            if is_last:
                yield True, self.serialize(cla=self.CLA,
                                           ins=InsType.INS_SIGN_TX,
                                           p1=0x00,
                                           p2=0x00,
                                           cdata=chunk)
                return
            else:
                yield False, self.serialize(cla=self.CLA,
                                            ins=InsType.INS_SIGN_TX,
                                            p1=0x00,
                                            p2=0x00,
                                            cdata=chunk)

    def simple_sign_tx(self, bip32_path: str, transaction) -> bytes:
        """Command builder for INS_SIGN_TX.

        Parameters
        ----------
        bip32_path : str
            String representation of BIP32 path.
        transaction : Transaction
            Representation of the transaction to be signed.

        Yields
        -------
        bytes
            APDU command chunk for INS_SIGN_TX.

        """
        bip32_paths: List[bytes] = bip32_path_from_string(bip32_path)

        cdata: bytes = b"".join([
            len(bip32_paths).to_bytes(1, byteorder="big"),
            *bip32_paths,
            rlp.encode(transaction)
        ])

        print(cdata)

        return self.serialize(cla=self.CLA,
                              ins=InsType.INS_SIGN_TX,
                              p1=0x00,
                              p2=0x00,
                              cdata=cdata)
