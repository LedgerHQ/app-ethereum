import pytest
from pathlib import Path
from typing import Callable
from ragger.error import ExceptionRAPDU
from ragger.firmware import Firmware
from ragger.backend import BackendInterface
from ragger.navigator import Navigator, NavInsID
from ledger_app_clients.ethereum.client import EthAppClient, TxData, StatusWord
from ledger_app_clients.ethereum.settings import SettingID, settings_toggle
from eth_utils import function_signature_to_4byte_selector
import struct


ROOT_SCREENSHOT_PATH = Path(__file__).parent

BIP32_PATH = "m/44'/60'/0'/0/0"
NONCE = 21
GAS_PRICE = 13
GAS_LIMIT = 21000
FROM = bytes.fromhex("1122334455667788990011223344556677889900")
TO = bytes.fromhex("0099887766554433221100998877665544332211")
NFTS = [ (1, 3), (5, 2), (7, 4) ] # tuples of (token_id, amount)
DATA = "Some data".encode()

class   NFTCollection:
    addr: bytes
    name: str
    chain_id: int
    def __init__(self, addr: bytes, name: str, chain_id: int):
        self.addr = addr
        self.name = name
        self.chain_id = chain_id

class   Action:
    fn: str
    data_fn: Callable
    nav_fn: Callable
    def __init__(self, fn: str, data_fn: Callable, nav_fn: Callable):
        self.fn = fn
        self.data_fn = data_fn
        self.nav_fn = nav_fn

def common_nav_nft(is_nano: bool, nano_steps: int, stax_steps: int, reject: bool) -> list[NavInsID]:
    moves = list()
    if is_nano:
        moves += [ NavInsID.RIGHT_CLICK ] * nano_steps
        if reject:
            moves += [ NavInsID.RIGHT_CLICK ]
        moves += [ NavInsID.BOTH_CLICK ]
    else:
        moves += [ NavInsID.USE_CASE_REVIEW_TAP ] * stax_steps
        if reject:
            moves += [
                NavInsID.USE_CASE_REVIEW_REJECT,
                NavInsID.USE_CASE_CHOICE_CONFIRM
            ]
        else:
            moves += [ NavInsID.USE_CASE_REVIEW_CONFIRM ]
    return moves

def snapshot_test_name(nft_type: str, fn: str, chain_id: int, reject: bool) -> str:
    name = "%s_%s_%s" % (nft_type, fn.split("(")[0], str(chain_id))
    if reject:
        name += "-rejected"
    return name

def common_test_nft(fw: Firmware,
                    back: BackendInterface,
                    nav: Navigator,
                    collec: NFTCollection,
                    action: Action,
                    reject: bool,
                    plugin_name: str):
    app_client = EthAppClient(back)
    selector = function_signature_to_4byte_selector(action.fn)

    if app_client._client.firmware.name == "nanos":
        pytest.skip("Not supported on LNS")
    with app_client.set_plugin(plugin_name,
                               collec.addr,
                               selector,
                               1):
        pass
    with app_client.provide_nft_metadata(collec.name, collec.addr, collec.chain_id):
        pass
    with app_client.sign_legacy(BIP32_PATH,
                                NONCE,
                                GAS_PRICE,
                                GAS_LIMIT,
                                collec.addr,
                                0,
                                collec.chain_id,
                                action.data_fn(action)):
        nav.navigate_and_compare(ROOT_SCREENSHOT_PATH,
                                 snapshot_test_name(plugin_name.lower(),
                                                    action.fn,
                                                    collec.chain_id,
                                                    reject),
                                 action.nav_fn(fw.is_nano,
                                               collec.chain_id,
                                               reject))

def common_test_nft_reject(test_fn: Callable,
                           fw: Firmware,
                           back: BackendInterface,
                           nav: Navigator,
                           collec: NFTCollection,
                           action: Action):
    try:
        test_fn(fw, back, nav, collec, action, True)
    except ExceptionRAPDU as e:
        assert e.status == StatusWord.CONDITION_NOT_SATISFIED
    else:
        assert False # An exception should have been raised

# ERC-721

ERC721_PLUGIN = "ERC721"
ERC721_SAFE_TRANSFER_FROM_DATA = "safeTransferFrom(address,address,uint256,bytes)"
ERC721_SAFE_TRANSFER_FROM = "safeTransferFrom(address,address,uint256)"
ERC721_TRANSFER_FROM = "transferFrom(address,address,uint256)"
ERC721_APPROVE = "approve(address,uint256)"
ERC721_SET_APPROVAL_FOR_ALL = "setApprovalForAll(address,bool)"

