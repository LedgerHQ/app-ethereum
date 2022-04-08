from collections import namedtuple
from pathlib import Path

import pytest

from speculos.client import SpeculosClient

from boilerplate_client.boilerplate_cmd import BoilerplateCommand


SCRIPT_DIR = Path(__file__).absolute().parent
API_URL = "http://127.0.0.1:5000"

@pytest.fixture(scope="session")
def client():
    file_path = SCRIPT_DIR.parent.parent / "bin" / "app.elf"
    args = ['--model', 'nanos', '--display', 'qt', '--sdk', '2.1']
    with SpeculosClient(app=str(file_path), args=args) as client:
        yield client


@pytest.fixture(scope="session")
def cmd(client):
    yield BoilerplateCommand(
        client=client,
        debug=True
    )
