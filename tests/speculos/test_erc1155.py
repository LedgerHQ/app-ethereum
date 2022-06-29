from time import sleep
from ethereum_client.utils import UINT64_MAX, apdu_as_string, compare_screenshot, save_screenshot, PATH_IMG, parse_sign_response
from ethereum_client.plugin import Plugin
import ethereum_client

SIGN_FIRST = apdu_as_string("e004000096058000002c8000003c800000000000000000000000f901090b8520b673dd0082bcb394495f947276749ce646f68ac8c248420045cb7b5e80b8e4f242432a0000000000000000000000006cbcd73cd8e8a42844662f0a0e76d7f79afd933d000000000000000000000000c2907efcce4011c491bbeda8a0fa63ba7aab596cabf06640f8ca8fc5e0ed471b10befcdf65a33e4300000000")
SIGN_MORE = apdu_as_string("e00480008b00006a0000000064000000000000000000000000000000000000000000000000000000000000000100000000000000000000000000000000000000000000000000000000000000a000000000000000000000000000000000000000000000000000000000000000043078303000000000000000000000000000000000000000000000000000000000018080")

PLUGIN = Plugin(
    type=1,
    version=1,
    name="ERC1155",
    addr="0x495f947276749ce646f68ac8c248420045cb7b5e",
    selector=0xf242432a,
    chainID=1,
    keyID=0,
    algorithm=1,
    sign=b"\x30\x45\x02\x21\x00\xec\x43\x77\xd1\x7e\x8d\x98\xd4\x24\xbf\x16\xb2\x9c\x69\x1b\xc1\xa0\x10\x82\x5f\xb5\xb8\xa3\x5d\xe0\x26\x8a\x9d\xc2\x2e\xab\x24\x02\x20\x67\x01\xb0\x16\xfe\x67\x18\xbf\x51\x9d\x18\xcc\x12\xe9\x83\x8e\x9e\xf8\x98\xcc\x4c\x14\x30\x17\x83\x90\x23\xc3\x26\x0b\x2d\x74",
)

PROVIDE_NFT_INFORMATION = Plugin(
    type=1,
    version=1,
    name="OpenSea Collection",
    addr="0x495f947276749ce646f68ac8c248420045cb7b5e",
    chainID=1,
    keyID=0,
    algorithm=1,
    sign=b"\x30\x45\x02\x21\x00\x83\xe3\x57\xa8\x28\xf1\x3d\x57\x4b\x12\x96\x21\x4a\x37\x49\xc1\x94\xab\x1d\xf1\xf8\xa2\x43\x65\x5c\x05\x3b\x1c\x72\xf9\x1e\x0c\x02\x20\x1e\xd9\x3c\xfa\xc7\xe8\x77\x59\x44\x5c\x4d\xa2\xe4\xbf\xd6\xe1\xcf\x04\x05\xea\x37\xc7\x29\x3b\xc9\x65\x94\x8f\x51\xbe\xf5\xcc",
)

def test_transfer_erc1155(cmd):
    result: list = []

    if cmd.model == "nanox" or cmd.model == "nanosp":
        cmd.set_plugin(plugin=PLUGIN)
        cmd.provide_nft_information(plugin=PROVIDE_NFT_INFORMATION)
            
        cmd.send_apdu(SIGN_FIRST)

        with cmd.send_apdu_context(SIGN_MORE, result) as ex:
            sleep(0.5)
            # Review transaction
            compare_screenshot(cmd, f"screenshots/erc1155/{PATH_IMG[cmd.model]}/transfer_erc1155/00000.png")
            cmd.client.press_and_release('right')
            
            # NFT Transfert
            compare_screenshot(cmd, f"screenshots/erc1155/{PATH_IMG[cmd.model]}/transfer_erc1155/00001.png")
            cmd.client.press_and_release('right')

            # To
            compare_screenshot(cmd, f"screenshots/erc1155/{PATH_IMG[cmd.model]}/transfer_erc1155/00002.png")
            cmd.client.press_and_release('right')

            # Collection Name
            compare_screenshot(cmd, f"screenshots/erc1155/{PATH_IMG[cmd.model]}/transfer_erc1155/00003.png")
            cmd.client.press_and_release('right')

            # NFT Address
            compare_screenshot(cmd, f"screenshots/erc1155/{PATH_IMG[cmd.model]}/transfer_erc1155/00004.png")
            cmd.client.press_and_release('right')

            # NFT ID 1/2, 2/2
            compare_screenshot(cmd, f"screenshots/erc1155/{PATH_IMG[cmd.model]}/transfer_erc1155/00005.png")
            cmd.client.press_and_release('right')
            compare_screenshot(cmd, f"screenshots/erc1155/{PATH_IMG[cmd.model]}/transfer_erc1155/00006.png")
            cmd.client.press_and_release('right')

            # Quantity
            compare_screenshot(cmd, f"screenshots/erc1155/{PATH_IMG[cmd.model]}/transfer_erc1155/00007.png")
            cmd.client.press_and_release('right')

            # Max Fees
            compare_screenshot(cmd, f"screenshots/erc1155/{PATH_IMG[cmd.model]}/transfer_erc1155/00008.png")
            cmd.client.press_and_release('right')

            # Accept and send
            compare_screenshot(cmd, f"screenshots/erc1155/{PATH_IMG[cmd.model]}/transfer_erc1155/00009.png")
            cmd.client.press_and_release('both')


        response: bytes = result[0]
        v, r, s = parse_sign_response(response)

        assert v == 0x25 # 37
        assert r.hex() == "ab3eca1a0b5c66bfe603252037682a024a12f92d799b4d74993a8bf4221bbe7d"
        assert s.hex() == "24de0c0598d1d8e5ea99b75fa26105478f45f43b510e504fc1b14f07fe7dda2a"


def test_transfer_erc1155_without_nft_provide_info(cmd):
    result: list = []

    if cmd.model == "nanox" or cmd.model == "nanosp":
        try:
            cmd.set_plugin(plugin=PLUGIN)

        
            cmd.send_apdu(SIGN_FIRST)

            with cmd.send_apdu_context(SIGN_MORE, result) as ex:
                pass
        
        except ethereum_client.exception.errors.UnknownDeviceError as error:
           assert error.args[0] == '0x6a80'


def test_transfer_erc1155_without_set_plugin(cmd):
    result: list = []

    if cmd.model == "nanox" or cmd.model == "nanosp":
        try:
            cmd.provide_nft_information(plugin=PROVIDE_NFT_INFORMATION)
            
            cmd.send_apdu(SIGN_FIRST)

            with cmd.send_apdu_context(SIGN_MORE, result) as ex:
                pass
        
        except ethereum_client.exception.errors.DenyError as error:
            assert error.args[0] == '0x6985'