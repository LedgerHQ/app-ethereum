from enum import IntEnum, auto
from typing import Iterator, Optional
from .eip712 import EIP712FieldType
from ragger.bip import pack_derivation_path
import struct

class InsType(IntEnum):
    SIGN = 0x04
    EIP712_SEND_STRUCT_DEF = 0x1a
    EIP712_SEND_STRUCT_IMPL = 0x1c
    EIP712_SEND_FILTERING = 0x1e
    EIP712_SIGN = 0x0c
    GET_CHALLENGE = 0x20
    PROVIDE_DOMAIN_NAME = 0x22

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
                   cdata: bytearray = bytes()) -> bytes:

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
                                            array_levels: [],
                                            key_name: str) -> bytes:
        data = bytearray()
        typedesc = 0
        typedesc |= (len(array_levels) > 0) << 7
        typedesc |= (type_size != None) << 6
        typedesc |= field_type
        data.append(typedesc)
        if field_type == EIP712FieldType.CUSTOM:
            data.append(len(type_name))
            data += self._string_to_bytes(type_name)
        if type_size != None:
            data.append(type_size)
        if len(array_levels) > 0:
            data.append(len(array_levels))
            for level in array_levels:
                data.append(0 if level == None else 1)
                if level != None:
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

    def eip712_send_struct_impl_struct_field(self, data: bytearray) -> Iterator[bytes]:
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

    def sign(self, bip32_path: str, rlp_data: bytes) -> list[bytes]:
        apdus = list()
        payload = pack_derivation_path(bip32_path)
        payload += rlp_data
        p1 = P1Type.SIGN_FIRST_CHUNK
        while len(payload) > 0:
            apdus.append(self._serialize(InsType.SIGN,
                                         p1,
                                         0x00,
                                         payload[:0xff]))
            payload = payload[0xff:]
            p1 = P1Type.SIGN_SUBSQT_CHUNK
        return apdus

    def get_challenge(self) -> bytes:
        return self._serialize(InsType.GET_CHALLENGE, 0x00, 0x00)

    def provide_domain_name(self, tlv_payload: bytes) -> list[bytes]:
        chunks = list()
        payload  = struct.pack(">H", len(tlv_payload))
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
