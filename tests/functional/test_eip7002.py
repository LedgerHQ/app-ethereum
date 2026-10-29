import struct

import client.response_parser as ResponseParser
from client.client import EthAppClient
from client.dynamic_networks import DynamicNetwork
from client.settings import SettingID, settings_toggle
from client.utils import recover_transaction
from dynamic_networks_cfg import get_network_config
from ragger.navigator.navigation_scenario import NavigateWithScenario
from web3 import Web3

PUBKEY_LENGTH = 48
WITHDRAWAL_REQUEST_PREDEPLOY_ADDR = bytes.fromhex("00000961Ef480Eb55e80D19ad83579A64c007002")
BIP32_PATH = "m/44'/60'/0'/0/0"
HOODI_CHAIN_ID = 560048
DEVNET_CHAIN_ID = 3151908


def get_request_data(validator_pk: bytes, amount: float) -> bytes:
    data = bytearray()
    assert len(validator_pk) == PUBKEY_LENGTH
    data += validator_pk
    data += struct.pack(">Q", int(amount * pow(10, 9)))
    return bytes(data)


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
        "maxPriorityFeePerGas": Web3.to_wei(0.292821186, "gwei"),
        "maxFeePerGas": Web3.to_wei(0.292821186, "gwei"),
        "gas": 74265,
        "to": WITHDRAWAL_REQUEST_PREDEPLOY_ADDR,
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


def test_eip7002_partial_withdrawal(scenario_navigator: NavigateWithScenario) -> None:
    validator_pk = "b8c426e9a7fa4dfa3ac1f20de1e151f35a6f30ca78944b9774ef8a25c954acce4570d876fc85f32698fbc7430c9fb9a4"
    common(scenario_navigator, get_request_data(bytes.fromhex(validator_pk), 13.37))


def test_eip7002_exit(scenario_navigator: NavigateWithScenario) -> None:
    validator_pk = "b8c426e9a7fa4dfa3ac1f20de1e151f35a6f30ca78944b9774ef8a25c954acce4570d876fc85f32698fbc7430c9fb9a4"
    common(scenario_navigator, get_request_data(bytes.fromhex(validator_pk), 0))


def test_eip7002_exit_hoodi(scenario_navigator: NavigateWithScenario) -> None:
    validator_pk = "b8c426e9a7fa4dfa3ac1f20de1e151f35a6f30ca78944b9774ef8a25c954acce4570d876fc85f32698fbc7430c9fb9a4"
    common(scenario_navigator, get_request_data(bytes.fromhex(validator_pk), 0), chain_id=HOODI_CHAIN_ID)


# Chain without the withdrawal predeploy: standard blind signing
def test_eip7002_exit_devnet(scenario_navigator: NavigateWithScenario) -> None:
    validator_pk = "b8c426e9a7fa4dfa3ac1f20de1e151f35a6f30ca78944b9774ef8a25c954acce4570d876fc85f32698fbc7430c9fb9a4"
    common(scenario_navigator, get_request_data(bytes.fromhex(validator_pk), 0), chain_id=DEVNET_CHAIN_ID, blind=True)
