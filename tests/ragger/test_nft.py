from typing import Callable, Optional, Any
import json
import pytest
from web3 import Web3

from ragger.error import ExceptionRAPDU
from ragger.firmware import Firmware
from ragger.backend import BackendInterface
from ragger.navigator.navigation_scenario import NavigateWithScenario

from constants import ABIS_FOLDER

from client.client import EthAppClient, StatusWord
import client.response_parser as ResponseParser
from client.utils import get_selector_from_data, recover_transaction


BIP32_PATH = "m/44'/60'/0'/0/0"
NONCE = 21
GAS_PRICE = 13
GAS_LIMIT = 21000
FROM = bytes.fromhex("1122334455667788990011223344556677889900")
TO = bytes.fromhex("0099887766554433221100998877665544332211")
NFTS = [(1, 3), (5, 2), (7, 4)]  # tuples of (token_id, amount)
DATA = "Some data".encode()
DEVICE_ADDR: Optional[bytes] = None


class NFTCollection:
    addr: bytes
    name: str
    chain_id: int

    def __init__(self, addr: bytes, name: str, chain_id: int, contract):
        self.addr = addr
        self.name = name
        self.chain_id = chain_id
        self.contract = contract


class Action:
    fn_name: str
    fn_args: list[Any]

    def __init__(self, fn_name: str, fn_args: list[Any]):
        self.fn_name = fn_name
        self.fn_args = fn_args


def common_test_nft(firmware: Firmware,
                    backend: BackendInterface,
                    scenario_navigator: NavigateWithScenario,
                    collec: NFTCollection,
                    action: Action,
                    reject: bool,
                    plugin_name: str):
    global DEVICE_ADDR
    app_client = EthAppClient(backend)

    if firmware == Firmware.NANOS:
        pytest.skip("Not supported on LNS")

    if collec.chain_id == 5:
        name = "Goerli"
        ticker = "ETH"
        # pylint: disable=line-too-long
        icon = "400040000195000093001f8b08000000000002ff85ceb10dc4300805d01fb970e9113c4a467346cb288ce092223ace80382b892ef9cd93ac0f0678881c4616d980b4bb99aa0801a5874d844ff695b5d7f6c23ad79058f79c8df7e8c5dc7d9fff13ffc61d71d7bcf32549bcef5672c5a430bb1cd6073f68c3cd3d302cd3feea88f547d6a99eb8e87ecbcdecd255b8f869033cfd932feae0c09000020000"
        # pylint: enable=line-too-long
    elif collec.chain_id == 137:
        name = "Polygon"
        ticker = "POL"
        # pylint: disable=line-too-long
        icon = "400040002188010086011f8b08000000000002ffbd55416ac33010943018422f0e855c42fee01e4b09f94a7a0c3df82b3905440ffd4243a1507ae81b4c1e9047180a2604779d58f6ae7664e8250b0ec613ad7646b3aba6b949fc9a2eee8f105f7bdc14083e653d3e1f4d6f4c82f0ed809b3780e70c5f6ab8a6cf8f1b474175a41a2f8db1d7b47b7a3b2276c9506881d8cd87d7bb10afd8a2356048ec12bfe90130cc59d12d9585166f05ffdcb363295b863f21bb54662bfe85cb8ca522402bec2a92ad8d336936e9b50416e19a55e000f837d2d2a2e3f79a7d39f78aec938eb9bf549ae91378fa16a118ca982ea3febe46aa18ca90f59c14ce1c21fbd3c744e087cc582a921e7bf9552a4a761fb1461f39c5114f75f1b9732f5117499f484f813ec84386416f6f75a30b17689fd519ef3cd6f3b83114c300a7dd66b129a276d3a3a3d20209cdc09ce1fac039c50480738e09b8471dc116c18e1a18d6784eb7453bb773ee19cff9d29b37c3f744cdfc0dedc7ee0968dff7487fe97b0acf8bf3c3b48be236f772f307b129c97400080000"
        # pylint: enable=line-too-long
    else:
        name = ""

    if name:
        app_client.provide_network_information(name, ticker, collec.chain_id, bytes.fromhex(icon))

    if DEVICE_ADDR is None:  # to only have to request it once
        with app_client.get_public_addr(display=False):
            pass
        _, DEVICE_ADDR, _ = ResponseParser.pk_addr(app_client.response().data)

    data = collec.contract.encode_abi(action.fn_name, action.fn_args)
    app_client.set_plugin(plugin_name,
                          collec.addr,
                          get_selector_from_data(data),
                          collec.chain_id)

    app_client.provide_nft_metadata(collec.name, collec.addr, collec.chain_id)
    tx_params = {
        "nonce": NONCE,
        "gasPrice": Web3.to_wei(GAS_PRICE, "gwei"),
        "gas": GAS_LIMIT,
        "to": collec.addr,
        "value": 0,
        "chainId": collec.chain_id,
        "data": data,
    }
    with app_client.sign(BIP32_PATH, tx_params):
        test_name  = f"{plugin_name.lower()}_{action.fn_name}_{str(collec.chain_id)}"
        if reject:
            test_name += "-rejected"
            scenario_navigator.review_reject(test_name=test_name)
        else:
            if firmware.is_nano:
                end_text = "Accept"
            else:
                end_text = "Sign"
            scenario_navigator.review_approve(test_name=test_name, custom_screen_text=end_text)

    # verify signature
    vrs = ResponseParser.signature(app_client.response().data)
    addr = recover_transaction(tx_params, vrs)
    assert addr == DEVICE_ADDR


