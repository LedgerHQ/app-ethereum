from pathlib import Path
from enum import IntEnum
from web3 import Web3
import pytest

from ragger.error import ExceptionRAPDU
from ragger.backend import BackendInterface
from ragger.firmware import Firmware
from ragger.navigator import Navigator, NavInsID
from ragger.navigator.navigation_scenario import NavigateWithScenario

from test_sign import common as sign_tx_common
from test_blind_sign import test_blind_sign as blind_sign
from test_nft import common_test_nft, collecs_721, actions_721, ERC721_PLUGIN
from test_eip191 import common as sign_eip191
from test_eip712 import test_eip712_filtering_empty_array as sign_eip712
from test_eip712 import test_eip712_v0 as sign_eip712_v0
from test_gcs import test_gcs_poap as sign_gcs_poap

from client.client import EthAppClient, StatusWord
from client.settings import SettingID, settings_toggle
from client.tx_simu import SimuType, TxSimu


class W3CFlags(IntEnum):
    W3C_ENABLE = 0x10
    W3C_OPT_IN = 0x20


@pytest.fixture(name="config", params=["benign", "threat", "warning", "issue"])
def config_fixture(request) -> str:
    return request.param


def __common_setting_handling(firmware: Firmware,
                              navigator: Navigator,
                              app_client: EthAppClient,
                              expected: bool) -> None:
    """Common setting handling for the tests"""

    response = app_client.get_app_configuration()
    assert response.status == StatusWord.OK
    flags = response.data[0]
    if bool(flags & W3CFlags.W3C_ENABLE) != expected:
        # Toggle the WEB3_CHECK setting
        settings_toggle(firmware, navigator, [SettingID.WEB3_CHECK])


def __get_simu_params(risk: str, simu_type: SimuType) -> TxSimu:
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


def test_tx_simulation_opt_in(firmware: Firmware,
                              backend: BackendInterface,
                              navigator: Navigator,
                              test_name: str,
                              default_screenshot_path: Path) -> None:
    """Test the TX Simulation Opt-In feature."""

    if firmware.is_nano:
        pytest.skip("Not yet supported on Nano")

    app_client = EthAppClient(backend)

    response = app_client.get_app_configuration()
    assert response.status == StatusWord.OK
    assert response.data[0] & W3CFlags.W3C_OPT_IN == 0

    with app_client.opt_in_tx_simulation():
        navigator.navigate_and_compare(default_screenshot_path,
                                       test_name,
                                       [NavInsID.USE_CASE_CHOICE_CONFIRM])

    response = app_client.get_app_configuration()
    assert response.status == StatusWord.OK
    mask = W3CFlags.W3C_OPT_IN | W3CFlags.W3C_ENABLE
    assert response.data[0] & mask == mask


def test_tx_simulation_disabled(firmware: Firmware,
                                backend: BackendInterface,
                                navigator: Navigator) -> None:
    """Test the TX Simulation APDU with the WEB3_CHECK setting disabled.
        It should return an error
    """

    if firmware.is_nano:
        pytest.skip("Not yet supported on Nano")

    app_client = EthAppClient(backend)

    __common_setting_handling(firmware, navigator, app_client, False)

    simu_params = __get_simu_params("benign", SimuType.TRANSACTION)
    simu_params.chain_id = 5
    try:
        __handle_simulation(app_client, simu_params)
    except ExceptionRAPDU as err:
        assert err.status == StatusWord.NOT_IMPLEMENTED
    else:
        assert False  # An exception should have been raised


def test_tx_simulation_enabled(firmware: Firmware,
                               backend: BackendInterface,
                               navigator: Navigator) -> None:
    """Test the TX Simulation APDU with the WEB3_CHECK setting enabled"""

    if firmware.is_nano:
        pytest.skip("Not yet supported on Nano")

    app_client = EthAppClient(backend)

    __common_setting_handling(firmware, navigator, app_client, True)

    simu_params = __get_simu_params("benign", SimuType.TRANSACTION)
    simu_params.chain_id = 5
    __handle_simulation(app_client, simu_params)


def test_tx_simulation_sign(firmware: Firmware,
                            backend: BackendInterface,
                            navigator: Navigator,
                            scenario_navigator: NavigateWithScenario,
                            test_name: str,
                            default_screenshot_path: Path,
                            config: str) -> None:
    """Test the TX Simulation APDU with a simple transaction"""

    if firmware.is_nano:
        pytest.skip("Not yet supported on Nano")

    app_client = EthAppClient(backend)

    __common_setting_handling(firmware, navigator, app_client, True)

    tx_params: dict = {
        "nonce": 21,
        "gasPrice": Web3.to_wei(13, 'gwei'),
        "gas": 21000,
        "to": bytes.fromhex("5a321744667052affa8386ed49e00ef223cbffc3"),
        "value": Web3.to_wei(1.22, "ether"),
        "chainId": 5
    }
    if config != "issue":
        simu_params = __get_simu_params(config, SimuType.TRANSACTION)
        _, tx_hash = app_client.serialize_tx(tx_params)
        simu_params.chain_id = tx_params["chainId"]
        simu_params.tx_hash = tx_hash
        __handle_simulation(app_client, simu_params)

    sign_tx_common(firmware,
                   backend,
                   navigator,
                   scenario_navigator,
                   default_screenshot_path,
                   tx_params,
                   test_name=test_name + f"_{config}",
                   with_simu=config not in ("benign", "issue"))


