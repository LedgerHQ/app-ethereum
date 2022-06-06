from ast import List
from contextlib import contextmanager
from ctypes.wintypes import INT
from re import A
import struct
from typing import Tuple

from speculos.client import SpeculosClient, ApduException

from ethereum_client.ethereum_cmd_builder import EthereumCommandBuilder, InsType
from ethereum_client.exception import DeviceException
from ethereum_client.transaction import Transaction


class EthereumCommand:
    def __init__(self,
                 client: SpeculosClient,
                 debug: bool = False,
                 model: str = "nanos") -> None:
        self.client = client
        self.builder = EthereumCommandBuilder(debug=debug)
        self.debug = debug
        self.model = model
    
    def get_configuration(self) -> Tuple[int, int, int, int]:
        try:
            response = self.client._apdu_exchange(
                self.builder.get_configuration()
            )  # type: int, bytes
        except ApduException as error:
            raise DeviceException(error_code=error.sw, ins=InsType.INS_GET_VERSION)

        # response = FLAG (1) || MAJOR (1) || MINOR (1) || PATCH (1)
        assert len(response) == 4

        info, major, minor, patch = struct.unpack(
            "BBBB",
            response
        )  # type: int, int, int

        return info, major, minor, patch

    @contextmanager
    def get_public_key(self, bip32_path: str, display: bool = False, result: List = list()) -> Tuple[bytes, bytes, bytes]:
        try:
            chunk: bytes = self.builder.get_public_key(bip32_path=bip32_path, display=display)

            with self.client.apdu_exchange_nowait(cla=chunk[0], ins=chunk[1],
                                                          p1=chunk[2], p2=chunk[3],
                                                          data=chunk[5:]) as exchange:
                yield exchange
                response: bytes = exchange.receive()
                
        except ApduException as error:
            raise DeviceException(error_code=error.sw, ins=InsType.INS_GET_PUBLIC_KEY)

        # response = pub_key_len (1) ||
        #            pub_key (var) ||
        #            chain_code_len (1) ||
        #            chain_code (var)
        offset: int = 0

        pub_key_len: int = response[offset]
        offset += 1

        uncompressed_addr_len: bytes = response[offset:offset + pub_key_len]
        offset += pub_key_len
        
        eth_addr_len: int = response[offset]
        offset += 1
        
        eth_addr: bytes = response[offset:offset + eth_addr_len]
        offset += eth_addr_len

        chain_code: bytes = response[offset:]

        assert len(response) == 1 + pub_key_len + 1 + eth_addr_len + 32 # 32 -> chain_code_len
        
        result.append(uncompressed_addr_len)
        result.append(eth_addr)
        result.append(chain_code)

    @contextmanager
    def test_zemu_hard_apdu_sign(self, transaction: Transaction) -> Tuple[int, int, int]:
        sign: bytes = b'\xe0\x04\x00\x00\x80\x05\x80\x00\x00\x2c\x80\x00\x00\x3c\x80\x00\x00\x01\x00\x00\x00\x00\x00\x00\x00\x00\xf8\x69\x46\x85\x06\xa8\xb1\x5e\x00\x82\xeb\xeb\x94\x6b\x17\x54\x74\xe8\x90\x94\xc4\x4d\xa9\x8b\x95\x4e\xed\xea\xc4\x95\x27\x1d\x0f\x80\xb8\x44\x09\x5e\xa7\xb3\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x7d\x27\x68\xde\x32\xb0\xb8\x0b\x7a\x34\x54\xc0\x6b\xda\xc9\x4a\x69\xdd\xc7\xa9\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\x01\x80\x80'
        simple_eth: bytes = b'\xe0\x04\x00\x00\x41\x05\x80\x00\x00\x2c\x80\x00\x00\x3c\x80\x00\x00\x01\x00\x00\x00\x00\x00\x00\x00\x00\xeb\x44\x85\x03\x06\xdc\x42\x00\x82\x52\x08\x94\x5a\x32\x17\x44\x66\x70\x52\xaf\xfa\x83\x86\xed\x49\xe0\x0e\xf2\x23\xcb\xff\xc3\x87\x6f\x9c\x9e\x7b\xf6\x18\x18\x80\x01\x80\x80'
        provide_erc20: bytes = b'\xe0\x0a\x00\x00\x67\x03\x44\x41\x49\x6b\x17\x54\x74\xe8\x90\x94\xc4\x4d\xa9\x8b\x95\x4e\xed\xea\xc4\x95\x27\x1d\x0f\x00\x00\x00\x12\x00\x00\x00\x01\x30\x45\x02\x21\x00\xb3\xaa\x97\x96\x33\x28\x4e\xb0\xf5\x54\x59\x09\x93\x33\xab\x92\xcf\x06\xfd\xd5\x8d\xc9\x0e\x9c\x07\x00\x00\xc8\xe9\x68\x86\x4c\x02\x20\x7b\x10\xec\x7d\x66\x09\xf5\x1d\xda\x53\xd0\x83\xa6\xe1\x65\xa0\xab\xf3\xa7\x7e\x13\x25\x0e\x6f\x26\x07\x72\x80\x9b\x49\xaf\xf5'

        b: bytes = b'\xe0\x04\x00\x00\x56\x05\x80\x00\x00\x2c\x80\x00\x00\x3c\x80\x00\x00\x01\x00\x00\x00\x00\x00\x00\x00\x00\xf8\x3f\x26\x8e\x02\xcc\x9b\xe5\xc5\x3e\xa4\x4b\xd4\x3c\x28\x9d\xcd\xdc\x82\x52\x08\x94\xda\xc1\x7f\x95\x8d\x2e\xe5\x23\xa2\x20\x62\x06\x99\x45\x97\xc1\x3d\x83\x1e\xc7\x92\x8d\xb8\xb0\x86\x1b\x8f\x7f\xe5\xdf\x83\xcd\x55\x3a\x82\x98\x78\x00\x00\x80\x01\x80\x80'
        test: bytes = b"".join([b'\xe0\x04\x00\x00\x89\x05\x80\x00\x00\x2c\x80\x00\x00\x3c\x80\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00',
                        b'\x02',
                        b'\xf8\x70\x02',
                        b'\x02', #nonce
                        b'\x85',
                        b'\x02\x54\x0b\xe4\x00', # max priority fee per gas
                        b'\x85',
                        b'\x02\x54\x0b\xe4\x00', # max fee per gas
                        b'\x86',
                        b'\x24\x61\x39\xca\x80\x80', # gas limit
                        b'\x94',
                        b'\xcc\xcc\xcc\xcc\xcc\xcc\xcc\xcc\xcc\xcc\xcc\xcc\xcc\xcc\xcc\xcc\xcc\xcc\xcc\xcc', # destination
                        b'\x81'+
                        b'\xFF' + # Amount
                        b'\x00' + # Payload
                        b'\xc0\x01\xa0\xe0\x7f\xb8\xa6\x4e\xa3\x78\x6c\x9a\x66\x49\xe5\x44\x29\xe2\x78\x6a\xf3\xea\x31\xc6\xd0\x61\x65\x34\x66\x78\xcf\x8c\xe4\x4f\x9b\xa0\x0e\x4a\x05\x26\xdb\x1e\x90\x5b\x71\x64\xa8\x58\xfd\x5e\xbd\x2f\x17\x59\xe2\x2e\x69\x55\x49\x94\x48\xbd\x27\x6a\x6a\xa6\x28\x30'])

        a = self.builder.simple_sign_tx(bip32_path="44'/60'/1'/0/0", transaction=transaction)


        try:
           with self.client._apdu_exchange_nowait(test) as ex:
               yield ex

            #response = self.client._apdu_exchange(
            #    sign
            #)
        except ApduException as error:
            raise DeviceException(error_code=error.sw, ins=InsType.INS_SIGN_TX)


    @contextmanager
    def simple_sign_tx(self, bip32_path: str, transaction: Transaction, result: List = list()) -> Tuple[int, bytes]:
        try:
            chunk: bytes = self.builder.simple_sign_tx(bip32_path=bip32_path, transaction=transaction)

            with self.client.apdu_exchange_nowait(cla=chunk[0], ins=chunk[1],
                                                          p1=chunk[2], p2=chunk[3],
                                                          data=chunk[5:]) as exchange:
                yield exchange
                response: bytes = exchange.receive()
                
        except ApduException as error:
            raise DeviceException(error_code=error.sw, ins=InsType.INS_SIGN_TX)

         # response = V (1) || R (32) || S (32)
        assert len(response) == 65

        offset: int = 0

        v: bytes = response[offset]
        offset += 1

        r: bytes = response[offset:offset + 32]
        offset += 32

        s: bytes = response[offset:]
        
        result.append(v)
        result.append(r)
        result.append(s)
