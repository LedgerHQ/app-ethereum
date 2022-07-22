from time import sleep

import pytest

import ethereum_client
from ethereum_client.utils import compare_screenshot, compare_screenshot, parse_sign_response, save_screenshot, PATH_IMG
from ethereum_client.transaction import PersonalTransaction

def test_personal_sign_metamask(cmd):
    result: list = []

    bip32_path="44'/60'/0'/0/0"
    transaction = PersonalTransaction(
        msg="Example `personal_sign` message"
    )

    with cmd.personal_sign_tx(bip32_path=bip32_path, transaction=transaction, result=result) as ex:
        sleep(0.5)
        
        if cmd.model == "nanos":
            # Sign message
            compare_screenshot(cmd, f"screenshots/eip191/{PATH_IMG[cmd.model]}/personal_sign_metamask/00000.png")
            cmd.client.press_and_release('right')

            # Message 1/2, 2/2
            compare_screenshot(cmd, f"screenshots/eip191/{PATH_IMG[cmd.model]}/personal_sign_metamask/00001.png")
            cmd.client.press_and_release('right')
            compare_screenshot(cmd, f"screenshots/eip191/{PATH_IMG[cmd.model]}/personal_sign_metamask/00002.png")
            cmd.client.press_and_release('right')

            # Sign message
            compare_screenshot(cmd, f"screenshots/eip191/{PATH_IMG[cmd.model]}/personal_sign_metamask/00003.png")
            cmd.client.press_and_release('both')
        if cmd.model == "nanox" or cmd.model == "nanosp":
            # Sign message
            compare_screenshot(cmd, f"screenshots/eip191/{PATH_IMG[cmd.model]}/personal_sign_metamask/00000.png")
            cmd.client.press_and_release('right')

            # Message
            compare_screenshot(cmd, f"screenshots/eip191/{PATH_IMG[cmd.model]}/personal_sign_metamask/00001.png")
            cmd.client.press_and_release('right')

            # Sign message
            compare_screenshot(cmd, f"screenshots/eip191/{PATH_IMG[cmd.model]}/personal_sign_metamask/00002.png")
            cmd.client.press_and_release('both')

    v, r, s = result

    assert v == 0x1c # 28
    assert r.hex() == "916099cf0d9c21911c85f0770a47a9696a8189e78c259cf099749748c507baae"
    assert s.hex() == "0d72234bc0ac2e94c5f7a5f4f9cd8610a52be4ea55515a85b9703f1bb158415c"
 
def test_personal_sign_reject(cmd):
    result: list = []

    bip32_path="44'/60'/0'/0/0"
    transaction = PersonalTransaction(
        msg="This is an reject sign"
    )

    with pytest.raises(ethereum_client.exception.errors.DenyError) as error:
        with cmd.personal_sign_tx(bip32_path=bip32_path, transaction=transaction, result=result) as ex:
            sleep(0.5)

            if cmd.model == "nanos":
                # Sign message
                compare_screenshot(cmd, f"screenshots/eip191/{PATH_IMG[cmd.model]}/personal_sign_reject/00000.png")
                cmd.client.press_and_release('right')

                # Message 1/2, 2/2
                compare_screenshot(cmd, f"screenshots/eip191/{PATH_IMG[cmd.model]}/personal_sign_reject/00001.png")
                cmd.client.press_and_release('right')
                compare_screenshot(cmd, f"screenshots/eip191/{PATH_IMG[cmd.model]}/personal_sign_reject/00002.png")
                cmd.client.press_and_release('right')

                # Sign message
                compare_screenshot(cmd, f"screenshots/eip191/{PATH_IMG[cmd.model]}/personal_sign_reject/00003.png")
                cmd.client.press_and_release('right')

                # Cancel signature
                compare_screenshot(cmd, f"screenshots/eip191/{PATH_IMG[cmd.model]}/personal_sign_reject/00004.png")
                cmd.client.press_and_release('both')

            if cmd.model == "nanox" or cmd.model == "nanosp":
                # Sign message
                compare_screenshot(cmd, f"screenshots/eip191/{PATH_IMG[cmd.model]}/personal_sign_reject/00000.png")
                cmd.client.press_and_release('right')

                # Message
                compare_screenshot(cmd, f"screenshots/eip191/{PATH_IMG[cmd.model]}/personal_sign_reject/00001.png")
                cmd.client.press_and_release('right')

                # Sign message
                compare_screenshot(cmd, f"screenshots/eip191/{PATH_IMG[cmd.model]}/personal_sign_reject/00002.png")
                cmd.client.press_and_release('right')

                # Cancel signature
                compare_screenshot(cmd, f"screenshots/eip191/{PATH_IMG[cmd.model]}/personal_sign_reject/00003.png")
                cmd.client.press_and_release('both')
        assert error.args[0] == '0x6985'

