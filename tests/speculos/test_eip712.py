from time import sleep

import pytest

from ethereum_client.utils import compare_screenshot, save_screenshot, PATH_IMG, parse_sign_response
from ethereum_client.transaction import EIP712
import ethereum_client

def test_sign_eip_712_hashed_msg(cmd):
    result: list = []

    bip32_path="44'/60'/0'/0'/0"
    transaction = EIP712(
        domain_hash="c24f499b8c957196651b13edd64aaccc3980009674b2aea0966c8a56ba81278e",
        msg_hash="9d96be8a7cca396e711a3ba356bd9878df02a726d753ddb6cda3c507d888bc77"
    )

    with cmd.sign_eip712(bip32_path=bip32_path, transaction=transaction, result=result) as ex:
        sleep(0.5)

        if cmd.model == "nanos":
            # Sign typed message
            compare_screenshot(cmd, f"screenshots/eip712/{PATH_IMG[cmd.model]}/sign_eip_712_hashed_msg/00000.png")
            cmd.client.press_and_release('right')

            # Domain hash 1/4, 2/4, 3/4, 4/4
            compare_screenshot(cmd, f"screenshots/eip712/{PATH_IMG[cmd.model]}/sign_eip_712_hashed_msg/00001.png")
            cmd.client.press_and_release('right')
            compare_screenshot(cmd, f"screenshots/eip712/{PATH_IMG[cmd.model]}/sign_eip_712_hashed_msg/00002.png")
            cmd.client.press_and_release('right')
            compare_screenshot(cmd, f"screenshots/eip712/{PATH_IMG[cmd.model]}/sign_eip_712_hashed_msg/00003.png")
            cmd.client.press_and_release('right')
            compare_screenshot(cmd, f"screenshots/eip712/{PATH_IMG[cmd.model]}/sign_eip_712_hashed_msg/00004.png")
            cmd.client.press_and_release('right')

            # Message hash 1/4, 2/4, 3/4, 4/4
            compare_screenshot(cmd, f"screenshots/eip712/{PATH_IMG[cmd.model]}/sign_eip_712_hashed_msg/00005.png")
            cmd.client.press_and_release('right')
            compare_screenshot(cmd, f"screenshots/eip712/{PATH_IMG[cmd.model]}/sign_eip_712_hashed_msg/00006.png")
            cmd.client.press_and_release('right')
            compare_screenshot(cmd, f"screenshots/eip712/{PATH_IMG[cmd.model]}/sign_eip_712_hashed_msg/00007.png")
            cmd.client.press_and_release('right')
            compare_screenshot(cmd, f"screenshots/eip712/{PATH_IMG[cmd.model]}/sign_eip_712_hashed_msg/00008.png")
            cmd.client.press_and_release('right')

            # Sign message
            compare_screenshot(cmd, f"screenshots/eip712/{PATH_IMG[cmd.model]}/sign_eip_712_hashed_msg/00009.png")
            cmd.client.press_and_release('both')

        if cmd.model == "nanox" or cmd.model == "nanosp":
            # Sign typed message
            compare_screenshot(cmd, f"screenshots/eip712/{PATH_IMG[cmd.model]}/sign_eip_712_hashed_msg/00000.png")
            cmd.client.press_and_release('right')

            # Domain hash 1/2, 2/2
            compare_screenshot(cmd, f"screenshots/eip712/{PATH_IMG[cmd.model]}/sign_eip_712_hashed_msg/00001.png")
            cmd.client.press_and_release('right')
            compare_screenshot(cmd, f"screenshots/eip712/{PATH_IMG[cmd.model]}/sign_eip_712_hashed_msg/00002.png")
            cmd.client.press_and_release('right')

            # Message hash 1/2, 2/2
            compare_screenshot(cmd, f"screenshots/eip712/{PATH_IMG[cmd.model]}/sign_eip_712_hashed_msg/00003.png")
            cmd.client.press_and_release('right')
            compare_screenshot(cmd, f"screenshots/eip712/{PATH_IMG[cmd.model]}/sign_eip_712_hashed_msg/00004.png")
            cmd.client.press_and_release('right')

            # Sign message
            compare_screenshot(cmd, f"screenshots/eip712/{PATH_IMG[cmd.model]}/sign_eip_712_hashed_msg/00005.png")
            cmd.client.press_and_release('both')

    v, r, s = result

    assert v == 0x1B #27
    assert r.hex() == "b1cf3dd6f2902ae9b181e158cc07f6ee6e6c456360b18842ece0b947dec89f07"
    assert s.hex() == "5372a9b1a495b76ccd75347b6f591867859fb73aa05a546b79c81073ddff5e8a"

