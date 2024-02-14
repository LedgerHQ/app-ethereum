# documentation about APDU format is available here:
# https://github.com/LedgerHQ/app-ethereum/blob/develop/doc/ethapp.adoc

import struct
from enum import IntEnum
from typing import Optional
from ragger.bip import pack_derivation_path

from .eip712 import EIP712FieldType


class InsType(IntEnum):
    GET_PUBLIC_ADDR = 0x02
    SIGN = 0x04
    PERSONAL_SIGN = 0x08
    PROVIDE_ERC20_TOKEN_INFORMATION = 0x0a
    PROVIDE_NFT_INFORMATION = 0x14
    SET_PLUGIN = 0x16
    EIP712_SEND_STRUCT_DEF = 0x1a
    EIP712_SEND_STRUCT_IMPL = 0x1c
    EIP712_SEND_FILTERING = 0x1e
    EIP712_SIGN = 0x0c
    GET_CHALLENGE = 0x20
    PROVIDE_DOMAIN_NAME = 0x22
    EXTERNAL_PLUGIN_SETUP = 0x12


class P1Type(IntEnum):
    COMPLETE_SEND = 0x00
    PARTIAL_SEND = 0x01
    SIGN_FIRST_CHUNK = 0x00
    SIGN_SUBSQT_CHUNK = 0x80


class P2Type(IntEnum):
    STRUCT_NAME = 0x00
    STRUCT_FIELD = 0xff
    ARRAY = 0x0f
    LEGACY_IMPLEM = 0x00
    NEW_IMPLEM = 0x01
    FILTERING_ACTIVATE = 0x00
    FILTERING_CONTRACT_NAME = 0x0f
    FILTERING_FIELD_NAME = 0xff


