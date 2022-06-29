from time import sleep
from ethereum_client.utils import UINT64_MAX, apdu_as_string, compare_screenshot, save_screenshot, PATH_IMG, parse_sign_response
from ethereum_client.plugin import Plugin
import ethereum_client

SIGN_FIRST = apdu_as_string("e004000096058000002c8000003c800000000000000000000000f88a0a852c3ce1ec008301f5679460f80121c31a0d46b5279700f9df786054aa5ee580b86442842e0e0000000000000000000000006cbcd73cd8e8a42844662f0a0e76d7f79afd933d000000000000000000000000c2907efcce4011c491bbeda8a0fa63ba7aab596c000000000000000000000000000000000000000000000000")
SIGN_MORE = apdu_as_string("e00480000b0000000000112999018080")

PLUGIN = Plugin(
    type=1,
    version=1,
    name="ERC721",
    addr="0x60f80121c31a0d46b5279700f9df786054aa5ee5",
    selector=0x42842e0e,
    chainID=1,
    keyID=0,
    algorithm=1,
    sign=b"\x30\x45\x02\x20\x2e\x22\x82\xd7\xd3\xea\x71\x4d\xa2\x83\x01\x0f\x51\x7a\xf4\x69\xe1\xd5\x96\x54\xaa\xee\x0f\xc4\x38\xf0\x17\xaa\x55\x7e\xae\xa5\x02\x21\x00\x8b\x36\x96\x79\x38\x10\x65\xbb\xe0\x11\x35\x72\x3a\x4f\x9a\xdb\x22\x92\x95\x01\x7d\x37\xc4\xd3\x01\x38\xb9\x0a\x51\xcf\x6a\xb6",
)

PROVIDE_NFT_INFORMATION = Plugin(
    type=1,
    version=1,
    name="Rarible",
    addr="0x60f80121c31a0d46b5279700f9df786054aa5ee5",
    chainID=1,
    keyID=0,
    algorithm=1,
    sign=b"\x30\x45\x02\x20\x25\x69\x69\x86\xef\x5f\x0e\xe2\xf7\x2d\x9c\x6e\x41\xd7\xe2\xbf\x2e\x4f\x06\x37\x3a\xb2\x6d\x73\xeb\xe3\x26\xc7\xfd\x4c\x7a\x66\x02\x21\x00\x84\xf6\xb0\x64\xd8\x75\x0a\xe6\x8e\xd5\xdd\x01\x22\x96\xf3\x70\x30\x39\x0e\xc0\x6f\xf5\x34\xc5\xda\x6f\x0f\x4a\x44\x60\xaf\x33",
)

def test_transfer_erc721(cmd):
    result: list = []

    if cmd.model == "nanox" or cmd.model == "nanosp":
        cmd.set_plugin(plugin=PLUGIN)
        cmd.provide_nft_information(plugin=PROVIDE_NFT_INFORMATION)
            
        cmd.send_apdu(SIGN_FIRST)

        with cmd.send_apdu_context(SIGN_MORE, result) as ex:
            sleep(0.5)
            # Review transaction
            compare_screenshot(cmd, f"screenshots/erc721/{PATH_IMG[cmd.model]}/transfer_erc721/00000.png")
            cmd.client.press_and_release('right')
                
            # NFT Transfer
            compare_screenshot(cmd, f"screenshots/erc721/{PATH_IMG[cmd.model]}/transfer_erc721/00001.png")
            cmd.client.press_and_release('right')

            # To
            compare_screenshot(cmd, f"screenshots/erc721/{PATH_IMG[cmd.model]}/transfer_erc721/00002.png")
            cmd.client.press_and_release('right')

            # Collection Name
            compare_screenshot(cmd, f"screenshots/erc721/{PATH_IMG[cmd.model]}/transfer_erc721/00003.png")
            cmd.client.press_and_release('right')

            # NFT Address
            compare_screenshot(cmd, f"screenshots/erc721/{PATH_IMG[cmd.model]}/transfer_erc721/00004.png")
            cmd.client.press_and_release('right')

            # NFT ID
            compare_screenshot(cmd, f"screenshots/erc721/{PATH_IMG[cmd.model]}/transfer_erc721/00005.png")
            cmd.client.press_and_release('right')

            # Max Fees
            compare_screenshot(cmd, f"screenshots/erc721/{PATH_IMG[cmd.model]}/transfer_erc721/00006.png")
            cmd.client.press_and_release('right')

            # Accept and send
            compare_screenshot(cmd, f"screenshots/erc721/{PATH_IMG[cmd.model]}/transfer_erc721/00007.png")
            cmd.client.press_and_release('both')

        response: bytes = result[0]
        v, r, s = parse_sign_response(response)

        assert v == 0x25 # 37
        assert r.hex() == "68ba082523584adbfc31d36d68b51d6f209ce0838215026bf1802a8f17dcdff4"
        assert s.hex() == "7c92908fa05c8bc86507a3d6a1c8b3c2722ee01c836d89a61df60c1ab0b43fff"


def test_transfer_erc721_without_nft_provide_info(cmd):
    result: list = []

    if cmd.model == "nanox" or cmd.model == "nanosp":
        try:
            cmd.set_plugin(plugin=PLUGIN)

            cmd.send_apdu(SIGN_FIRST)

            with cmd.send_apdu_context(SIGN_MORE, result) as ex:
                pass
        except ethereum_client.exception.errors.UnknownDeviceError as error:
            assert error.args[0] == '0x6a80'
    


def test_transfer_erc721_without_set_plugin(cmd):
    result: list = []

    if cmd.model == "nanox" or cmd.model == "nanosp":
        try:
            cmd.provide_nft_information(plugin=PROVIDE_NFT_INFORMATION)
            
            cmd.send_apdu(SIGN_FIRST)

            with cmd.send_apdu_context(SIGN_MORE, result) as ex:
                pass
        
        except ethereum_client.exception.errors.DenyError as error:
            assert error.args[0] == '0x6985'