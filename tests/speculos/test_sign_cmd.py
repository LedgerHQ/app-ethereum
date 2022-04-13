from urllib import response
import boilerplate_client
import struct


def test_sign(cmd):
    # result: list = []
    # transaction = "dog"
    # with cmd.simple_sign_tx(bip32_path="44'/60'/1'/0/0", transaction=transaction, result=result) as ex:
    #     pass
    # v, r, s = result
    # print("HERE: ", result)
    # assert 2 == 1
    v, r, s = cmd.test_zemu_hard_apdu_sign()

    assert v == 0x25
    assert r.hex() == "92243511396b65a4faa735a5472ea99b3ce0f7f2338eab426206730bc0ddc57f"
    assert s.hex() == "161bc0f861064d840de4f4304cfd19a571017e62df7d8f70cf605c0f025593b6"
    