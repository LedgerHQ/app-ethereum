from pathlib import Path
from typing import List
import pytest

from ledgered.devices import Device

from ragger.backend import BackendInterface
from ragger.navigator import Navigator
from ragger.utils.misc import get_current_app_name_and_version

from client.settings import SettingID, get_settings_moves


@pytest.mark.parametrize(
    "name, setting",
    [
        ("tx_checks", [SettingID.TRANSACTION_CHECKS]),
        ("blind_sign", [SettingID.BLIND_SIGNING]),
        ("nonce", [SettingID.NONCE]),
        ("eip712_token", [SettingID.VERBOSE_EIP712]),
        ("debug_token", [SettingID.DEBUG_DATA]),
        ("hash", [SettingID.DISPLAY_HASH]),
        ("multiple1", [SettingID.BLIND_SIGNING, SettingID.DEBUG_DATA]),
        ("multiple2", [SettingID.BLIND_SIGNING, SettingID.VERBOSE_EIP712]),
        ("multiple3", [SettingID.BLIND_SIGNING, SettingID.TRANSACTION_CHECKS]),
    ]
)
def test_settings(device: Device,
                  navigator: Navigator,
                  test_name: str,
                  default_screenshot_path: Path,
                  name: str,
                  setting: List[SettingID]):
    """Check the settings"""

    if device.is_nano and SettingID.TRANSACTION_CHECKS in setting:
        pytest.skip("Skipping TX_CHECK on Nano")

    moves = get_settings_moves(device, setting)
    navigator.navigate_and_compare(default_screenshot_path,
                                   f"{test_name}/{name}",
                                   moves, screen_change_before_first_instruction=False)


def test_check_version(backend: BackendInterface, app_version: tuple[int, int, int]):
    """Check version and name"""

    # Send the APDU
    app_name, version = get_current_app_name_and_version(backend)
    print(f" Name: {app_name}")
    print(f" Version: {version}")
    print(f" app_version: {app_version}")
    vers_str = ".".join(map(str, app_version))
    assert version.split("-")[0] == vers_str
