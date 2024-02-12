from time import sleep

import pytest

import ethereum_client
from ethereum_client.utils import compare_screenshot, compare_screenshot, save_screenshot, PATH_IMG
from ethereum_client.transaction import Transaction


# https://github.com/ethereum/EIPs/blob/master/EIPS/eip-155.md

def test_sign_simple(cmd):
    result: list = []

    # Ether coin type
    bip32_path="44'/60'/1'/0/0"

    transaction = Transaction(
        txType=0xEB,
        nonce=68,
        gasPrice=0x0306dc4200,
        gasLimit=0x5208,
        to="0x5a321744667052affa8386ed49e00ef223cbffc3",
        value=0x6f9c9e7bf61818,
        chainID=1,
    )

    with cmd.simple_sign_tx(bip32_path=bip32_path, transaction=transaction, result=result) as ex:
        sleep(0.5)
        
        if cmd.model == "nanos":
            # Review transaction
            compare_screenshot(cmd, f"screenshots/sign/{PATH_IMG[cmd.model]}/simple/00000.png")
            cmd.client.press_and_release('right')

            # Amount 1/3, 2/3, 3/3
            compare_screenshot(cmd, f"screenshots/sign/{PATH_IMG[cmd.model]}/simple/00001.png")
            cmd.client.press_and_release('right')
            compare_screenshot(cmd, f"screenshots/sign/{PATH_IMG[cmd.model]}/simple/00002.png")
            cmd.client.press_and_release('right')
            compare_screenshot(cmd, f"screenshots/sign/{PATH_IMG[cmd.model]}/simple/00003.png")
            cmd.client.press_and_release('right')

            # Address 1/3, 2/3, 3/3
            compare_screenshot(cmd, f"screenshots/sign/{PATH_IMG[cmd.model]}/simple/00004.png")
            cmd.client.press_and_release('right')
            compare_screenshot(cmd, f"screenshots/sign/{PATH_IMG[cmd.model]}/simple/00005.png")
            cmd.client.press_and_release('right')
            compare_screenshot(cmd, f"screenshots/sign/{PATH_IMG[cmd.model]}/simple/00006.png")
            cmd.client.press_and_release('right')

            # Max Fees
            compare_screenshot(cmd, f"screenshots/sign/{PATH_IMG[cmd.model]}/simple/00007.png")
            cmd.client.press_and_release('right')

            # Accept and send
            compare_screenshot(cmd, f"screenshots/sign/{PATH_IMG[cmd.model]}/simple/00008.png")
            cmd.client.press_and_release('both')

        if cmd.model == "nanox" or cmd.model == "nanosp":
            # Review transaction
            compare_screenshot(cmd, f"screenshots/sign/{PATH_IMG[cmd.model]}/simple/00000.png")
            cmd.client.press_and_release('right')

            # Amount
            compare_screenshot(cmd, f"screenshots/sign/{PATH_IMG[cmd.model]}/simple/00001.png")
            cmd.client.press_and_release('right')

            # Address
            compare_screenshot(cmd, f"screenshots/sign/{PATH_IMG[cmd.model]}/simple/00002.png")
            cmd.client.press_and_release('right')

            # Max Fees
            compare_screenshot(cmd, f"screenshots/sign/{PATH_IMG[cmd.model]}/simple/00003.png")
            cmd.client.press_and_release('right')

            # Accept and send
            compare_screenshot(cmd, f"screenshots/sign/{PATH_IMG[cmd.model]}/simple/00004.png")
            cmd.client.press_and_release('both')

    v, r, s = result

    assert v == 0x26 # 38
    assert r.hex() == "6f389d15320f0501383526ed03de917c14212716f09a262dbc98431086a5db49"
    assert s.hex() == "0dc994b7b97230bb35fdf6fec2f4d8ff4cfb8bfeb2a652c364c738ff033c05dd"


