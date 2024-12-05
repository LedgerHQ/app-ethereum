from ragger.backend import BackendInterface

from client.client import EthAppClient, StatusWord


def test_tx_simulation(backend: BackendInterface) -> None:
    app_client = EthAppClient(backend)

    chain_id = 5
    risk = 0x1234
    category = 1
    message = "This is a test message"
    url = "https://www.ledger.com"
    response = app_client.provide_tx_simulation(risk, category, message, url, chain_id)
    assert response.status == StatusWord.OK
