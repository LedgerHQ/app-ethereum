import pytest
import os
import fnmatch
from typing import List
from ethereum_client.client import EthereumClient, SettingType
from eip712 import InputData
from pathlib import Path
from configparser import ConfigParser

bip32 = [
    0x8000002c,
    0x8000003c,
    0x80000000,
    0,
    0
]


def input_files() -> List[str]:
    files = []
    for file in os.scandir("%s/eip712/input_files" % (os.path.dirname(__file__))):
        if fnmatch.fnmatch(file, "*-data.json"):
            files.append(file.path)
    return sorted(files)

@pytest.fixture(params=input_files())
def input_file(request) -> str:
    return Path(request.param)

@pytest.fixture(params=[True, False])
def verbose(request) -> bool:
    return request.param

@pytest.fixture(params=[False, True])
def filtering(request) -> bool:
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


def test_eip712_new(app_client: EthereumClient, input_file: Path, verbose: bool, filtering: bool):
    print("=====> %s" % (input_file))
    if app_client._client.firmware.device != "nanos":
        test_path = "%s/%s" % (input_file.parent, "-".join(input_file.stem.split("-")[:-1]))
        conf_file = "%s.ini" % (test_path)
        filter_file = None

        if filtering:
            filter_file = "%s-filter.json" % (test_path)

        config = ConfigParser()
        config.read(conf_file)

        # sanity check
        assert "signature" in config.sections()
        assert "v" in config["signature"]
        assert "r" in config["signature"]
        assert "s" in config["signature"]

        if not filtering or Path(filter_file).is_file():
            if verbose:
                app_client.settings_set({
                    SettingType.VERBOSE_EIP712: True
                })

            assert InputData.process_file(app_client, input_file, filter_file) == True
            v, r, s = app_client.eip712_sign_new(bip32)
            #print("[signature]")
            #print("v = %s" % (v.hex()))
            #print("r = %s" % (r.hex()))
            #print("s = %s" % (s.hex()))

            assert v == bytes.fromhex(config["signature"]["v"])
            assert r == bytes.fromhex(config["signature"]["r"])
            assert s == bytes.fromhex(config["signature"]["s"])
        else:
            print("No filter file found, skipping...")
    else:
        print("Not supported by LNS, skipping...")
