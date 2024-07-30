from enum import Enum, auto
from ragger.firmware import Firmware
from ragger.navigator import Navigator, NavInsID, NavIns
from typing import Union


class SettingID(Enum):
    VERBOSE_ENS = auto()
    VERBOSE_EIP712 = auto()
    NONCE = auto()
    DEBUG_DATA = auto()


def get_device_settings(firmware: Firmware) -> list[SettingID]:
    if firmware == Firmware.NANOS:
        return [
            SettingID.NONCE,
            SettingID.DEBUG_DATA,
        ]
    return [
        SettingID.VERBOSE_ENS,
        SettingID.VERBOSE_EIP712,
        SettingID.NONCE,
        SettingID.DEBUG_DATA,
    ]


def get_setting_position(firmware: Firmware, setting_idx: int, per_page: int) -> tuple[int, int]:
    if firmware == Firmware.STAX:
        screen_height = 672  # px
        header_height = 88  # px
        footer_height = 92  # px
        x_offset = 350  # px
    else:
        screen_height = 600  # px
        header_height = 92  # px
        footer_height = 97  # px
        x_offset = 420  # px
    index_in_page = setting_idx % per_page
    if index_in_page == 0:
        y_offset = header_height + 10
    elif per_page == 3:
        if setting_idx == 1:
            # 2nd setting over 3: middle of the screen
            y_offset = screen_height // 2
        else:
            # Last setting
            y_offset = screen_height - footer_height - 10
    else:
        # 2 per page, requesting the 2nd one; middle of screen is ok
        y_offset = screen_height // 2
    return x_offset, y_offset


def settings_toggle(firmware: Firmware, nav: Navigator, to_toggle: list[SettingID]):
    moves: list[Union[NavIns, NavInsID]] = []
    settings = get_device_settings(firmware)
    # Assume the app is on the home page
    if firmware.is_nano:
        moves += [NavInsID.RIGHT_CLICK] * 2
        moves += [NavInsID.BOTH_CLICK]
        for setting in settings:
            if setting in to_toggle:
                moves += [NavInsID.BOTH_CLICK]
            moves += [NavInsID.RIGHT_CLICK]
        moves += [NavInsID.BOTH_CLICK]  # Back
    else:
        moves += [NavInsID.USE_CASE_HOME_SETTINGS]
        settings_per_page = 3 if firmware == Firmware.STAX else 2
        for setting in settings:
            setting_idx = settings.index(setting)
            if (setting_idx > 0) and (setting_idx % settings_per_page) == 0:
                moves += [NavInsID.USE_CASE_SETTINGS_NEXT]
            if setting in to_toggle:
                moves += [NavIns(NavInsID.TOUCH, get_setting_position(firmware, setting_idx, settings_per_page))]
        moves += [NavInsID.USE_CASE_SETTINGS_MULTI_PAGE_EXIT]
    nav.navigate(moves, screen_change_before_first_instruction=False)
