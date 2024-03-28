from ast import List
from contextlib import contextmanager
import struct
from time import sleep
from typing import Tuple

from speculos.client import SpeculosClient, ApduException

from ethereum_client.ethereum_cmd_builder import EthereumCommandBuilder, InsType
from ethereum_client.exception import DeviceException
from ethereum_client.transaction import EIP712, PersonalTransaction, Transaction
from ethereum_client.plugin import ERC20Information, Plugin
from ethereum_client.utils import parse_sign_response


class EthereumCommand:
    def __init__(self,
                 client: SpeculosClient,
                 debug: bool = False,
                 model: str = "nanos",
                 golden_run: bool = False) -> None:
        self.client = client
        self.builder = EthereumCommandBuilder(debug=debug)
        self.debug = debug
        self.model = model
        self.golden_run = golden_run

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

    def set_plugin(self, plugin: Plugin):
        try:
            self.client._apdu_exchange(
                self.builder.set_plugin(plugin=plugin)
            )

        except ApduException as error:
            raise DeviceException(error_code=error.sw, ins=InsType.INS_SET_PLUGIN)

    def provide_nft_information(self, plugin: Plugin):
        try:
            self.client._apdu_exchange(
                self.builder.provide_nft_information(plugin=plugin)
            )

        except ApduException as error:
            raise DeviceException(error_code=error.sw, ins=InsType.INS_PROVIDE_NFT_INFORMATION)

    def provide_erc20_token_information(self, info: ERC20Information):
        try:
            self.client._apdu_exchange(
                self.builder.provide_erc20_token_information(info=info)
            )

        except ApduException as error:
            raise DeviceException(error_code=error.sw, ins=InsType.INS_PROVIDE_ERC20)


    @contextmanager
    def get_public_key(self, bip32_path: str, result: List, display: bool = False) -> Tuple[bytes, bytes, bytes]:
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
    def perform_privacy_operation(self, bip32_path: str, result: List, display: bool = False, shared_secret: bool = False) -> Tuple[bytes, bytes, bytes]:
        try:
            chunk: bytes = self.builder.perform_privacy_operation(bip32_path=bip32_path, display=display, shared_secret=shared_secret)

            with self.client.apdu_exchange_nowait(cla=chunk[0], ins=chunk[1],
                                                          p1=chunk[2], p2=chunk[3],
                                                          data=chunk[5:]) as exchange:
                yield exchange
                response: bytes = exchange.receive()

        except ApduException as error:
            raise DeviceException(error_code=error.sw, ins=InsType.INS_PERFORM_PRIVACY_OPERATION)

        # response = Public encryption key or shared secret	(32)
        assert len(response) == 32

        result.append(response)

    def send_apdu(self, apdu: bytes) -> bytes:
        try:
            self.client.apdu_exchange(cla=apdu[0], ins=apdu[1],
                                        p1=apdu[2], p2=apdu[3],
                                        data=apdu[5:])
        except ApduException as error:
            raise DeviceException(error_code=error.sw, ins=InsType.INS_SIGN_TX)

    @contextmanager
    def send_apdu_context(self, apdu: bytes, result: List = list()) -> bytes:
        try:

            with self.client.apdu_exchange_nowait(cla=apdu[0], ins=apdu[1],
                                                    p1=apdu[2], p2=apdu[3],
                                                    data=apdu[5:]) as exchange:
                yield exchange
                result.append(exchange.receive())

        except ApduException as error:
            raise DeviceException(error_code=error.sw, ins=InsType.INS_SIGN_TX)



    @contextmanager
    def simple_sign_tx(self, bip32_path: str, transaction: Transaction, result: List = list()) -> None:
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
        v, r, s = parse_sign_response(response)

        result.append(v)
        result.append(r)
        result.append(s)


    @contextmanager
    def sign_eip712(self, bip32_path: str, transaction: EIP712, result: List = list()) -> None:
        try:
            chunk: bytes = self.builder.sign_eip712(bip32_path=bip32_path, transaction=transaction)

            with self.client.apdu_exchange_nowait(cla=chunk[0], ins=chunk[1],
                                                          p1=chunk[2], p2=chunk[3],
                                                          data=chunk[5:]) as exchange:
                yield exchange
                response: bytes = exchange.receive()

        except ApduException as error:
            raise DeviceException(error_code=error.sw, ins=InsType.INS_SIGN_EIP712)

        # response = V (1) || R (32) || S (32)
        assert len(response) == 65
        v, r, s = parse_sign_response(response)

        result.append(v)
        result.append(r)
        result.append(s)


    @contextmanager
    def personal_sign_tx(self, bip32_path: str, transaction: PersonalTransaction, result: List = list()) -> None:
        try:
            for islast_apdu, apdu in self.builder.personal_sign_tx(bip32_path=bip32_path, transaction=transaction):
                if islast_apdu:
                    with self.client.apdu_exchange_nowait(cla=apdu[0], ins=apdu[1],
                                                    p1=apdu[2], p2=apdu[3],
                                                    data=apdu[5:]) as exchange:
                        # the "yield" here allows to wait for a button interaction (click right, left, both)
                        yield exchange
                        response: bytes = exchange.receive()
                else:
                    self.send_apdu(apdu)

        except ApduException as error:
            raise DeviceException(error_code=error.sw, ins=InsType.INS_SIGN_TX)

         # response = V (1) || R (32) || S (32)
        v, r, s = parse_sign_response(response)

        result.append(v)
        result.append(r)
        result.append(s)
