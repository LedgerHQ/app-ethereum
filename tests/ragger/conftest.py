import pytest
from pathlib import Path
from ragger import Firmware
from ragger.backend import SpeculosBackend, LedgerCommBackend, LedgerWalletBackend, BackendInterface
from ethereum_client.client import EthereumClient

ELFS_DIR = (Path(__file__).parent.parent / "elfs").resolve()
FWS = [
    Firmware("nanos", "2.1"),
    Firmware("nanox", "2.0.2"),
    Firmware("nanosp", "1.0.3")
]

# adding a pytest CLI option "--backend"
def pytest_addoption(parser):
    print(help(parser.addoption))
    parser.addoption("--backend", action="store", default="speculos")

# accessing the value of the "--backend" option as a fixture
@pytest.fixture(scope="session")
def backend_name(pytestconfig) -> str:
    return pytestconfig.getoption("backend")

# Providing the firmware as a fixture
@pytest.fixture(params=FWS)
def firmware(request) -> Firmware:
    return request.param

def get_elf_path(firmware: Firmware) -> Path:
    assert ELFS_DIR.is_dir(), f"{ELFS_DIR} is not a directory"
    app = ELFS_DIR / ("app-%s.elf" % firmware.device)
    assert app.is_file(), f"{app} must exist"
    return app

# Depending on the "--backend" option value, a different backend is
# instantiated, and the tests will either run on Speculos or on a physical
# device depending on the backend
def create_backend(backend: str, firmware: Firmware) -> BackendInterface:
    if backend.lower() == "ledgercomm":
        return LedgerCommBackend(firmware, interface="hid")
    elif backend.lower() == "ledgerwallet":
        return LedgerWalletBackend(firmware)
    elif backend.lower() == "speculos":
        return SpeculosBackend(get_elf_path(firmware), firmware)
    else:
        raise ValueError(f"Backend '{backend}' is unknown. Valid backends are: {BACKENDS}")

# This fixture will create and return the backend client
@pytest.fixture
def backend_client(backend_name: str, firmware: Firmware) -> BackendInterface:
    with create_backend(backend_name, firmware) as b:
        yield b

# This final fixture will return the properly configured app client, to be used in tests
@pytest.fixture
def app_client(backend_client: BackendInterface) -> EthereumClient:
    return EthereumClient(backend_client)