def test_sign_reject(cmd):
    result: list = []
    
    # Ether coin type
    bip32_path="44'/60'/1'/0/0"

    transaction = Transaction(
        txType=0xEB,
        nonce=0,
        gasPrice=0x0306dc4200,
        gasLimit=0x5208,
        to="0x5a321744667052affa8386ed49e00ef223cbffc3",
        value=0x6f9c9e7bf61818,
        chainID=1,
    )

    with pytest.raises(ethereum_client.exception.errors.DenyError) as error:

        with cmd.simple_sign_tx(bip32_path=bip32_path, transaction=transaction, result=result) as ex:
            sleep(0.5)

            if cmd.model == "nanos":
                # Review transaction
                compare_screenshot(cmd, f"screenshots/sign/{PATH_IMG[cmd.model]}/reject/00000.png")
                cmd.client.press_and_release('right')

                # Amount 1/3, 2/3, 3/3
                compare_screenshot(cmd, f"screenshots/sign/{PATH_IMG[cmd.model]}/reject/00001.png")
                cmd.client.press_and_release('right')
                compare_screenshot(cmd, f"screenshots/sign/{PATH_IMG[cmd.model]}/reject/00002.png")
                cmd.client.press_and_release('right')
                compare_screenshot(cmd, f"screenshots/sign/{PATH_IMG[cmd.model]}/reject/00003.png")
                cmd.client.press_and_release('right')

                # Address 1/3, 2/3, 3/3
                compare_screenshot(cmd, f"screenshots/sign/{PATH_IMG[cmd.model]}/reject/00004.png")
                cmd.client.press_and_release('right')
                compare_screenshot(cmd, f"screenshots/sign/{PATH_IMG[cmd.model]}/reject/00005.png")
                cmd.client.press_and_release('right')
                compare_screenshot(cmd, f"screenshots/sign/{PATH_IMG[cmd.model]}/reject/00006.png")
                cmd.client.press_and_release('right')

                # Max Fees
                compare_screenshot(cmd, f"screenshots/sign/{PATH_IMG[cmd.model]}/reject/00007.png")
                cmd.client.press_and_release('right')

                # Accept and send
                compare_screenshot(cmd, f"screenshots/sign/{PATH_IMG[cmd.model]}/reject/00008.png")
                cmd.client.press_and_release('right')
                
                # Reject
                compare_screenshot(cmd, f"screenshots/sign/{PATH_IMG[cmd.model]}/reject/00009.png")
                cmd.client.press_and_release('both')

            if cmd.model == "nanox" or cmd.model == "nanosp":
                # Review transaction
                compare_screenshot(cmd, f"screenshots/sign/{PATH_IMG[cmd.model]}/reject/00000.png")
                cmd.client.press_and_release('right')

                # Amount
                compare_screenshot(cmd, f"screenshots/sign/{PATH_IMG[cmd.model]}/reject/00001.png")
                cmd.client.press_and_release('right')

                # Address
                compare_screenshot(cmd, f"screenshots/sign/{PATH_IMG[cmd.model]}/reject/00002.png")
                cmd.client.press_and_release('right')

                # Max Fees
                compare_screenshot(cmd, f"screenshots/sign/{PATH_IMG[cmd.model]}/reject/00003.png")
                cmd.client.press_and_release('right')

                # Accept and send
                compare_screenshot(cmd, f"screenshots/sign/{PATH_IMG[cmd.model]}/reject/00004.png")
                cmd.client.press_and_release('right')

                # Reject
                compare_screenshot(cmd, f"screenshots/sign/{PATH_IMG[cmd.model]}/reject/00005.png")
                cmd.client.press_and_release('both')

        assert error.args[0] == '0x6985'


