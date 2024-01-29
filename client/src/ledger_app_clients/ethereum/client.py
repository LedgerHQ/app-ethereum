import rlp
from enum import IntEnum
from ragger.backend import BackendInterface
from ragger.utils import RAPDU
from typing import Optional

from .command_builder import CommandBuilder
from .eip712 import EIP712FieldType
from .keychain import sign_data, Key
from .tlv import format_tlv

from web3 import Web3


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
                                            array_levels: list,
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

    def eip712_sign_new(self, bip32_path: str):
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

    def sign(self,
             bip32_path: str,
             tx_params: dict):
        tx = Web3().eth.account.create().sign_transaction(tx_params).rawTransaction
        prefix = bytes()
        suffix = []
        if tx[0] in [0x01, 0x02]:
            prefix = tx[:1]
            tx = tx[len(prefix):]
        else:  # legacy
            if "chainId" in tx_params:
                suffix = [int(tx_params["chainId"]), bytes(), bytes()]
        decoded = rlp.decode(tx)[:-3]  # remove already computed signature
        tx = prefix + rlp.encode(decoded + suffix)
        chunks = self._cmd_builder.sign(bip32_path, tx, suffix)
        for chunk in chunks[:-1]:
            with self._send(chunk):
                pass
        return self._send(chunks[-1])

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
            sig = sign_data(Key.CAL, tmp[5:])
        return self._send(self._cmd_builder.set_external_plugin(plugin_name, contract_address, method_selelector, sig))

    def personal_sign(self, path: str, msg: bytes):
        chunks = self._cmd_builder.personal_sign(path, msg)
        for chunk in chunks[:-1]:
            with self._send(chunk):
                pass
        return self._send(chunks[-1])

    def provide_token_metadata(self,
                               ticker: str,
                               addr: bytes,
                               decimals: int,
                               chain_id: int,
                               sig: Optional[bytes] = None):
        if sig is None:
            # Temporarily get a command with an empty signature to extract the payload and
            # compute the signature on it
            tmp = self._cmd_builder.provide_erc20_token_information(ticker,
                                                                    addr,
                                                                    decimals,
                                                                    chain_id,
                                                                    bytes())
            # skip APDU header & empty sig
            sig = sign_data(Key.CAL, tmp[6:])
        return self._send(self._cmd_builder.provide_erc20_token_information(ticker,
                                                                            addr,
                                                                            decimals,
                                                                            chain_id,
                                                                            sig))
