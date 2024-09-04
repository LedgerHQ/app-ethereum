from pathlib import Path
from web3 import Web3

from ragger.error import ExceptionRAPDU
from ragger.backend import BackendInterface
from ragger.firmware import Firmware
from ragger.navigator import Navigator, NavInsID
from ragger.navigator.navigation_scenario import NavigateWithScenario

from client.client import EthAppClient, StatusWord
import client.response_parser as ResponseParser
from client.settings import SettingID, settings_toggle
from client.utils import recover_transaction


# Values used across all tests
CHAIN_ID = 1
ADDR = bytes.fromhex("0011223344556677889900112233445566778899")
ADDR2 = bytes.fromhex("5a321744667052affa8386ed49e00ef223cbffc3")
ADDR3 = bytes.fromhex("dac17f958d2ee523a2206206994597c13d831ec7")
ADDR4 = bytes.fromhex("b2bb2b958afa2e96dab3f3ce7162b87daea39017")
BIP32_PATH = "m/44'/60'/0'/0/0"
BIP32_PATH2 = "m/44'/60'/1'/0/0"
NONCE = 21
NONCE2 = 68
GAS_PRICE = 13
GAS_PRICE2 = 5
GAS_LIMIT = 21000
AMOUNT = 1.22
AMOUNT2 = 0.31415


def common(firmware: Firmware,
           backend: BackendInterface,
           navigator: Navigator,
           scenario_navigator: NavigateWithScenario,
           default_screenshot_path: Path,
           tx_params: dict,
           test_name: str = "",
           path: str = BIP32_PATH,
           confirm: bool = False):
    app_client = EthAppClient(backend)

    if tx_params["chainId"] == 3:
        name = "Ropsten"
        ticker = "ETH"
        # pylint: disable=line-too-long
        icon = "4000400021bd0100bb011f8b08000000000002ffed94bf4bc34014c7ff8643514441f2829b20e405a50e0eed26d242dd2df40f30505b6d3bb8ba74b260373b38386670a85b978274ea1f211d9cb228356df079975c9aa44db22bfd4ec97d8ef7e37bf78e68a57fa469894629f8a7b845032b99df2b8c2699443c00851994bb4dc06300c676a8adc797606b82afd327c66698654170d67710f331a5973956383fa073d4bb4bbc0d926fd23be291b5543a78e119b36c443c8be20904bc4439be2192c1ce821f9eb16d7ae25c0f65708a10e26bf4c5399e06bca285c233d67504c74b1fbf0044f82e553094610c0b7c835b28e4d9f8adf958919c8d6c97e385a82d0b4bbce076c833989c776e7c9551aa44de625d1ce46cceaf38d25415d1926b9e47b6cf6b826a9a7628979ab2bfa1bf4186cfd3b3f86bcc1dec495e14544593dc3f3330b0332f80a7d7bdf0add0f9c826aa08a8e13ebd89e8d1b970f9b59bde70c3f5a317e0c3dd90e518dcee1e17efd7abe710408626416b21899eaa8058e05ff598091035d6787cd3f18d5b1c00e190bac7bb6b260cb770e8847a8da4091e72870cbaeb260e70afa65bd356ca03f1704c76dafbe218a9efcb4a7f4dbf34cf947a00080000"
        # pylint: enable=line-too-long
    elif tx_params["chainId"] == 56:
        name = "BSC"
        ticker = "BNB"
        # pylint: disable=line-too-long
        icon = "4000400021ff0100fd011f8b08000000000002ffdd554b6ac330101d61f0225090307861c81d5c0c5d1482afe20b945c293941aed2e05d21e810012d0c42d58c64eb63d7dd475944d14833f3deccbc18f322ebf190c6e8875f32373f016a6314e32000b8e873fb0580dd8c69c1ad32336b6e0f7bba468b6501141e1e288c5be7d47e07a8aa0feba77bc70fc031b7c71e6db422f75f67d9ded20b6d8268a4649317c9fd69857022f21c85f6e790e4a3683f0a61112a214e84274278a5fd005048ccbd243e0e51720d46e08e960189d66d8470a26488c223e57e4e118eb4bff8ba4ca26a288d258121d8712311809652063617ffd0af3b47b96a5089578527b2d98c6f5d7844e70fef14e08bfacfcc44126ea48d4847070297757815e2166a75899acfb712774de2d0617dafc1cec86fbda0539e7488fa8f88f66ea8bf1c42b7dee8ac0f762c5df4fe38dba77ffcfbaff6148663c9aff4b460ee6b7c7e082c2dcd8a9f3ef0639f157bfc62d8ddfa2860a1bef5babe737fa09d6df4874748ece3466b7a25636d99fbafc0fe149ff6ac0b1a33f1d2786958684d2698e64327f39128c0325f8c8ae44ad144088d9fcf667b3edd7c9b68be674e4388541f783e08a9be3cf34152e9fd21d7a795be65eebfad3e76561fa7aeeb2ad4c7665f5fd9795f9f0bb9a9effa2f7d37231748abe08201dbf87ff0e2f843ddf7f378957f4df30b82b2cf8e00080000"
        # pylint: enable=line-too-long
    else:
        name = ""

    if name:
        app_client.provide_network_information(name, ticker, tx_params["chainId"], bytes.fromhex(icon))

    with app_client.get_public_addr(bip32_path=path, display=False):
        pass
    _, DEVICE_ADDR, _ = ResponseParser.pk_addr(app_client.response().data)

    with app_client.sign(path, tx_params):
        if not firmware.is_nano and confirm:
            navigator.navigate_and_compare(default_screenshot_path,
                                           f"{test_name}/confirm",
                                           [NavInsID.USE_CASE_CHOICE_CONFIRM],
                                           screen_change_after_last_instruction=False)

        if firmware.is_nano:
            end_text = "Accept"
        else:
            end_text = "Sign"

        scenario_navigator.review_approve(custom_screen_text=end_text, do_comparison=test_name!="")

    # verify signature
    vrs = ResponseParser.signature(app_client.response().data)
    addr = recover_transaction(tx_params, vrs)
    assert addr == DEVICE_ADDR


