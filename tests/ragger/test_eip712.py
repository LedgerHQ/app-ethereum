import pytest
import os
import fnmatch
from typing import List
from ethereum_client import EthereumClient
from eip712 import InputData

bip32 = [
    0x8000002c,
    0x8000003c,
    0x80000000,
    0,
    0
]


def input_files() -> List[str]:
    files = []
    for file in os.scandir("./eip712/input_files"):
        if fnmatch.fnmatch(file, "*-test.json"):
            files.append(file.path)
    return sorted(files)

@pytest.fixture(params=input_files())
def input_file(request) -> str:
    return request.param

@pytest.fixture(params=[True, False])
def verbose(request) -> bool:
    return request.param


def test_eip712_legacy(app_client: EthereumClient):
    v, r, s = app_client.eip712_sign_legacy(
        bip32,
        bytes.fromhex('6137beb405d9ff777172aa879e33edb34a1460e701802746c5ef96e741710e59'),
        bytes.fromhex('eb4221181ff3f1a83ea7313993ca9218496e424604ba9492bb4052c03d5c3df8')
    )

    assert v == bytes.fromhex("1c")
    assert r == bytes.fromhex("ea66f747173762715751c889fea8722acac3fc35db2c226d37a2e58815398f64")
    assert s == bytes.fromhex("52d8ba9153de9255da220ffd36762c0b027701a3b5110f0a765f94b16a9dfb55")


def test_eip712_new(app_client: EthereumClient, input_file, verbose):
    if app_client._client.firmware.device != "nanos": # not supported
        print("=====> %s" % (input_file))

        if verbose:
            app_client.setting_toggle_verbose_eip712()
        InputData.process_file(app_client, input_file, False)
        v, r, s = app_client.eip712_sign_new(bip32)
    assert 1 == 1 # TODO: Replace by the actual v,r,s asserts
