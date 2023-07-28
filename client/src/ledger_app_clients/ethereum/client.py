import rlp
from enum import IntEnum
from ragger.backend import BackendInterface
from ragger.utils import RAPDU

from .command_builder import CommandBuilder
from .eip712 import EIP712FieldType
from .keychain import sign_data, Key
from .tlv import format_tlv


WEI_IN_ETH = 1e+18


class StatusWord(IntEnum):
    OK                      = 0x9000
    ERROR_NO_INFO           = 0x6a00
    INVALID_DATA            = 0x6a80
    INSUFFICIENT_MEMORY     = 0x6a84
    INVALID_INS             = 0x6d00
    INVALID_P1_P2           = 0x6b00
    CONDITION_NOT_SATISFIED = 0x6985
    REF_DATA_NOT_FOUND      = 0x6a88

class DOMAIN_NAME_TAG(IntEnum):
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

    def _send(self, payload: bytearray):
        return self._client.exchange_async_raw(payload)

    def response(self) -> RAPDU:
        return self._client._last_async_response

    def eip712_send_struct_def_struct_name(self, name: str):
        return self._send(self._cmd_builder.eip712_send_struct_def_struct_name(name))

    def eip712_send_struct_def_struct_field(self,
                                            field_type: EIP712FieldType,
                                            type_name: str,
                                            type_size: int,
                                            array_levels: [],
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
        chunks = self._cmd_builder.eip712_send_struct_impl_struct_field(raw_value)
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

    def send_fund(self,
                  bip32_path: str,
                  nonce: int,
                  gas_price: int,
                  gas_limit: int,
                  to: bytes,
                  amount: float,
                  chain_id: int):
        data = list()
        data.append(nonce)
        data.append(gas_price)
        data.append(gas_limit)
        data.append(to)
        data.append(int(amount * WEI_IN_ETH))
        data.append(bytes())
        data.append(chain_id)
        data.append(bytes())
        data.append(bytes())

        chunks = self._cmd_builder.sign(bip32_path, rlp.encode(data))
        for chunk in chunks[:-1]:
            with self._send(chunk):
                pass
        return self._send(chunks[-1])

    def get_challenge(self):
        return self._send(self._cmd_builder.get_challenge())

    def provide_domain_name(self, challenge: int, name: str, addr: bytes):
        payload  = format_tlv(DOMAIN_NAME_TAG.STRUCTURE_TYPE, 3) # TrustedDomainName
        payload += format_tlv(DOMAIN_NAME_TAG.STRUCTURE_VERSION, 1)
        payload += format_tlv(DOMAIN_NAME_TAG.SIGNER_KEY_ID, 0) # test key
        payload += format_tlv(DOMAIN_NAME_TAG.SIGNER_ALGO, 1) # secp256k1
        payload += format_tlv(DOMAIN_NAME_TAG.CHALLENGE, challenge)
        payload += format_tlv(DOMAIN_NAME_TAG.COIN_TYPE, 0x3c) # ETH in slip-44
        payload += format_tlv(DOMAIN_NAME_TAG.DOMAIN_NAME, name)
        payload += format_tlv(DOMAIN_NAME_TAG.ADDRESS, addr)
        payload += format_tlv(DOMAIN_NAME_TAG.SIGNATURE,
                              sign_data(Key.DOMAIN_NAME, payload))

        chunks = self._cmd_builder.provide_domain_name(payload)
        for chunk in chunks[:-1]:
            with self._send(chunk):
                pass
        return self._send(chunks[-1])