class CommandBuilder:
    _CLA: int = 0xE0

    def _serialize(self,
                   ins: InsType,
                   p1: int,
                   p2: int,
                   cdata: bytes = bytes()) -> bytes:

        header = bytearray()
        header.append(self._CLA)
        header.append(ins)
        header.append(p1)
        header.append(p2)
        header.append(len(cdata))
        return header + cdata

    def _string_to_bytes(self, string: str) -> bytes:
        data = bytearray()
        for char in string:
            data.append(ord(char))
        return data

    def eip712_send_struct_def_struct_name(self, name: str) -> bytes:
        return self._serialize(InsType.EIP712_SEND_STRUCT_DEF,
                               P1Type.COMPLETE_SEND,
                               P2Type.STRUCT_NAME,
                               self._string_to_bytes(name))

    def eip712_send_struct_def_struct_field(self,
                                            field_type: EIP712FieldType,
                                            type_name: str,
                                            type_size: int,
                                            array_levels: list,
                                            key_name: str) -> bytes:
        data = bytearray()
        typedesc = 0
        typedesc |= (len(array_levels) > 0) << 7
        typedesc |= (type_size is not None) << 6
        typedesc |= field_type
        data.append(typedesc)
        if field_type == EIP712FieldType.CUSTOM:
            data.append(len(type_name))
            data += self._string_to_bytes(type_name)
        if type_size is not None:
            data.append(type_size)
        if len(array_levels) > 0:
            data.append(len(array_levels))
            for level in array_levels:
                data.append(0 if level is None else 1)
                if level is not None:
                    data.append(level)
        data.append(len(key_name))
        data += self._string_to_bytes(key_name)
        return self._serialize(InsType.EIP712_SEND_STRUCT_DEF,
                               P1Type.COMPLETE_SEND,
                               P2Type.STRUCT_FIELD,
                               data)

    def eip712_send_struct_impl_root_struct(self, name: str) -> bytes:
        return self._serialize(InsType.EIP712_SEND_STRUCT_IMPL,
                               P1Type.COMPLETE_SEND,
                               P2Type.STRUCT_NAME,
                               self._string_to_bytes(name))

    def eip712_send_struct_impl_array(self, size: int) -> bytes:
        data = bytearray()
        data.append(size)
        return self._serialize(InsType.EIP712_SEND_STRUCT_IMPL,
                               P1Type.COMPLETE_SEND,
                               P2Type.ARRAY,
                               data)

    def eip712_send_struct_impl_struct_field(self, data: bytearray) -> list[bytes]:
        chunks = list()
        # Add a 16-bit integer with the data's byte length (network byte order)
        data_w_length = bytearray()
        data_w_length.append((len(data) & 0xff00) >> 8)
        data_w_length.append(len(data) & 0x00ff)
        data_w_length += data
        while len(data_w_length) > 0:
            p1 = P1Type.PARTIAL_SEND if len(data_w_length) > 0xff else P1Type.COMPLETE_SEND
            chunks.append(self._serialize(InsType.EIP712_SEND_STRUCT_IMPL,
                                          p1,
                                          P2Type.STRUCT_FIELD,
                                          data_w_length[:0xff]))
            data_w_length = data_w_length[0xff:]
        return chunks

    def eip712_sign_new(self, bip32_path: str) -> bytes:
        data = pack_derivation_path(bip32_path)
        return self._serialize(InsType.EIP712_SIGN,
                               P1Type.COMPLETE_SEND,
                               P2Type.NEW_IMPLEM,
                               data)

    def eip712_sign_legacy(self,
                           bip32_path: str,
                           domain_hash: bytes,
                           message_hash: bytes) -> bytes:
        data = pack_derivation_path(bip32_path)
        data += domain_hash
        data += message_hash
        return self._serialize(InsType.EIP712_SIGN,
                               P1Type.COMPLETE_SEND,
                               P2Type.LEGACY_IMPLEM,
                               data)

    def eip712_filtering_activate(self):
        return self._serialize(InsType.EIP712_SEND_FILTERING,
                               P1Type.COMPLETE_SEND,
                               P2Type.FILTERING_ACTIVATE,
                               bytearray())

    def _eip712_filtering_send_name(self, name: str, sig: bytes) -> bytes:
        data = bytearray()
        data.append(len(name))
        data += self._string_to_bytes(name)
        data.append(len(sig))
        data += sig
        return data

    def eip712_filtering_message_info(self, name: str, filters_count: int, sig: bytes) -> bytes:
        data = bytearray()
        data.append(len(name))
        data += self._string_to_bytes(name)
        data.append(filters_count)
        data.append(len(sig))
        data += sig
        return self._serialize(InsType.EIP712_SEND_FILTERING,
                               P1Type.COMPLETE_SEND,
                               P2Type.FILTERING_CONTRACT_NAME,
                               data)

    def eip712_filtering_show_field(self, name: str, sig: bytes) -> bytes:
        return self._serialize(InsType.EIP712_SEND_FILTERING,
                               P1Type.COMPLETE_SEND,
                               P2Type.FILTERING_FIELD_NAME,
                               self._eip712_filtering_send_name(name, sig))

    def set_external_plugin(self, plugin_name: str, contract_address: bytes, selector: bytes, sig: bytes) -> bytes:
        data = bytearray()
        data.append(len(plugin_name))
        data += self._string_to_bytes(plugin_name)
        data += contract_address
        data += selector
        data += sig

        return self._serialize(InsType.EXTERNAL_PLUGIN_SETUP,
                               P1Type.COMPLETE_SEND,
                               0x00,
                               data)

    def sign(self, bip32_path: str, rlp_data: bytes, vrs: list) -> list[bytes]:
        apdus = list()
        payload = pack_derivation_path(bip32_path)
        payload += rlp_data
        p1 = P1Type.SIGN_FIRST_CHUNK
        while len(payload) > 0:
            chunk_size = 0xff

            # TODO: Fix the app & remove this, issue #409
            if len(vrs) == 3:
                if len(payload) > chunk_size:
                    import rlp
                    diff = len(rlp.encode(vrs)) - (len(payload) - chunk_size)
                    if diff > 0:
                        chunk_size -= diff

            apdus.append(self._serialize(InsType.SIGN,
                                         p1,
                                         0x00,
                                         payload[:chunk_size]))
            payload = payload[chunk_size:]
            p1 = P1Type.SIGN_SUBSQT_CHUNK
        return apdus

    def get_challenge(self) -> bytes:
        return self._serialize(InsType.GET_CHALLENGE, 0x00, 0x00)

    def provide_domain_name(self, tlv_payload: bytes) -> list[bytes]:
        chunks = list()
        payload = struct.pack(">H", len(tlv_payload))
        payload += tlv_payload
        p1 = 1
        while len(payload) > 0:
            chunks.append(self._serialize(InsType.PROVIDE_DOMAIN_NAME,
                                          p1,
                                          0x00,
                                          payload[:0xff]))
            payload = payload[0xff:]
            p1 = 0
        return chunks

    def get_public_addr(self,
                        display: bool,
                        chaincode: bool,
                        bip32_path: str,
                        chain_id: Optional[int]) -> bytes:
        payload = pack_derivation_path(bip32_path)
        if chain_id is not None:
            payload += struct.pack(">Q", chain_id)
        return self._serialize(InsType.GET_PUBLIC_ADDR,
                               int(display),
                               int(chaincode),
                               payload)

    def set_plugin(self,
                   type_: int,
                   version: int,
                   plugin_name: str,
                   contract_addr: bytes,
                   selector: bytes,
                   chain_id: int,
                   key_id: int,
                   algo_id: int,
                   sig: bytes) -> bytes:
        payload = bytearray()
        payload.append(type_)
        payload.append(version)
        payload.append(len(plugin_name))
        payload += plugin_name.encode()
        payload += contract_addr
        payload += selector
        payload += struct.pack(">Q", chain_id)
        payload.append(key_id)
        payload.append(algo_id)
        payload.append(len(sig))
        payload += sig
        return self._serialize(InsType.SET_PLUGIN, 0x00, 0x00, payload)

    def provide_nft_information(self,
                                type_: int,
                                version: int,
                                collection_name: str,
                                addr: bytes,
                                chain_id: int,
                                key_id: int,
                                algo_id: int,
                                sig: bytes):
        payload = bytearray()
        payload.append(type_)
        payload.append(version)
        payload.append(len(collection_name))
        payload += collection_name.encode()
        payload += addr
        payload += struct.pack(">Q", chain_id)
        payload.append(key_id)
        payload.append(algo_id)
        payload.append(len(sig))
        payload += sig
        return self._serialize(InsType.PROVIDE_NFT_INFORMATION, 0x00, 0x00, payload)

    def personal_sign(self, path: str, msg: bytes):
        payload = pack_derivation_path(path)
        payload += struct.pack(">I", len(msg))
        payload += msg
        chunks = list()
        p1 = P1Type.SIGN_FIRST_CHUNK
        while len(payload) > 0:
            chunk_size = 0xff
            chunks.append(self._serialize(InsType.PERSONAL_SIGN,
                                          p1,
                                          0x00,
                                          payload[:chunk_size]))
            payload = payload[chunk_size:]
            p1 = P1Type.SIGN_SUBSQT_CHUNK
        return chunks

    def provide_erc20_token_information(self,
                                        ticker: str,
                                        addr: bytes,
                                        decimals: int,
                                        chain_id: int,
                                        sig: bytes) -> bytes:
        payload = bytearray()
        payload.append(len(ticker))
        payload += ticker.encode()
        payload += addr
        payload += struct.pack(">I", decimals)
        payload += struct.pack(">I", chain_id)
        payload += sig
        return self._serialize(InsType.PROVIDE_ERC20_TOKEN_INFORMATION,
                               0x00,
                               0x00,
                               payload)
