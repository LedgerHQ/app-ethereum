
from pickle import TRUE
from typing import Tuple


def test_get_public_key(cmd):
    # ETHER COIN
    uncompressed_addr_len, eth_addr, chain_code = cmd.get_public_key(
        bip32_path="44'/60'/1'/0/0",
        display=False
    )  # type: bytes, bytes, bytes

    print("HERE", uncompressed_addr_len)

    assert len(uncompressed_addr_len) == 65
    assert len(eth_addr) == 40
    assert len(chain_code) == 32

    assert uncompressed_addr_len == b'\x04\xea\x02&\x91\xc7\x87\x00\xd2\xc3\xa0\xc7E\xbe\xa4\xf2\xb8\xe5\xe3\x13\x97j\x10B\xf6\xa1Vc\\\xb2\x05\xda\x1a\xcb\xfe\x04*\nZ\x89eyn6"E\x89\x0eT\xbd-\xbex\xec\x1e\x18df\xf2\xe9\xd0\xf5\xd5\xd8\xdf'
    assert eth_addr == b'463e4e114AA57F54f2Fd2C3ec03572C6f75d84C2'
    assert chain_code == b'\xaf\x89\xcd)\xea${8I\xec\xc80\xc2\xc8\x94\\e1\xd6P\x87\x07?\x9f\xd09\x00\xa0\xea\xa7\x96\xc8'

    # DAI COIN
    uncompressed_addr_len, eth_addr, chain_code = cmd.get_public_key(
        bip32_path="44'/700'/1'/0/0",
        display=False
    )  # type: bytes, bytes, bytes

    print("HERE2", uncompressed_addr_len)

    assert len(uncompressed_addr_len) == 65
    assert len(eth_addr) == 40
    assert len(chain_code) == 32

    assert uncompressed_addr_len == b'\x04V\x8a\x15\xdc\xed\xc8[\x16\x17\x8d\xaf\xcax\x91v~{\x9c\x06\xba\xaa\xde\xf4\xe7\x9f\x86\x1d~\xed)\xdc\n8\x9c\x84\xf01@E\x13]\xd7~6\x8e\x8e\xabb-\xad\xcdo\xc3Fw\xb7\xc8y\xdbQ/\xc3\xe5\x18'
    assert eth_addr == b'Ba9A9aED0a1AbBE1da1155F64e73e57Af7995880'
    assert chain_code == b'4\xaa\x95\xf4\x02\x12\x12-T\x155\x86\xed\xc5\x0b\x1d8\x81\xae\xce\xbd\x1a\xbbv\x9a\xc7\xd5\x1a\xd0KT\xe4'
