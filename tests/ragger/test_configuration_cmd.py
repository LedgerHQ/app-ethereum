from pathlib import Path
from typing import List
import pytest

from ragger.backend import BackendInterface
from ragger.firmware import Firmware
from ragger.navigator import Navigator
from ragger.utils.misc import get_current_app_name_and_version

from client.settings import SettingID, get_settings_moves


@pytest.mark.parametrize(
    "name, setting",
    [
        ("web3_check", [SettingID.WEB3_CHECK]),
        ("blind_sign", [SettingID.BLIND_SIGNING]),
        ("trusted_name", [SettingID.VERBOSE_ENS]),
        ("nonce", [SettingID.NONCE]),
        ("eip712_token", [SettingID.VERBOSE_EIP712]),
        ("debug_token", [SettingID.DEBUG_DATA]),
        ("multiple1", [SettingID.BLIND_SIGNING, SettingID.DEBUG_DATA]),
        ("multiple2", [SettingID.BLIND_SIGNING, SettingID.VERBOSE_EIP712]),
        ("multiple3", [SettingID.BLIND_SIGNING, SettingID.WEB3_CHECK]),
    ]
)
def test_settings(firmware: Firmware,
                  navigator: Navigator,
                  test_name: str,
                  default_screenshot_path: Path,
                  name: str,
                  setting: List[SettingID]):
    """Check the settings"""

    if firmware.is_nano:
        pytest.skip("Skipping on Nano")

    moves = get_settings_moves(firmware, setting)
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
