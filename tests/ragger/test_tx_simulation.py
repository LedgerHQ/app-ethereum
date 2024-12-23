from pathlib import Path
from web3 import Web3
import pytest

from ragger.error import ExceptionRAPDU
from ragger.backend import BackendInterface
from ragger.firmware import Firmware
from ragger.navigator import Navigator
from ragger.navigator.navigation_scenario import NavigateWithScenario

from test_sign import BIP32_PATH
from test_sign import test_sign_simple as sign_tx
from test_sign import common as sign_tx_common
from test_blind_sign import test_blind_sign as blind_sign
from test_nft import common_test_nft, collecs_721, actions_721, ERC721_PLUGIN

from client.client import EthAppClient, StatusWord
from client.settings import SettingID, settings_toggle


def common_setting_handling(firmware: Firmware,
                            navigator: Navigator,
                            app_client: EthAppClient,
                            expected: bool) -> None:
    """Common setting handling for the tests"""

    response = app_client.get_app_configuration()
    assert response.status == StatusWord.OK
    flags = response.data[0]
    if bool(flags & 0x010) != expected:
        # Toggle the W3C setting
        settings_toggle(firmware, navigator, [SettingID.W3C])


def test_tx_simulation_disabled(firmware: Firmware,
                                backend: BackendInterface,
                                navigator: Navigator) -> None:
    """Test the TX Simulation APDU with the W3C setting disabled.
        It should return an error
    """

    if firmware.is_nano:
        pytest.skip("Not yet supported on Nano")

    app_client = EthAppClient(backend)

    common_setting_handling(firmware, navigator, app_client, False)

    tx_hash = bytes.fromhex("deadbeaf"*8)
    chain_id = 5
    risk = 0x1234
    category = 1
    message = "This is a test message"
    url = "https://www.ledger.com"
    try:
        app_client.provide_tx_simulation(tx_hash, risk, category, message, url, chain_id, True)
    except ExceptionRAPDU as err:
        assert err.status == StatusWord.NOT_IMPLEMENTED
    else:
        assert False  # An exception should have been raised


def test_tx_simulation_enabled(firmware: Firmware,
                               backend: BackendInterface,
                               navigator: Navigator) -> None:
    """Test the TX Simulation APDU with the W3C setting enabled"""

    if firmware.is_nano:
        pytest.skip("Not yet supported on Nano")

    app_client = EthAppClient(backend)

    tx_hash = bytes.fromhex("deadbeaf"*8)
    chain_id = 5
    risk = 0x1234
    category = 1
    message = "This is a test message"
    url = "https://www.ledger.com"

    common_setting_handling(firmware, navigator, app_client, True)

    response = app_client.provide_tx_simulation(tx_hash, risk, category, message, url, chain_id, True)
    assert response.status == StatusWord.OK


def test_tx_simulation_with_good_tx(firmware: Firmware,
                                    backend: BackendInterface,
                                    navigator: Navigator,
                                    scenario_navigator: NavigateWithScenario,
                                    test_name: str,
                                    default_screenshot_path: Path) -> None:
    """Test the TX Simulation APDU with a simple transaction"""

    if firmware.is_nano:
        pytest.skip("Not yet supported on Nano")

    app_client = EthAppClient(backend)

    common_setting_handling(firmware, navigator, app_client, True)

    sign_tx(firmware,
            backend,
            navigator,
            scenario_navigator,
            test_name,
            default_screenshot_path,
            BIP32_PATH,
            True,
            True)


def test_tx_simulation_with_bad_tx(firmware: Firmware,
                                   backend: BackendInterface,
                                   navigator: Navigator,
                                   scenario_navigator: NavigateWithScenario,
                                   test_name: str,
                                   default_screenshot_path: Path) -> None:
    """Test the TX Simulation APDU with a simple transaction
        but given a wrong tx hash in TX Simu APDU
    """

    if firmware.is_nano:
        pytest.skip("Not yet supported on Nano")

    app_client = EthAppClient(backend)

    chain_id = 5
    risk = 0x8234
    category = 1
    message = "This is a test message"
    url = "https://www.ledger.com"

    tx_params: dict = {
        "nonce": 21,
        "gasPrice": Web3.to_wei(13, 'gwei'),
        "gas": 21000,
        "to": bytes.fromhex("5a321744667052affa8386ed49e00ef223cbffc3"),
        "value": Web3.to_wei(1.22, "ether"),
        "chainId": chain_id
    }
    # Wrong tx hash
    tx_hash = bytes.fromhex("deadbeaf"*8)

    common_setting_handling(firmware, navigator, app_client, True)

    response = app_client.provide_tx_simulation(tx_hash, risk, category, message, url, chain_id, False)
    assert response.status == StatusWord.OK

    sign_tx_common(firmware,
                   backend,
                   navigator,
                   scenario_navigator,
                   default_screenshot_path,
                   tx_params,
                   test_name,
                   with_simu_nav=True)


def test_tx_simulation_sign_no_simu(firmware: Firmware,
                                    backend: BackendInterface,
                                    navigator: Navigator,
                                    scenario_navigator: NavigateWithScenario,
                                    test_name: str,
                                    default_screenshot_path: Path) -> None:
    """Test the TX Transaction APDU without TX Simulation APDU
        Here, the W3C setting is enabled"""

    if firmware.is_nano:
        pytest.skip("Not yet supported on Nano")

    tx_params: dict = {
        "nonce": 21,
        "gasPrice": Web3.to_wei(13, 'gwei'),
        "gas": 21000,
        "to": bytes.fromhex("5a321744667052affa8386ed49e00ef223cbffc3"),
        "value": Web3.to_wei(1.22, "ether"),
        "chainId": 5
    }

    app_client = EthAppClient(backend)
    common_setting_handling(firmware, navigator, app_client, True)

    sign_tx_common(firmware,
                   backend,
                   navigator,
                   scenario_navigator,
                   default_screenshot_path,
                   tx_params,
                   test_name,
                   with_simu_nav=True)


def test_tx_simulation_nft(firmware: Firmware,
                           backend: BackendInterface,
                           navigator: Navigator,
                           scenario_navigator: NavigateWithScenario,
                           default_screenshot_path: Path,
                           test_name: str) -> None:
    """Test the TX Simulation APDU with a Plugin & NFT transaction"""

    if firmware.is_nano:
        pytest.skip("Not yet supported on Nano")

    app_client = EthAppClient(backend)

    common_setting_handling(firmware, navigator, app_client, True)

    common_test_nft(firmware,
                    backend,
                    navigator,
                    scenario_navigator,
                    default_screenshot_path,
                    collecs_721[0],
                    actions_721[0],
                    False,
                    ERC721_PLUGIN,
                    test_name,
                    True)


def test_tx_simulation_blind_sign(firmware: Firmware,
                                  backend: BackendInterface,
                                  navigator: Navigator,
                                  scenario_navigator: NavigateWithScenario,
                                  test_name: str,
                                  default_screenshot_path: Path) -> None:
    """Test the TX Simulation APDU with a simple transaction"""

    if firmware.is_nano:
        pytest.skip("Not yet supported on Nano")

    app_client = EthAppClient(backend)

    common_setting_handling(firmware, navigator, app_client, True)

    blind_sign(firmware,
               backend,
               navigator,
               scenario_navigator,
               default_screenshot_path,
               test_name,
               False,
               0.0,
               True)
