from time import sleep

import ethereum_client
from ethereum_client.utils import apdu_as_string, compare_screenshot, compare_screenshot, parse_sign_response, save_screenshot, PATH_IMG
from ethereum_client.transaction import PersonalTransaction

def test_personal_sign_metamask(cmd):
    result: list = []

    bip32_path="44'/60'/0'/0/0"
    transaction = PersonalTransaction(
        msg="Example `personal_sign` message"
    )

    with cmd.simple_personal_sign_tx(bip32_path=bip32_path, transaction=transaction, result=result) as ex:
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

    try:
        with cmd.simple_personal_sign_tx(bip32_path=bip32_path, transaction=transaction, result=result) as ex:
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

    except ethereum_client.exception.errors.DenyError as error:
        assert error.args[0] == '0x6985'

def test_personal_sign_non_ascii(cmd):
    result: list = []

    bip32_path="44'/60'/0'/0/0"
    transaction = PersonalTransaction(
        msg="0x9c22ff5f21f0b81b113e63f7db6da94fedef11b2119b4088b89664fb9a3cb658"
    )

    with cmd.simple_personal_sign_tx(bip32_path=bip32_path, transaction=transaction, result=result) as ex:
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

# ============================
# The encoded message is greater than the maximum length of an apdu, that's why we cut it into 3 apdu
# ============================

FIRST_PART = apdu_as_string("e008000096058000002c8000003c8000000000000000000000000000015357656c636f6d6520746f204f70656e536561210a0a436c69636b20746f207369676e20696e20616e642061636365707420746865204f70656e536561205465726d73206f6620536572766963653a2068747470733a2f2f6f70656e7365612e696f2f746f730a0a5468697320726571756573742077696c6c206e6f7420")
SECOND_PART = apdu_as_string("e00880009674726967676572206120626c6f636b636861696e207472616e73616374696f6e206f7220636f737420616e792067617320666565732e0a0a596f75722061757468656e7469636174696f6e207374617475732077696c6c20726573657420616674657220323420686f7572732e0a0a57616c6c657420616464726573733a0a3078393835386566666432333262343033336534376439")
THIRD_PART = apdu_as_string("e008800040303030336434316563333465636165646139340a0a4e6f6e63653a0a32623032633861302d663734662d343535342d393832312d613238303534646339313231")

def test_personal_sign_opensea(cmd):
    result: list = []

    # useless but allows to see which info are in the apdu
    bip32_path="44'/60'/0'/0/0"
    transaction = PersonalTransaction(
        msg="Welcome to OpenSea!\n\nClick to sign in and accept the OpenSea Terms of Service: https://opensea.io/tos\n\nThis request will not trigger a blockchain transaction or cost any gas fees.\n\nYour authentication status will reset after 24 hours.\n\nWallet address:\n0x9858effd232b4033e47d90003d41ec34ecaeda94\n\nNonce:\n2b02c8a0-f74f-4554-9821-a28054dc9121"
    )

    cmd.send_apdu(FIRST_PART)
    cmd.send_apdu(SECOND_PART)

    with cmd.send_apdu_context(THIRD_PART, result=result) as ex:
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

    response: bytes = result[0]
    v, r, s = parse_sign_response(response)

    assert v == 0x1c # 28
    assert r.hex() == "61a68c986f087730d2f6ecf89d6d1e48ab963ac461102bb02664bc05c3db75bb"
    assert s.hex() == "5714729ef441e097673a7b29a681e97f6963d875eeed2081f26b0b6686cd2bd2"