## data formatting functions

def data_erc721_transfer_from(action: Action) -> TxData:
    return TxData(
        function_signature_to_4byte_selector(action.fn),
        [
            FROM,
            TO,
            struct.pack(">H", NFTS[0][0])
        ]
    )

def data_erc721_safe_transfer_from_data(action: Action) -> TxData:
    txd = data_erc721_transfer_from(action)
    txd.parameters += [ DATA ]
    return txd

def data_erc721_approve(action: Action) -> TxData:
    return TxData(
        function_signature_to_4byte_selector(action.fn),
        [
            TO,
            struct.pack(">H", NFTS[0][0])
        ]
    )

def data_erc721_set_approval_for_all(action: Action) -> TxData:
    return TxData(
        function_signature_to_4byte_selector(action.fn),
        [
            TO,
            struct.pack("b", False)
        ]
    )

## ui nav functions

def nav_erc721_transfer_from(is_nano: bool,
                             chain_id: int,
                             reject: bool) -> list[NavInsID]:
    nano_steps = 7
    stax_steps = 3
    if chain_id != 1:
        nano_steps += 1
        stax_steps += 1
    return common_nav_nft(is_nano, nano_steps, stax_steps, reject)

def nav_erc721_approve(is_nano: bool,
                       chain_id: int,
                       reject: bool) -> list[NavInsID]:
    nano_steps = 7
    stax_steps = 3
    if chain_id != 1:
        nano_steps += 1
        stax_steps += 1
    return common_nav_nft(is_nano, nano_steps, stax_steps, reject)

def nav_erc721_set_approval_for_all(is_nano: bool,
                                    chain_id: int,
                                    reject: bool) -> list[NavInsID]:
    nano_steps = 6
    if chain_id != 1:
        nano_steps += 1
    return common_nav_nft(is_nano, nano_steps, 3, reject)

collecs_721 = [
    NFTCollection(bytes.fromhex("bc4ca0eda7647a8ab7c2061c2e118a18a936f13d"),
        "Bored Ape Yacht Club",
        1),
    NFTCollection(bytes.fromhex("670fd103b1a08628e9557cd66b87ded841115190"),
        "y00ts",
        137),
    NFTCollection(bytes.fromhex("2909cf13e458a576cdd9aab6bd6617051a92dacf"),
        "goerlirocks",
        5)
]
actions_721 = [
    Action(ERC721_SAFE_TRANSFER_FROM_DATA,
           data_erc721_safe_transfer_from_data,
           nav_erc721_transfer_from),
    Action(ERC721_SAFE_TRANSFER_FROM,
           data_erc721_transfer_from,
           nav_erc721_transfer_from),
    Action(ERC721_TRANSFER_FROM,
           data_erc721_transfer_from,
           nav_erc721_transfer_from),
    Action(ERC721_APPROVE,
           data_erc721_approve,
           nav_erc721_approve),
    Action(ERC721_SET_APPROVAL_FOR_ALL,
           data_erc721_set_approval_for_all,
           nav_erc721_set_approval_for_all)
]


@pytest.fixture(params=collecs_721)
def collec_721(request) -> NFTCollection:
    return request.param
@pytest.fixture(params=actions_721)
def action_721(request) -> Action:
    return request.param

def test_erc721(firmware: Firmware,
                 backend: BackendInterface,
                 navigator: Navigator,
                 collec_721: NFTCollection,
                 action_721: Action,
                 reject: bool = False):
    common_test_nft(firmware,
                    backend,
                    navigator,
                    collec_721,
                    action_721,
                    reject,
                    ERC721_PLUGIN)

def test_erc721_reject(firmware: Firmware,
                        backend: BackendInterface,
                        navigator: Navigator):
    common_test_nft_reject(test_erc721,
                           firmware,
                           backend,
                           navigator,
                           collecs_721[0],
                           actions_721[0])

# ERC-1155

ERC1155_PLUGIN = "ERC1155"
ERC1155_SAFE_TRANSFER_FROM = "safeTransferFrom(address,address,uint256,uint256,bytes)"
ERC1155_SAFE_BATCH_TRANSFER_FROM = "safeBatchTransferFrom(address,address,uint256[],uint256[],bytes)"
ERC1155_SET_APPROVAL_FOR_ALL = "setApprovalForAll(address,bool)"

