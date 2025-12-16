import hashlib
import json
from pathlib import Path

from ragger.navigator.navigation_scenario import NavigateWithScenario
from web3 import Web3

from client.client import EthAppClient
import client.response_parser as ResponseParser
from client.utils import recover_transaction
from constants import ABIS_FOLDER


BEACON_DEPOSIT_CONTRACT_ADDR = bytes.fromhex("00000000219ab540356cBB839Cbe05303d7705Fa")
BLS_WITHDRAWAL_PREFIX = 0x00
BIP32_PATH = "m/44'/60'/0'/0/0"

def test_eth2_deposit(scenario_navigator: NavigateWithScenario) -> None:
    app_client = EthAppClient(scenario_navigator.backend)
    with app_client.get_eth2_public_addr(display=False):
        pass
    with Path(f"{ABIS_FOLDER}/beacon_deposit.abi.json").open() as f:
        contract = Web3().eth.contract(
            abi=json.load(f),
            address=bytes.fromhex("23F8abfC2824C397cCB3DA89ae772984107dDB99"),
        )
    # https://github.com/ethereum/consensus-specs/blob/master/specs/phase0/validator.md#withdrawal-credentials
    credentials = bytearray(hashlib.sha256(app_client.response().data).digest())
    credentials[0] = BLS_WITHDRAWAL_PREFIX
    data = contract.encode_abi("deposit", [
        bytes.fromhex("a377e13e3b146513c0c9dd5231ced86a21597e5b83fa83ac8c27c4620f180c151d3e709107d73257fa451c58149e4065"),
        credentials,
        bytes.fromhex("00") * 96,
        bytes.fromhex("4f1f479ababcef72faa5f9acf66faec651d143bc76f3880d0a25b72557bfd70a"),
    ])
    tx_params = {
        "chainId": 1,
        "nonce": 27,
        "maxPriorityFeePerGas": Web3.to_wei(0.292821186, "gwei"),
        "maxFeePerGas": Web3.to_wei(0.292821186, "gwei"),
        "gas": 74265,
        "to": BEACON_DEPOSIT_CONTRACT_ADDR,
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