def test_personal_sign_non_ascii(cmd):
    result: list = []

    bip32_path="44'/60'/0'/0/0"
    transaction = PersonalTransaction(
        msg="0x9c22ff5f21f0b81b113e63f7db6da94fedef11b2119b4088b89664fb9a3cb658"
    )

    with cmd.personal_sign_tx(bip32_path=bip32_path, transaction=transaction, result=result) as ex:
        sleep(0.5)
        
        if cmd.model == "nanos":
            # Sign message
            compare_screenshot(cmd, f"screenshots/eip191/{PATH_IMG[cmd.model]}/personal_sign_non_ascii/00000.png")
            cmd.client.press_and_release('right')

            # Message 1/4, 2/4, 3/4, 4/4
            compare_screenshot(cmd, f"screenshots/eip191/{PATH_IMG[cmd.model]}/personal_sign_non_ascii/00001.png")
            cmd.client.press_and_release('right')
            compare_screenshot(cmd, f"screenshots/eip191/{PATH_IMG[cmd.model]}/personal_sign_non_ascii/00002.png")
            cmd.client.press_and_release('right')
            compare_screenshot(cmd, f"screenshots/eip191/{PATH_IMG[cmd.model]}/personal_sign_non_ascii/00003.png")
            cmd.client.press_and_release('right')
            compare_screenshot(cmd, f"screenshots/eip191/{PATH_IMG[cmd.model]}/personal_sign_non_ascii/00004.png")
            cmd.client.press_and_release('right')

            # Sign message
            compare_screenshot(cmd, f"screenshots/eip191/{PATH_IMG[cmd.model]}/personal_sign_non_ascii/00005.png")
            cmd.client.press_and_release('both')
        if cmd.model == "nanox" or cmd.model == "nanosp":
            # Sign message
            compare_screenshot(cmd, f"screenshots/eip191/{PATH_IMG[cmd.model]}/personal_sign_non_ascii/00000.png")
            cmd.client.press_and_release('right')

            # Message 1/2, 2/2
            compare_screenshot(cmd, f"screenshots/eip191/{PATH_IMG[cmd.model]}/personal_sign_non_ascii/00001.png")
            cmd.client.press_and_release('right')
            compare_screenshot(cmd, f"screenshots/eip191/{PATH_IMG[cmd.model]}/personal_sign_non_ascii/00002.png")
            cmd.client.press_and_release('right')

            # Sign message
            compare_screenshot(cmd, f"screenshots/eip191/{PATH_IMG[cmd.model]}/personal_sign_non_ascii/00003.png")
            cmd.client.press_and_release('both')

    v, r, s = result

    assert v == 0x1c # 28
    assert r.hex() == "64bdbdb6959425445d00ff2536a7018d2dce904e1f7475938fe4221c3c72500c"
    assert s.hex() == "7c9208e99b6b9266a73aae17b73472d06499746edec34fd47a9dab42f06f2e42"

