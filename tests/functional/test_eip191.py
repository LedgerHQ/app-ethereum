import client.response_parser as ResponseParser
import pytest
from client.client import EthAppClient
from client.status_word import StatusWord
from client.utils import recover_message
from ledgered.devices import Device
from ragger.error import ExceptionRAPDU
from ragger.navigator.navigation_scenario import NavigateWithScenario

BIP32_PATH = "m/44'/60'/0'/0/0"


def common(
    scenario_navigator: NavigateWithScenario,
    test_name: str,
    msg: str | bytes,
):

    backend = scenario_navigator.backend
    app_client = EthAppClient(backend)

    with app_client.get_public_addr(display=False):
        pass
    _, DEVICE_ADDR, _ = ResponseParser.pk_addr(app_client.response().data)

    if isinstance(msg, str):
        msg = msg.encode("ascii")

    try:
        with app_client.personal_sign(BIP32_PATH, msg):
            scenario_navigator.review_approve(test_name=test_name)

    except ExceptionRAPDU as err:
        raise AssertionError(f"Unexpected exception: {err}") from err

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


def test_personal_sign_too_many_pages(scenario_navigator: NavigateWithScenario, device: Device):
    """A message that needs more review pages than the UI can address must be refused."""
    app_client = EthAppClient(scenario_navigator.backend)

    if device.is_nano:
        # Nano turns whitespace into spaces: wide words that each fill a line instead
        msg = b"WWWWWW " * 780
    else:
        # one line per byte
        msg = b"\n" * 5000

    with pytest.raises(ExceptionRAPDU) as e:
        with app_client.personal_sign(BIP32_PATH, msg):
            pass
    assert e.value.status == StatusWord.SWO_INCORRECT_DATA


def test_personal_sign_details_spare_pages_overflow(scenario_navigator: NavigateWithScenario, device: Device):
    """A message whose details view page count only overflows once spare pages are added must be refused."""
    if device.is_nano:
        pytest.skip("Only the details view of wallet-size devices adds spare pages")

    app_client = EthAppClient(scenario_navigator.backend)

    lines_per_page = 12 if device.name == "stax" else 11
    # 255 base pages, spare pages then push the count past 8 bits
    msg = b"\n" * (255 * lines_per_page - 5)

    with pytest.raises(ExceptionRAPDU) as e:
        with app_client.personal_sign(BIP32_PATH, msg):
            pass
    assert e.value.status == StatusWord.SWO_INCORRECT_DATA


def test_personal_sign_long_message(scenario_navigator: NavigateWithScenario):
    """A long message whose pages are all reachable is still signed."""
    app_client = EthAppClient(scenario_navigator.backend)

    with app_client.get_public_addr(display=False):
        pass
    _, device_addr, _ = ResponseParser.pk_addr(app_client.response().data)

    msg = b"Lorem ipsum dolor sit amet, consectetur adipiscing elit. " * 40
    with app_client.personal_sign(BIP32_PATH, msg):
        scenario_navigator.review_approve(do_comparison=False)

    vrs = ResponseParser.signature(app_client.response().data)
    assert recover_message(msg, vrs) == device_addr


def test_personal_sign_reject(scenario_navigator: NavigateWithScenario):

    backend = scenario_navigator.backend
    app_client = EthAppClient(backend)

    msg = "This is an reject sign"
    try:
        with app_client.personal_sign(BIP32_PATH, msg.encode("ascii")):
            scenario_navigator.review_reject()

    except ExceptionRAPDU as e:
        assert e.status == StatusWord.SWO_CONDITIONS_NOT_SATISFIED
    else:
        raise AssertionError("An exception should have been raised")
