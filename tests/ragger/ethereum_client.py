from enum import IntEnum, auto
from typing import Iterator
from ragger.backend import BackendInterface
from ragger.utils import RAPDU

class   InsType(IntEnum):
    EIP712_SEND_STRUCT_DEF = 0x1a,
    EIP712_SEND_STRUCT_IMPL = 0x1c,
    EIP712_SEND_FILTERING = 0x1e,
    EIP712_SIGN = 0x0c

class   P1Type(IntEnum):
    COMPLETE_SEND = 0x00,
    PARTIAL_SEND = 0x01

class   P2Type(IntEnum):
    STRUCT_NAME = 0x00,
    STRUCT_FIELD = 0xff,
    ARRAY = 0x0f,
    LEGACY_IMPLEM = 0x00
    NEW_IMPLEM = 0x01,

class   EIP712FieldType(IntEnum):
    CUSTOM = 0,
    INT = auto()
    UINT = auto()
    ADDRESS = auto()
    BOOL = auto()
    STRING = auto()
    FIX_BYTES = auto()
    DYN_BYTES = auto()


class   EthereumClientCmdBuilder:
    _CLA: int = 0xE0

    def _serialize(self,
                   ins: InsType,
                   p1: int,
                   p2: int,
                   cdata: bytearray = bytearray()) -> bytes:

        header = bytearray()
        header.append(self._CLA)
        header.append(ins)
        header.append(p1)
        header.append(p2)
        header.append(len(cdata))
        return header + cdata

    def eip712_send_struct_def_struct_name(self, name: str) -> bytes:
        data = bytearray()
        for char in name:
            data.append(ord(char))
        return self._serialize(InsType.EIP712_SEND_STRUCT_DEF,
                               P1Type.COMPLETE_SEND,
                               P2Type.STRUCT_NAME,
                               data)

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
            for char in type_name:
                data.append(ord(char))
        if type_size != None:
            data.append(type_size)
        if len(array_levels) > 0:
            data.append(len(array_levels))
            for level in array_levels:
                data.append(0 if level == None else 1)
                if level != None:
                    data.append(level)
        data.append(len(key_name))
        for char in key_name:
            data.append(ord(char))
        return self._serialize(InsType.EIP712_SEND_STRUCT_DEF,
                               P1Type.COMPLETE_SEND,
                               P2Type.STRUCT_FIELD,
                               data)

    def eip712_send_struct_impl_root_struct(self, name: str) -> bytes:
        data = bytearray()
        for char in name:
            data.append(ord(char))
        return self._serialize(InsType.EIP712_SEND_STRUCT_IMPL,
                               P1Type.COMPLETE_SEND,
                               P2Type.STRUCT_NAME,
                               data)

    def eip712_send_struct_impl_array(self, size: int) -> bytes:
        data = bytearray()
        data.append(size)
        return self._serialize(InsType.EIP712_SEND_STRUCT_IMPL,
                               P1Type.COMPLETE_SEND,
                               P2Type.ARRAY,
                               data)

    def eip712_send_struct_impl_struct_field(self, data: bytearray) -> Iterator[bytes]:
        # Add a 16-bit integer with the data's byte length (network byte order)
        data_w_length = bytearray()
        data_w_length.append((len(data) & 0xff00) >> 8)
        data_w_length.append(len(data) & 0x00ff)
        data_w_length += data
        while len(data_w_length) > 0:
            p1 = P1Type.PARTIAL_SEND if len(data_w_length) > 0xff else P1Type.COMPLETE_SEND
            yield self._serialize(InsType.EIP712_SEND_STRUCT_IMPL,
                                  p1,
                                  P2Type.STRUCT_FIELD,
                                  data_w_length[:0xff])
            data_w_length = data_w_length[0xff:]

    def _format_bip32(self, bip32, data = bytearray()) -> bytearray:
        data.append(len(bip32))
        for idx in bip32:
            data.append((idx & 0xff000000) >> 24)
            data.append((idx & 0x00ff0000) >> 16)
            data.append((idx & 0x0000ff00) >> 8)
            data.append((idx & 0x000000ff))
        return data

    def eip712_sign_new(self, bip32) -> bytes:
        data = self._format_bip32(bip32)
        return self._serialize(InsType.EIP712_SIGN,
                               P1Type.COMPLETE_SEND,
                               P2Type.NEW_IMPLEM,
                               data)

    def eip712_sign_legacy(self,
                           bip32,
                           domain_hash: bytes,
                           message_hash: bytes) -> bytes:
        data = self._format_bip32(bip32)
        data += domain_hash
        data += message_hash
        return self._serialize(InsType.EIP712_SIGN,
                               P1Type.COMPLETE_SEND,
                               P2Type.LEGACY_IMPLEM,
                               data)


class   EthereumResponseParser:
    def sign(self, data: bytes):
        assert len(data) == (1 + 32 + 32)

        v = data[0:1]
        data = data[1:]

        r = data[0:32]
        data = data[32:]

        s = data[0:32]
        data = data[32:]

        return v, r, s


class   EthereumClient:
    def __init__(self, client: BackendInterface, debug: bool = False):
        self._client = client
        self._debug = debug
        self._cmd_builder = EthereumClientCmdBuilder()
        self._resp_parser = EthereumResponseParser()

    def _send(self, payload: bytearray) -> None:
        self._client.send_raw(payload)

    def _recv(self) -> RAPDU:
        return self._client.receive()

    def eip712_send_struct_def_struct_name(self, name: str):
        self._send(self._cmd_builder.eip712_send_struct_def_struct_name(name))
        return self._recv()

    def eip712_send_struct_def_struct_field(self,
                                            field_type: EIP712FieldType,
                                            type_name: str,
                                            type_size: int,
                                            array_levels: [],
                                            key_name: str):
        self._send(self._cmd_builder.eip712_send_struct_def_struct_field(
            field_type,
            type_name,
            type_size,
            array_levels,
            key_name))
        return self._recv()

    def eip712_send_struct_impl_root_struct(self, name: str):
        self._send(self._cmd_builder.eip712_send_struct_impl_root_struct(name))
        return self._recv()

    def eip712_send_struct_impl_array(self, size: int):
        self._send(self._cmd_builder.eip712_send_struct_impl_array(size))
        return self._recv()

    def eip712_send_struct_impl_struct_field(self, raw_value: bytes):
        ret = None
        for apdu in self._cmd_builder.eip712_send_struct_impl_struct_field(raw_value):
            self._send(apdu)
            # TODO: Do clicks
            ret = self._recv()
        return ret

    def eip712_sign_new(self, bip32):
        self._send(self._cmd_builder.eip712_sign_new(bip32))
        return self._recv()

    def eip712_sign_legacy(self,
                           bip32,
                           domain_hash: bytes,
                           message_hash: bytes):
        self._send(self._cmd_builder.eip712_sign_legacy(bip32,
                                                        domain_hash,
                                                        message_hash))
        self._client.right_click() # sign typed message screen
        for _ in range(2): # two hashes (domain + message)
            if self._client.firmware.device == "nanos":
                screens_per_hash = 4
            else:
                screens_per_hash = 2
            for _ in range(screens_per_hash):
                self._client.right_click()
        self._client.both_click() # approve signature

        resp = self._recv()

        assert resp.status == 0x9000
        return self._resp_parser.sign(resp.data)
