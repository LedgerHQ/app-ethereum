import os
import fnmatch
from ethereum_client import EthereumClient
from eip712 import InputData

bip32 = [
    0x8000002c,
    0x8000003c,
    0x80000000,
    0,
    0
]


def test_eip712_legacy(app_client: EthereumClient):
    v, r, s = app_client.eip712_sign_legacy(
        bip32,
        bytes.fromhex('6137beb405d9ff777172aa879e33edb34a1460e701802746c5ef96e741710e59'),
        bytes.fromhex('eb4221181ff3f1a83ea7313993ca9218496e424604ba9492bb4052c03d5c3df8')
    )

    assert v == bytes.fromhex("1c")
    assert r == bytes.fromhex("ea66f747173762715751c889fea8722acac3fc35db2c226d37a2e58815398f64")
    assert s == bytes.fromhex("52d8ba9153de9255da220ffd36762c0b027701a3b5110f0a765f94b16a9dfb55")


def test_eip712_new(app_client: EthereumClient):
    if app_client._client.firmware.device == "nanos": # not supported
        return

    # Loop through JSON files
    for file in os.scandir("./eip712/input_files"):
        if fnmatch.fnmatch(file, "*-test.json"):
            print(file.path)
            InputData.process_file(app_client, file.path, False)
            app_client.eip712_sign_new(bip32)
    assert 1 == 1
