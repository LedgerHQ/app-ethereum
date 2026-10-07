import hashlib
import json
from pathlib import Path

import client.response_parser as ResponseParser
from client.client import EthAppClient
from client.status_word import StatusWord
from client.utils import recover_transaction
from constants import ABIS_FOLDER
from ragger.navigator.navigation_scenario import NavigateWithScenario
from web3 import Web3

BEACON_DEPOSIT_CONTRACT_ADDR = bytes.fromhex("00000000219ab540356cBB839Cbe05303d7705Fa")
BLS_WITHDRAWAL_PREFIX = 0x00
WEI_PER_GWEI = 10**9


def _sha256(*parts: bytes) -> bytes:
    digest = hashlib.sha256()
    for part in parts:
        digest.update(part)
    return digest.digest()


def deposit_data_root(pubkey: bytes, credentials: bytes, amount_gwei: int, signature: bytes) -> bytes:
    """SSZ hash_tree_root of the DepositData container, recomputed by the app."""
    pubkey_root = _sha256(pubkey, bytes(64 - len(pubkey)))
    signature_root = _sha256(_sha256(signature[:64]), _sha256(signature[64:], bytes(32)))
    return _sha256(
        _sha256(pubkey_root, credentials),
        _sha256(amount_gwei.to_bytes(8, "little") + bytes(24), signature_root),
    )


BIP32_PATH = "m/44'/60'/0'/0/0"


def test_eth2_deposit(scenario_navigator: NavigateWithScenario) -> None:
    app_client = EthAppClient(scenario_navigator.backend)
    with app_client.get_eth2_public_addr(display=False):
        pass
    with Path(f"{ABIS_FOLDER}/beacon_deposit.abi.json").open(encoding="utf-8") as f:
        contract = Web3().eth.contract(  # type: ignore[call-overload]  # web3 stubs reject raw-bytes addresses reused as contract_addr
            abi=json.load(f),
            address=bytes.fromhex("23F8abfC2824C397cCB3DA89ae772984107dDB99"),
        )
    # https://github.com/ethereum/consensus-specs/blob/master/specs/phase0/validator.md#withdrawal-credentials
    credentials = bytearray(hashlib.sha256(app_client.response().data).digest())
    credentials[0] = BLS_WITHDRAWAL_PREFIX
    pubkey = bytes.fromhex("a377e13e3b146513c0c9dd5231ced86a21597e5b83fa83ac8c27c4620f180c151d3e709107d73257fa451c58149e4065")
    signature = bytes.fromhex("00") * 96
    # The root must commit to the fields the device is shown, otherwise the
    # plugin refuses the deposit
    root = deposit_data_root(pubkey, bytes(credentials), Web3.to_wei(1.23035, "ether") // WEI_PER_GWEI, signature)
    data = contract.encode_abi(
        "deposit",
        [
            pubkey,
            credentials,
            signature,
            root,
        ],
    )
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


def test_eth2_deposit_non_default_withdrawal_index(scenario_navigator: NavigateWithScenario) -> None:
    """The withdrawal key index used to validate the credentials must be on screen.

    That index comes from INS_SET_ETH2_WITHDRAWAL_INDEX, an unauthenticated command with
    no user confirmation, and it decides which withdrawal key can later claim the stake.
    A host pre-seeding it must not be able to do so invisibly.
    """
    app_client = EthAppClient(scenario_navigator.backend)
    index = 5

    response = app_client.set_eth2_withdrawal_index(index)
    assert response.status == StatusWord.SWO_SUCCESS

    # Credentials for the withdrawal key the device will derive at that index
    with app_client.get_eth2_public_addr(display=False, bip32_path=f"m/12381/3600/{index}/0"):
        pass
    credentials = bytearray(hashlib.sha256(app_client.response().data).digest())
    credentials[0] = BLS_WITHDRAWAL_PREFIX

    with Path(f"{ABIS_FOLDER}/beacon_deposit.abi.json").open(encoding="utf-8") as f:
        contract = Web3().eth.contract(  # type: ignore[call-overload]  # web3 stubs reject raw-bytes addresses reused as contract_addr
            abi=json.load(f),
            address=bytes.fromhex("23F8abfC2824C397cCB3DA89ae772984107dDB99"),
        )
    pubkey = bytes.fromhex("a377e13e3b146513c0c9dd5231ced86a21597e5b83fa83ac8c27c4620f180c151d3e709107d73257fa451c58149e4065")
    signature = bytes.fromhex("00") * 96
    root = deposit_data_root(pubkey, bytes(credentials), Web3.to_wei(1.23035, "ether") // WEI_PER_GWEI, signature)
    tx_params = {
        "chainId": 1,
        "nonce": 27,
        "maxPriorityFeePerGas": Web3.to_wei(0.292821186, "gwei"),
        "maxFeePerGas": Web3.to_wei(0.292821186, "gwei"),
        "gas": 74265,
        "to": BEACON_DEPOSIT_CONTRACT_ADDR,
        "value": Web3.to_wei(1.23035, "ether"),
        "data": contract.encode_abi("deposit", [pubkey, credentials, signature, root]),
    }
    with app_client.sign(BIP32_PATH, tx_params):
        scenario_navigator.review_approve()
