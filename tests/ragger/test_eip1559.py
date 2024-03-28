from ledger_app_clients.ethereum.client import EthAppClient
import ledger_app_clients.ethereum.response_parser as ResponseParser
from ledger_app_clients.ethereum.utils import recover_transaction

from ragger.backend import BackendInterface
from ragger.firmware import Firmware
from ragger.navigator import Navigator, NavInsID

from web3 import Web3

from constants import ROOT_SNAPSHOT_PATH


BIP32_PATH = "m/44'/60'/0'/0/0"
NONCE = 1
GAS_PRICE = 40000
GAS_LIMIT = 10
ADDR = bytes.fromhex("cccccccccccccccccccccccccccccccccccccccc")
AMOUNT = 0.01
CHAIN_ID = 1


def test_sign_eip_1559(firmware: Firmware,
                       backend: BackendInterface,
                       navigator: Navigator,
                       test_name: str):

    tx_params = {
        "nonce": NONCE,
        "gas": Web3.to_wei(GAS_PRICE, "gwei"),
        "maxFeePerGas": Web3.to_wei(GAS_LIMIT, "gwei"),
        "maxPriorityFeePerGas": Web3.to_wei(GAS_LIMIT, "gwei"),
        "to": ADDR,
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
