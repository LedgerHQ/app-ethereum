import client.response_parser as ResponseParser
import pytest
from client.client import EthAppClient
from client.safe import AccountType, LesMultiSigRole, SafeAccount
from client.status_word import StatusWord
from ledgered.devices import DeviceType
from ragger.error import ExceptionRAPDU
from ragger.navigator import NavIns, NavInsID
from ragger.navigator.navigation_scenario import NavigateWithScenario, NavigationScenarioData, UseCase


def common(app_client: EthAppClient) -> int:
    challenge = app_client.get_challenge()
    return ResponseParser.challenge(challenge.data)


def test_safe_descriptor(scenario_navigator: NavigateWithScenario, test_name: str) -> None:
    """Test the Safe descriptor APDU"""

    backend = scenario_navigator.backend
    app_client = EthAppClient(backend)

    challenge = common(app_client)

    signer = SafeAccount(
        AccountType.SIGNER,
        challenge,
        [
            bytes.fromhex("1" * 40),
            bytes.fromhex("2" * 40),
            bytes.fromhex("3" * 40),
            bytes.fromhex("4" * 40),
            bytes.fromhex("5" * 40),
            bytes.fromhex("6" * 40),
            bytes.fromhex("7" * 40),
            bytes.fromhex("8" * 40),
            bytes.fromhex("9" * 40),
            bytes.fromhex("a" * 40),
            bytes.fromhex("b" * 40),
            bytes.fromhex("c" * 40),
            bytes.fromhex("d" * 40),
            bytes.fromhex("e" * 40),
            bytes.fromhex("f" * 40),
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
    assert response.status == StatusWord.SWO_SUCCESS, f"Unexpected status: {response.status}"

    with app_client.provide_safe_account(signer):
        scenario_navigator.address_review_approve(test_name=test_name)
    response = app_client.response()
    assert response.status == StatusWord.SWO_SUCCESS, f"Unexpected status: {response.status}"


def test_signer_descriptor_error(scenario_navigator: NavigateWithScenario) -> None:
    """Test the SaSignerfe descriptor APDU"""

    backend = scenario_navigator.backend
    app_client = EthAppClient(backend)

    challenge = common(app_client)

    signer = SafeAccount(
        account_type=AccountType.SIGNER,
        challenge=challenge,
        address=[bytes.fromhex("1" * 40)],
    )

    try:
        with app_client.provide_safe_account(signer):
            # Safe descriptor doesn't start any navigation
            pass
    except ExceptionRAPDU as e:
        assert e.status == StatusWord.SWO_COMMAND_NOT_ALLOWED
    else:
        raise AssertionError("Expected COMMAND_NOT_ALLOWED not raised")


def test_safe_descriptor_reject(scenario_navigator: NavigateWithScenario) -> None:
    """Declining the Safe Account review must return an error, not success.

    Not compared against snapshots: the rejecting tap answers the APDU, and ragger
    surfaces that error from inside wait_for_screen_change(), racing the status screen
    the same tap draws.
    """
    app_client = EthAppClient(scenario_navigator.backend)
    challenge = common(app_client)

    signer = SafeAccount(
        AccountType.SIGNER,
        challenge,
        [bytes.fromhex("1" * 40), bytes.fromhex("2" * 40)],
    )
    safe = SafeAccount(
        AccountType.SAFE,
        challenge,
        [bytes.fromhex("5a321744667052affa8386ed49e00ef223cbffc3")],
        LesMultiSigRole.PROPOSER,
        2,
        len(signer.address),
    )

    with app_client.provide_safe_account(safe):
        # Safe descriptor doesn't start any navigation
        pass
    response = app_client.response()
    assert response.status == StatusWord.SWO_SUCCESS, f"Unexpected status: {response.status}"

    with pytest.raises(ExceptionRAPDU) as err:
        with app_client.provide_safe_account(signer):
            scenario_navigator.address_review_reject(do_comparison=False)
    assert err.value.status == StatusWord.SWO_CONDITIONS_NOT_SATISFIED


def test_safe_descriptor_100_signers(scenario_navigator: NavigateWithScenario, test_name: str) -> None:
    """Every signer of a maximum size Safe must remain reachable"""

    backend = scenario_navigator.backend
    app_client = EthAppClient(backend)

    challenge = common(app_client)

    signer = SafeAccount(
        AccountType.SIGNER,
        challenge,
        [(i + 1).to_bytes(20, "big") for i in range(100)],
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
    assert response.status == StatusWord.SWO_SUCCESS, f"Unexpected status: {response.status}"

    with app_client.provide_safe_account(signer):
        if backend.device.is_nano:
            scenario_navigator.address_review_approve(test_name=test_name)
        else:
            # Open the last signers group, down to the last signer
            # (review pages before the threshold, threshold position, last group position, signers pages)
            layouts = {
                DeviceType.FLEX: (2, (428, 204), (428, 280), 17),
                DeviceType.STAX: (1, (360, 365), (358, 300), 13),
                DeviceType.APEX_P: (2, (272, 132), (272, 190), 17),
            }
            nb_swipes, threshold_pos, last_group_pos, nb_pages = layouts[backend.device.type]
            moves: list[NavIns | NavInsID] = [NavInsID.SWIPE_CENTER_TO_LEFT] * nb_swipes
            moves += [NavIns(NavInsID.TOUCH, threshold_pos), NavIns(NavInsID.TOUCH, last_group_pos)]
            moves += [NavInsID.USE_CASE_VIEW_DETAILS_NEXT] * (nb_pages - 1)
            moves += [NavInsID.USE_CASE_VIEW_DETAILS_EXIT]
            scenario_navigator.navigator.navigate_and_compare(
                scenario_navigator.screenshot_path,
                f"{test_name}/part1",
                moves,
            )
            scenario = NavigationScenarioData(backend.device, backend, UseCase.ADDRESS_CONFIRMATION, True)
            scenario_navigator.navigator.navigate_until_text_and_compare(
                scenario.navigation,
                scenario.validation,
                scenario.pattern,
                scenario_navigator.screenshot_path,
                f"{test_name}/part2",
                screen_change_before_first_instruction=False,
            )
    response = app_client.response()
    assert response.status == StatusWord.SWO_SUCCESS, f"Unexpected status: {response.status}"
