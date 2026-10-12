import copy
import json
from pathlib import Path

import client.response_parser as ResponseParser
import pytest
from client.client import EthAppClient
from client.eip712 import InputData, json_to_tlv
from client.gating import Gating
from client.proxy_info import ProxyInfo
from client.settings import SettingID, settings_toggle
from client.status_word import StatusWord
from client.utils import TxType, recover_message, recover_transaction
from ragger.error import ExceptionRAPDU
from ragger.navigator import NavInsID
from ragger.navigator.navigation_scenario import NavigateWithScenario
from test_blind_sign import BIP32_PATH, common_tx_params
from test_blind_sign import test_blind_sign as blind_sign
from test_eip712 import eip712_json_path, set_wallet_addr
from test_eip712 import test_eip712_new as sign_eip712
from test_eip712_v2 import (
    assert_recovers,
    enable_blind_signing,
    get_wallet_addr,
)
from test_eip712_v2_message_info import domain_separator, message, type_hash


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


def test_gating_blind_signing_proxy_descriptor(scenario_navigator: NavigateWithScenario) -> None:
    """Test a Gating descriptor on the proxy address, with the proxy implementation provided"""
    backend = scenario_navigator.backend
    app_client = EthAppClient(backend)

    with app_client.get_public_addr(bip32_path=BIP32_PATH, display=False):
        pass
    _, device_addr, _ = ResponseParser.pk_addr(app_client.response().data)

    settings_toggle(backend.device, scenario_navigator.navigator, [SettingID.BLIND_SIGNING])

    tx_params = common_tx_params(0.0)
    proxy_info = ProxyInfo(
        ResponseParser.challenge(app_client.get_challenge().data),
        tx_params["to"],
        tx_params["chainId"],
        bytes.fromhex("Dad77910DbDFdE764fC21FCD4E74D71bBACA6D8D"),
    )
    response = app_client.provide_proxy_info(proxy_info.serialize())
    assert response.status == StatusWord.SWO_SUCCESS

    descriptor = Gating(
        TxType.TRANSACTION,
        tx_params["to"],
        "To scan for threats and verify this transaction before signing, use Ledger Multisig.",
        "ledger.com/ledger-multisig",
        tx_params["chainId"],
        bytes.fromhex(tx_params["data"][2:10]),
    )
    response = app_client.provide_gating(descriptor)
    assert response.status == StatusWord.SWO_SUCCESS

    # Same screens as test_gating_blind_signing_with_proxy
    with app_client.sign(BIP32_PATH, tx_params):
        scenario_navigator.review_approve_with_warning(nb_warnings=2, do_comparison=False)

    vrs = ResponseParser.signature(app_client.response().data)
    assert recover_transaction(tx_params, vrs) == device_addr


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


def gating_v2_data() -> dict:
    json_file = Path(eip712_json_path()) / "00-simple_mail-data.json"
    with open(json_file, encoding="utf-8") as file:
        return json.load(file)


def gating_v2(data: dict, **overrides) -> Gating:
    """A gating descriptor for a V2 message: for its domain and its primary type."""
    arguments: dict = {
        "domain_separator": domain_separator(data),
        "primary_type_hash": type_hash(data),
    } | overrides
    return Gating(
        TxType.TYPED_DATA,
        arguments.pop("address", None),
        "To scan for threats and verify this transaction before signing, use Ledger Multisig.",
        "ledger.com/ledger-multisig",
        **arguments,
    )


def provide_gating_v2(app_client: EthAppClient, descriptor: Gating) -> None:
    response = app_client.provide_gating(descriptor)
    assert response and response.status == StatusWord.SWO_SUCCESS


def check_gated(scenario_navigator: NavigateWithScenario, data: dict) -> None:
    enable_blind_signing(scenario_navigator)
    app_client = EthAppClient(scenario_navigator.backend)

    json_to_tlv.process_data(app_client, data, BIP32_PATH)
    provide_gating_v2(app_client, gating_v2(data))
    with app_client.eip712_v2_sign():
        # the warning of a raw review, and the gating one
        scenario_navigator.review_approve_with_warning(do_comparison=False, nb_warnings=2)

    vrs = ResponseParser.signature(app_client.response().data)
    assert get_wallet_addr(app_client) == recover_message(data, vrs)