def test_sign_eip_712_hashed_msg_reject(cmd):
    result: list = []

    bip32_path="44'/60'/0'/0'/0"
    transaction = EIP712(
        domain_hash="c24f499b8c957196651b13edd64aaccc3980009674b2aea0966c8a56ba81278e",
        msg_hash="9d96be8a7cca396e711a3ba356bd9878df02a726d753ddb6cda3c507d888bc77"
    )

    with pytest.raises(ethereum_client.exception.errors.DenyError) as error:
        with cmd.sign_eip712(bip32_path=bip32_path, transaction=transaction, result=result) as ex:
            sleep(0.5)

            if cmd.model == "nanos":
                # Sign typed message
                compare_screenshot(cmd, f"screenshots/eip712/{PATH_IMG[cmd.model]}/sign_eip_712_hashed_msg_reject/00000.png")
                cmd.client.press_and_release('right')

                # Domain hash 1/4, 2/4, 3/4, 4/4
                compare_screenshot(cmd, f"screenshots/eip712/{PATH_IMG[cmd.model]}/sign_eip_712_hashed_msg_reject/00001.png")
                cmd.client.press_and_release('right')
                compare_screenshot(cmd, f"screenshots/eip712/{PATH_IMG[cmd.model]}/sign_eip_712_hashed_msg_reject/00002.png")
                cmd.client.press_and_release('right')
                compare_screenshot(cmd, f"screenshots/eip712/{PATH_IMG[cmd.model]}/sign_eip_712_hashed_msg_reject/00003.png")
                cmd.client.press_and_release('right')
                compare_screenshot(cmd, f"screenshots/eip712/{PATH_IMG[cmd.model]}/sign_eip_712_hashed_msg_reject/00004.png")
                cmd.client.press_and_release('right')

                # Message hash 1/4, 2/4, 3/4, 4/4
                compare_screenshot(cmd, f"screenshots/eip712/{PATH_IMG[cmd.model]}/sign_eip_712_hashed_msg_reject/00005.png")
                cmd.client.press_and_release('right')
                compare_screenshot(cmd, f"screenshots/eip712/{PATH_IMG[cmd.model]}/sign_eip_712_hashed_msg_reject/00006.png")
                cmd.client.press_and_release('right')
                compare_screenshot(cmd, f"screenshots/eip712/{PATH_IMG[cmd.model]}/sign_eip_712_hashed_msg_reject/00007.png")
                cmd.client.press_and_release('right')
                compare_screenshot(cmd, f"screenshots/eip712/{PATH_IMG[cmd.model]}/sign_eip_712_hashed_msg_reject/00008.png")
                cmd.client.press_and_release('right')

                # Sign message
                compare_screenshot(cmd, f"screenshots/eip712/{PATH_IMG[cmd.model]}/sign_eip_712_hashed_msg_reject/00009.png")
                cmd.client.press_and_release('right')

                # Cancel signature
                compare_screenshot(cmd, f"screenshots/eip712/{PATH_IMG[cmd.model]}/sign_eip_712_hashed_msg_reject/00010.png")
                cmd.client.press_and_release('both')

            if cmd.model == "nanox" or cmd.model == "nanosp":
                # Sign typed message
                compare_screenshot(cmd, f"screenshots/eip712/{PATH_IMG[cmd.model]}/sign_eip_712_hashed_msg_reject/00000.png")
                cmd.client.press_and_release('right')

                # Domain hash 1/2, 2/2
                compare_screenshot(cmd, f"screenshots/eip712/{PATH_IMG[cmd.model]}/sign_eip_712_hashed_msg_reject/00001.png")
                cmd.client.press_and_release('right')
                compare_screenshot(cmd, f"screenshots/eip712/{PATH_IMG[cmd.model]}/sign_eip_712_hashed_msg_reject/00002.png")
                cmd.client.press_and_release('right')

                # Message hash 1/2, 2/2
                compare_screenshot(cmd, f"screenshots/eip712/{PATH_IMG[cmd.model]}/sign_eip_712_hashed_msg_reject/00003.png")
                cmd.client.press_and_release('right')
                compare_screenshot(cmd, f"screenshots/eip712/{PATH_IMG[cmd.model]}/sign_eip_712_hashed_msg_reject/00004.png")
                cmd.client.press_and_release('right')

                # Sign message
                compare_screenshot(cmd, f"screenshots/eip712/{PATH_IMG[cmd.model]}/sign_eip_712_hashed_msg_reject/00005.png")
                cmd.client.press_and_release('right')

                # Cancel signature
                compare_screenshot(cmd, f"screenshots/eip712/{PATH_IMG[cmd.model]}/sign_eip_712_hashed_msg_reject/00006.png")
                cmd.client.press_and_release('both')
        assert error.args[0] == '0x6985'

def test_sign_eip_712_bad_domain(cmd):
    result: list = []

    bip32_path="44'/60'/0'/0'/0"
    transaction = EIP712(
        domain_hash="deadbeef",
        msg_hash="9d96be8a7cca396e711a3ba356bd9878df02a726d753ddb6cda3c507d888bc77"
    )

    with pytest.raises(ethereum_client.exception.errors.UnknownDeviceError) as error:

        with cmd.sign_eip712(bip32_path=bip32_path, transaction=transaction, result=result) as ex:
            pass

        assert error.args[0] == '0x6a80'

def test_sign_eip_712_bad_msg(cmd):
    result: list = []

    bip32_path="44'/60'/0'/0'/0"
    transaction = EIP712(
        domain_hash="c24f499b8c957196651b13edd64aaccc3980009674b2aea0966c8a56ba81278e",
        msg_hash="deadbeef"
    )

    with pytest.raises(ethereum_client.exception.errors.UnknownDeviceError) as error:

        with cmd.sign_eip712(bip32_path=bip32_path, transaction=transaction, result=result) as ex:
            pass

        assert error.args[0] == '0x6a80'