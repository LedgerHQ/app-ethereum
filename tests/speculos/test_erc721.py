from time import sleep

import pytest

import ethereum_client
from ethereum_client.utils import compare_screenshot, PATH_IMG, parse_sign_response
from ethereum_client.plugin import Plugin

SIGN_FIRST = bytes.fromhex("e004000096058000002c8000003c800000000000000000000000f88a0a852c3ce1ec008301f5679460f80121c31a0d46b5279700f9df786054aa5ee580b86442842e0e0000000000000000000000006cbcd73cd8e8a42844662f0a0e76d7f79afd933d000000000000000000000000c2907efcce4011c491bbeda8a0fa63ba7aab596c000000000000000000000000000000000000000000000000")
SIGN_MORE = bytes.fromhex("e00480000b0000000000112999018080")

PLUGIN = Plugin(
    type=1,
    version=1,
    name="ERC721",
    addr="0x60f80121c31a0d46b5279700f9df786054aa5ee5",
    selector=0x42842e0e,
    chainID=1,
    keyID=0,
    algorithm=1,
    sign="304502202e2282d7d3ea714da283010f517af469e1d59654aaee0fc438f017aa557eaea50221008b369679381065bbe01135723a4f9adb229295017d37c4d30138b90a51cf6ab6",
)

PROVIDE_NFT_INFORMATION = Plugin(
    type=1,
    version=1,
    name="Rarible",
    addr="0x60f80121c31a0d46b5279700f9df786054aa5ee5",
    chainID=1,
    keyID=0,
    algorithm=1,
    sign="3045022025696986ef5f0ee2f72d9c6e41d7e2bf2e4f06373ab26d73ebe326c7fd4c7a6602210084f6b064d8750ae68ed5dd012296f37030390ec06ff534c5da6f0f4a4460af33",
)

def test_transfer_erc721(cmd):
    result: list = []

    if cmd.model in ("nanox", "nanosp"):
        cmd.set_plugin(plugin=PLUGIN)
        cmd.provide_nft_information(plugin=PROVIDE_NFT_INFORMATION)

        cmd.send_apdu(SIGN_FIRST)

        with cmd.send_apdu_context(SIGN_MORE, result) as ex:
            sleep(0.5)

            # Loop to check the screens:
            # Review transaction
            # NFT Transfer
            # To
            # Collection Name
            # NFT Address
            # NFT ID
            # Max Fees
            # Accept and send
            nb_png = 7

            for png_index in range(nb_png + 1):
                compare_screenshot(cmd, f"screenshots/erc721/{PATH_IMG[cmd.model]}/transfer_erc721/{png_index:05d}.png")
                cmd.client.press_and_release('both' if png_index == nb_png else 'right')

        response: bytes = result[0]
        v, r, s = parse_sign_response(response)

        assert v == 0x25 # 37
        assert r.hex() == "68ba082523584adbfc31d36d68b51d6f209ce0838215026bf1802a8f17dcdff4"
        assert s.hex() == "7c92908fa05c8bc86507a3d6a1c8b3c2722ee01c836d89a61df60c1ab0b43fff"


def test_transfer_erc721_without_nft_provide_info(cmd):
    result: list = []

    if cmd.model in ("nanox", "nanosp"):
        with pytest.raises(ethereum_client.exception.errors.UnknownDeviceError) as error:

            cmd.set_plugin(plugin=PLUGIN)

            cmd.send_apdu(SIGN_FIRST)

            with cmd.send_apdu_context(SIGN_MORE, result) as ex:
                pass

            assert error.args[0] == '0x6a80'


def test_transfer_erc721_without_set_plugin(cmd):
    result: list = []

    if cmd.model in ("nanox", "nanosp"):
        with pytest.raises(ethereum_client.exception.errors.DenyError) as error:
            cmd.provide_nft_information(plugin=PROVIDE_NFT_INFORMATION)

            cmd.send_apdu(SIGN_FIRST)

            with cmd.send_apdu_context(SIGN_MORE, result) as ex:
                pass

            assert error.args[0] == '0x6985'
