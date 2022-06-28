from time import sleep
from ethereum_client.utils import UINT64_MAX, apdu_as_string, compare_screenshot, save_screenshot, PATH_IMG, parse_sign_response
from ethereum_client.plugin import Plugin
import ethereum_client

def test_sign_eip_1559(cmd):
    result: list = []

    bip32_path="44'/60'/0'/0/0"

    