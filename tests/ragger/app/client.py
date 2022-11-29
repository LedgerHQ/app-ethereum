from enum import IntEnum, auto
from typing import Optional
from ragger.backend import BackendInterface
from ragger.utils import RAPDU
from app.command_builder import EthereumCmdBuilder
from app.setting import SettingType, SettingImpl
from app.eip712 import EIP712FieldType
from app.response_parser import EthereumRespParser
import signal
import time


class   EthereumClient:
    _settings: dict[SettingType, SettingImpl] = {
        SettingType.BLIND_SIGNING: SettingImpl(
            [ "nanos", "nanox", "nanosp" ]
        ),
        SettingType.DEBUG_DATA: SettingImpl(
            [ "nanos", "nanox", "nanosp" ]
        ),
        SettingType.NONCE: SettingImpl(
            [ "nanos", "nanox", "nanosp" ]
        ),
        SettingType.VERBOSE_EIP712: SettingImpl(
            [ "nanox", "nanosp" ]
        )
    }
    _click_delay = 1/4
    _eip712_filtering = False

    def __init__(self, client: BackendInterface):
        self._client = client
        self._cmd_builder = EthereumCmdBuilder()
        self._resp_parser = EthereumRespParser()
        signal.signal(signal.SIGALRM, self._click_signal_timeout)
        for setting in self._settings.values():
            setting.value = False

    def _send(self, payload: bytearray):
        return self._client.exchange_async_raw(payload)

    def _recv(self) -> RAPDU:
        return self._client._last_async_response

    def _click_signal_timeout(self, _signum: int, _frame):
        self._client.right_click()

    def _enable_click_until_response(self):
        signal.setitimer(signal.ITIMER_REAL,
                         self._click_delay,
                         self._click_delay)

    def _disable_click_until_response(self):
        signal.setitimer(signal.ITIMER_REAL, 0, 0)

    def eip712_send_struct_def_struct_name(self, name: str):
        with self._send(self._cmd_builder.eip712_send_struct_def_struct_name(name)):
            pass
        return self._recv().status == 0x9000

    def eip712_send_struct_def_struct_field(self,
                                            field_type: EIP712FieldType,
                                            type_name: str,
                                            type_size: int,
                                            array_levels: [],
                                            key_name: str):
        with self._send(self._cmd_builder.eip712_send_struct_def_struct_field(
                        field_type,
                        type_name,
                        type_size,
                        array_levels,
                        key_name)):
            pass
        return self._recv()

    def eip712_send_struct_impl_root_struct(self, name: str):
        with self._send(self._cmd_builder.eip712_send_struct_impl_root_struct(name)):
            self._enable_click_until_response()
        self._disable_click_until_response()
        return self._recv()

    def eip712_send_struct_impl_array(self, size: int):
        with self._send(self._cmd_builder.eip712_send_struct_impl_array(size)):
            pass
        return self._recv()

    def eip712_send_struct_impl_struct_field(self, raw_value: bytes):
        for apdu in self._cmd_builder.eip712_send_struct_impl_struct_field(raw_value):
            with self._send(apdu):
                self._enable_click_until_response()
            self._disable_click_until_response()
            assert self._recv().status == 0x9000

    def eip712_sign_new(self, bip32):
        with self._send(self._cmd_builder.eip712_sign_new(bip32)):
            time.sleep(0.5) # tight on timing, needed by the CI otherwise might fail sometimes
            if not self._settings[SettingType.VERBOSE_EIP712].value and \
               not self._eip712_filtering: # need to skip the message hash
                self._client.right_click()
                self._client.right_click()
            self._client.both_click() # approve signature
        resp = self._recv()
        assert resp.status == 0x9000
        return self._resp_parser.sign(resp.data)

    def eip712_sign_legacy(self,
                           bip32,
                           domain_hash: bytes,
                           message_hash: bytes):
        with self._send(self._cmd_builder.eip712_sign_legacy(bip32,
                                                             domain_hash,
                                                             message_hash)):
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

    def settings_set(self, new_values: dict[SettingType, bool]):
        # Go to settings
        for _ in range(2):
            self._client.right_click()
        self._client.both_click()

        for enum in self._settings.keys():
            if self._client.firmware.device in self._settings[enum].devices:
                if enum in new_values.keys():
                    if new_values[enum] != self._settings[enum].value:
                        self._client.both_click()
                        self._settings[enum].value = new_values[enum]
                self._client.right_click()
        self._client.both_click()

    def eip712_filtering_activate(self):
        with self._send(self._cmd_builder.eip712_filtering_activate()):
            pass
        self._eip712_filtering = True
        assert self._recv().status == 0x9000

    def eip712_filtering_message_info(self, name: str, filters_count: int, sig: bytes):
        with self._send(self._cmd_builder.eip712_filtering_message_info(name, filters_count, sig)):
            self._enable_click_until_response()
        self._disable_click_until_response()
        assert self._recv().status == 0x9000

    def eip712_filtering_show_field(self, name: str, sig: bytes):
        with self._send(self._cmd_builder.eip712_filtering_show_field(name, sig)):
            pass
        assert self._recv().status == 0x9000
