# documentation about APDU format is available here:
# https://github.com/LedgerHQ/app-ethereum/blob/develop/doc/ethapp.adoc

import struct
from enum import IntEnum
from typing import List, Optional
from ragger.bip import pack_derivation_path

from .eip712 import EIP712FieldType


class InsType(IntEnum):
    GET_PUBLIC_ADDR = 0x02
    GET_ETH2_PUBLIC_ADDR = 0x0e
    SIGN = 0x04
    PERSONAL_SIGN = 0x08
    PROVIDE_ERC20_TOKEN_INFORMATION = 0x0a
    EXTERNAL_PLUGIN_SETUP = 0x12
    PROVIDE_NFT_INFORMATION = 0x14
    SET_PLUGIN = 0x16
    PERFORM_PRIVACY_OPERATION = 0x18
    EIP712_SEND_STRUCT_DEF = 0x1a
    EIP712_SEND_STRUCT_IMPL = 0x1c
    EIP712_SEND_FILTERING = 0x1e
    EIP712_SIGN = 0x0c
    GET_CHALLENGE = 0x20
    PROVIDE_TRUSTED_NAME = 0x22
    PROVIDE_ENUM_VALUE = 0x24
    PROVIDE_TRANSACTION_INFO = 0x26
    PROVIDE_NETWORK_INFORMATION = 0x30


class P1Type(IntEnum):
    COMPLETE_SEND = 0x00
    PARTIAL_SEND = 0x01
    SIGN_FIRST_CHUNK = 0x00
    SIGN_SUBSQT_CHUNK = 0x80
    FIRST_CHUNK = 0x01
    FOLLOWING_CHUNK = 0x00


