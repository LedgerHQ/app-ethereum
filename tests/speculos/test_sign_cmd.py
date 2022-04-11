from urllib import response
import boilerplate_client
import struct


def test_sign(cmd):
    transaction = "dog"

    response = cmd.simple_sign_tx(bip32_path="44'/60'/1'/0/0", transaction=transaction)

    print(response)

    