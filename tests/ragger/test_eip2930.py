from web3 import Web3

from ledger_app_clients.ethereum.client import EthAppClient
import ledger_app_clients.ethereum.response_parser as ResponseParser
from ledger_app_clients.ethereum.utils import recover_transaction

from ragger.backend import BackendInterface
from ragger.firmware import Firmware
from ragger.navigator import Navigator, NavInsID

from constants import ROOT_SNAPSHOT_PATH


BIP32_PATH = "m/44'/60'/0'/0/0"
NONCE = 21
GAS_PRICE = 5
GAS_LIMIT = 21000
ADDR = bytes.fromhex("b2bb2b958afa2e96dab3f3ce7162b87daea39017")
AMOUNT = 0.01
CHAIN_ID = 3


def test_sign_eip_2930(firmware: Firmware,
                       backend: BackendInterface,
                       navigator: Navigator,
                       test_name: str):

    tx_params = {
        "nonce": NONCE,
        "gasPrice": Web3.to_wei(GAS_PRICE, "gwei"),
        "gas": GAS_LIMIT,
        "to": ADDR,
        "value": Web3.to_wei(AMOUNT, "ether"),
        "chainId": CHAIN_ID,
    }

    app_client = EthAppClient(backend)

    with app_client.get_public_addr(bip32_path=BIP32_PATH, display=False):
        pass
    _, DEVICE_ADDR, _ = ResponseParser.pk_addr(app_client.response().data)

    with app_client.sign(BIP32_PATH, tx_params):
        if firmware.device.startswith("nano"):
            next_action = NavInsID.RIGHT_CLICK
            confirm_action = NavInsID.BOTH_CLICK
            end_text = "Accept"
        else:
            next_action = NavInsID.USE_CASE_REVIEW_TAP
            confirm_action = NavInsID.USE_CASE_REVIEW_CONFIRM
            end_text = "Sign"

        navigator.navigate_until_text_and_compare(next_action,
                                                  [confirm_action],
                                                  end_text,
                                                  ROOT_SNAPSHOT_PATH,
                                                  test_name)

    # verify signature
    vrs = ResponseParser.signature(app_client.response().data)
    addr = recover_transaction(tx_params, vrs)
    assert addr == DEVICE_ADDR
