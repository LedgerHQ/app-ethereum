from enum import Enum, auto
from ragger.firmware import Firmware
from ragger.navigator import Navigator, NavInsID, NavIns
from typing import Union


class SettingID(Enum):
    BLIND_SIGNING = auto()
    DEBUG_DATA = auto()
    NONCE = auto()
    VERBOSE_EIP712 = auto()
    VERBOSE_ENS = auto()


def get_device_settings(device: str) -> list[SettingID]:
    if device == "nanos":
        return [
            SettingID.BLIND_SIGNING,
            SettingID.DEBUG_DATA,
            SettingID.NONCE
        ]
    if (device == "nanox") or (device == "nanosp") or (device == "stax"):
        return [
            SettingID.BLIND_SIGNING,
            SettingID.DEBUG_DATA,
            SettingID.NONCE,
            SettingID.VERBOSE_EIP712,
            SettingID.VERBOSE_ENS
        ]
    return []


settings_per_page = 3


def get_setting_position(device: str, setting: Union[NavInsID, SettingID]) -> tuple[int, int]:
    screen_height = 672  # px
    header_height = 85  # px
    footer_height = 124  # px
    usable_height = screen_height - (header_height + footer_height)
    setting_height = usable_height // settings_per_page
    index_in_page = get_device_settings(device).index(SettingID(setting)) % settings_per_page
    return 350, header_height + (setting_height * index_in_page) + (setting_height // 2)


def settings_toggle(fw: Firmware, nav: Navigator, to_toggle: list[SettingID]):
    moves: list[Union[NavIns, NavInsID]] = list()
    settings = get_device_settings(fw.device)
    # Assume the app is on the home page
    if fw.device.startswith("nano"):
        moves += [NavInsID.RIGHT_CLICK] * 2
        moves += [NavInsID.BOTH_CLICK]
        for setting in settings:
            if setting in to_toggle:
                moves += [NavInsID.BOTH_CLICK]
            moves += [NavInsID.RIGHT_CLICK]
        moves += [NavInsID.BOTH_CLICK]  # Back
    else:
        moves += [NavInsID.USE_CASE_HOME_SETTINGS]
        moves += [NavInsID.USE_CASE_SETTINGS_NEXT]
        for setting in settings:
            setting_idx = settings.index(setting)
            if (setting_idx > 0) and (setting_idx % settings_per_page) == 0:
                moves += [NavInsID.USE_CASE_SETTINGS_NEXT]
            if setting in to_toggle:
                moves += [NavIns(NavInsID.TOUCH, get_setting_position(fw.device, setting))]
        moves += [NavInsID.USE_CASE_SETTINGS_MULTI_PAGE_EXIT]
    nav.navigate(moves, screen_change_before_first_instruction=False)
