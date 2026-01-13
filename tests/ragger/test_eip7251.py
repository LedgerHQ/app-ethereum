from ragger.navigator.navigation_scenario import NavigateWithScenario
from web3 import Web3

from client.client import EthAppClient
import client.response_parser as ResponseParser
from client.utils import recover_transaction


PUBKEY_LENGTH = 48
CONSOLIDATION_REQUEST_PREDEPLOY_ADDR = bytes.fromhex("0000BBdDc7CE488642fb579F8B00f3a590007251")
BIP32_PATH = "m/44'/60'/0'/0/0"

def get_request_data(source_pk: bytes, target_pk: bytes) -> bytes:
    assert len(source_pk) == PUBKEY_LENGTH
    assert len(target_pk) == PUBKEY_LENGTH
    return source_pk + target_pk

def common(scenario_navigator: NavigateWithScenario, data: bytes) -> None:
    app_client = EthAppClient(scenario_navigator.backend)
    tx_params = {
        "chainId": 1,
        "nonce": 27,
        "maxPriorityFeePerGas": Web3.to_wei(2.5, "gwei"),
        "maxFeePerGas": Web3.to_wei(2.565, "gwei"),
        "gas": 200000,
        "to": CONSOLIDATION_REQUEST_PREDEPLOY_ADDR,
        "value": 1,
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


def test_eip7251_consolidate(scenario_navigator: NavigateWithScenario) -> None:
    source_pk = "b26add12e8fd4bfc463ba29ed255afb478a1391bb39b1f5a97cfbd8300391ec9387b126e9cb3eaafdd910a7c6b1eeb72"
    target_pk = "a5565b3a6a3818346d4610a6c771b45f55ba94e1cceb6525648b148c40cfafcc93611b6acf28ac5408860188ca01cff6"
    common(scenario_navigator, get_request_data(bytes.fromhex(source_pk), bytes.fromhex(target_pk)))


def test_eip7251_compound(scenario_navigator: NavigateWithScenario) -> None:
    validator_pk = "a09a2c66b3cc6253bd83e2d3aa698fcc760c8d03b4782870e0b2638dffefcc7bc9e053215b004faa6e15ac76cac10bc2"
    common(scenario_navigator, get_request_data(bytes.fromhex(validator_pk), bytes.fromhex(validator_pk)))
