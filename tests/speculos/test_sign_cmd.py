from urllib import response
import boilerplate_client
import struct

from boilerplate_client.transaction import Transaction


def test_sign(cmd):
    result: list = []
    
    transaction = Transaction(
        txType=0xEB,
        nonce=68,
        gasPrice=0x0306dc4200,
        gasLimit=0x5208,
        to="0x5a321744667052affa8386ed49e00ef223cbffc3",
        value=0x6f9c9e7bf61818,
        memo="0x80018080",
    )

    with cmd.simple_sign_tx(bip32_path="44'/60'/1'/0/0", transaction=transaction, result=result) as ex:
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

    assert v == 0x26
    assert r.hex() == "6f389d15320f0501383526ed03de917c14212716f09a262dbc98431086a5db49"
    assert s.hex() == "0dc994b7b97230bb35fdf6fec2f4d8ff4cfb8bfeb2a652c364c738ff033c05dd"
    