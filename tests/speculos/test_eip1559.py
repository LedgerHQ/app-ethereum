from time import sleep

from ethereum_client.utils import compare_screenshot, save_screenshot, PATH_IMG, parse_sign_response

def test_sign_eip_1559(cmd):
    result: list = []
    # with bip32_path "44'/60'/0'/0/0"
    apdu_sign_eip_1559 = bytes.fromhex("e004000088058000002c8000003c80000000000000000000000002f87001018502540be4008502540be40086246139ca800094cccccccccccccccccccccccccccccccccccccccc8000c001a0e07fb8a64ea3786c9a6649e54429e2786af3ea31c6d06165346678cf8ce44f9ba00e4a0526db1e905b7164a858fd5ebd2f1759e22e6955499448bd276a6aa62830")

    with cmd.send_apdu_context(apdu_sign_eip_1559, result) as ex:
        sleep(0.5)
        
        if cmd.model == "nanos":
            # Review transaction
            compare_screenshot(cmd, f"screenshots/eip1559/{PATH_IMG[cmd.model]}/sign_eip_1559/00000.png")
            cmd.client.press_and_release('right')

            # Amount
            compare_screenshot(cmd, f"screenshots/eip1559/{PATH_IMG[cmd.model]}/sign_eip_1559/00001.png")
            cmd.client.press_and_release('right')

            # Address 1/3, 2/3, 3/3
            compare_screenshot(cmd, f"screenshots/eip1559/{PATH_IMG[cmd.model]}/sign_eip_1559/00002.png")
            cmd.client.press_and_release('right')
            compare_screenshot(cmd, f"screenshots/eip1559/{PATH_IMG[cmd.model]}/sign_eip_1559/00003.png")
            cmd.client.press_and_release('right')
            compare_screenshot(cmd, f"screenshots/eip1559/{PATH_IMG[cmd.model]}/sign_eip_1559/00004.png")
            cmd.client.press_and_release('right')

            # Max Fees
            compare_screenshot(cmd, f"screenshots/eip1559/{PATH_IMG[cmd.model]}/sign_eip_1559/00005.png")
            cmd.client.press_and_release('right')

            # Accept and send
            compare_screenshot(cmd, f"screenshots/eip1559/{PATH_IMG[cmd.model]}/sign_eip_1559/00006.png")
            cmd.client.press_and_release('both')

        if cmd.model == "nanox" or cmd.model == "nanosp":
            # Review transaction
            compare_screenshot(cmd, f"screenshots/eip1559/{PATH_IMG[cmd.model]}/sign_eip_1559/00000.png")
            cmd.client.press_and_release('right')

            # Amount
            compare_screenshot(cmd, f"screenshots/eip1559/{PATH_IMG[cmd.model]}/sign_eip_1559/00001.png")
            cmd.client.press_and_release('right')

            # Address
            compare_screenshot(cmd, f"screenshots/eip1559/{PATH_IMG[cmd.model]}/sign_eip_1559/00002.png")
            cmd.client.press_and_release('right')
            
            # Max Fees
            compare_screenshot(cmd, f"screenshots/eip1559/{PATH_IMG[cmd.model]}/sign_eip_1559/00003.png")
            cmd.client.press_and_release('right')

            # Accept and send
            compare_screenshot(cmd, f"screenshots/eip1559/{PATH_IMG[cmd.model]}/sign_eip_1559/00004.png")
            cmd.client.press_and_release('both')

    response: bytes = result[0]
    v, r, s = parse_sign_response(response)

    assert v == 0x01
    assert r.hex() == "3d6dfabc6c52374bfa34cb2c433856a0bcd9484870dd1b50249f7164a5fce052"
    assert s.hex() == "0548a774dd0b63930d83cb2e1a836fe3ef24444e8b758b00585d9a076c0e98a8"
