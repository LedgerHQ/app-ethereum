from urllib import response
import boilerplate_client
import struct

from boilerplate_client.utils import UINT64_MAX
from boilerplate_client.transaction import Transaction


# https://github.com/ethereum/EIPs/blob/master/EIPS/eip-155.md

def test_simple_sign(cmd):
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
        # Review transaction
        cmd.client.press_and_release('right')
        # Amount 1/3, 2/3, 3/3
        cmd.client.press_and_release('right')
        cmd.client.press_and_release('right')
        cmd.client.press_and_release('right')
        # Address 1/3, 2/3, 3/3
        cmd.client.press_and_release('right')
        cmd.client.press_and_release('right')
        cmd.client.press_and_release('right')
        # Max Fees
        cmd.client.press_and_release('right')
        #Accept and send
        cmd.client.press_and_release('both')

    v, r, s = result

    assert v == 0x26 # 38
    assert r.hex() == "6f389d15320f0501383526ed03de917c14212716f09a262dbc98431086a5db49"
    assert s.hex() == "0dc994b7b97230bb35fdf6fec2f4d8ff4cfb8bfeb2a652c364c738ff033c05dd"
    

def test_sign_dai_coin_type_on_network_5234(cmd):
    result: list = []
    
    # DAI coin type
    bip32_path="44'/700'/1'/0/0"

    transaction = Transaction(
        txType=0xEB,
        nonce=0,
        gasPrice=0x0306dc4200,
        gasLimit=0x5208,
        to="0x5a321744667052affa8386ed49e00ef223cbffc3",
        value=0x6f9c9e7bf61818,
        chainID=5243,
    )

    with cmd.simple_sign_tx(bip32_path=bip32_path, transaction=transaction, result=result) as ex:
        # Review transaction
        cmd.client.press_and_release('right')
        # Amount 1/3, 2/3, 3/3
        cmd.client.press_and_release('right')
        cmd.client.press_and_release('right')
        cmd.client.press_and_release('right')
        # Address 1/3, 2/3, 3/3
        cmd.client.press_and_release('right')
        cmd.client.press_and_release('right')
        cmd.client.press_and_release('right')
        # Network 5243
        cmd.client.press_and_release('right')
        # Max Fees
        cmd.client.press_and_release('right')
        #Accept and send
        cmd.client.press_and_release('both')

    v, r, s = result
    
    assert v == 0x1A # 26
    assert r.hex() == "7ebfa5d5cac1e16bb1f1a8c67706b5c6019c0f198df6bb44e742a9de72330961"
    assert s.hex() == "537419d8d1443d38ea87943c110789decb43b8f4fea8fae256fe842f669da634"


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
    try:
        with cmd.simple_sign_tx(bip32_path=bip32_path, transaction=transaction, result=result) as ex:
            # Review transaction
            cmd.client.press_and_release('right')
            # Amount 1/3, 2/3, 3/3
            cmd.client.press_and_release('right')
            cmd.client.press_and_release('right')
            cmd.client.press_and_release('right')
            # Address 1/3, 2/3, 3/3
            cmd.client.press_and_release('right')
            cmd.client.press_and_release('right')
            cmd.client.press_and_release('right')
            # Max Fees
            cmd.client.press_and_release('right')
            # Accept and send
            cmd.client.press_and_release('right')
            # Reject
            cmd.client.press_and_release('both')
    except boilerplate_client.exception.errors.DenyError as error:
        assert error.args[0] == '0x6985'


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

    try:
        with cmd.simple_sign_tx(bip32_path=bip32_path, transaction=transaction, result=result) as ex:
            pass
    except boilerplate_client.exception.errors.UnknownDeviceError as error:
        # Throw error of transaction type not supported
        assert error.args[0] == '0x6501'

    transaction.txType = 0x7F
    try:
        with cmd.simple_sign_tx(bip32_path=bip32_path, transaction=transaction, result=result) as ex:
            pass
    except boilerplate_client.exception.errors.UnknownDeviceError as error:
        # Throw error of transaction type not supported
        assert error.args[0] == '0x6501'       


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
        # Review transaction
        cmd.client.press_and_release('right')
        # Amount 1/3, 2/3, 3/3
        cmd.client.press_and_release('right')
        cmd.client.press_and_release('right')
        cmd.client.press_and_release('right')
        # Address 1/3, 2/3, 3/3
        cmd.client.press_and_release('right')
        cmd.client.press_and_release('right')
        cmd.client.press_and_release('right')
        # Max Fees
        cmd.client.press_and_release('right')
        #Accept and send
        cmd.client.press_and_release('both')

    v, r, s = result

    assert v == 0x26 # 38
    assert r.hex() == "7f17f9efa5a6065f885a44a5f5d68a62381c6b2b23047817b4569c61ccf571c6"
    assert s.hex() == "4b67d37cfe473e0b2daf246fa82c7595bcff0c1515d69089037d0c061f14b3b3"