def test_gating_eip712_v2(scenario_navigator: NavigateWithScenario) -> None:
    """A V2 message is gated by its domain separator and the hash of its primary type, as its message info is."""
    check_gated(scenario_navigator, gating_v2_data())


def test_gating_eip712_v2_domain_without_contract(scenario_navigator: NavigateWithScenario) -> None:
    """A domain without a verifyingContract is gated too: the descriptor names no contract."""
    check_gated(scenario_navigator, message(1))


def with_domain(data: dict, **fields: object) -> dict:
    other = copy.deepcopy(data)
    other["domain"].update(fields)
    return other


def flip_bit(value: bytes, index: int) -> bytes:
    flipped = bytearray(value)
    flipped[index] ^= 0x01
    return bytes(flipped)


def v2_descriptors_of_another_message() -> dict[str, Gating]:
    data = gating_v2_data()
    separator = domain_separator(data)
    expected = type_hash(data)
    sig_ctx: dict = {}
    InputData.init_signature_context(sig_ctx, data["types"], data["domain"], {})
    return {
        "other_domain_name": gating_v2(data, domain_separator=domain_separator(with_domain(data, name="Other"))),
        "other_chain": gating_v2(data, domain_separator=domain_separator(with_domain(data, chainId=137))),
        "other_contract": gating_v2(
            data, domain_separator=domain_separator(with_domain(data, verifyingContract="0x" + "cd" * 20))
        ),
        "separator_last_bit_flipped": gating_v2(data, domain_separator=flip_bit(separator, -1)),
        "other_type": gating_v2(data, primary_type_hash=type_hash(data, "EIP712Domain")),
        "type_hash_first_bit_flipped": gating_v2(data, primary_type_hash=flip_bit(expected, 0)),
        "type_hash_last_bit_flipped": gating_v2(data, primary_type_hash=flip_bit(expected, -1)),
        "hashes_swapped": gating_v2(data, domain_separator=expected, primary_type_hash=separator),
        # a V1 message is named by its contract and its schema hash, not a V2 one
        "v1_descriptor": Gating(
            TxType.TYPED_DATA,
            bytes.fromhex(data["domain"]["verifyingContract"][2:]),
            "To scan for threats and verify this transaction before signing, use Ledger Multisig.",
            "ledger.com/ledger-multisig",
            data["domain"]["chainId"],
            bytes(sig_ctx["schema_hash"]),
        ),
    }


V2_OTHER_DESCRIPTORS = v2_descriptors_of_another_message()


@pytest.mark.parametrize("descriptor", V2_OTHER_DESCRIPTORS.values(), ids=V2_OTHER_DESCRIPTORS.keys())
def test_gating_eip712_v2_other_message_refused(scenario_navigator: NavigateWithScenario, descriptor: Gating) -> None:
    """A descriptor that does not name the domain and the primary type of the message refuses the review."""
    enable_blind_signing(scenario_navigator)
    app_client = EthAppClient(scenario_navigator.backend)

    json_to_tlv.process_data(app_client, gating_v2_data(), BIP32_PATH)
    provide_gating_v2(app_client, descriptor)
    with pytest.raises(ExceptionRAPDU) as e:
        with app_client.eip712_v2_sign():
            pass
    assert e.value.status == StatusWord.SWO_INCORRECT_DATA
    assert_recovers(app_client)


V2_CONTRACT_TAGS = {
    "address": {"address": bytes.fromhex("cd" * 20)},
    "chain_id": {"chain_id": 1},
    "selector": {"selector": bytes(28)},
}


@pytest.mark.parametrize("tag", V2_CONTRACT_TAGS.values(), ids=V2_CONTRACT_TAGS.keys())
def test_gating_eip712_v2_naming_a_contract_refused(scenario_navigator: NavigateWithScenario, tag: dict) -> None:
    """A V2 descriptor that also names a contract is refused."""
    app_client = EthAppClient(scenario_navigator.backend)

    with pytest.raises(ExceptionRAPDU) as e:
        app_client.provide_gating(gating_v2(gating_v2_data(), **tag))
    assert e.value.status == StatusWord.SWO_INCORRECT_DATA