## data formatting functions

def data_erc1155_safe_transfer_from(action: Action) -> TxData:
    return TxData(
        function_signature_to_4byte_selector(action.fn),
        [
            FROM,
            TO,
            struct.pack(">H", NFTS[0][0]),
            struct.pack(">H", NFTS[0][1]),
            DATA
        ]
    )

def data_erc1155_safe_batch_transfer_from(action: Action) -> TxData:
    data = TxData(
        function_signature_to_4byte_selector(action.fn),
        [
            FROM,
            TO
        ])
    data.parameters += [ int(32 * 4).to_bytes(8, "big") ] # token_ids offset
    data.parameters += [int(32 * (4 + len(NFTS) + 1)).to_bytes(8, "big") ] # amounts offset
    data.parameters += [ int(len(NFTS)).to_bytes(8, "big") ] # token_ids length
    for nft in NFTS:
        data.parameters += [ struct.pack(">H", nft[0]) ] # token_id
    data.parameters += [ int(len(NFTS)).to_bytes(8, "big") ] # amounts length
    for nft in NFTS:
        data.parameters += [ struct.pack(">H", nft[1]) ] # amount
    return data

def data_erc1155_set_approval_for_all(action: Action) -> TxData:
    return TxData(
        function_signature_to_4byte_selector(action.fn),
        [
            TO,
            struct.pack("b", False)
        ]
    )

## ui nav functions

def nav_erc1155_safe_transfer_from(is_nano: bool,
                                   chain_id: int,
                                   reject: bool) -> list:
    nano_steps = 8
    if chain_id != 1:
        nano_steps += 1
    return common_nav_nft(is_nano, nano_steps, 4, reject)

def nav_erc1155_safe_batch_transfer_from(is_nano: bool,
                                         chain_id: int,
                                         reject: bool) -> list:
    nano_steps = 7
    stax_steps = 3
    if chain_id != 1:
        nano_steps += 1
        stax_steps += 1
    return common_nav_nft(is_nano, nano_steps, stax_steps, reject)

def nav_erc1155_set_approval_for_all(is_nano: bool,
                                     chain_id: int,
                                     reject: bool) -> list:
    nano_steps = 6
    if chain_id != 1:
        nano_steps += 1
    return common_nav_nft(is_nano, nano_steps, 3, reject)

collecs_1155 = [
    NFTCollection(bytes.fromhex("495f947276749ce646f68ac8c248420045cb7b5e"),
        "OpenSea Shared Storefront",
        1),
    NFTCollection(bytes.fromhex("2953399124f0cbb46d2cbacd8a89cf0599974963"),
        "OpenSea Collections",
        137),
    NFTCollection(bytes.fromhex("f4910c763ed4e47a585e2d34baa9a4b611ae448c"),
        "OpenSea Collections",
        5)
]
actions_1155 = [
    Action(ERC1155_SAFE_TRANSFER_FROM,
           data_erc1155_safe_transfer_from,
           nav_erc1155_safe_transfer_from),
    Action(ERC1155_SAFE_BATCH_TRANSFER_FROM,
           data_erc1155_safe_batch_transfer_from,
           nav_erc1155_safe_batch_transfer_from),
    Action(ERC1155_SET_APPROVAL_FOR_ALL,
           data_erc1155_set_approval_for_all,
           nav_erc1155_set_approval_for_all)
]
@pytest.fixture(params=collecs_1155)
def collec_1155(request) -> bool:
    return request.param
@pytest.fixture(params=actions_1155)
def action_1155(request) -> Action:
    return request.param

def test_erc1155(firmware: Firmware,
                 backend: BackendInterface,
                 navigator: Navigator,
                 collec_1155: NFTCollection,
                 action_1155: Action,
                 reject: bool = False):
    common_test_nft(firmware,
                    backend,
                    navigator,
                    collec_1155,
                    action_1155,
                    reject,
                    ERC1155_PLUGIN)

def test_erc1155_reject(firmware: Firmware,
                        backend: BackendInterface,
                        navigator: Navigator):
    common_test_nft_reject(test_erc1155,
                           firmware,
                           backend,
                           navigator,
                           collecs_1155[0],
                           actions_1155[0])
