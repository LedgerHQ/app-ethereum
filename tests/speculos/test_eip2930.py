from time import sleep

from ethereum_client.utils import compare_screenshot, PATH_IMG, parse_sign_response

def test_sign_eip_2930(cmd):
    result: list = []
    apdu_sign_eip_2930 = bytes.fromhex("e004000096058000002c8000003c80000000000000000000000001f886030685012a05f20082520894b2bb2b958afa2e96dab3f3ce7162b87daea39017872386f26fc1000080f85bf85994de0b295669a9fd93d5f28d9ec85e40f4cb697baef842a00000000000000000000000000000000000000000000000000000000000000003a00000000000000000000000000000000000000000000000000000000000000007")

    with cmd.send_apdu_context(apdu_sign_eip_2930, result):
        sleep(0.5)

        # Loop to check the screens:
        # Review transaction
        # Amount
        # From (1/3, 2/3, 3/3 on NanoS)
        # Address (1/3, 2/3, 3/3 on NanoS)
        # Network
        # Max Fees
        # Accept and send
        if cmd.model == "nanos":
            nb_png = 10
        elif cmd.model in ("nanox", "nanosp"):
            nb_png = 6

        for png_index in range(nb_png + 1):
            compare_screenshot(cmd, f"screenshots/eip2930/{PATH_IMG[cmd.model]}/sign_eip_2930/{png_index:05d}.png")
            cmd.client.press_and_release('both' if png_index == nb_png else 'right')

    response: bytes = result[0]
    v, r, s = parse_sign_response(response)

    assert v == 0x01
    assert r.hex() == "a74d82400f49d1f9d85f734c22a1648d4ab74bb6367bef54c6abb0936be3d8b7"
    assert s.hex() == "7a84a09673394c3c1bd76be05620ee17a2d0ff32837607625efa433cc017854e"