class P2Type(IntEnum):
    STRUCT_NAME = 0x00
    STRUCT_FIELD = 0xff
    ARRAY = 0x0f
    LEGACY_IMPLEM = 0x00
    NEW_IMPLEM = 0x01
    FILTERING_ACTIVATE = 0x00
    FILTERING_DISCARDED_PATH = 0x01
    FILTERING_MESSAGE_INFO = 0x0f
    FILTERING_TRUSTED_NAME = 0xfb
    FILTERING_DATETIME = 0xfc
    FILTERING_TOKEN_ADDR_CHECK = 0xfd
    FILTERING_AMOUNT_FIELD = 0xfe
    FILTERING_RAW = 0xff
    NETWORK_CONFIG = 0x00
    NETWORK_ICON = 0x01


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

    def eip712_send_struct_def_struct_name(self, name: str) -> bytes:
        return self._serialize(InsType.EIP712_SEND_STRUCT_DEF,
                               P1Type.COMPLETE_SEND,
                               P2Type.STRUCT_NAME,
                               name.encode())

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
            data += type_name.encode()
        if type_size is not None:
            data.append(type_size)
        if len(array_levels) > 0:
            data.append(len(array_levels))
            for level in array_levels:
                data.append(0 if level is None else 1)
                if level is not None:
                    data.append(level)
        data.append(len(key_name))
        data += key_name.encode()
        return self._serialize(InsType.EIP712_SEND_STRUCT_DEF,
                               P1Type.COMPLETE_SEND,
                               P2Type.STRUCT_FIELD,
                               data)

    def eip712_send_struct_impl_root_struct(self, name: str) -> bytes:
        return self._serialize(InsType.EIP712_SEND_STRUCT_IMPL,
                               P1Type.COMPLETE_SEND,
                               P2Type.STRUCT_NAME,
                               name.encode())

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
        data += name.encode()
        data.append(len(sig))
        data += sig
        return data

    def eip712_filtering_discarded_path(self, path: str) -> bytes:
        data = bytearray()
        data.append(len(path))
        data += path.encode()
        return self._serialize(InsType.EIP712_SEND_FILTERING,
                               P1Type.COMPLETE_SEND,
                               P2Type.FILTERING_DISCARDED_PATH,
                               data)

    def eip712_filtering_message_info(self, name: str, filters_count: int, sig: bytes) -> bytes:
        data = bytearray()
        data.append(len(name))
        data += name.encode()
        data.append(filters_count)
        data.append(len(sig))
        data += sig
        return self._serialize(InsType.EIP712_SEND_FILTERING,
                               P1Type.COMPLETE_SEND,
                               P2Type.FILTERING_MESSAGE_INFO,
                               data)

    def eip712_filtering_amount_join_token(self, token_idx: int, sig: bytes, discarded: bool) -> bytes:
        data = bytearray()
        data.append(token_idx)
        data.append(len(sig))
        data += sig
        return self._serialize(InsType.EIP712_SEND_FILTERING,
                               int(discarded),
                               P2Type.FILTERING_TOKEN_ADDR_CHECK,
                               data)

    def eip712_filtering_amount_join_value(self, token_idx: int, name: str, sig: bytes, discarded: bool) -> bytes:
        data = bytearray()
        data.append(len(name))
        data += name.encode()
        data.append(token_idx)
        data.append(len(sig))
        data += sig
        return self._serialize(InsType.EIP712_SEND_FILTERING,
                               int(discarded),
                               P2Type.FILTERING_AMOUNT_FIELD,
                               data)

    def eip712_filtering_datetime(self, name: str, sig: bytes, discarded: bool) -> bytes:
        return self._serialize(InsType.EIP712_SEND_FILTERING,
                               int(discarded),
                               P2Type.FILTERING_DATETIME,
                               self._eip712_filtering_send_name(name, sig))

    def eip712_filtering_trusted_name(self,
                                      name: str,
                                      name_types: list[int],
                                      name_sources: list[int],
                                      sig: bytes,
                                      discarded: bool) -> bytes:
        data = bytearray()
        data.append(len(name))
        data += name.encode()
        data.append(len(name_types))
        for t in name_types:
            data.append(t)
        data.append(len(name_sources))
        for s in name_sources:
            data.append(s)
        data.append(len(sig))
        data += sig
        return self._serialize(InsType.EIP712_SEND_FILTERING,
                               int(discarded),
                               P2Type.FILTERING_TRUSTED_NAME,
                               data)

    def eip712_filtering_raw(self, name: str, sig: bytes, discarded: bool) -> bytes:
        return self._serialize(InsType.EIP712_SEND_FILTERING,
                               int(discarded),
                               P2Type.FILTERING_RAW,
                               self._eip712_filtering_send_name(name, sig))

    def set_external_plugin(self, plugin_name: str, contract_address: bytes, selector: bytes, sig: bytes) -> bytes:
        data = bytearray()
        data.append(len(plugin_name))
        data += plugin_name.encode()
        data += contract_address
        data += selector
        data += sig

        return self._serialize(InsType.EXTERNAL_PLUGIN_SETUP,
                               P1Type.COMPLETE_SEND,
                               0x00,
                               data)

    def sign(self, bip32_path: str, rlp_data: bytes, p2: int) -> list[bytes]:
        apdus = list()
        payload = pack_derivation_path(bip32_path)
        payload += rlp_data
        p1 = P1Type.SIGN_FIRST_CHUNK
        while len(payload) > 0:
            apdus.append(self._serialize(InsType.SIGN,
                                         p1,
                                         p2,
                                         payload[:0xff]))
            payload = payload[0xff:]
            p1 = P1Type.SIGN_SUBSQT_CHUNK
        return apdus

    def get_challenge(self) -> bytes:
        return self._serialize(InsType.GET_CHALLENGE, 0x00, 0x00)

    def provide_trusted_name(self, tlv_payload: bytes) -> list[bytes]:
        chunks = list()
        payload = struct.pack(">H", len(tlv_payload))
        payload += tlv_payload
        p1 = 1
        while len(payload) > 0:
            chunks.append(self._serialize(InsType.PROVIDE_TRUSTED_NAME,
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

    def get_eth2_public_addr(self,
                             display: bool,
                             bip32_path: str) -> bytes:
        payload = pack_derivation_path(bip32_path)
        return self._serialize(InsType.GET_ETH2_PUBLIC_ADDR,
                               int(display),
                               0x00,
                               payload)

    def perform_privacy_operation(self,
                                  display: bool,
                                  bip32_path: str,
                                  pubkey: bytes) -> bytes:
        payload = pack_derivation_path(bip32_path)
        return self._serialize(InsType.PERFORM_PRIVACY_OPERATION,
                               int(display),
                               0x01 if pubkey else 0x00,
                               payload + pubkey)

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

    def provide_network_information(self,
                                    tlv_payload: bytes,
                                    icon: Optional[bytes] = None) -> list[bytes]:
        chunks: List[bytes] = []

        # Check if the TLV payload is larger than 0xff
        assert len(tlv_payload) < 0xff, "Payload too large"
        # Serialize the payload
        chunks.append(self._serialize(InsType.PROVIDE_NETWORK_INFORMATION,
                                      0x00,
                                      P2Type.NETWORK_CONFIG,
                                      tlv_payload))

        if icon:
            p1 = P1Type.FIRST_CHUNK
            while len(icon) > 0:
                chunks.append(self._serialize(InsType.PROVIDE_NETWORK_INFORMATION,
                                              p1,
                                              P2Type.NETWORK_ICON,
                                              icon[:0xff]))
                icon = icon[0xff:]
                p1 = P1Type.FOLLOWING_CHUNK
        return chunks

    def common_tlv_serialize(self, tlv_payload: bytes, ins: InsType) -> list[bytes]:
        chunks = list()
        payload = struct.pack(">H", len(tlv_payload))
        payload += tlv_payload
        p1 = 1
        while len(payload) > 0:
            chunks.append(self._serialize(ins,
                                          p1,
                                          0x00,
                                          payload[:0xff]))
            payload = payload[0xff:]
            p1 = 0
        return chunks

    def provide_enum_value(self, tlv_payload: bytes) -> list[bytes]:
        return self.common_tlv_serialize(tlv_payload, InsType.PROVIDE_ENUM_VALUE)

    def provide_transaction_info(self, tlv_payload: bytes) -> list[bytes]:
        return self.common_tlv_serialize(tlv_payload, InsType.PROVIDE_TRANSACTION_INFO)