def test_sign_limit_nonce(cmd):
    result: list = []
    
    # Ether coin type
    bip32_path="44'/60'/1'/0/0"

    # EIP-2681: Limit account nonce to 2^64-1
    transaction = Transaction(
        txType=0xEB,
        nonce=2**64-1,
        gasPrice=10,
        gasLimit=50000,
        to="0x5a321744667052affa8386ed49e00ef223cbffc3",
        value=0x08762,
        chainID=1,
    )

    with cmd.simple_sign_tx(bip32_path=bip32_path, transaction=transaction, result=result) as ex:
        sleep(0.5)

        if cmd.model == "nanos":
            # Review transaction
            compare_screenshot(cmd, f"screenshots/sign/{PATH_IMG[cmd.model]}/limit_nonce/00000.png")
            cmd.client.press_and_release('right')

            # Amount 1/3, 2/3, 3/3
            compare_screenshot(cmd, f"screenshots/sign/{PATH_IMG[cmd.model]}/limit_nonce/00001.png")
            cmd.client.press_and_release('right')
            compare_screenshot(cmd, f"screenshots/sign/{PATH_IMG[cmd.model]}/limit_nonce/00002.png")
            cmd.client.press_and_release('right')
            compare_screenshot(cmd, f"screenshots/sign/{PATH_IMG[cmd.model]}/limit_nonce/00003.png")
            cmd.client.press_and_release('right')

            # Address 1/3, 2/3, 3/3
            compare_screenshot(cmd, f"screenshots/sign/{PATH_IMG[cmd.model]}/limit_nonce/00004.png")
            cmd.client.press_and_release('right')
            compare_screenshot(cmd, f"screenshots/sign/{PATH_IMG[cmd.model]}/limit_nonce/00005.png")
            cmd.client.press_and_release('right')
            compare_screenshot(cmd, f"screenshots/sign/{PATH_IMG[cmd.model]}/limit_nonce/00006.png")
            cmd.client.press_and_release('right')

            # Max Fees
            compare_screenshot(cmd, f"screenshots/sign/{PATH_IMG[cmd.model]}/limit_nonce/00007.png")
            cmd.client.press_and_release('right')

            # Accept and send
            compare_screenshot(cmd, f"screenshots/sign/{PATH_IMG[cmd.model]}/limit_nonce/00008.png")
            cmd.client.press_and_release('both')

        if cmd.model == "nanox" or cmd.model == "nanosp":
            # Review transaction
            compare_screenshot(cmd, f"screenshots/sign/{PATH_IMG[cmd.model]}/limit_nonce/00000.png")
            cmd.client.press_and_release('right')

            # Amount
            compare_screenshot(cmd, f"screenshots/sign/{PATH_IMG[cmd.model]}/limit_nonce/00001.png")
            cmd.client.press_and_release('right')

            # Address
            compare_screenshot(cmd, f"screenshots/sign/{PATH_IMG[cmd.model]}/limit_nonce/00002.png")
            cmd.client.press_and_release('right')

            # Max Fees
            compare_screenshot(cmd, f"screenshots/sign/{PATH_IMG[cmd.model]}/limit_nonce/00003.png")
            cmd.client.press_and_release('right')

            # Accept and send
            compare_screenshot(cmd, f"screenshots/sign/{PATH_IMG[cmd.model]}/limit_nonce/00004.png")
            cmd.client.press_and_release('both')

    v, r, s = result

    assert v == 0x26 # 38
    assert r.hex() == "7f17f9efa5a6065f885a44a5f5d68a62381c6b2b23047817b4569c61ccf571c6"
    assert s.hex() == "4b67d37cfe473e0b2daf246fa82c7595bcff0c1515d69089037d0c061f14b3b3"


