from pathlib import Path
from web3 import Web3

from ragger.backend import BackendInterface
from ragger.firmware import Firmware
from ragger.navigator import Navigator
from ragger.navigator.navigation_scenario import NavigateWithScenario

from test_sign import common


# Values used across all tests
ADDR = bytes.fromhex("5a321744667052affa8386ed49e00ef223cbffc3")
BIP32_PATH = "m/44'/1001'/0'/0/0"
NONCE = 68
GAS_PRICE = 13
GAS_LIMIT = 21000
VALUE = 0.31415


# Transfer on Clone app
def test_clone_thundercore(firmware: Firmware,
                           backend: BackendInterface,
                           navigator: Navigator,
                           scenario_navigator: NavigateWithScenario,
                           default_screenshot_path: Path,
                           test_name: str):
    tx_params: dict = {
        "nonce": NONCE,
        "gasPrice": Web3.to_wei(GAS_PRICE, 'gwei'),
        "gas": GAS_LIMIT,
        "to": ADDR,
        "value": Web3.to_wei(VALUE, "ether"),
        "chainId": 108
    }
    common(firmware, backend, navigator, scenario_navigator, default_screenshot_path, tx_params, test_name, BIP32_PATH)