def test_tx_simulation_no_simu(firmware: Firmware,
                               backend: BackendInterface,
                               navigator: Navigator,
                               scenario_navigator: NavigateWithScenario,
                               test_name: str,
                               default_screenshot_path: Path) -> None:
    """Test the TX Transaction APDU without TX Simulation APDU
        but with the WEB3_CHECK setting enabled"""

    if firmware.is_nano:
        pytest.skip("Not yet supported on Nano")

    app_client = EthAppClient(backend)

    __common_setting_handling(firmware, navigator, app_client, True)

    tx_params: dict = {
        "nonce": 21,
        "gasPrice": Web3.to_wei(13, 'gwei'),
        "gas": 21000,
        "to": bytes.fromhex("5a321744667052affa8386ed49e00ef223cbffc3"),
        "value": Web3.to_wei(1.22, "ether"),
        "chainId": 5
    }

    sign_tx_common(firmware,
                   backend,
                   navigator,
                   scenario_navigator,
                   default_screenshot_path,
                   tx_params,
                   test_name=test_name,
                   with_simu=False)


def test_tx_simulation_nft(firmware: Firmware,
                           backend: BackendInterface,
                           navigator: Navigator,
                           scenario_navigator: NavigateWithScenario,
                           default_screenshot_path: Path,
                           test_name: str) -> None:
    """Test the TX Simulation APDU with a Plugin & NFT transaction"""

    if firmware.is_nano:
        pytest.skip("Not yet supported on Nano")

    app_client = EthAppClient(backend)

    __common_setting_handling(firmware, navigator, app_client, True)

    simu_params = __get_simu_params("warning", SimuType.TRANSACTION)

    common_test_nft(firmware,
                    backend,
                    navigator,
                    scenario_navigator,
                    default_screenshot_path,
                    test_name,
                    collecs_721[0],
                    actions_721[0],
                    False,
                    ERC721_PLUGIN,
                    simu_params)


def test_tx_simulation_blind_sign(firmware: Firmware,
                                  backend: BackendInterface,
                                  navigator: Navigator,
                                  scenario_navigator: NavigateWithScenario,
                                  test_name: str,
                                  default_screenshot_path: Path,
                                  config: str) -> None:
    """Test the TX Simulation APDU with a Blind Sign transaction"""

    if firmware.is_nano:
        pytest.skip("Not yet supported on Nano")

    app_client = EthAppClient(backend)

    __common_setting_handling(firmware, navigator, app_client, True)

    if config != "issue":
        simu_params = __get_simu_params(config, SimuType.TRANSACTION)
    else:
        simu_params = None

    test_name += f"_{config}"
    blind_sign(firmware,
               backend,
               navigator,
               scenario_navigator,
               default_screenshot_path,
               test_name,
               False,
               0.0,
               simu_params)


def test_tx_simulation_eip191(firmware: Firmware,
                              backend: BackendInterface,
                              navigator: Navigator,
                              scenario_navigator: NavigateWithScenario,
                              test_name: str,
                              default_screenshot_path: Path,
                              config: str) -> None:
    """Test the TX Simulation APDU with a Message Streaming based on EIP191"""

    if firmware.is_nano:
        pytest.skip("Not yet supported on Nano")
    if config in ("benign", "warning"):
        pytest.skip("Skipping useless tests")

    # TODO Re-activate when partners are ready for eip191
    pytest.skip("Skip until partners are ready for eip191")

    app_client = EthAppClient(backend)

    __common_setting_handling(firmware, navigator, app_client, True)

    simu_params = __get_simu_params(config, SimuType.PERSONAL_MESSAGE)
    msg = "Example `personal_sign` message with TX Simulation"
    if config == "issue":
        msg += " and wrong hash"
        simu_params.tx_hash = bytes.fromhex("deadbeaf"*8)
        test_name += f"_{config}"

    sign_eip191(backend,
                navigator,
                scenario_navigator,
                default_screenshot_path,
                test_name,
                msg,
                simu_params)


def test_tx_simulation_eip712(firmware: Firmware,
                              backend: BackendInterface,
                              navigator: Navigator,
                              test_name: str,
                              default_screenshot_path: Path) -> None:
    """Test the TX Simulation APDU with a Message Streaming based on EIP712"""

    if firmware.is_nano:
        pytest.skip("Not yet supported on Nano")

    app_client = EthAppClient(backend)

    __common_setting_handling(firmware, navigator, app_client, True)

    simu_params = __get_simu_params("threat", SimuType.TYPED_DATA)

    sign_eip712(firmware,
                backend,
                navigator,
                default_screenshot_path,
                test_name,
                False,
                simu_params)


def test_tx_simulation_eip712_v0(firmware: Firmware,
                                 backend: BackendInterface,
                                 navigator: Navigator) -> None:
    """Test the TX Simulation APDU with a Message Streaming based on EIP712_v0"""

    if firmware.is_nano:
        pytest.skip("Not yet supported on Nano")

    app_client = EthAppClient(backend)

    __common_setting_handling(firmware, navigator, app_client, True)

    simu_params = __get_simu_params("threat", SimuType.TYPED_DATA)

    sign_eip712_v0(firmware,
                   backend,
                   navigator,
                   simu_params)


def test_tx_simulation_gcs(firmware: Firmware,
                           backend: BackendInterface,
                           navigator: Navigator,
                           scenario_navigator: NavigateWithScenario,
                           test_name: str,
                           default_screenshot_path: Path) -> None:
    """Test the TX Simulation APDU with a Message Streaming based on EIP712"""

    if firmware.is_nano:
        pytest.skip("Not yet supported on Nano")

    app_client = EthAppClient(backend)

    __common_setting_handling(firmware, navigator, app_client, True)

    simu_params = __get_simu_params("warning", SimuType.TRANSACTION)

    sign_gcs_poap(firmware,
                  backend,
                  navigator,
                  scenario_navigator,
                  test_name,
                  default_screenshot_path,
                  simu_params)
