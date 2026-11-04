import json
from pathlib import Path

import client.response_parser as ResponseParser
from client.client import EthAppClient
from client.gating import Gating
from client.settings import SettingID, settings_toggle
from client.status_word import StatusWord
from client.utils import TxType, recover_transaction
from ragger.navigator import NavInsID
from ragger.navigator.navigation_scenario import NavigateWithScenario
from test_blind_sign import BIP32_PATH, common_tx_params
from test_blind_sign import test_blind_sign as blind_sign
from test_eip712 import eip712_json_path, set_wallet_addr
from test_eip712 import test_eip712_new as sign_eip712


def test_gating_blind_signing(scenario_navigator: NavigateWithScenario) -> None:
    """Test the Gating descriptor APDU with a blind signing transaction"""

    tx_params = common_tx_params(0.0)
    descriptor = Gating(
        TxType.TRANSACTION,
        tx_params["to"],
        "To scan for threats and verify this transaction before signing, use Ledger Multisig.",
        "ledger.com/ledger-multisig",
        tx_params["chainId"],
        bytes.fromhex(tx_params["data"][2:10]),
    )
    blind_sign(
        scenario_navigator.navigator,
        scenario_navigator,
        scenario_navigator.test_name,
        False,
        0.0,
        gating_params=descriptor,
    )


def test_gating_blind_signing_with_proxy(
    scenario_navigator: NavigateWithScenario,
) -> None:
    """Test the Gating descriptor APDU with a blind signing transaction behind a proxy"""

    descriptor = Gating(
        TxType.TRANSACTION,
        bytes.fromhex("Dad77910DbDFdE764fC21FCD4E74D71bBACA6D8D"),
        "To scan for threats and verify this transaction before signing, use Ledger Multisig.",
        "ledger.com/ledger-multisig",
        1,
    )

    blind_sign(
        scenario_navigator.navigator,
        scenario_navigator,
        scenario_navigator.test_name,
        False,
        0.0,
        gating_params=descriptor,
        with_proxy=True,
    )


def test_gating_eip712(scenario_navigator: NavigateWithScenario) -> None:
    """Test the Gating descriptor APDU with a EIP712 transaction"""

    json_file = Path(eip712_json_path()) / "00-simple_mail-data.json"
    with open(json_file, encoding="utf-8") as file:
        data = json.load(file)

    descriptor = Gating(
        TxType.TYPED_DATA,
        bytes.fromhex(data["domain"]["verifyingContract"][2:]),
        "To scan for threats and verify this transaction before signing, use Ledger Multisig.",
        "ledger.com/ledger-multisig",
        data["domain"].get("chainId", 0),
    )
    # Ensure the wallet address is initialized
    set_wallet_addr(scenario_navigator.backend)
    sign_eip712(scenario_navigator, json_file, False, False, descriptor)


def test_gating_blind_signing_reject(scenario_navigator: NavigateWithScenario) -> None:
    """Declining a gated review must return an error and credit no consent.

    Not compared against snapshots: the rejecting tap answers the APDU, and ragger
    surfaces that error from inside wait_for_screen_change(), racing the status screen
    the same tap draws.
    """
    descriptor = Gating(
        TxType.TRANSACTION,
        bytes.fromhex("6b175474e89094c44da98b954eedeac495271d0f"),
        "To scan for threats and verify this transaction before signing, use Ledger Multisig.",
        "ledger.com/ledger-multisig",
        1,
    )
    blind_sign(
        scenario_navigator.navigator,
        scenario_navigator,
        scenario_navigator.test_name,
        True,
        0.0,
        gating_params=descriptor,
        do_comparison=False,
    )


def test_gating_blind_signing_debug_data(scenario_navigator: NavigateWithScenario) -> None:
    """Test the Gating descriptor APDU with a selector, with the contract data displayed"""
    backend = scenario_navigator.backend
    navigator = scenario_navigator.navigator
    device = backend.device
    app_client = EthAppClient(backend)

    with app_client.get_public_addr(bip32_path=BIP32_PATH, display=False):
        pass
    _, device_addr, _ = ResponseParser.pk_addr(app_client.response().data)

    settings_toggle(device, navigator, [SettingID.BLIND_SIGNING, SettingID.DEBUG_DATA])

    tx_params = common_tx_params(0.0)
    data = bytes.fromhex(tx_params["data"][2:])
    selector = data[:4]
    params = (len(data) - len(selector)) // 32
    descriptor = Gating(
        TxType.TRANSACTION,
        tx_params["to"],
        "To scan for threats and verify this transaction before signing, use Ledger Multisig.",
        "ledger.com/ledger-multisig",
        tx_params["chainId"],
        selector,
    )
    response = app_client.provide_gating(descriptor)
    assert response.status == StatusWord.SWO_SUCCESS

    if device.is_nano:
        # verify | selector or parameter | confirm
        confirm_block = [NavInsID.RIGHT_CLICK] * 2 + [NavInsID.BOTH_CLICK]
    else:
        confirm_block = [NavInsID.SWIPE_CENTER_TO_LEFT] * 2 + [NavInsID.USE_CASE_REVIEW_CONFIRM]
    with app_client.sign(BIP32_PATH, tx_params):
        navigator.navigate_and_compare(
            scenario_navigator.screenshot_path,
            f"{scenario_navigator.test_name}/debug_data",
            confirm_block * (1 + params),
            screen_change_after_last_instruction=False,
        )
        # Gating and blind signing warnings
        scenario_navigator.review_approve_with_warning(nb_warnings=2)

    vrs = ResponseParser.signature(app_client.response().data)
    assert recover_transaction(tx_params, vrs) == device_addr
