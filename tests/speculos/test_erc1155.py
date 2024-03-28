from time import sleep

import pytest

import ethereum_client
from ethereum_client.utils import compare_screenshot, save_screenshot, PATH_IMG, parse_sign_response
from ethereum_client.plugin import Plugin

SIGN_FIRST = bytes.fromhex("e004000096058000002c8000003c800000000000000000000000f901090b8520b673dd0082bcb394495f947276749ce646f68ac8c248420045cb7b5e80b8e4f242432a0000000000000000000000006cbcd73cd8e8a42844662f0a0e76d7f79afd933d000000000000000000000000c2907efcce4011c491bbeda8a0fa63ba7aab596cabf06640f8ca8fc5e0ed471b10befcdf65a33e4300000000")
SIGN_MORE = bytes.fromhex("e00480008b00006a0000000064000000000000000000000000000000000000000000000000000000000000000100000000000000000000000000000000000000000000000000000000000000a000000000000000000000000000000000000000000000000000000000000000043078303000000000000000000000000000000000000000000000000000000000018080")

PLUGIN = Plugin(
    type=1,
    version=1,
    name="ERC1155",
    addr="0x495f947276749ce646f68ac8c248420045cb7b5e",
    selector=0xf242432a,
    chainID=1,
    keyID=0,
    algorithm=1,
    sign="3045022100ec4377d17e8d98d424bf16b29c691bc1a010825fb5b8a35de0268a9dc22eab2402206701b016fe6718bf519d18cc12e9838e9ef898cc4c143017839023c3260b2d74",
)

