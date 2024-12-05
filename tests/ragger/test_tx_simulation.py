from pathlib import Path
from web3 import Web3
import rlp
import pytest
from eth_utils import keccak

from ragger.error import ExceptionRAPDU
from ragger.backend import BackendInterface
from ragger.firmware import Firmware
from ragger.navigator import Navigator
from ragger.navigator.navigation_scenario import NavigateWithScenario

from test_sign import common as sign_tx

from client.client import EthAppClient, StatusWord
from client.settings import SettingID, settings_toggle



def test_tx_simulation_disabled(firmware: Firmware, backend: BackendInterface) -> None:
    """Test the TX Simulation APDU with the W3C setting disabled.
        It should return an error
    """

    if firmware.is_nano:
        pytest.skip("Not yet supported on Nano")

    app_client = EthAppClient(backend)

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

    # Toggle the W3C setting
    settings_toggle(firmware, navigator, [SettingID.W3C])

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
    # Compute the tx hash
    tx = Web3().eth.account.create().sign_transaction(tx_params).rawTransaction
    decoded = rlp.decode(tx)[:-3]  # remove already computed signature
    tx_hash = keccak(rlp.encode(decoded + [int(tx_params["chainId"]), bytes(), bytes()]))

    # Toggle the W3C setting
    settings_toggle(firmware, navigator, [SettingID.W3C])

    response = app_client.provide_tx_simulation(tx_hash, risk, category, message, url, chain_id, False)
    assert response.status == StatusWord.OK

    sign_tx(firmware,
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

    # Toggle the W3C setting
    settings_toggle(firmware, navigator, [SettingID.W3C])

    response = app_client.provide_tx_simulation(tx_hash, risk, category, message, url, chain_id, False)
    assert response.status == StatusWord.OK

    sign_tx(firmware,
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

    # Toggle the W3C setting
    settings_toggle(firmware, navigator, [SettingID.W3C])

    sign_tx(firmware,
            backend,
            navigator,
            scenario_navigator,
            default_screenshot_path,
            tx_params,
            test_name,
            with_simu=True)
