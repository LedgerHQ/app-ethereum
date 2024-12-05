import pytest

from ragger.error import ExceptionRAPDU
from ragger.backend import BackendInterface

from client.client import EthAppClient, StatusWord

def test_provide_erc20_token(backend: BackendInterface):

    app_client = EthAppClient(backend)

    addr = bytes.fromhex("e41d2489571d322189246dafa5ebde1f4699f498")
    response = app_client.provide_token_metadata("ZRX", addr, 18, 1)
    assert response.status == StatusWord.OK


def test_provide_erc20_token_error(backend: BackendInterface):

    app_client = EthAppClient(backend)

    addr = bytes.fromhex("e41d2489571d322189246dafa5ebde1f4699f498")
    sign = bytes.fromhex("deadbeef")
    with pytest.raises(ExceptionRAPDU) as err:
        app_client.provide_token_metadata("ZRX", addr, 18, 1, sign)

    assert err.value.status == StatusWord.INVALID_DATA