PROVIDE_NFT_INFORMATION = Plugin(
    type=1,
    version=1,
    name="OpenSea Collection",
    addr="0x495f947276749ce646f68ac8c248420045cb7b5e",
    chainID=1,
    keyID=0,
    algorithm=1,
    sign="304502210083e357a828f13d574b1296214a3749c194ab1df1f8a243655c053b1c72f91e0c02201ed93cfac7e87759445c4da2e4bfd6e1cf0405ea37c7293bc965948f51bef5cc",
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

            # NFT Transfer
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
        with pytest.raises(ethereum_client.exception.errors.UnknownDeviceError) as error:

            cmd.set_plugin(plugin=PLUGIN)

            cmd.send_apdu(SIGN_FIRST)

            with cmd.send_apdu_context(SIGN_MORE, result) as ex:
                pass

            assert error.args[0] == '0x6a80'


def test_transfer_erc1155_without_set_plugin(cmd):
    result: list = []

    if cmd.model == "nanox" or cmd.model == "nanosp":
        with pytest.raises(ethereum_client.exception.errors.DenyError) as error:

            cmd.provide_nft_information(plugin=PROVIDE_NFT_INFORMATION)

            cmd.send_apdu(SIGN_FIRST)

            with cmd.send_apdu_context(SIGN_MORE, result) as ex:
                pass

            assert error.args[0] == '0x6985'


# ===========================
#        Batch
# ===========================

SIGN_FIRST_BATCH  = bytes.fromhex("e004000096058000002c8000003c800000000000000000000000f9020b0e850d8cfd86008301617d94495f947276749ce646f68ac8c248420045cb7b5e80b901e42eb2c2d60000000000000000000000006cbcd73cd8e8a42844662f0a0e76d7f79afd933d000000000000000000000000c2907efcce4011c491bbeda8a0fa63ba7aab596c00000000000000000000000000000000000000000000")
SIGN_MORE_1_BATCH = bytes.fromhex("e004800096000000000000000000a0000000000000000000000000000000000000000000000000000000000000012000000000000000000000000000000000000000000000000000000000000001a00000000000000000000000000000000000000000000000000000000000000003abf06640f8ca8fc5e0ed471b10befcdf65a33e430000000000006a0000000064def9d99ff495856496c028c0")
SIGN_MORE_2_BATCH = bytes.fromhex("e00480009689732473fcd0bbbe000000000000a30000000001abf06640f8ca8fc5e0ed471b10befcdf65a33e430000000000006a00000000640000000000000000000000000000000000000000000000000000000000000003000000000000000000000000000000000000000000000000000000000000000700000000000000000000000000000000000000000000000000000000000000010000")
SIGN_MORE_3_BATCH = bytes.fromhex("e00480006100000000000000000000000000000000000000000000000000000000000200000000000000000000000000000000000000000000000000000000000000043078303000000000000000000000000000000000000000000000000000000000018080")

PLUGIN_BATCH = Plugin(
    type=1,
    version=1,
    name="ERC1155",
    addr="0x495f947276749ce646f68ac8c248420045cb7b5e",
    selector=0x2eb2c2d6,
    chainID=1,
    keyID=0,
    algorithm=1,
    sign="304502210087b35cefc53fd94e25404933eb0d5ff08f20ba655d181de3b24ff0099dc3317f02204a216aa9e0b84bef6e20fcb036bd49647bf0cab66732b99b49ec277ffb682aa1",
)

PROVIDE_NFT_INFORMATION_BATCH = Plugin(
    type=1,
    version=1,
    name="OpenSea Shared Storefront",
    addr="0x495f947276749ce646f68ac8c248420045cb7b5e",
    chainID=1,
    keyID=0,
    algorithm=1,
    sign="3045022100c74cd613a27a9f4887210f5a3a0e12745e1ba0ab3a0d284cb6485d89c3cce4e602205a13e62a91164985cf58a838f8f531c0b91b980d206a5ba8df28270023ef93a3",
)

def test_transfer_batch_erc1155(cmd):
    result: list = []

    if cmd.model == "nanox" or cmd.model == "nanosp":
        cmd.set_plugin(plugin=PLUGIN_BATCH)
        cmd.provide_nft_information(plugin=PROVIDE_NFT_INFORMATION_BATCH)

        cmd.send_apdu(SIGN_FIRST_BATCH)
        cmd.send_apdu(SIGN_MORE_1_BATCH)
        cmd.send_apdu(SIGN_MORE_2_BATCH)

        with cmd.send_apdu_context(SIGN_MORE_3_BATCH, result) as ex:
            sleep(0.5)
            # Review transaction
            compare_screenshot(cmd, f"screenshots/erc1155/{PATH_IMG[cmd.model]}/test_transfer_batch_erc1155/00000.png")
            cmd.client.press_and_release('right')

            # NFT Batch Transfer
            compare_screenshot(cmd, f"screenshots/erc1155/{PATH_IMG[cmd.model]}/test_transfer_batch_erc1155/00001.png")
            cmd.client.press_and_release('right')

            # To
            compare_screenshot(cmd, f"screenshots/erc1155/{PATH_IMG[cmd.model]}/test_transfer_batch_erc1155/00002.png")
            cmd.client.press_and_release('right')

            # Collection Name
            compare_screenshot(cmd, f"screenshots/erc1155/{PATH_IMG[cmd.model]}/test_transfer_batch_erc1155/00003.png")
            cmd.client.press_and_release('right')

            # NFT Address
            compare_screenshot(cmd, f"screenshots/erc1155/{PATH_IMG[cmd.model]}/test_transfer_batch_erc1155/00004.png")
            cmd.client.press_and_release('right')

            # Total Quantity
            compare_screenshot(cmd, f"screenshots/erc1155/{PATH_IMG[cmd.model]}/test_transfer_batch_erc1155/00005.png")
            cmd.client.press_and_release('right')

            # Max Fees
            compare_screenshot(cmd, f"screenshots/erc1155/{PATH_IMG[cmd.model]}/test_transfer_batch_erc1155/00006.png")
            cmd.client.press_and_release('right')

            # Accept and send
            compare_screenshot(cmd, f"screenshots/erc1155/{PATH_IMG[cmd.model]}/test_transfer_batch_erc1155/00007.png")
            cmd.client.press_and_release('both')

        response: bytes = result[0]
        v, r, s = parse_sign_response(response)

        assert v == 0x25 # 37
        assert r.hex() == "ee17b599747775a5056c6f654b476bdec0f3fea2c03a4754a31f736e61015082"
        assert s.hex() == "3d76f264da438a5bda69389e59c08216e98ddb6649323bd5055980ae31f79c1c"
