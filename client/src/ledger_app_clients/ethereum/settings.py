from enum import Enum, auto
from ragger.firmware import Firmware
from ragger.navigator import Navigator, NavInsID, NavIns
from typing import Union


class SettingID(Enum):
    BLIND_SIGNING = auto()
    VERBOSE_ENS = auto()
    NONCE = auto()
    VERBOSE_EIP712 = auto()
    DEBUG_DATA = auto()


def get_device_settings(firmware: Firmware) -> list[SettingID]:
    if firmware == Firmware.NANOS:
        return [
            SettingID.BLIND_SIGNING,
            SettingID.NONCE,
            SettingID.DEBUG_DATA,
        ]
    return [
        SettingID.BLIND_SIGNING,
        SettingID.VERBOSE_ENS,
        SettingID.NONCE,
        SettingID.VERBOSE_EIP712,
        SettingID.DEBUG_DATA,
    ]


def get_setting_position(firmware: Firmware, setting_idx: int, per_page: int) -> tuple[int, int]:
    if firmware == Firmware.STAX:
        screen_height = 672  # px
        screen_width = 400  # px
        header_height = 88  # px
        footer_height = 92  # px
    else:
        screen_height = 600  # px
        screen_width = 480  # px
        header_height = 96  # px
        footer_height = 96  # px

    index_in_page = setting_idx % per_page
    usable_height = screen_height - (header_height + footer_height)
    setting_height = usable_height // per_page
    offset = (setting_height * index_in_page) + (setting_height // 2)
    return screen_width // 2, header_height + offset


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