def common_reject(backend: BackendInterface,
                  scenario_navigator: NavigateWithScenario,
                  tx_params: dict,
                  path: str = BIP32_PATH):
    app_client = EthAppClient(backend)

    try:
        with app_client.sign(path, tx_params):
            scenario_navigator.review_reject()

    except ExceptionRAPDU as e:
        assert e.status == StatusWord.CONDITION_NOT_SATISFIED
    else:
        assert False  # An exception should have been raised


def common_fail(backend: BackendInterface,
                tx_params: dict,
                expected: StatusWord,
                path: str = BIP32_PATH):
    app_client = EthAppClient(backend)

    try:
        with app_client.sign(path, tx_params):
            pass

    except ExceptionRAPDU as e:
        assert e.status == expected
    else:
        assert False  # An exception should have been raised


def test_legacy(firmware: Firmware,
                backend: BackendInterface,
                navigator: Navigator,
                scenario_navigator: NavigateWithScenario,
                default_screenshot_path: Path):
    tx_params: dict = {
        "nonce": NONCE,
        "gasPrice": Web3.to_wei(GAS_PRICE, "gwei"),
        "gas": GAS_LIMIT,
        "to": ADDR,
        "value": Web3.to_wei(AMOUNT, "ether"),
        "chainId": CHAIN_ID
    }
    common(firmware, backend, navigator, scenario_navigator, default_screenshot_path, tx_params)


# Transfer amount >= 2^87 Eth on Ethereum app should fail
def test_legacy_send_error(backend: BackendInterface):
    tx_params: dict = {
        "nonce": 38,
        "gasPrice": 56775612312210000000001234554332,
        "gas": GAS_LIMIT,
        "to": ADDR3,
        "value": 12345678912345678912345678000000000000000000,
        "chainId": CHAIN_ID
    }
    common_fail(backend, tx_params, StatusWord.EXCEPTION_OVERFLOW, path=BIP32_PATH2)


# Transfer bsc
def test_legacy_send_bsc(firmware: Firmware,
                         backend: BackendInterface,
                         navigator: Navigator,
                         scenario_navigator: NavigateWithScenario,
                         test_name: str,
                         default_screenshot_path: Path):
    tx_params: dict = {
        "nonce": 1,
        "gasPrice": Web3.to_wei(GAS_PRICE2, 'gwei'),
        "gas": GAS_LIMIT,
        "to": ADDR2,
        "value": Web3.to_wei(AMOUNT2, "ether"),
        "chainId": 56
    }
    common(firmware, backend, navigator, scenario_navigator, default_screenshot_path, tx_params, test_name, BIP32_PATH2)


# Transfer on network 112233445566 on Ethereum
def test_legacy_chainid(firmware: Firmware,
                        backend: BackendInterface,
                        navigator: Navigator,
                        scenario_navigator: NavigateWithScenario,
                        test_name: str,
                        default_screenshot_path: Path):
    tx_params: dict = {
        "nonce": NONCE2,
        "gasPrice": Web3.to_wei(GAS_PRICE, 'gwei'),
        "gas": GAS_LIMIT,
        "to": ADDR2,
        "value": Web3.to_wei(AMOUNT2, "ether"),
        "chainId": 112233445566
    }
    common(firmware, backend, navigator, scenario_navigator, default_screenshot_path, tx_params, test_name, BIP32_PATH2)


