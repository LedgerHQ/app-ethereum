import client.response_parser as ResponseParser
from client.client import EthAppClient
from client.dynamic_networks import DynamicNetwork
from client.settings import SettingID, settings_toggle
from client.utils import recover_transaction
from dynamic_networks_cfg import get_network_config
from ragger.navigator.navigation_scenario import NavigateWithScenario
from web3 import Web3

PUBKEY_LENGTH = 48
CONSOLIDATION_REQUEST_PREDEPLOY_ADDR = bytes.fromhex("0000BBdDc7CE488642fb579F8B00f3a590007251")
BIP32_PATH = "m/44'/60'/0'/0/0"
HOODI_CHAIN_ID = 560048
DEVNET_CHAIN_ID = 3151908


def get_request_data(source_pk: bytes, target_pk: bytes) -> bytes:
    assert len(source_pk) == PUBKEY_LENGTH
    assert len(target_pk) == PUBKEY_LENGTH
    return source_pk + target_pk


def common(scenario_navigator: NavigateWithScenario, data: bytes, chain_id: int = 1, blind: bool = False) -> None:
    app_client = EthAppClient(scenario_navigator.backend)
    if blind:
        settings_toggle(scenario_navigator.backend.device, scenario_navigator.navigator, [SettingID.BLIND_SIGNING])
    if chain_id == HOODI_CHAIN_ID:
        name, ticker, icon = get_network_config(scenario_navigator.backend.device.type, HOODI_CHAIN_ID)
        app_client.provide_network_info(DynamicNetwork(name, ticker, HOODI_CHAIN_ID, icon))
    tx_params = {
        "chainId": chain_id,
        "nonce": 27,
        "maxPriorityFeePerGas": Web3.to_wei(2.5, "gwei"),
        "maxFeePerGas": Web3.to_wei(2.565, "gwei"),
        "gas": 200000,
        "to": CONSOLIDATION_REQUEST_PREDEPLOY_ADDR,
        "value": 1,
        "data": data,
    }
    with app_client.sign(BIP32_PATH, tx_params):
        if blind:
            scenario_navigator.review_approve_with_warning()
        else:
            scenario_navigator.review_approve()

    # verify signature
    vrs = ResponseParser.signature(app_client.response().data)

    with app_client.get_public_addr(bip32_path=BIP32_PATH, display=False):
        pass
    _, device_addr, _ = ResponseParser.pk_addr(app_client.response().data)

    addr = recover_transaction(tx_params, vrs)
    assert addr == device_addr


def test_eip7251_consolidate(scenario_navigator: NavigateWithScenario) -> None:
    source_pk = "b26add12e8fd4bfc463ba29ed255afb478a1391bb39b1f5a97cfbd8300391ec9387b126e9cb3eaafdd910a7c6b1eeb72"
    target_pk = "a5565b3a6a3818346d4610a6c771b45f55ba94e1cceb6525648b148c40cfafcc93611b6acf28ac5408860188ca01cff6"
    common(
        scenario_navigator,
        get_request_data(bytes.fromhex(source_pk), bytes.fromhex(target_pk)),
    )


def test_eip7251_compound(scenario_navigator: NavigateWithScenario) -> None:
    validator_pk = "a09a2c66b3cc6253bd83e2d3aa698fcc760c8d03b4782870e0b2638dffefcc7bc9e053215b004faa6e15ac76cac10bc2"
    common(
        scenario_navigator,
        get_request_data(bytes.fromhex(validator_pk), bytes.fromhex(validator_pk)),
    )


def test_eip7251_consolidate_hoodi(scenario_navigator: NavigateWithScenario) -> None:
    source_pk = "b26add12e8fd4bfc463ba29ed255afb478a1391bb39b1f5a97cfbd8300391ec9387b126e9cb3eaafdd910a7c6b1eeb72"
    target_pk = "a5565b3a6a3818346d4610a6c771b45f55ba94e1cceb6525648b148c40cfafcc93611b6acf28ac5408860188ca01cff6"
    common(
        scenario_navigator,
        get_request_data(bytes.fromhex(source_pk), bytes.fromhex(target_pk)),
        chain_id=HOODI_CHAIN_ID,
    )


# Chain without the consolidation predeploy: standard blind signing
def test_eip7251_consolidate_devnet(scenario_navigator: NavigateWithScenario) -> None:
    source_pk = "b26add12e8fd4bfc463ba29ed255afb478a1391bb39b1f5a97cfbd8300391ec9387b126e9cb3eaafdd910a7c6b1eeb72"
    target_pk = "a5565b3a6a3818346d4610a6c771b45f55ba94e1cceb6525648b148c40cfafcc93611b6acf28ac5408860188ca01cff6"
    common(
        scenario_navigator,
        get_request_data(bytes.fromhex(source_pk), bytes.fromhex(target_pk)),
        chain_id=DEVNET_CHAIN_ID,
        blind=True,
    )
