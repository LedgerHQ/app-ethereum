from time import sleep

from ethereum_client.utils import compare_screenshot, save_screenshot, PATH_IMG, parse_sign_response

def test_sign_eip_2930(cmd):
    result: list = []
    apdu_sign_eip_2930 = bytes.fromhex("e004000096058000002c8000003c80000000000000000000000001f886030685012a05f20082520894b2bb2b958afa2e96dab3f3ce7162b87daea39017872386f26fc1000080f85bf85994de0b295669a9fd93d5f28d9ec85e40f4cb697baef842a00000000000000000000000000000000000000000000000000000000000000003a00000000000000000000000000000000000000000000000000000000000000007")

    with cmd.send_apdu_context(apdu_sign_eip_2930, result) as ex:
        sleep(0.5)

        if cmd.model == "nanos":
            # Review transaction
            compare_screenshot(cmd, f"screenshots/eip2930/{PATH_IMG[cmd.model]}/sign_eip_2930/00000.png")
            cmd.client.press_and_release('right')

            # Amount
            compare_screenshot(cmd, f"screenshots/eip2930/{PATH_IMG[cmd.model]}/sign_eip_2930/00001.png")
            cmd.client.press_and_release('right')

            # Address 1/3, 2/3, 3/3
            compare_screenshot(cmd, f"screenshots/eip2930/{PATH_IMG[cmd.model]}/sign_eip_2930/00002.png")
            cmd.client.press_and_release('right')
            compare_screenshot(cmd, f"screenshots/eip2930/{PATH_IMG[cmd.model]}/sign_eip_2930/00003.png")
            cmd.client.press_and_release('right')
            compare_screenshot(cmd, f"screenshots/eip2930/{PATH_IMG[cmd.model]}/sign_eip_2930/00004.png")
            cmd.client.press_and_release('right')

            # Network
            compare_screenshot(cmd, f"screenshots/eip2930/{PATH_IMG[cmd.model]}/sign_eip_2930/00005.png")
            cmd.client.press_and_release('right')

            # Max Fees
            compare_screenshot(cmd, f"screenshots/eip2930/{PATH_IMG[cmd.model]}/sign_eip_2930/00006.png")
            cmd.client.press_and_release('right')

            # Accept and send
            compare_screenshot(cmd, f"screenshots/eip2930/{PATH_IMG[cmd.model]}/sign_eip_2930/00007.png")
            cmd.client.press_and_release('both')

        if cmd.model == "nanox" or cmd.model == "nanosp":
            # Review transaction
            compare_screenshot(cmd, f"screenshots/eip2930/{PATH_IMG[cmd.model]}/sign_eip_2930/00000.png")
            cmd.client.press_and_release('right')

            # Amount
            compare_screenshot(cmd, f"screenshots/eip2930/{PATH_IMG[cmd.model]}/sign_eip_2930/00001.png")
            cmd.client.press_and_release('right')

            # Address
            compare_screenshot(cmd, f"screenshots/eip2930/{PATH_IMG[cmd.model]}/sign_eip_2930/00002.png")
            cmd.client.press_and_release('right')

            # Network
            compare_screenshot(cmd, f"screenshots/eip2930/{PATH_IMG[cmd.model]}/sign_eip_2930/00003.png")
            cmd.client.press_and_release('right')

            # Max Fees
            compare_screenshot(cmd, f"screenshots/eip2930/{PATH_IMG[cmd.model]}/sign_eip_2930/00004.png")
            cmd.client.press_and_release('right')

            # Accept and send
            compare_screenshot(cmd, f"screenshots/eip2930/{PATH_IMG[cmd.model]}/sign_eip_2930/00005.png")
            cmd.client.press_and_release('both')


    response: bytes = result[0]
    v, r, s = parse_sign_response(response)

    assert v == 0x01
    assert r.hex() == "a74d82400f49d1f9d85f734c22a1648d4ab74bb6367bef54c6abb0936be3d8b7"
    assert s.hex() == "7a84a09673394c3c1bd76be05620ee17a2d0ff32837607625efa433cc017854e"
