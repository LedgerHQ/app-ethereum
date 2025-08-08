from enum import Enum, auto
from typing import Union
from ledgered.devices import Device, DeviceType

from ragger.navigator import Navigator, NavInsID, NavIns


class SettingID(Enum):
    TRANSACTION_CHECKS = auto()
    BLIND_SIGNING = auto()
    NONCE = auto()
    VERBOSE_EIP712 = auto()
    DEBUG_DATA = auto()
    EIP7702 = auto()
    DISPLAY_HASH = auto()


# Settings Positions per device. Returns the tuple (page, x, y)
SETTINGS_POSITIONS = {
    DeviceType.STAX: {
        SettingID.TRANSACTION_CHECKS: (0, 350, 130),
        SettingID.BLIND_SIGNING: (0, 350, 335),
        SettingID.NONCE: (1, 350, 130),
        SettingID.VERBOSE_EIP712: (1, 350, 270),
        SettingID.DEBUG_DATA: (1, 350, 445),
        SettingID.EIP7702: (2, 350, 130),
        SettingID.DISPLAY_HASH: (2, 350, 335),
    },
    DeviceType.FLEX: {
        SettingID.TRANSACTION_CHECKS: (0, 420, 130),
        SettingID.BLIND_SIGNING: (0, 420, 350),
        SettingID.NONCE: (1, 420, 130),
        SettingID.VERBOSE_EIP712: (1, 420, 270),
        SettingID.DEBUG_DATA: (2, 420, 140),
        SettingID.EIP7702: (2, 420, 270),
        SettingID.DISPLAY_HASH: (3, 420, 130),
    },
    DeviceType.APEX_P: {
        SettingID.TRANSACTION_CHECKS: (0, 260, 90),
        SettingID.BLIND_SIGNING: (0, 260, 235),
        SettingID.NONCE: (1, 260, 90),
        SettingID.VERBOSE_EIP712: (1, 260, 190),
        SettingID.DEBUG_DATA: (2, 260, 90),
        SettingID.EIP7702: (2, 260, 190),
        SettingID.DISPLAY_HASH: (3, 260, 90),
    },
    DeviceType.APEX_M: {
        SettingID.TRANSACTION_CHECKS: (0, 260, 90),
        SettingID.BLIND_SIGNING: (0, 260, 235),
        SettingID.NONCE: (1, 260, 90),
        SettingID.VERBOSE_EIP712: (1, 260, 190),
        SettingID.DEBUG_DATA: (2, 260, 90),
        SettingID.EIP7702: (2, 260, 190),
        SettingID.DISPLAY_HASH: (3, 260, 90),
    },
}


# The order of the settings is important, as it is used to navigate
def get_device_settings(device: Device) -> list[SettingID]:
    """Get the list of settings available on the device"""
    all_settings = []
    if not device.is_nano:
        all_settings.append(SettingID.TRANSACTION_CHECKS)
    all_settings += [
        SettingID.BLIND_SIGNING,
        SettingID.NONCE,
        SettingID.VERBOSE_EIP712,
        SettingID.DEBUG_DATA,
        SettingID.EIP7702,
        SettingID.DISPLAY_HASH,
    ]
    return all_settings


def get_settings_moves(device: Device,
                       to_toggle: list[SettingID]) -> list[Union[NavIns, NavInsID]]:
    """Get the navigation instructions to toggle the settings"""
    moves: list[Union[NavIns, NavInsID]] = []
    settings = get_device_settings(device)
    # Assume the app is on the 1st page of Settings
    if device.is_nano:
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
                page, x, y = SETTINGS_POSITIONS[device.type][setting]
                moves += [NavInsID.USE_CASE_SETTINGS_NEXT] * (page - current_page)
                moves += [NavIns(NavInsID.TOUCH, (x, y))]
                if setting == SettingID.TRANSACTION_CHECKS:
                    # Assume Opt-In is not done, Add a confirmation step
                    moves += [NavInsID.USE_CASE_CHOICE_CONFIRM]
                    # Dismiss the notification
                    moves += [NavInsID.TAPPABLE_CENTER_TAP]
                current_page = page
        moves += [NavInsID.USE_CASE_SETTINGS_MULTI_PAGE_EXIT]
    return moves


def settings_toggle(device: Device, navigator: Navigator, to_toggle: list[SettingID]):
    """Toggle the settings"""
    moves = get_settings_moves(device, to_toggle)
    navigator.navigate(moves, screen_change_before_first_instruction=False)