def test_personal_sign_opensea(cmd):
    result: list = []

    bip32_path="44'/60'/0'/0/0"
    transaction = PersonalTransaction(
        msg="Welcome to OpenSea!\n\nClick to sign in and accept the OpenSea Terms of Service: https://opensea.io/tos\n\nThis request will not trigger a blockchain transaction or cost any gas fees.\n\nYour authentication status will reset after 24 hours.\n\nWallet address:\n0x9858effd232b4033e47d90003d41ec34ecaeda94\n\nNonce:\n2b02c8a0-f74f-4554-9821-a28054dc9121"
    )

    with cmd.personal_sign_tx(bip32_path, transaction, result) as ex:
        sleep(0.5)
        
        if cmd.model == "nanos":
            # Sign message
            compare_screenshot(cmd, f"screenshots/eip191/{PATH_IMG[cmd.model]}/personal_sign_opensea/00000.png")
            cmd.client.press_and_release('right')

            # Message 1/5, 2/5, 3/5, 4/5, 5/5
            compare_screenshot(cmd, f"screenshots/eip191/{PATH_IMG[cmd.model]}/personal_sign_opensea/00001.png")
            cmd.client.press_and_release('right')
            compare_screenshot(cmd, f"screenshots/eip191/{PATH_IMG[cmd.model]}/personal_sign_opensea/00002.png")
            cmd.client.press_and_release('right')
            compare_screenshot(cmd, f"screenshots/eip191/{PATH_IMG[cmd.model]}/personal_sign_opensea/00003.png")
            cmd.client.press_and_release('right')
            compare_screenshot(cmd, f"screenshots/eip191/{PATH_IMG[cmd.model]}/personal_sign_opensea/00004.png")
            cmd.client.press_and_release('right')
            compare_screenshot(cmd, f"screenshots/eip191/{PATH_IMG[cmd.model]}/personal_sign_opensea/00005.png")
            cmd.client.press_and_release('right')

            # Sign message
            compare_screenshot(cmd, f"screenshots/eip191/{PATH_IMG[cmd.model]}/personal_sign_opensea/00006.png")
            cmd.client.press_and_release('both')

        if cmd.model == "nanox" or cmd.model == "nanosp":
            # Sign message
            compare_screenshot(cmd, f"screenshots/eip191/{PATH_IMG[cmd.model]}/personal_sign_opensea/00000.png")
            cmd.client.press_and_release('right')

            # Message 1/5, 2/5, 3/5, 4/5, 5/5
            compare_screenshot(cmd, f"screenshots/eip191/{PATH_IMG[cmd.model]}/personal_sign_opensea/00001.png")
            cmd.client.press_and_release('right')
            compare_screenshot(cmd, f"screenshots/eip191/{PATH_IMG[cmd.model]}/personal_sign_opensea/00002.png")
            cmd.client.press_and_release('right')
            compare_screenshot(cmd, f"screenshots/eip191/{PATH_IMG[cmd.model]}/personal_sign_opensea/00003.png")
            cmd.client.press_and_release('right')
            compare_screenshot(cmd, f"screenshots/eip191/{PATH_IMG[cmd.model]}/personal_sign_opensea/00004.png")
            cmd.client.press_and_release('right')
            compare_screenshot(cmd, f"screenshots/eip191/{PATH_IMG[cmd.model]}/personal_sign_opensea/00005.png")
            cmd.client.press_and_release('right')

            # Sign message
            compare_screenshot(cmd, f"screenshots/eip191/{PATH_IMG[cmd.model]}/personal_sign_opensea/00006.png")
            cmd.client.press_and_release('both')

    v, r, s = result

    assert v == 0x1c # 28
    assert r.hex() == "61a68c986f087730d2f6ecf89d6d1e48ab963ac461102bb02664bc05c3db75bb"
    assert s.hex() == "5714729ef441e097673a7b29a681e97f6963d875eeed2081f26b0b6686cd2bd2"