def test_1559(firmware: Firmware,
              backend: BackendInterface,
              navigator: Navigator,
              scenario_navigator: NavigateWithScenario,
              default_screenshot_path: Path):
    tx_params: dict = {
        "nonce": NONCE,
        "maxFeePerGas": Web3.to_wei(145, "gwei"),
        "maxPriorityFeePerGas": Web3.to_wei(1.5, "gwei"),
        "gas": GAS_LIMIT,
        "to": ADDR,
        "value": Web3.to_wei(AMOUNT, "ether"),
        "chainId": CHAIN_ID
    }
    common(firmware, backend, navigator, scenario_navigator, default_screenshot_path, tx_params)


def test_sign_simple(firmware: Firmware,
                     backend: BackendInterface,
                     navigator: Navigator,
                     scenario_navigator: NavigateWithScenario,
                     test_name: str,
                     default_screenshot_path: Path):
    tx_params: dict = {
        "nonce": NONCE2,
        "gasPrice": Web3.to_wei(GAS_PRICE, 'gwei'),
        "gas": GAS_LIMIT,
        "to": ADDR2,
        "value": Web3.to_wei(AMOUNT2, "ether"),
        "chainId": CHAIN_ID
    }
    common(firmware, backend, navigator, scenario_navigator, default_screenshot_path, tx_params, test_name, "m/44'/60'/1'/0/0")


def test_sign_limit_nonce(firmware: Firmware,
                          backend: BackendInterface,
                          navigator: Navigator,
                          scenario_navigator: NavigateWithScenario,
                          test_name: str,
                          default_screenshot_path: Path):
    tx_params: dict = {
        "nonce": 2**64-1,
        "gasPrice": 10,
        "gas": 50000,
        "to": ADDR2,
        "value": 0x08762,
        "chainId": CHAIN_ID
    }
    common(firmware, backend, navigator, scenario_navigator, default_screenshot_path, tx_params, test_name, "m/44'/60'/1'/0/0")


def test_sign_nonce_display(firmware: Firmware,
                            backend: BackendInterface,
                            navigator: Navigator,
                            scenario_navigator: NavigateWithScenario,
                            test_name: str,
                            default_screenshot_path: Path):

    settings_toggle(firmware, navigator, [SettingID.NONCE])

    tx_params: dict = {
        "nonce": NONCE2,
        "gasPrice": Web3.to_wei(GAS_PRICE, 'gwei'),
        "gas": GAS_LIMIT,
        "to": ADDR2,
        "value": Web3.to_wei(AMOUNT2, "ether"),
        "chainId": CHAIN_ID
    }
    common(firmware, backend, navigator, scenario_navigator, default_screenshot_path, tx_params, test_name, "m/44'/60'/1'/0/0")


def test_sign_reject(backend: BackendInterface, scenario_navigator: NavigateWithScenario):
    tx_params: dict = {
        "nonce": NONCE2,
        "gasPrice": Web3.to_wei(GAS_PRICE, 'gwei'),
        "gas": GAS_LIMIT,
        "to": ADDR2,
        "value": Web3.to_wei(AMOUNT2, "ether"),
        "chainId": CHAIN_ID
    }
    common_reject(backend, scenario_navigator, tx_params, "m/44'/60'/1'/0/0")


def test_sign_error_transaction_type(backend: BackendInterface):
    tx_params: dict = {
        "type": 0,
        "nonce": 0,
        "gasPrice": 10,
        "gas": 50000,
        "to": ADDR2,
        "value": 0x19,
        "chainId": CHAIN_ID
    }

    app_client = EthAppClient(backend)
    try:
        with app_client.sign(BIP32_PATH2, tx_params):
            pass

    except TypeError:
        pass
    else:
        assert False  # An exception should have been raised


def test_sign_eip_2930(firmware: Firmware,
                       backend: BackendInterface,
                       navigator: Navigator,
                       scenario_navigator: NavigateWithScenario,
                       test_name: str,
                       default_screenshot_path: Path):

    tx_params = {
        "nonce": NONCE,
        "gasPrice": Web3.to_wei(GAS_PRICE2, "gwei"),
        "gas": GAS_LIMIT,
        "to": ADDR4,
        "value": Web3.to_wei(0.01, "ether"),
        "chainId": 3,
        "accessList": [
            {
                "address": "0x0000000000000000000000000000000000000001",
                "storageKeys": [
                    "0x0100000000000000000000000000000000000000000000000000000000000000"
                ]
            }
        ],
    }
    common(firmware, backend, navigator, scenario_navigator, default_screenshot_path, tx_params, test_name)
