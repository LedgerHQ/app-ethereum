from pathlib import Path
from enum import IntEnum
from web3 import Web3
import pytest

from ledgered.devices import Device

from ragger.error import ExceptionRAPDU
from ragger.backend import BackendInterface
from ragger.navigator import Navigator, NavInsID
from ragger.navigator.navigation_scenario import NavigateWithScenario

from test_sign import common as sign_tx_common
from test_blind_sign import test_blind_sign as blind_sign
from test_nft import common_test_nft, collecs_721, actions_721, ERC721_PLUGIN
from test_eip191 import common as sign_eip191
from test_eip712 import test_eip712_filtering_empty_array as sign_eip712
from test_eip712 import test_eip712_v0 as sign_eip712_v0
from test_gcs import test_gcs_poap as sign_gcs_poap

from client.client import EthAppClient
from client.status_word import StatusWord
from client.settings import SettingID, settings_toggle
from client.utils import TxType
from client.tx_simu import TxSimu


class TxCheckFlags(IntEnum):
    TX_CHECKS_ENABLE = 0x10
    TX_CHECKS_OPT_IN = 0x20


@pytest.fixture(name="config", params=["benign", "threat", "warning", "issue"])
def config_fixture(request) -> str:
    return request.param


def __common_setting_handling(device: Device,
                              navigator: Navigator,
                              app_client: EthAppClient,
                              expected: bool) -> None:
    """Common setting handling for the tests"""

    response = app_client.get_app_configuration()
    assert response.status == StatusWord.OK
    flags = response.data[0]
    if bool(flags & TxCheckFlags.TX_CHECKS_ENABLE) != expected:
        # Toggle the TRANSACTION_CHECKS setting
        settings_toggle(device, navigator, [SettingID.TRANSACTION_CHECKS])


def __get_simu_params(risk: str, simu_type: TxType) -> TxSimu:
    """Common simu parameters for the tests"""

    simu_params = {
        "tiny_url": "https://www.ledger.com",
        "simu_type": simu_type
    }

    if risk == "benign":
        simu_params["risk"] = 0
        simu_params["category"] = 0
    elif risk == "warning":
        simu_params["risk"] = 1
        simu_params["category"] = 4
    elif risk in ("threat", "issue"):
        simu_params["risk"] = 2
        simu_params["category"] = 2
    else:
        raise ValueError(f"Unknown risk value: {risk}")

    return TxSimu(**simu_params)


def __handle_simulation(app_client: EthAppClient, simu_params: TxSimu) -> None:
    if not simu_params.tx_hash:
        simu_params.tx_hash = bytes.fromhex("deadbeaf"*8)
    response = app_client.provide_tx_simulation(simu_params)
    assert response.status == StatusWord.OK


def test_tx_simulation_opt_in(backend: BackendInterface,
                              navigator: Navigator,
                              test_name: str,
                              default_screenshot_path: Path) -> None:
    """Test the TX Simulation Opt-In feature."""

    app_client = EthAppClient(backend)
    device = backend.device
    if device.is_nano:
        pytest.skip("Not yet supported on Nano")

    response = app_client.get_app_configuration()
    assert response.status == StatusWord.OK
    assert response.data[0] & TxCheckFlags.TX_CHECKS_OPT_IN == 0

    with app_client.opt_in_tx_simulation():
        navigator.navigate_and_compare(default_screenshot_path,
                                       test_name,
                                       [NavInsID.USE_CASE_CHOICE_CONFIRM])

    response = app_client.get_app_configuration()
    assert response.status == StatusWord.OK
    mask = TxCheckFlags.TX_CHECKS_OPT_IN | TxCheckFlags.TX_CHECKS_ENABLE
    assert response.data[0] & mask == mask


def test_tx_simulation_disabled(backend: BackendInterface, navigator: Navigator) -> None:
    """Test the TX Simulation APDU with the TRANSACTION_CHECKS setting disabled.
        It should return an error
    """

    app_client = EthAppClient(backend)
    device = backend.device
    if device.is_nano:
        pytest.skip("Not yet supported on Nano")

    __common_setting_handling(device, navigator, app_client, False)

    simu_params = __get_simu_params("benign", TxType.TRANSACTION)
    simu_params.chain_id = 5
    try:
        __handle_simulation(app_client, simu_params)
    except ExceptionRAPDU as err:
        assert err.status == StatusWord.NOT_IMPLEMENTED
    else:
        assert False  # An exception should have been raised


def test_tx_simulation_enabled(backend: BackendInterface, navigator: Navigator) -> None:
    """Test the TX Simulation APDU with the TRANSACTION_CHECKS setting enabled"""

    app_client = EthAppClient(backend)
    device = backend.device

    if device.is_nano:
        pytest.skip("Not yet supported on Nano")

    __common_setting_handling(device, navigator, app_client, True)

    simu_params = __get_simu_params("benign", TxType.TRANSACTION)
    simu_params.chain_id = 5
    __handle_simulation(app_client, simu_params)


def test_tx_simulation_sign(scenario_navigator: NavigateWithScenario, config: str) -> None:
    """Test the TX Simulation APDU with a simple transaction"""

    backend = scenario_navigator.backend
    app_client = EthAppClient(backend)
    device = backend.device

    if device.is_nano:
        pytest.skip("Not yet supported on Nano")

    __common_setting_handling(device, scenario_navigator.navigator, app_client, True)

    tx_params: dict = {
        "nonce": 21,
        "gasPrice": Web3.to_wei(13, 'gwei'),
        "gas": 21000,
        "to": bytes.fromhex("5a321744667052affa8386ed49e00ef223cbffc3"),
        "value": Web3.to_wei(1.22, "ether"),
        "chainId": 5
    }
    if config != "issue":
        simu_params = __get_simu_params(config, TxType.TRANSACTION)
        _, tx_hash = app_client.serialize_tx(tx_params)
        simu_params.chain_id = tx_params["chainId"]
        simu_params.tx_hash = tx_hash
        __handle_simulation(app_client, simu_params)

    sign_tx_common(scenario_navigator,
                   tx_params,
                   scenario_navigator.test_name + f"_{config}",
                   with_simu=config not in ("benign", "issue"))


