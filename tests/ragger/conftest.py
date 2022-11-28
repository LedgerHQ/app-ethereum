import pytest
from pathlib import Path
from ragger.firmware import Firmware
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
    parser.addoption("--display", action="store_true", default=False)

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

@pytest.fixture
def arg_display(pytestconfig) -> bool:
    return pytestconfig.getoption("display")

# Providing the firmware as a fixture
@pytest.fixture
def firmware(arg_model: str) -> Firmware:
    for fw in FWS:
        if fw.device == arg_model:
            return fw
    raise ValueError("Unknown device model \"%s\"" % (arg_model))

def get_speculos_args(arg_path: str, display: bool, fw: Firmware):
    extra_args = list()

    if display:
        extra_args += ["--display", "qt"]
    elf_dir = Path(arg_path).resolve()
    assert elf_dir.is_dir(), ("%s is not a directory" % (arg_path))
    app = elf_dir / ("app-%s.elf" % (fw.device))
    assert app.is_file(), ("Firmware %s does not exist !" % (app))
    return (app, {"args": extra_args})

# Depending on the "--backend" option value, a different backend is
# instantiated, and the tests will either run on Speculos or on a physical
# device depending on the backend
def create_backend(backend: str, arg_path: str, display: bool, fw: Firmware) -> BackendInterface:
    if backend.lower() == "ledgercomm":
        return LedgerCommBackend(fw, interface="hid")
    elif backend.lower() == "ledgerwallet":
        return LedgerWalletBackend(fw)
    elif backend.lower() == "speculos":
        app, args = get_speculos_args(arg_path, display, fw)
        return SpeculosBackend(app, fw, **args)
    else:
        raise ValueError(f"Backend '{backend}' is unknown. Valid backends are: {BACKENDS}")

# This fixture will create and return the backend client
@pytest.fixture
def backend_client(arg_backend: str, arg_path: str, arg_display: bool, firmware: Firmware) -> BackendInterface:
    with create_backend(arg_backend, arg_path, arg_display, firmware) as b:
        yield b

# This final fixture will return the properly configured app client, to be used in tests
@pytest.fixture
def app_client(backend_client: BackendInterface) -> EthereumClient:
    return EthereumClient(backend_client)
