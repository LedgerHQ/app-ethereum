from enum import Enum, auto
from typing import Union
from ragger.firmware import Firmware
from ragger.navigator import Navigator, NavInsID, NavIns


class SettingID(Enum):
    WEB3_CHECK = auto()
    BLIND_SIGNING = auto()
    VERBOSE_ENS = auto()
    NONCE = auto()
    VERBOSE_EIP712 = auto()
    DEBUG_DATA = auto()
    EIP7702 = auto()


def get_device_settings(firmware: Firmware) -> list[SettingID]:
    """Get the list of settings available on the device"""
    if firmware.is_nano:
        return [
            SettingID.BLIND_SIGNING,
            SettingID.VERBOSE_ENS,
            SettingID.NONCE,
            SettingID.VERBOSE_EIP712,
            SettingID.DEBUG_DATA,
            SettingID.EIP7702,
        ]
    return [
        SettingID.WEB3_CHECK,
        SettingID.BLIND_SIGNING,
        SettingID.VERBOSE_ENS,
        SettingID.NONCE,
        SettingID.VERBOSE_EIP712,
        SettingID.DEBUG_DATA,
        SettingID.EIP7702,
    ]


def get_setting_position(firmware: Firmware, setting: SettingID) -> tuple[int, int, int]:
    """Get the position of the setting on the device"""
    if firmware == Firmware.STAX:
        x = 350
    else:
        x = 420
    if setting == SettingID.WEB3_CHECK:
        if firmware == Firmware.STAX:
            page, y = 0, 130
        else:
            page, y = 0, 140
    elif setting == SettingID.BLIND_SIGNING:
        if firmware == Firmware.STAX:
            page, y = 0, 335
        else:
            page, y = 0, 350
    elif setting == SettingID.VERBOSE_ENS:
        if firmware == Firmware.STAX:
            page, y = 1, 130
        else:
            page, y = 1, 140
    elif setting == SettingID.NONCE:
        if firmware == Firmware.STAX:
            page, y = 1, 300
        else:
            page, y = 1, 315
    elif setting == SettingID.VERBOSE_EIP712:
        if firmware == Firmware.STAX:
            page, y = 1, 445
        else:
            page, y = 2, 140
    elif setting == SettingID.DEBUG_DATA:
        if firmware == Firmware.STAX:
            page, y = 2, 130
        else:
            page, y = 2, 315
    elif setting == SettingID.EIP7702:
        if firmware == Firmware.STAX:
            page, y = 2, 300
        else:
            page, y = 3, 140
    else:
        raise ValueError(f"Unknown setting: {setting}")
    return page, x, y


def get_settings_moves(firmware: Firmware,
                       to_toggle: list[SettingID]) -> list[Union[NavIns, NavInsID]]:
    """Get the navigation instructions to toggle the settings"""
    moves: list[Union[NavIns, NavInsID]] = []
    settings = get_device_settings(firmware)
    # Assume the app is on the 1st page of Settings
    if firmware.is_nano:
        moves += [NavInsID.RIGHT_CLICK, NavInsID.BOTH_CLICK]
        for setting in settings:
            if setting in to_toggle:
                moves += [NavInsID.BOTH_CLICK]
            moves += [NavInsID.RIGHT_CLICK]
        moves += [NavInsID.BOTH_CLICK]  # Back
    else:
        current_page = 0
        moves += [NavInsID.USE_CASE_HOME_SETTINGS]
        for setting in settings:
            if setting in to_toggle:
                page, x, y = get_setting_position(firmware, setting)
                moves += [NavInsID.USE_CASE_SETTINGS_NEXT] * (page - current_page)
                moves += [NavIns(NavInsID.TOUCH, (x, y))]
                if setting == SettingID.WEB3_CHECK:
                    # Assume Opt-In is not done, Add a confirmation step
                    moves += [NavInsID.USE_CASE_CHOICE_CONFIRM]
                    # Dismiss the notification
                    moves += [NavInsID.TAPPABLE_CENTER_TAP]
                current_page = page
        moves += [NavInsID.USE_CASE_SETTINGS_MULTI_PAGE_EXIT]
    return moves


def settings_toggle(firmware: Firmware, navigator: Navigator, to_toggle: list[SettingID]):
    """Toggle the settings"""
    moves = get_settings_moves(firmware, to_toggle)
    navigator.navigate(moves, screen_change_before_first_instruction=False)