def test_tx_simulation_no_simu(scenario_navigator: NavigateWithScenario) -> None:
    """Test the TX Transaction APDU without TX Simulation APDU
        but with the TRANSACTION_CHECKS setting enabled"""

    backend = scenario_navigator.backend
    app_client = EthAppClient(backend)
    device = backend.device

    if device.is_nano:
        pytest.skip("Not yet supported on Nano")

    __common_setting_handling(device, scenario_navigator.navigator, app_client, True)

    tx_params: dict = {
        "nonce": 21,
        "gasPrice": Web3.to_wei(13, 'gwei'),
        "gas": 21000,
        "to": bytes.fromhex("5a321744667052affa8386ed49e00ef223cbffc3"),
        "value": Web3.to_wei(1.22, "ether"),
        "chainId": 5
    }

    sign_tx_common(scenario_navigator,
                   tx_params,
                   scenario_navigator.test_name,
                   with_simu=False)


def test_tx_simulation_nft(scenario_navigator: NavigateWithScenario) -> None:
    """Test the TX Simulation APDU with a Plugin & NFT transaction"""

    backend = scenario_navigator.backend
    app_client = EthAppClient(backend)
    device = backend.device

    if device.is_nano:
        pytest.skip("Not yet supported on Nano")

    __common_setting_handling(device, scenario_navigator.navigator, app_client, True)

    simu_params = __get_simu_params("warning", TxType.TRANSACTION)

    common_test_nft(scenario_navigator,
                    scenario_navigator.test_name,
                    collecs_721[0],
                    actions_721[0],
                    False,
                    ERC721_PLUGIN,
                    simu_params)


def test_tx_simulation_blind_sign(scenario_navigator: NavigateWithScenario, config: str) -> None:
    """Test the TX Simulation APDU with a Blind Sign transaction"""

    backend = scenario_navigator.backend
    navigator = scenario_navigator.navigator
    app_client = EthAppClient(backend)
    device = backend.device

    if device.is_nano:
        pytest.skip("Not yet supported on Nano")

    __common_setting_handling(device, navigator, app_client, True)

    if config != "issue":
        simu_params = __get_simu_params(config, TxType.TRANSACTION)
    else:
        simu_params = None

    blind_sign(navigator,
               scenario_navigator,
               scenario_navigator.test_name + f"_{config}",
               False,
               0.0,
               simu_params)


def test_tx_simulation_eip191(scenario_navigator: NavigateWithScenario, config: str) -> None:
    """Test the TX Simulation APDU with a Message Streaming based on EIP191"""

    # TODO Re-activate when partners are ready for eip191
    pytest.skip("Skip until partners are ready for eip191")

    backend = scenario_navigator.backend
    app_client = EthAppClient(backend)
    device = backend.device

    if device.is_nano:
        pytest.skip("Not yet supported on Nano")
    if config in ("benign", "warning"):
        pytest.skip("Skipping useless tests")

    __common_setting_handling(device, scenario_navigator.navigator, app_client, True)

    test_name = scenario_navigator.test_name
    simu_params = __get_simu_params(config, TxType.PERSONAL_MESSAGE)
    msg = "Example `personal_sign` message with TX Simulation"
    if config == "issue":
        msg += " and wrong hash"
        simu_params.tx_hash = bytes.fromhex("deadbeaf"*8)
        test_name += f"_{config}"

    sign_eip191(scenario_navigator,
                test_name,
                msg,
                simu_params)


def test_tx_simulation_eip712(scenario_navigator: NavigateWithScenario) -> None:
    """Test the TX Simulation APDU with a Message Streaming based on EIP712"""

    app_client = EthAppClient(scenario_navigator.backend)

    if scenario_navigator.backend.device.is_nano:
        pytest.skip("Not yet supported on Nano")

    __common_setting_handling(scenario_navigator.backend.device, scenario_navigator.navigator, app_client, True)

    simu_params = __get_simu_params("threat", TxType.TYPED_DATA)

    sign_eip712(scenario_navigator, simu_params)


def test_tx_simulation_eip712_v0(scenario_navigator: NavigateWithScenario) -> None:
    """Test the TX Simulation APDU with a Message Streaming based on EIP712_v0"""

    app_client = EthAppClient(scenario_navigator.backend)

    if scenario_navigator.backend.device.is_nano:
        pytest.skip("Not yet supported on Nano")

    __common_setting_handling(scenario_navigator.backend.device, scenario_navigator.navigator, app_client, True)

    simu_params = __get_simu_params("threat", TxType.TYPED_DATA)

    sign_eip712_v0(scenario_navigator, simu_params)


def test_tx_simulation_gcs(navigator: Navigator,
                           scenario_navigator: NavigateWithScenario) -> None:
    """Test the TX Simulation APDU with a Message Streaming based on EIP712"""

    backend = scenario_navigator.backend
    app_client = EthAppClient(backend)
    device = backend.device

    if device.is_nano:
        pytest.skip("Not yet supported on Nano")

    __common_setting_handling(device, navigator, app_client, True)

    simu_params = __get_simu_params("warning", TxType.TRANSACTION)

    sign_gcs_poap(scenario_navigator, simu_params)
