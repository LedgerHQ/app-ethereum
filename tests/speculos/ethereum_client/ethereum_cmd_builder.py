import enum
import logging
import struct
from typing import List, Tuple, Union, Iterator, cast

from ethereum_client.transaction import EIP712, PersonalTransaction, Transaction
from ethereum_client.plugin import ERC20Information, Plugin
from ethereum_client.utils import packed_bip32_path_from_string

MAX_APDU_LEN: int = 255

def chunked(size, source):
    for i in range(0, len(source), size):
        yield source[i:i+size]

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
    INS_PERFORM_PRIVACY_OPERATION = 0x18


class EthereumCommandBuilder:
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

    def _same_header_builder(self, data: Union[Plugin, ERC20Information], ins: int) -> bytes:
        return self.serialize(cla=self.CLA,
                              ins=ins,
                              p1=0x00,
                              p2=0x00,
                              cdata=data.serialize())

    def set_plugin(self, plugin: Plugin) -> bytes:
        return self._same_header_builder(plugin, InsType.INS_SET_PLUGIN)

    def provide_nft_information(self, plugin: Plugin) -> bytes:
        return self._same_header_builder(plugin, InsType.INS_PROVIDE_NFT_INFORMATION)

    def provide_erc20_token_information(self, info: ERC20Information):
        return self._same_header_builder(info, InsType.INS_PROVIDE_ERC20)

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
        cdata = packed_bip32_path_from_string(bip32_path)
          
        return self.serialize(cla=self.CLA,
                              ins=InsType.INS_GET_PUBLIC_KEY,
                              p1=0x01 if display else 0x00,
                              p2=0x01,
                              cdata=cdata)

    def perform_privacy_operation(self, bip32_path: str, display: bool, shared_secret: bool) -> bytes:
        """Command builder for INS_PERFORM_PRIVACY_OPERATION.

        Parameters
        ----------
        bip32_path : str
            String representation of BIP32 path.
        Third party public key on Curve25519 : 32 bytes
            Optionnal if returning the shared secret
        
        """
        cdata = packed_bip32_path_from_string(bip32_path)
        
        return self.serialize(cla=self.CLA,
                              ins=InsType.INS_PERFORM_PRIVACY_OPERATION,
                              p1=0x01 if display else 0x00,
                              p2=0x01 if shared_secret else 0x00,
                              cdata=cdata)


    def simple_sign_tx(self, bip32_path: str, transaction: Transaction) -> bytes:
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
        cdata = packed_bip32_path_from_string(bip32_path)
        
        tx: bytes = transaction.serialize()

        cdata = cdata + tx

        return self.serialize(cla=self.CLA,
                              ins=InsType.INS_SIGN_TX,
                              p1=0x00,
                              p2=0x00,
                              cdata=cdata)

    def sign_eip712(self, bip32_path: str, transaction: EIP712) -> bytes:
        """Command builder for INS_SIGN_EIP712.

        Parameters
        ----------
        bip32_path : str
            String representation of BIP32 path.
        transaction : EIP712
            Domain hash  -> 32 bytes
            Message hash -> 32 bytes

        Yields
        -------
        bytes
            APDU command chunk for INS_SIGN_EIP712.

        """
        cdata = packed_bip32_path_from_string(bip32_path)

        
        tx: bytes = transaction.serialize()

        cdata = cdata + tx

        return self.serialize(cla=self.CLA,
                              ins=InsType.INS_SIGN_EIP712,
                              p1=0x00,
                              p2=0x00,
                              cdata=cdata)

    def personal_sign_tx(self, bip32_path: str, transaction: PersonalTransaction) -> Tuple[bool,bytes]:
        """Command builder for INS_SIGN_PERSONAL_TX.

        Parameters
        ----------
        bip32_path : str
            String representation of BIP32 path.
        transaction : Transaction
            Representation of the transaction to be signed.

        Yields
        -------
        bytes
            APDU command chunk for INS_SIGN_PERSONAL_TX.

        """

        cdata = packed_bip32_path_from_string(bip32_path)
        
        tx: bytes = transaction.serialize()

        cdata = cdata + tx
        last_chunk = len(cdata) // MAX_APDU_LEN
        
        # The generator allows to send apdu frames because we can't send an apdu > 255
        for i, (chunk) in enumerate(chunked(MAX_APDU_LEN, cdata)):
            if i == 0 and i == last_chunk:
                yield True, self.serialize(cla=self.CLA,
                         ins=InsType.INS_SIGN_PERSONAL_TX,
                         p1=0x00,
                         p2=0x00,
                         cdata=chunk)
            elif i == 0:
                yield False, self.serialize(cla=self.CLA,
                         ins=InsType.INS_SIGN_PERSONAL_TX,
                         p1=0x00,
                         p2=0x00,
                         cdata=chunk)
            elif i == last_chunk:
                yield True, self.serialize(cla=self.CLA,
                         ins=InsType.INS_SIGN_PERSONAL_TX,
                         p1=0x80,
                         p2=0x00,
                         cdata=chunk)
            else:
                yield False, self.serialize(cla=self.CLA,
                         ins=InsType.INS_SIGN_PERSONAL_TX,
                         p1=0x80,
                         p2=0x00,
                         cdata=chunk)