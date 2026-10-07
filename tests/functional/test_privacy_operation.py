import pytest
from client.client import EthAppClient
from client.status_word import StatusWord
from ragger.error import ExceptionRAPDU
from ragger.navigator import NavInsID
from ragger.navigator.navigation_scenario import NavigateWithScenario

# The host-supplied peer u-coordinate, as 32 raw bytes.
PEER_KEY = bytes.fromhex("5901c19a086d1be4b907ec0325bffa758c3eb78192c3df4afa2afd2736a39963")


def _drive_review(scenario_navigator: NavigateWithScenario, approve: bool, compare: bool = False) -> None:
    """Walk the privacy-operation review to its last page and answer it.

    Not driven through NavigateWithScenario: both of its scenarios end by dismissing a
    status page, and this review returns straight to the home screen instead
    (io_seproxyhal_send_status(..., idle=true)), so that trailing dismiss waits for a
    screen change that never comes.

    The anchor is the page carrying the answer. Its text is unique: the confirm page is
    titled "Provide ..." where the first page is titled "Review ...".
    """
    text = "Provide"
    if scenario_navigator.backend.device.is_nano:
        navigate = NavInsID.RIGHT_CLICK
        # Each answer has its own page, so navigate straight to the one being given.
        if not approve:
            text = "Reject"
        validation = [NavInsID.BOTH_CLICK]
    else:
        navigate = NavInsID.SWIPE_CENTER_TO_LEFT
        if approve:
            validation = [NavInsID.USE_CASE_REVIEW_CONFIRM]
        else:
            # Rejecting opens a confirmation modal that has to be answered too.
            validation = [NavInsID.USE_CASE_REVIEW_REJECT, NavInsID.USE_CASE_CHOICE_CONFIRM]
    if compare:
        # The displayed key is the value the command hands back, so pinning the screen
        # pins the returned secret too.
        scenario_navigator.navigator.navigate_until_text_and_compare(
            navigate_instruction=navigate,
            validation_instructions=validation,
            text=text,
            path=scenario_navigator.screenshot_path,
            test_case_name=scenario_navigator.test_name,
            screen_change_after_last_instruction=False,
        )
    else:
        scenario_navigator.navigator.navigate_until_text(
            navigate_instruction=navigate,
            validation_instructions=validation,
            text=text,
            screen_change_after_last_instruction=False,
        )


def test_perform_privacy_operation_public(scenario_navigator: NavigateWithScenario):
    """P1=confirm defers the answer until the user acts, so the review has to be driven."""
    app_client = EthAppClient(scenario_navigator.backend)

    with app_client.perform_privacy_operation(async_mode=True):
        _drive_review(scenario_navigator, approve=True, compare=True)
    response = app_client.response()
    assert response is not None
    assert response.status == StatusWord.SWO_SUCCESS
    assert len(response.data) == 32


def test_perform_privacy_operation_secret(scenario_navigator: NavigateWithScenario):
    app_client = EthAppClient(scenario_navigator.backend)

    with app_client.perform_privacy_operation(pubkey=PEER_KEY, async_mode=True):
        _drive_review(scenario_navigator, approve=True, compare=True)
    response = app_client.response()
    assert response is not None
    assert response.status == StatusWord.SWO_SUCCESS
    assert len(response.data) == 32


def test_perform_privacy_operation_secret_reject(scenario_navigator: NavigateWithScenario):
    """Declining the review must abort before the shared secret is returned.

    Not compared against snapshots: the rejecting tap answers the APDU, and ragger
    surfaces that error from inside wait_for_screen_change(), racing the status screen
    the same tap draws.
    """
    app_client = EthAppClient(scenario_navigator.backend)
    with pytest.raises(ExceptionRAPDU) as err:
        with app_client.perform_privacy_operation(pubkey=PEER_KEY, async_mode=True):
            _drive_review(scenario_navigator, approve=False)
    assert err.value.status == StatusWord.SWO_CONDITIONS_NOT_SATISFIED
