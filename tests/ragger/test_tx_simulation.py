from ragger.error import ExceptionRAPDU
from ragger.backend import BackendInterface
from ragger.firmware import Firmware
from ragger.navigator import Navigator

from client.client import EthAppClient, StatusWord
from client.settings import SettingID, settings_toggle


def test_tx_simulation_disabled(backend: BackendInterface) -> None:
    app_client = EthAppClient(backend)

    chain_id = 5
    risk = 0x1234
    category = 1
    message = "This is a test message"
    url = "https://www.ledger.com"
    try:
        app_client.provide_tx_simulation(risk, category, message, url, chain_id)
    except ExceptionRAPDU as err:
        assert err.status == StatusWord.NOT_IMPLEMENTED
    else:
        assert False  # An exception should have been raised


def test_tx_simulation(firmware: Firmware,
                       backend: BackendInterface,
                       navigator: Navigator) -> None:
    app_client = EthAppClient(backend)

    chain_id = 5
    risk = 0x1234
    category = 1
    message = "This is a test message"
    url = "https://www.ledger.com"

    # Toggle the W3C setting
    settings_toggle(firmware, navigator, [SettingID.W3C])

    response = app_client.provide_tx_simulation(risk, category, message, url, chain_id)
    assert response.status == StatusWord.OK