def test_sign_error_transaction_type(cmd):
    result: list = []
    
    # Ether coin type
    bip32_path="44'/60'/1'/0/0"

    # the txType is between 0x00 and 0x7F
    transaction = Transaction(
        txType=0x00,
        nonce=0,
        gasPrice=10,
        gasLimit=50000,
        to="0x5a321744667052affa8386ed49e00ef223cbffc3",
        value=0x19,
        chainID=1,
    )

    with pytest.raises(ethereum_client.exception.errors.UnknownDeviceError) as error:

        with cmd.simple_sign_tx(bip32_path=bip32_path, transaction=transaction, result=result) as ex:
            pass

        assert error.args[0] == '0x6501'

    transaction.txType = 0x7F
    with pytest.raises(ethereum_client.exception.errors.UnknownDeviceError) as error:
        with cmd.simple_sign_tx(bip32_path=bip32_path, transaction=transaction, result=result) as ex:
            pass

        assert error.args[0] == '0x6501'


def test_sign_nonce_display(cmd):
    # Activate nonce display
    # Application is ready
    cmd.client.press_and_release('left')
    # Quit
    cmd.client.press_and_release('left')
    # Settings
    cmd.client.press_and_release('both')
    # Blind signing
    cmd.client.press_and_release('right')
    # Debug data
    cmd.client.press_and_release('right')
    # Nonce display
    cmd.client.press_and_release('both')
    cmd.client.press_and_release('right')
    # Back
    cmd.client.press_and_release('both')

    result: list = []

    # Ether coin type
    bip32_path="44'/60'/1'/0/0"

    transaction = Transaction(
        txType=0xEB,
        nonce=68,
        gasPrice=0x0306dc4200,
        gasLimit=0x5208,
        to="0x5a321744667052affa8386ed49e00ef223cbffc3",
        value=0x6f9c9e7bf61818,
        chainID=1,
    )

    with cmd.simple_sign_tx(bip32_path=bip32_path, transaction=transaction, result=result) as ex:
        sleep(0.5)
        
        if cmd.model == "nanos":
            # Review transaction
            compare_screenshot(cmd, f"screenshots/sign/{PATH_IMG[cmd.model]}/nonce_display/00000.png")
            cmd.client.press_and_release('right')

            # Amount 1/3, 2/3, 3/3
            compare_screenshot(cmd, f"screenshots/sign/{PATH_IMG[cmd.model]}/nonce_display/00001.png")
            cmd.client.press_and_release('right')
            compare_screenshot(cmd, f"screenshots/sign/{PATH_IMG[cmd.model]}/nonce_display/00002.png")
            cmd.client.press_and_release('right')
            compare_screenshot(cmd, f"screenshots/sign/{PATH_IMG[cmd.model]}/nonce_display/00003.png")
            cmd.client.press_and_release('right')

            # Address 1/3, 2/3, 3/3
            compare_screenshot(cmd, f"screenshots/sign/{PATH_IMG[cmd.model]}/nonce_display/00004.png")
            cmd.client.press_and_release('right')
            compare_screenshot(cmd, f"screenshots/sign/{PATH_IMG[cmd.model]}/nonce_display/00005.png")
            cmd.client.press_and_release('right')
            compare_screenshot(cmd, f"screenshots/sign/{PATH_IMG[cmd.model]}/nonce_display/00006.png")
            cmd.client.press_and_release('right')

            # Nonce
            compare_screenshot(cmd, f"screenshots/sign/{PATH_IMG[cmd.model]}/nonce_display/00007.png")
            cmd.client.press_and_release('right')

            # Max Fees
            compare_screenshot(cmd, f"screenshots/sign/{PATH_IMG[cmd.model]}/nonce_display/00008.png")
            cmd.client.press_and_release('right')

            # Accept and send
            compare_screenshot(cmd, f"screenshots/sign/{PATH_IMG[cmd.model]}/nonce_display/00009.png")
            cmd.client.press_and_release('both')

        if cmd.model == "nanox" or cmd.model == "nanosp":
            # Review transaction
            compare_screenshot(cmd, f"screenshots/sign/{PATH_IMG[cmd.model]}/nonce_display/00000.png")
            cmd.client.press_and_release('right')

            # Amount
            compare_screenshot(cmd, f"screenshots/sign/{PATH_IMG[cmd.model]}/nonce_display/00001.png")
            cmd.client.press_and_release('right')

            # Address
            compare_screenshot(cmd, f"screenshots/sign/{PATH_IMG[cmd.model]}/nonce_display/00002.png")
            cmd.client.press_and_release('right')

            # Nonce
            compare_screenshot(cmd, f"screenshots/sign/{PATH_IMG[cmd.model]}/nonce_display/00003.png")
            cmd.client.press_and_release('right')

            # Max Fees
            compare_screenshot(cmd, f"screenshots/sign/{PATH_IMG[cmd.model]}/nonce_display/00004.png")
            cmd.client.press_and_release('right')

            # Accept and send
            compare_screenshot(cmd, f"screenshots/sign/{PATH_IMG[cmd.model]}/nonce_display/00005.png")
            cmd.client.press_and_release('both')

    v, r, s = result

    assert v == 0x26 # 38
    assert r.hex() == "6f389d15320f0501383526ed03de917c14212716f09a262dbc98431086a5db49"
    assert s.hex() == "0dc994b7b97230bb35fdf6fec2f4d8ff4cfb8bfeb2a652c364c738ff033c05dd"


