from typing import Optional
from Crypto.Hash import keccak

from ragger.error import ExceptionRAPDU
from ragger.navigator import NavInsID
from ragger.navigator.navigation_scenario import NavigateWithScenario

from client.client import EthAppClient
from client.status_word import StatusWord
import client.response_parser as ResponseParser
from client.utils import recover_message
from client.tx_simu import TxSimu


BIP32_PATH = "m/44'/60'/0'/0/0"


def handle_simulation(app_client: EthAppClient, msg: str, simu_params: TxSimu) -> None:
    msg_to_sign = b"\x19Ethereum Signed Message:\n" + str(len(msg)).encode("utf-8") + msg.encode("utf-8")
    if not simu_params.tx_hash:
        simu_params.tx_hash = keccak.new(digest_bits=256, data=msg_to_sign).digest()
    response = app_client.provide_tx_simulation(simu_params)
    assert response.status == StatusWord.OK


def common(scenario_navigator: NavigateWithScenario,
           test_name: str,
           msg: str | bytes,
           simu_params: Optional[TxSimu] = None):

    backend = scenario_navigator.backend
    navigator = scenario_navigator.navigator
    screenshot_path = scenario_navigator.screenshot_path
    app_client = EthAppClient(backend)

    with app_client.get_public_addr(display=False):
        pass
    _, DEVICE_ADDR, _ = ResponseParser.pk_addr(app_client.response().data)

    if simu_params is not None:
        simu_params.from_addr = DEVICE_ADDR
        handle_simulation(app_client, msg, simu_params)

    if isinstance(msg, str):
        msg = msg.encode('ascii')

    try:
        with app_client.personal_sign(BIP32_PATH, msg):
            if simu_params is not None:
                navigator.navigate_and_compare(screenshot_path,
                                               f"{test_name}/warning",
                                               [NavInsID.USE_CASE_CHOICE_REJECT],
                                               screen_change_after_last_instruction=False)

            if simu_params and "tx_hash" in simu_params:
                navigator.navigate_until_text_and_compare(NavInsID.SWIPE_CENTER_TO_LEFT,
                                                          [NavInsID.USE_CASE_CHOICE_CONFIRM],
                                                          "Transaction Check",
                                                          screenshot_path,
                                                          test_name)
            else:
                scenario_navigator.review_approve(test_name=test_name)

    except ExceptionRAPDU as err:
        if simu_params and "tx_hash" in simu_params:
            assert err.status == StatusWord.CONDITION_NOT_SATISFIED
        else:
            assert False, f"Unexpected exception: {err}"

    if simu_params is None or "tx_hash" not in simu_params:
        # verify signature
        vrs = ResponseParser.signature(app_client.response().data)
        addr = recover_message(msg, vrs)
        assert addr == DEVICE_ADDR


def test_personal_sign_metamask(scenario_navigator: NavigateWithScenario, test_name: str):

    msg = "Example `personal_sign` message"
    common(scenario_navigator, test_name, msg)


def test_personal_sign_non_ascii(scenario_navigator: NavigateWithScenario, test_name: str):

    msg = bytes.fromhex("9c22ff5f21f0b81b113e63f7db6da94fedef11b2119b4088b89664fb9a3cb658")
    common(scenario_navigator, test_name, msg)


def test_personal_sign_opensea(scenario_navigator: NavigateWithScenario, test_name: str):

    msg = "Welcome to OpenSea!\n\n"
    msg += "Click to sign in and accept the OpenSea Terms of Service: https://opensea.io/tos\n\n"
    msg += "This request will not trigger a blockchain transaction or cost any gas fees.\n\n"
    msg += "Your authentication status will reset after 24 hours.\n\n"
    msg += "Wallet address:\n0x9858effd232b4033e47d90003d41ec34ecaeda94\n\nNonce:\n2b02c8a0-f74f-4554-9821-a28054dc9121"
    common(scenario_navigator, test_name, msg)


def test_personal_sign_reject(scenario_navigator: NavigateWithScenario):

    backend = scenario_navigator.backend
    app_client = EthAppClient(backend)

    msg = "This is an reject sign"
    try:
        with app_client.personal_sign(BIP32_PATH, msg.encode('ascii')):
            scenario_navigator.review_reject()

    except ExceptionRAPDU as e:
        assert e.status == StatusWord.CONDITION_NOT_SATISFIED
    else:
        assert False  # An exception should have been raised
