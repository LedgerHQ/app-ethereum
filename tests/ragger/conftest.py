import pytest
from pathlib import Path
from ragger import Firmware
from ragger.backend import SpeculosBackend, LedgerCommBackend, LedgerWalletBackend, BackendInterface
from ethereum_client.client import EthereumClient

FWS = [
    Firmware("nanos", "2.1"),
    Firmware("nanox", "2.0.2"),
    Firmware("nanosp", "1.0.3")
]

def pytest_addoption(parser):
    parser.addoption("--backend", action="store", default="speculos")
    parser.addoption("--path", action="store", default="./elfs")
    parser.addoption("--model", action="store", required=True)

# accessing the value of the "--backend" option as a fixture
@pytest.fixture
def arg_backend(pytestconfig) -> str:
    return pytestconfig.getoption("backend")

@pytest.fixture
def arg_path(pytestconfig) -> str:
    return pytestconfig.getoption("path")

@pytest.fixture
def arg_model(pytestconfig) -> str:
    return pytestconfig.getoption("model")

# Providing the firmware as a fixture
@pytest.fixture
def firmware(arg_model: str) -> Firmware:
    for fw in FWS:
        if fw.device == arg_model:
            return fw
    raise ValueError("Unknown device model \"%s\"" % (arg_model))

def get_elf_path(arg_path: str, firmware: Firmware) -> Path:
    elf_dir = Path(arg_path).resolve()
    assert elf_dir.is_dir(), ("%s is not a directory" % (arg_path))
    app = elf_dir / ("app-%s.elf" % firmware.device)
    assert app.is_file(), ("Firmware %s does not exist !" % (app))
    return app

# Depending on the "--backend" option value, a different backend is
# instantiated, and the tests will either run on Speculos or on a physical
# device depending on the backend
def create_backend(backend: str, arg_path: str, firmware: Firmware) -> BackendInterface:
    if backend.lower() == "ledgercomm":
        return LedgerCommBackend(firmware, interface="hid")
    elif backend.lower() == "ledgerwallet":
        return LedgerWalletBackend(firmware)
    elif backend.lower() == "speculos":
        return SpeculosBackend(get_elf_path(arg_path, firmware), firmware)
    else:
        raise ValueError(f"Backend '{backend}' is unknown. Valid backends are: {BACKENDS}")

# This fixture will create and return the backend client
@pytest.fixture
def backend_client(arg_backend: str, arg_path: str, firmware: Firmware) -> BackendInterface:
    with create_backend(arg_backend, arg_path, firmware) as b:
        yield b

# This final fixture will return the properly configured app client, to be used in tests
@pytest.fixture
def app_client(backend_client: BackendInterface) -> EthereumClient:
    return EthereumClient(backend_client)
