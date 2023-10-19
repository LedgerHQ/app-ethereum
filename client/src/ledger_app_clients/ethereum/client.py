import rlp
from enum import IntEnum
from ragger.backend import BackendInterface
from ragger.utils import RAPDU
from typing import List, Optional

from .command_builder import CommandBuilder
from .eip712 import EIP712FieldType
from .keychain import sign_data, Key
from .tlv import format_tlv


WEI_IN_ETH = 1e+18
GWEI_IN_ETH = 1e+9

class TxData:
    selector: bytes
    parameters: list[bytes]
    def __init__(self, selector: bytes, params: list[bytes]):
        self.selector = selector
        self.parameters = params

class StatusWord(IntEnum):
    OK = 0x9000
    ERROR_NO_INFO = 0x6a00
    INVALID_DATA = 0x6a80
    INSUFFICIENT_MEMORY = 0x6a84
    INVALID_INS = 0x6d00
    INVALID_P1_P2 = 0x6b00
    CONDITION_NOT_SATISFIED = 0x6985
    REF_DATA_NOT_FOUND = 0x6a88

class DomainNameTag(IntEnum):
    STRUCTURE_TYPE = 0x01
    STRUCTURE_VERSION = 0x02
    CHALLENGE = 0x12
    SIGNER_KEY_ID = 0x13
    SIGNER_ALGO = 0x14
    SIGNATURE = 0x15
    DOMAIN_NAME = 0x20
    COIN_TYPE = 0x21
    ADDRESS = 0x22


