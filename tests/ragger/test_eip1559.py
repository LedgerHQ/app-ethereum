from ledger_app_clients.ethereum.client import EthAppClient
import ledger_app_clients.ethereum.response_parser as ResponseParser

from ragger.backend import BackendInterface
from ragger.firmware import Firmware
from ragger.navigator import Navigator, NavInsID

from constants import ROOT_SNAPSHOT_PATH


def test_sign_eip_1559(firmware: Firmware,
                     backend: BackendInterface,
                     navigator: Navigator,
                     test_name: str):

    # with bip32_path "44'/60'/0'/0/0"
    apdu_sign_eip = bytes.fromhex("e004000088058000002c8000003c80000000000000000000000002f87001018502540be4008502540be40086246139ca800094cccccccccccccccccccccccccccccccccccccccc8000c001a0e07fb8a64ea3786c9a6649e54429e2786af3ea31c6d06165346678cf8ce44f9ba00e4a0526db1e905b7164a858fd5ebd2f1759e22e6955499448bd276a6aa62830")

    app_client = EthAppClient(backend)

    with app_client._send(apdu_sign_eip):
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
    v, r, s = ResponseParser.signature(app_client.response().data)

    assert v.hex() == "01"
    assert r.hex() == "3d6dfabc6c52374bfa34cb2c433856a0bcd9484870dd1b50249f7164a5fce052"
    assert s.hex() == "0548a774dd0b63930d83cb2e1a836fe3ef24444e8b758b00585d9a076c0e98a8"
