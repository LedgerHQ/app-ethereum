import pytest

from ragger.backend import BackendInterface
from ragger.backend.speculos import SpeculosBackend

from client.client import EthAppClient, StatusWord


def test_perform_privacy_operation_public(backend: BackendInterface):

    if isinstance(backend, SpeculosBackend):
        pytest.skip("Not supported on speculos")

    app_client = EthAppClient(backend)

    response = app_client.perform_privacy_operation()
    assert response.status == StatusWord.OK
    assert len(response.data) == 32
    print(f"Data: {response.data.hex()}")


def test_perform_privacy_operation_secret(backend: BackendInterface):

    if isinstance(backend, SpeculosBackend):
        pytest.skip("Not supported on speculos")

    pubkey = "5901c19a086d1be4b907ec0325bffa758c3eb78192c3df4afa2afd2736a39963".encode("utf-8")

    app_client = EthAppClient(backend)

    response = app_client.perform_privacy_operation(pubkey=pubkey)
    assert response.status == StatusWord.OK
    assert len(response.data) == 32
    print(f"Data: {response.data.hex()}")