class EthAppClient:
    def __init__(self, client: BackendInterface):
        self._client = client
        self._cmd_builder = CommandBuilder()

    def _send(self, payload: bytes):
        return self._client.exchange_async_raw(payload)

    def response(self) -> Optional[RAPDU]:
        return self._client.last_async_response

    def eip712_send_struct_def_struct_name(self, name: str):
        return self._send(self._cmd_builder.eip712_send_struct_def_struct_name(name))

    def eip712_send_struct_def_struct_field(self,
                                            field_type: EIP712FieldType,
                                            type_name: str,
                                            type_size: int,
                                            array_levels: List,
                                            key_name: str):
        return self._send(self._cmd_builder.eip712_send_struct_def_struct_field(
                          field_type,
                          type_name,
                          type_size,
                          array_levels,
                          key_name))

    def eip712_send_struct_impl_root_struct(self, name: str):
        return self._send(self._cmd_builder.eip712_send_struct_impl_root_struct(name))

    def eip712_send_struct_impl_array(self, size: int):
        return self._send(self._cmd_builder.eip712_send_struct_impl_array(size))

    def eip712_send_struct_impl_struct_field(self, raw_value: bytes):
        chunks = self._cmd_builder.eip712_send_struct_impl_struct_field(bytearray(raw_value))
        for chunk in chunks[:-1]:
            with self._send(chunk):
                pass
        return self._send(chunks[-1])

    def eip712_sign_new(self, bip32_path: str, verbose: bool):
        return self._send(self._cmd_builder.eip712_sign_new(bip32_path))

    def eip712_sign_legacy(self,
                           bip32_path: str,
                           domain_hash: bytes,
                           message_hash: bytes):
        return self._send(self._cmd_builder.eip712_sign_legacy(bip32_path,
                                                               domain_hash,
                                                               message_hash))

    def eip712_filtering_activate(self):
        return self._send(self._cmd_builder.eip712_filtering_activate())

    def eip712_filtering_message_info(self, name: str, filters_count: int, sig: bytes):
        return self._send(self._cmd_builder.eip712_filtering_message_info(name, filters_count, sig))

    def eip712_filtering_show_field(self, name: str, sig: bytes):
        return self._send(self._cmd_builder.eip712_filtering_show_field(name, sig))

    def _sign(self, bip32_path: str, raw_tx: bytes):
        chunks = self._cmd_builder.sign(bip32_path, raw_tx)
        for chunk in chunks[:-1]:
            with self._send(chunk):
                pass
        return self._send(chunks[-1])

    def _data_to_payload(self, data: TxData) -> bytes:
        payload = bytearray(data.selector)
        for param in data.parameters:
            payload += param.rjust(32, b'\x00')
        return payload

    def _sign_common(self,
                     tx: list,
                     gas_price: float,
                     gas_limit: int,
                     destination: bytes,
                     amount: float,
                     data: TxData):
        tx.append(int(gas_price * GWEI_IN_ETH))
        tx.append(gas_limit)
        tx.append(destination)
        if amount > 0:
            tx.append(int(amount * WEI_IN_ETH))
        else:
            tx.append(bytes())
        if data is not None:
            tx.append(self._data_to_payload(data))
        else:
            tx.append(bytes())
        return tx

    def sign_legacy(self,
                    bip32_path: str,
                    nonce: int,
                    gas_price: float,
                    gas_limit: int,
                    destination: bytes,
                    amount: float,
                    chain_id: int,
                    data: TxData = None):
        tx = list()
        tx.append(nonce)
        tx = self._sign_common(tx, gas_price, gas_limit, destination, amount, data)
        tx.append(chain_id)
        tx.append(bytes())
        tx.append(bytes())
        return self._sign(bip32_path, rlp.encode(tx))

    def sign_1559(self,
                  bip32_path: str,
                  chain_id: int,
                  nonce: int,
                  max_prio_gas_price: float,
                  max_gas_price: float,
                  gas_limit: int,
                  destination: bytes,
                  amount: float,
                  data: TxData = None,
                  access_list = list()):
        tx = list()
        tx.append(chain_id)
        tx.append(nonce)
        tx.append(int(max_prio_gas_price * GWEI_IN_ETH))
        tx = self._sign_common(tx, max_gas_price, gas_limit, destination, amount, data)
        tx.append(access_list)
        tx.append(False)
        tx.append(bytes())
        tx.append(bytes())
        # prefix with transaction type
        return self._sign(bip32_path, b'\x02' + rlp.encode(tx))

    def get_challenge(self):
        return self._send(self._cmd_builder.get_challenge())

    def get_public_addr(self,
                        display: bool = True,
                        chaincode: bool = False,
                        bip32_path: str = "m/44'/60'/0'/0/0",
                        chain_id: Optional[int] = None):
        return self._send(self._cmd_builder.get_public_addr(display,
                                                            chaincode,
                                                            bip32_path,
                                                            chain_id))

    def provide_domain_name(self, challenge: int, name: str, addr: bytes):
        payload = format_tlv(DomainNameTag.STRUCTURE_TYPE, 3)  # TrustedDomainName
        payload += format_tlv(DomainNameTag.STRUCTURE_VERSION, 1)
        payload += format_tlv(DomainNameTag.SIGNER_KEY_ID, 0)  # test key
        payload += format_tlv(DomainNameTag.SIGNER_ALGO, 1)  # secp256k1
        payload += format_tlv(DomainNameTag.CHALLENGE, challenge)
        payload += format_tlv(DomainNameTag.COIN_TYPE, 0x3c)  # ETH in slip-44
        payload += format_tlv(DomainNameTag.DOMAIN_NAME, name)
        payload += format_tlv(DomainNameTag.ADDRESS, addr)
        payload += format_tlv(DomainNameTag.SIGNATURE,
                              sign_data(Key.DOMAIN_NAME, payload))

        chunks = self._cmd_builder.provide_domain_name(payload)
        for chunk in chunks[:-1]:
            with self._send(chunk):
                pass
        return self._send(chunks[-1])

    def set_plugin(self,
                   plugin_name: str,
                   contract_addr: bytes,
                   selector: bytes,
                   chain_id: int,
                   type_: int = 1,
                   version: int = 1,
                   key_id: int = 2,
                   algo_id: int = 1,
                   sig: Optional[bytes] = None):
        if sig is None:
            # Temporarily get a command with an empty signature to extract the payload and
            # compute the signature on it
            tmp = self._cmd_builder.set_plugin(type_,
                                               version,
                                               plugin_name,
                                               contract_addr,
                                               selector,
                                               chain_id,
                                               key_id,
                                               algo_id,
                                               bytes())
            # skip APDU header & empty sig
            sig = sign_data(Key.SET_PLUGIN, tmp[5:-1])
        return self._send(self._cmd_builder.set_plugin(type_,
                                                       version,
                                                       plugin_name,
                                                       contract_addr,
                                                       selector,
                                                       chain_id,
                                                       key_id,
                                                       algo_id,
                                                       sig))

    def provide_nft_metadata(self,
                             collection: str,
                             addr: bytes,
                             chain_id: int,
                             type_: int = 1,
                             version: int = 1,
                             key_id: int = 1,
                             algo_id: int = 1,
                             sig: Optional[bytes] = None):
        if sig is None:
            # Temporarily get a command with an empty signature to extract the payload and
            # compute the signature on it
            tmp = self._cmd_builder.provide_nft_information(type_,
                                                            version,
                                                            collection,
                                                            addr,
                                                            chain_id,
                                                            key_id,
                                                            algo_id,
                                                            bytes())
            # skip APDU header & empty sig
            sig = sign_data(Key.NFT, tmp[5:-1])
        return self._send(self._cmd_builder.provide_nft_information(type_,
                                                                    version,
                                                                    collection,
                                                                    addr,
                                                                    chain_id,
                                                                    key_id,
                                                                    algo_id,
                                                                    sig))

    def set_external_plugin(self,
                            plugin_name: str,
                            contract_address: bytes,
                            method_selelector: bytes,
                            sig: Optional[bytes] = None):
        if sig is None:
            # Temporarily get a command with an empty signature to extract the payload and
            # compute the signature on it
            tmp = self._cmd_builder.set_external_plugin(plugin_name, contract_address, method_selelector, bytes())

            # skip APDU header & empty sig
            sig = sign_data(Key.SET_PLUGIN, tmp[5:-1])
        return self._send(self._cmd_builder.set_external_plugin(plugin_name, contract_address, method_selelector, sig))
