import pytest

from ragger.navigator.navigation_scenario import NavigateWithScenario
from ragger.error import ExceptionRAPDU

from client.client import EthAppClient
from client.status_word import StatusWord
from client.safe import SafeAccount, AccountType, LesMultiSigRole
import client.response_parser as ResponseParser



def common(app_client: EthAppClient) -> int:
    challenge = app_client.get_challenge()
    return ResponseParser.challenge(challenge.data)


def test_safe_descriptor(scenario_navigator: NavigateWithScenario,
                         test_name: str) -> None:
    """Test the Safe descriptor APDU"""

    backend = scenario_navigator.backend
    app_client = EthAppClient(backend)
    device = backend.device

    if device.is_nano:
        pytest.skip("Not yet supported on Nano")

    challenge = common(app_client)

    signer = SafeAccount(
        AccountType.SIGNER,
        challenge,
        [
            bytes.fromhex("1"*40),
            bytes.fromhex("2"*40),
            bytes.fromhex("3"*40),
            bytes.fromhex("4"*40),
            bytes.fromhex("5"*40),
            bytes.fromhex("6"*40),
            bytes.fromhex("7"*40),
            bytes.fromhex("8"*40),
            bytes.fromhex("9"*40),
            bytes.fromhex("a"*40),
            bytes.fromhex("b"*40),
            bytes.fromhex("c"*40),
            bytes.fromhex("d"*40),
            bytes.fromhex("e"*40),
            bytes.fromhex("f"*40),
        ],
    )
    safe = SafeAccount(
        AccountType.SAFE,
        challenge,
        [bytes.fromhex("5a321744667052affa8386ed49e00ef223cbffc3")],
        LesMultiSigRole.PROPOSER,
        5,
        len(signer.address),
    )

    with app_client.provide_safe_account(safe):
        # Safe descriptor doesn't start any navigation
        pass
    response = app_client.response()
    assert response.status == StatusWord.OK, f"Unexpected status: {response.status}"

    with app_client.provide_safe_account(signer):
        scenario_navigator.address_review_approve(test_name=test_name)
    response = app_client.response()
    assert response.status == StatusWord.OK, f"Unexpected status: {response.status}"


def test_signer_descriptor_error(scenario_navigator: NavigateWithScenario) -> None:
    """Test the SaSignerfe descriptor APDU"""

    backend = scenario_navigator.backend
    app_client = EthAppClient(backend)
    device = backend.device

    if device.is_nano:
        pytest.skip("Not yet supported on Nano")

    challenge = common(app_client)

    signer = SafeAccount(
        account_type=AccountType.SIGNER,
        challenge=challenge,
        address=[bytes.fromhex("1"*40)],
    )

    try:
        with app_client.provide_safe_account(signer):
            # Safe descriptor doesn't start any navigation
            pass
    except ExceptionRAPDU as e:
        assert e.status == StatusWord.COMMAND_NOT_ALLOWED
    else:
        assert False, "Expected COMMAND_NOT_ALLOWED not raised"
