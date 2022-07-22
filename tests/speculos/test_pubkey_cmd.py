from time import sleep

import pytest

import ethereum_client
from ethereum_client.utils import compare_screenshot, save_screenshot, PATH_IMG


def test_get_public_key(cmd):
    # ETHER COIN without display
    result: list = []
    with cmd.get_public_key(bip32_path="44'/60'/1'/0/0", display=False, result=result) as exchange:
        pass

    uncompressed_addr_len, eth_addr, chain_code = result

    assert len(uncompressed_addr_len) == 65
    assert len(eth_addr) == 40
    assert len(chain_code) == 32

    assert uncompressed_addr_len == b'\x04\xea\x02&\x91\xc7\x87\x00\xd2\xc3\xa0\xc7E\xbe\xa4\xf2\xb8\xe5\xe3\x13\x97j\x10B\xf6\xa1Vc\\\xb2\x05\xda\x1a\xcb\xfe\x04*\nZ\x89eyn6"E\x89\x0eT\xbd-\xbex\xec\x1e\x18df\xf2\xe9\xd0\xf5\xd5\xd8\xdf'
    assert eth_addr == b'463e4e114AA57F54f2Fd2C3ec03572C6f75d84C2'
    assert chain_code == b'\xaf\x89\xcd)\xea${8I\xec\xc80\xc2\xc8\x94\\e1\xd6P\x87\x07?\x9f\xd09\x00\xa0\xea\xa7\x96\xc8'

    # DAI COIN with display
    result: list = []
    with cmd.get_public_key(bip32_path="44'/700'/1'/0/0", display=True, result=result) as exchange:
        sleep(0.5)

        if cmd.model == "nanos":
            # Verify address
            compare_screenshot(cmd, f"screenshots/pubkey/{PATH_IMG[cmd.model]}/get_public_key/00000.png")
            cmd.client.press_and_release('right')

            # Address 1/3, 2/3, 3/3
            compare_screenshot(cmd, f"screenshots/pubkey/{PATH_IMG[cmd.model]}/get_public_key/00001.png")
            cmd.client.press_and_release('right')
            compare_screenshot(cmd, f"screenshots/pubkey/{PATH_IMG[cmd.model]}/get_public_key/00002.png")
            cmd.client.press_and_release('right')
            compare_screenshot(cmd, f"screenshots/pubkey/{PATH_IMG[cmd.model]}/get_public_key/00003.png")
            cmd.client.press_and_release('right')

            # Approved
            compare_screenshot(cmd, f"screenshots/pubkey/{PATH_IMG[cmd.model]}/get_public_key/00004.png")
            cmd.client.press_and_release('both')

        if cmd.model == "nanox" or cmd.model == "nanosp":
            # Verify address
            compare_screenshot(cmd, f"screenshots/pubkey/{PATH_IMG[cmd.model]}/get_public_key/00000.png")
            cmd.client.press_and_release('right')

            # Address
            compare_screenshot(cmd, f"screenshots/pubkey/{PATH_IMG[cmd.model]}/get_public_key/00001.png")
            cmd.client.press_and_release('right')

            # Approve
            compare_screenshot(cmd, f"screenshots/pubkey/{PATH_IMG[cmd.model]}/get_public_key/00002.png")
            cmd.client.press_and_release('both')

    uncompressed_addr_len, eth_addr, chain_code = result
    assert len(uncompressed_addr_len) == 65
    assert len(eth_addr) == 40
    assert len(chain_code) == 32

    assert uncompressed_addr_len == b'\x04V\x8a\x15\xdc\xed\xc8[\x16\x17\x8d\xaf\xcax\x91v~{\x9c\x06\xba\xaa\xde\xf4\xe7\x9f\x86\x1d~\xed)\xdc\n8\x9c\x84\xf01@E\x13]\xd7~6\x8e\x8e\xabb-\xad\xcdo\xc3Fw\xb7\xc8y\xdbQ/\xc3\xe5\x18'
    assert eth_addr == b'Ba9A9aED0a1AbBE1da1155F64e73e57Af7995880'
    assert chain_code == b'4\xaa\x95\xf4\x02\x12\x12-T\x155\x86\xed\xc5\x0b\x1d8\x81\xae\xce\xbd\x1a\xbbv\x9a\xc7\xd5\x1a\xd0KT\xe4'


def test_reject_get_public_key(cmd):
    # DAI COIN with display
    result: list = []

    with pytest.raises(ethereum_client.exception.errors.DenyError) as error:

        with cmd.get_public_key(bip32_path="44'/700'/1'/0/0", display=True, result=result) as exchange:
            sleep(0.5)

            if cmd.model == "nanos":
                # Verify address
                compare_screenshot(cmd, f"screenshots/pubkey/{PATH_IMG[cmd.model]}/reject_get_public_key/00000.png")
                cmd.client.press_and_release('right')

                # Address 1/3, 2/3, 3/3
                compare_screenshot(cmd, f"screenshots/pubkey/{PATH_IMG[cmd.model]}/reject_get_public_key/00001.png")
                cmd.client.press_and_release('right')
                compare_screenshot(cmd, f"screenshots/pubkey/{PATH_IMG[cmd.model]}/reject_get_public_key/00002.png")
                cmd.client.press_and_release('right')
                compare_screenshot(cmd, f"screenshots/pubkey/{PATH_IMG[cmd.model]}/reject_get_public_key/00003.png")
                cmd.client.press_and_release('right')

                # Approve
                compare_screenshot(cmd, f"screenshots/pubkey/{PATH_IMG[cmd.model]}/reject_get_public_key/00004.png")
                cmd.client.press_and_release('right')
                
                # Reject
                compare_screenshot(cmd, f"screenshots/pubkey/{PATH_IMG[cmd.model]}/reject_get_public_key/00005.png")
                cmd.client.press_and_release('both')
            
            if cmd.model == "nanox" or cmd.model == "nanosp":
                # Verify address
                compare_screenshot(cmd, f"screenshots/pubkey/{PATH_IMG[cmd.model]}/reject_get_public_key/00000.png")
                cmd.client.press_and_release('right')

                # Address
                compare_screenshot(cmd, f"screenshots/pubkey/{PATH_IMG[cmd.model]}/reject_get_public_key/00001.png")
                cmd.client.press_and_release('right')

                # Approve
                compare_screenshot(cmd, f"screenshots/pubkey/{PATH_IMG[cmd.model]}/reject_get_public_key/00002.png")
                cmd.client.press_and_release('right')

                # Reject
                compare_screenshot(cmd, f"screenshots/pubkey/{PATH_IMG[cmd.model]}/reject_get_public_key/00003.png")
                cmd.client.press_and_release('both')

        assert error.args[0] == '0x6985'
