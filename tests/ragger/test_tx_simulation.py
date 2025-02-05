from pathlib import Path
from web3 import Web3
import pytest

from ragger.error import ExceptionRAPDU
from ragger.backend import BackendInterface
from ragger.firmware import Firmware
from ragger.navigator import Navigator
from ragger.navigator.navigation_scenario import NavigateWithScenario

from test_sign import common as sign_tx_common
from test_blind_sign import test_blind_sign as blind_sign
from test_nft import common_test_nft, collecs_721, actions_721, ERC721_PLUGIN
from test_eip191 import common as sign_eip191
from test_eip712 import test_eip712_filtering_empty_array as sign_eip712

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
        app_client.provide_tx_simulation(tx_hash, risk, category, message, url, chain_id)
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

    response = app_client.provide_tx_simulation(tx_hash, risk, category, message, url, chain_id)
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

    chain_id = 5
    risk = 0xB234
    category = 3
    message = "This is a THREAT message"
    url = "https://www.ledger.com"

    tx_params: dict = {
        "nonce": 21,
        "gasPrice": Web3.to_wei(13, 'gwei'),
        "gas": 21000,
        "to": bytes.fromhex("5a321744667052affa8386ed49e00ef223cbffc3"),
        "value": Web3.to_wei(1.22, "ether"),
        "chainId": chain_id
    }
    _, tx_hash = app_client.serialize_tx(tx_params)

    common_setting_handling(firmware, navigator, app_client, True)

    response = app_client.provide_tx_simulation(tx_hash, risk, category, message, url, tx_params["chainId"])
    assert response.status == StatusWord.OK

    sign_tx_common(firmware,
                   backend,
                   navigator,
                   scenario_navigator,
                   default_screenshot_path,
                   tx_params,
                   test_name,
                   with_simu=True)


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

    response = app_client.provide_tx_simulation(tx_hash, risk, category, message, url, chain_id)
    assert response.status == StatusWord.OK

    sign_tx_common(firmware,
                   backend,
                   navigator,
                   scenario_navigator,
                   default_screenshot_path,
                   tx_params,
                   test_name,
                   with_simu=True)


def test_tx_simulation_sign_no_simu(firmware: Firmware,
                                    backend: BackendInterface,
                                    navigator: Navigator,
                                    scenario_navigator: NavigateWithScenario,
                                    test_name: str,
                                    default_screenshot_path: Path) -> None:
    """Test the TX Transaction APDU without TX Simulation APDU
        but with the W3C setting enabled"""

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
                   with_simu=True)


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

    simu_params = {
        "risk": 0x8234,
        "category": 2,
        "message": "This is a WARNING message",
        "url": "https://www.ledger.com"
    }

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
                    simu_params)


def test_tx_simulation_blind_sign(firmware: Firmware,
                                  backend: BackendInterface,
                                  navigator: Navigator,
                                  scenario_navigator: NavigateWithScenario,
                                  test_name: str,
                                  default_screenshot_path: Path) -> None:
    """Test the TX Simulation APDU with a Blind Sign transaction"""

    if firmware.is_nano:
        pytest.skip("Not yet supported on Nano")

    app_client = EthAppClient(backend)

    common_setting_handling(firmware, navigator, app_client, True)

    simu_params = {
        "risk": 0x8234,
        "category": 2,
        "message": "This is a BLIND+WARNING message",
        "url": "https://www.ledger.com"
    }

    blind_sign(firmware,
               backend,
               navigator,
               scenario_navigator,
               default_screenshot_path,
               test_name,
               False,
               0.0,
               simu_params)


def test_tx_simulation_eip191(firmware: Firmware,
                              backend: BackendInterface,
                              navigator: Navigator,
                              scenario_navigator: NavigateWithScenario,
                              test_name: str,
                              default_screenshot_path: Path) -> None:
    """Test the TX Simulation APDU with a Message Streaming based on EIP191"""

    if firmware.is_nano:
        pytest.skip("Not yet supported on Nano")

    app_client = EthAppClient(backend)

    common_setting_handling(firmware, navigator, app_client, True)

    simu_params = {
        "chain_id": 0,
        "risk": 0xB234,
        "category": 5,
        "message": "This is a THREAT message",
        "url": "https://www.ledger.com"
    }
    # TX message
    msg = "Example `personal_sign` message with TX Simulation"

    sign_eip191(backend,
                navigator,
                scenario_navigator,
                default_screenshot_path,
                test_name,
                msg,
                simu_params)


def test_tx_simulation_eip712(firmware: Firmware,
                              backend: BackendInterface,
                              navigator: Navigator,
                              test_name: str,
                              default_screenshot_path: Path) -> None:
    """Test the TX Simulation APDU with a Message Streaming based on EIP712"""

    if firmware.is_nano:
        pytest.skip("Not yet supported on Nano")

    app_client = EthAppClient(backend)

    common_setting_handling(firmware, navigator, app_client, True)

    simu_params = {
        "chain_id": 0,
        "risk": 0xB234,
        "category": 5,
        "message": "This is a THREAT message",
        "url": "https://www.ledger.com"
    }

    sign_eip712(firmware,
                backend,
                navigator,
                default_screenshot_path,
                test_name,
                False,
                simu_params)
