from ledger_app_clients.ethereum.client import EthAppClient
import ledger_app_clients.ethereum.response_parser as ResponseParser

from ragger.backend import BackendInterface
from ragger.firmware import Firmware
from ragger.navigator import Navigator, NavInsID

from constants import ROOT_SNAPSHOT_PATH


def test_sign_eip_2930(firmware: Firmware,
                     backend: BackendInterface,
                     navigator: Navigator,
                     test_name: str):

    apdu_sign_eip = bytes.fromhex("e004000096058000002c8000003c80000000000000000000000001f886030685012a05f20082520894b2bb2b958afa2e96dab3f3ce7162b87daea39017872386f26fc1000080f85bf85994de0b295669a9fd93d5f28d9ec85e40f4cb697baef842a00000000000000000000000000000000000000000000000000000000000000003a00000000000000000000000000000000000000000000000000000000000000007")

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
    assert r.hex() == "a74d82400f49d1f9d85f734c22a1648d4ab74bb6367bef54c6abb0936be3d8b7"
    assert s.hex() == "7a84a09673394c3c1bd76be05620ee17a2d0ff32837607625efa433cc017854e"