def common_test_nft_reject(test_fn: Callable,
                           firmware: Firmware,
                           backend: BackendInterface,
                           scenario_navigator: NavigateWithScenario,
                           collec: NFTCollection,
                           action: Action):
    with pytest.raises(ExceptionRAPDU) as e:
        test_fn(firmware, backend, scenario_navigator, collec, action, True)
    assert e.value.status == StatusWord.CONDITION_NOT_SATISFIED

# ERC-721


ERC721_PLUGIN = "ERC721"

with open(f"{ABIS_FOLDER}/erc721.json", encoding="utf-8") as file:
    contract_erc721 = Web3().eth.contract(
        abi=json.load(file),
        address=bytes(20)
    )

collecs_721 = [
    NFTCollection(bytes.fromhex("bc4ca0eda7647a8ab7c2061c2e118a18a936f13d"),
                  "Bored Ape Yacht Club",
                  1,
                  contract_erc721),
    NFTCollection(bytes.fromhex("670fd103b1a08628e9557cd66b87ded841115190"),
                  "y00ts",
                  137,
                  contract_erc721),
    NFTCollection(bytes.fromhex("2909cf13e458a576cdd9aab6bd6617051a92dacf"),
                  "goerlirocks",
                  5,
                  contract_erc721),
]
actions_721 = [
    Action("safeTransferFrom", [FROM, TO, NFTS[0][0], DATA]),
    Action("safeTransferFrom", [FROM, TO, NFTS[0][0]]),
    Action("transferFrom", [FROM, TO, NFTS[0][0]]),
    Action("approve", [TO, NFTS[0][0]]),
    Action("setApprovalForAll", [TO, False]),
]


@pytest.fixture(name="collec_721", params=collecs_721)
def collec_721_fixture(request) -> NFTCollection:
    return request.param


@pytest.fixture(name="action_721", params=actions_721)
def action_721_fixture(request) -> Action:
    return request.param


def test_erc721(firmware: Firmware,
                backend: BackendInterface,
                scenario_navigator: NavigateWithScenario,
                collec_721: NFTCollection,
                action_721: Action,
                reject: bool = False):
    common_test_nft(firmware,
                    backend,
                    scenario_navigator,
                    collec_721,
                    action_721,
                    reject,
                    ERC721_PLUGIN)


def test_erc721_reject(firmware: Firmware,
                       backend: BackendInterface,
                       scenario_navigator: NavigateWithScenario):
    common_test_nft_reject(test_erc721,
                           firmware,
                           backend,
                           scenario_navigator,
                           collecs_721[0],
                           actions_721[0])


# ERC-1155

ERC1155_PLUGIN = "ERC1155"

with open(f"{ABIS_FOLDER}/erc1155.json", encoding="utf-8") as file:
    contract_erc1155 = Web3().eth.contract(
        abi=json.load(file),
        address=bytes(20)
    )


collecs_1155 = [
    NFTCollection(bytes.fromhex("495f947276749ce646f68ac8c248420045cb7b5e"),
                  "OpenSea Shared Storefront",
                  1,
                  contract_erc1155),
    NFTCollection(bytes.fromhex("2953399124f0cbb46d2cbacd8a89cf0599974963"),
                  "OpenSea Collections",
                  137,
                  contract_erc1155),
    NFTCollection(bytes.fromhex("f4910c763ed4e47a585e2d34baa9a4b611ae448c"),
                  "OpenSea Collections",
                  5,
                  contract_erc1155),
]
actions_1155 = [
    Action("safeTransferFrom", [FROM, TO, NFTS[0][0], NFTS[0][1], DATA]),
    Action("safeBatchTransferFrom",
           [
               FROM,
               TO,
               list(map(lambda nft: nft[0], NFTS)),
               list(map(lambda nft: nft[1], NFTS)),
               DATA
           ]),
    Action("setApprovalForAll", [TO, False]),
]


@pytest.fixture(name="collec_1155", params=collecs_1155)
def collec_1155_fixture(request) -> bool:
    return request.param


@pytest.fixture(name="action_1155", params=actions_1155)
def action_1155_fixture(request) -> Action:
    return request.param


def test_erc1155(firmware: Firmware,
                 backend: BackendInterface,
                 scenario_navigator: NavigateWithScenario,
                 collec_1155: NFTCollection,
                 action_1155: Action,
                 reject: bool = False):
    common_test_nft(firmware,
                    backend,
                    scenario_navigator,
                    collec_1155,
                    action_1155,
                    reject,
                    ERC1155_PLUGIN)


def test_erc1155_reject(firmware: Firmware,
                        backend: BackendInterface,
                        scenario_navigator: NavigateWithScenario):
    common_test_nft_reject(test_erc1155,
                           firmware,
                           backend,
                           scenario_navigator,
                           collecs_1155[0],
                           actions_1155[0])