def test_sign_blind_simple(cmd):
    # Activate blind signing
    # Application is ready
    cmd.client.press_and_release('left')
    # Quit
    cmd.client.press_and_release('left')
    # Settings
    cmd.client.press_and_release('both')
    # Blind signing
    cmd.client.press_and_release('both')
    cmd.client.press_and_release('right')
    # Debug data
    cmd.client.press_and_release('right')
    # Nonce display
    cmd.client.press_and_release('right')
    # Back
    cmd.client.press_and_release('both')

    result: list = []

    # Ether coin type
    bip32_path="44'/60'/1'/0/0"

    transaction = Transaction(
        txType=0xEB,
        nonce=68,
        gasPrice=0x0306dc4200,
        gasLimit=0x5208,
        to="0x5a321744667052affa8386ed49e00ef223cbffc3",
        value=0x6f9c9e7bf61818,
        chainID=1,
        data="ok",
    )

    with cmd.simple_sign_tx(bip32_path=bip32_path, transaction=transaction, result=result) as ex:
        sleep(0.5)
        
        if cmd.model == "nanos":
            # Review transaction
            compare_screenshot(cmd, f"screenshots/sign/{PATH_IMG[cmd.model]}/blind_simple/00000.png")
            cmd.client.press_and_release('right')

            # Blind Signing
            compare_screenshot(cmd, f"screenshots/sign/{PATH_IMG[cmd.model]}/blind_simple/00001.png")
            cmd.client.press_and_release('right')

            # Amount 1/3, 2/3, 3/3
            compare_screenshot(cmd, f"screenshots/sign/{PATH_IMG[cmd.model]}/blind_simple/00002.png")
            cmd.client.press_and_release('right')
            compare_screenshot(cmd, f"screenshots/sign/{PATH_IMG[cmd.model]}/blind_simple/00003.png")
            cmd.client.press_and_release('right')
            compare_screenshot(cmd, f"screenshots/sign/{PATH_IMG[cmd.model]}/blind_simple/00004.png")
            cmd.client.press_and_release('right')

            # Address 1/3, 2/3, 3/3
            compare_screenshot(cmd, f"screenshots/sign/{PATH_IMG[cmd.model]}/blind_simple/00005.png")
            cmd.client.press_and_release('right')
            compare_screenshot(cmd, f"screenshots/sign/{PATH_IMG[cmd.model]}/blind_simple/00006.png")
            cmd.client.press_and_release('right')
            compare_screenshot(cmd, f"screenshots/sign/{PATH_IMG[cmd.model]}/blind_simple/00007.png")
            cmd.client.press_and_release('right')

            # Max Fees
            compare_screenshot(cmd, f"screenshots/sign/{PATH_IMG[cmd.model]}/blind_simple/00008.png")
            cmd.client.press_and_release('right')

            # Accept and send
            compare_screenshot(cmd, f"screenshots/sign/{PATH_IMG[cmd.model]}/blind_simple/00009.png")
            cmd.client.press_and_release('both')

        if cmd.model == "nanox" or cmd.model == "nanosp":
            # Review transaction
            compare_screenshot(cmd, f"screenshots/sign/{PATH_IMG[cmd.model]}/blind_simple/00000.png")
            cmd.client.press_and_release('right')

            # Blind Signing
            compare_screenshot(cmd, f"screenshots/sign/{PATH_IMG[cmd.model]}/blind_simple/00001.png")
            cmd.client.press_and_release('right')

            # Amount
            compare_screenshot(cmd, f"screenshots/sign/{PATH_IMG[cmd.model]}/blind_simple/00002.png")
            cmd.client.press_and_release('right')

            # Address
            compare_screenshot(cmd, f"screenshots/sign/{PATH_IMG[cmd.model]}/blind_simple/00003.png")
            cmd.client.press_and_release('right')

            # Max Fees
            compare_screenshot(cmd, f"screenshots/sign/{PATH_IMG[cmd.model]}/blind_simple/00004.png")
            cmd.client.press_and_release('right')

            # Accept and send
            compare_screenshot(cmd, f"screenshots/sign/{PATH_IMG[cmd.model]}/blind_simple/00005.png")
            cmd.client.press_and_release('both')

    v, r, s = result

    assert v == 0x26 # 38
    assert r.hex() == "98163696ad14f54e0e7207306b6f66665131cee601052facab8fd24250e15470"
    assert s.hex() == "318e573fc809f7dcb8f9718c8bd2946b2c3c83cedf3720e66e06fb63ceea3174"


