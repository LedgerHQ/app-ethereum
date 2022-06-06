from pathlib import Path

import pytest

from speculos.client import SpeculosClient

from ethereum_client.ethereum_cmd import EthereumCommand


SCRIPT_DIR = Path(__file__).absolute().parent
API_URL = "http://127.0.0.1:5000"

def pytest_addoption(parser):
    # nanos or nanox
    parser.addoption("--model", action="store", default="nanos")
    # qt: default, requires a X server
    # headless: nothing is displayed
    parser.addoption("--display", action="store", default="qt")

@pytest.fixture()
def client(pytestconfig):
    file_path = SCRIPT_DIR.parent.parent / "bin" / "app.elf"
    model = pytestconfig.getoption("model")
    version = '2.1' # latest version of nanos_sdk

    if model == "nanox":
        version = '2.0.2' # latest version of nanox_sdk

    args = ['--log-level', 'speculos:DEBUG','--model', model, '--display', pytestconfig.getoption("display"), '--sdk', version]
    with SpeculosClient(app=str(file_path), args=args) as client:
        yield client


@pytest.fixture()
def cmd(client, pytestconfig):
    yield EthereumCommand(
        client=client,
        debug=True,
        model=pytestconfig.getoption("model"),
    )
