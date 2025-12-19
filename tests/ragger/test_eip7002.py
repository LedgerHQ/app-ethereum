import struct

from ragger.navigator.navigation_scenario import NavigateWithScenario
from web3 import Web3

from client.client import EthAppClient
import client.response_parser as ResponseParser
from client.utils import recover_transaction


PUBKEY_LENGTH = 48
WITHDRAWAL_REQUEST_PREDEPLOY_ADDR = bytes.fromhex("00000961Ef480Eb55e80D19ad83579A64c007002")
BIP32_PATH = "m/44'/60'/0'/0/0"

def get_request_data(validator_pk: bytes, amount: float) -> bytes:
    data = bytearray()
    assert len(validator_pk) == PUBKEY_LENGTH
    data += validator_pk
    data += struct.pack(">Q", int(amount * pow(10,9)))
    return data

def common(scenario_navigator: NavigateWithScenario, data: bytes) -> None:
    app_client = EthAppClient(scenario_navigator.backend)
    with app_client.get_eth2_public_addr(display=False):
        pass
    tx_params = {
        "chainId": 1,
        "nonce": 27,
        "maxPriorityFeePerGas": Web3.to_wei(0.292821186, "gwei"),
        "maxFeePerGas": Web3.to_wei(0.292821186, "gwei"),
        "gas": 74265,
        "to": WITHDRAWAL_REQUEST_PREDEPLOY_ADDR,
        "value": Web3.to_wei(1.23035, "ether"),
        "data": data,
    }
    with app_client.sign(BIP32_PATH, tx_params):
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