def test_sign_blind_error_disabled(cmd):
    result: list = []

    # Ether coin type
    bip32_path="44'/60'/1'/0/0"

    transaction = Transaction(
        txType=0xEB,
        nonce=68,
        gasPrice=0x0306dc4200,
        gasLimit=0x5208,
        to="0x5a321744667052affa8386ed49e00ef223cbffc3",
        value=0x6f9c9e7bf61818,
        chainID=1,
        data="ok",
    )

    with pytest.raises(ethereum_client.exception.errors.UnknownDeviceError) as error:

        with cmd.simple_sign_tx(bip32_path=bip32_path, transaction=transaction, result=result) as ex:
            sleep(0.5)

            if cmd.model == "nanos":
                pass
            if cmd.model == "nanox" or cmd.model == "nanosp":
                pass

        assert error.args[0] == '0x6a80'


def test_sign_blind_and_nonce_display(cmd):
    # Activate blind signing
    # Application is ready
    cmd.client.press_and_release('left')
    # Quit
    cmd.client.press_and_release('left')
    # Settings
    cmd.client.press_and_release('both')
    # Blind signing
    cmd.client.press_and_release('both')
    cmd.client.press_and_release('right')
    # Debug data
    cmd.client.press_and_release('right')
    # Nonce display
    cmd.client.press_and_release('both')
    cmd.client.press_and_release('right')
    # Back
    cmd.client.press_and_release('both')

    result: list = []

    # Ether coin type
    bip32_path="44'/60'/1'/0/0"

    transaction = Transaction(
        txType=0xEB,
        nonce=1844674,
        gasPrice=0x0306dc4200,
        gasLimit=0x5208,
        to="0x5a321744667052affa8386ed49e00ef223cbffc3",
        value=0x6f9c9e7bf61818,
        chainID=1,
        data="That's a little message :)",
    )

    with cmd.simple_sign_tx(bip32_path=bip32_path, transaction=transaction, result=result) as ex:
        sleep(0.5)
        
        if cmd.model == "nanos":
            # Review transaction
            compare_screenshot(cmd, f"screenshots/sign/{PATH_IMG[cmd.model]}/blind_and_nonce_display/00000.png")
            cmd.client.press_and_release('right')

            # Blind Signing
            compare_screenshot(cmd, f"screenshots/sign/{PATH_IMG[cmd.model]}/blind_and_nonce_display/00001.png")
            cmd.client.press_and_release('right')

            # Amount 1/3, 2/3, 3/3
            compare_screenshot(cmd, f"screenshots/sign/{PATH_IMG[cmd.model]}/blind_and_nonce_display/00002.png")
            cmd.client.press_and_release('right')
            compare_screenshot(cmd, f"screenshots/sign/{PATH_IMG[cmd.model]}/blind_and_nonce_display/00003.png")
            cmd.client.press_and_release('right')
            compare_screenshot(cmd, f"screenshots/sign/{PATH_IMG[cmd.model]}/blind_and_nonce_display/00004.png")
            cmd.client.press_and_release('right')

            # Address 1/3, 2/3, 3/3
            compare_screenshot(cmd, f"screenshots/sign/{PATH_IMG[cmd.model]}/blind_and_nonce_display/00005.png")
            cmd.client.press_and_release('right')
            compare_screenshot(cmd, f"screenshots/sign/{PATH_IMG[cmd.model]}/blind_and_nonce_display/00006.png")
            cmd.client.press_and_release('right')
            compare_screenshot(cmd, f"screenshots/sign/{PATH_IMG[cmd.model]}/blind_and_nonce_display/00007.png")
            cmd.client.press_and_release('right')

            # Nonce
            compare_screenshot(cmd, f"screenshots/sign/{PATH_IMG[cmd.model]}/blind_and_nonce_display/00008.png")
            cmd.client.press_and_release('right')

            # Max Fees
            compare_screenshot(cmd, f"screenshots/sign/{PATH_IMG[cmd.model]}/blind_and_nonce_display/00009.png")
            cmd.client.press_and_release('right')

            # Accept and send
            compare_screenshot(cmd, f"screenshots/sign/{PATH_IMG[cmd.model]}/blind_and_nonce_display/00010.png")
            cmd.client.press_and_release('both')

        if cmd.model == "nanox" or cmd.model == "nanosp":
            # Review transaction
            compare_screenshot(cmd, f"screenshots/sign/{PATH_IMG[cmd.model]}/blind_and_nonce_display/00000.png")
            cmd.client.press_and_release('right')

            # Blind Signing
            compare_screenshot(cmd, f"screenshots/sign/{PATH_IMG[cmd.model]}/blind_and_nonce_display/00001.png")
            cmd.client.press_and_release('right')

            # Amount
            compare_screenshot(cmd, f"screenshots/sign/{PATH_IMG[cmd.model]}/blind_and_nonce_display/00002.png")
            cmd.client.press_and_release('right')

            # Address
            compare_screenshot(cmd, f"screenshots/sign/{PATH_IMG[cmd.model]}/blind_and_nonce_display/00003.png")
            cmd.client.press_and_release('right')

            # Nonce
            compare_screenshot(cmd, f"screenshots/sign/{PATH_IMG[cmd.model]}/blind_and_nonce_display/00004.png")
            cmd.client.press_and_release('right')

            # Max Fees
            compare_screenshot(cmd, f"screenshots/sign/{PATH_IMG[cmd.model]}/blind_and_nonce_display/00005.png")
            cmd.client.press_and_release('right')

            # Accept and send
            compare_screenshot(cmd, f"screenshots/sign/{PATH_IMG[cmd.model]}/blind_and_nonce_display/00006.png")
            cmd.client.press_and_release('both')

    v, r, s = result

    assert v == 0x26 # 38
    assert r.hex() == "c8d7cd5c1711ea1af7da048d15d1a95fc9347d4622afa11f32320d73384984d1"
    assert s.hex() == "3165ca0a27f565e1a87560ed3d3a144c4ac9732370428da5e6952e93659f6ac2"
