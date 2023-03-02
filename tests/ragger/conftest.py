import pytest
from ragger.conftest import configuration
from ragger.backend import BackendInterface
from ethereum_client.client import EthereumClient

# This final fixture will return the properly configured app client, to be used in tests
@pytest.fixture
def app_client(backend: BackendInterface) -> EthereumClient:
    return EthereumClient(backend)

# Pull all features from the base ragger conftest using the overridden configuration
pytest_plugins = ("ragger.conftest.base_conftest", )
