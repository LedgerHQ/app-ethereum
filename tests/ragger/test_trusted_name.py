from typing import Optional
import pytest
from web3 import Web3

from ragger.backend import BackendInterface
from ragger.firmware import Firmware
from ragger.error import ExceptionRAPDU
from ragger.navigator import Navigator
from ragger.navigator.navigation_scenario import NavigateWithScenario

import client.response_parser as ResponseParser
from client.client import EthAppClient, StatusWord, TrustedNameType, TrustedNameSource
from client.settings import SettingID, settings_toggle


# Values used across all tests
CHAIN_ID = 1
NAME = "ledger.eth"
ADDR = bytes.fromhex("0011223344556677889900112233445566778899")
KEY_ID = 1
ALGO_ID = 1
BIP32_PATH = "m/44'/60'/0'/0/0"
NONCE = 21
GAS_PRICE = 13
GAS_LIMIT = 21000
AMOUNT = 1.22


@pytest.fixture(name="verbose", params=[False, True])
def verbose_fixture(request) -> bool:
    return request.param


def common(firmware: Firmware, app_client: EthAppClient, get_challenge: bool = True) -> Optional[int]:

    if firmware == Firmware.NANOS:
        pytest.skip("Not supported on LNS")

    if get_challenge:
        challenge = app_client.get_challenge()
        return ResponseParser.challenge(challenge.data)
    return None


def test_trusted_name_v1(firmware: Firmware,
                         backend: BackendInterface,
                         navigator: Navigator,
                         scenario_navigator: NavigateWithScenario,
                         verbose: bool,
                         test_name: str):
    app_client = EthAppClient(backend)
    challenge = common(firmware, app_client)

    if verbose:
        settings_toggle(firmware, navigator, [SettingID.VERBOSE_ENS])
        test_name += "_verbose"

    app_client.provide_trusted_name_v1(ADDR, NAME, challenge)

    with app_client.sign(BIP32_PATH,
                         {
                             "nonce": NONCE,
                             "gasPrice": Web3.to_wei(GAS_PRICE, "gwei"),
                             "gas": GAS_LIMIT,
                             "to": ADDR,
                             "value": Web3.to_wei(AMOUNT, "ether"),
                             "chainId": CHAIN_ID
                         }):
        if firmware.is_nano:
            end_text = "Accept"
        else:
            end_text = "Sign"

        scenario_navigator.review_approve(test_name=test_name, custom_screen_text=end_text)


def test_trusted_name_v1_wrong_challenge(firmware: Firmware, backend: BackendInterface):
    app_client = EthAppClient(backend)
    challenge = common(firmware, app_client)

    with pytest.raises(ExceptionRAPDU) as e:
        app_client.provide_trusted_name_v1(ADDR, NAME, ~challenge & 0xffffffff)
    assert e.value.status == StatusWord.INVALID_DATA


def test_trusted_name_v1_wrong_addr(firmware: Firmware,
                                    backend: BackendInterface,
                                    scenario_navigator: NavigateWithScenario,
                                    test_name: str):
    app_client = EthAppClient(backend)
    challenge = common(firmware, app_client)

    app_client.provide_trusted_name_v1(ADDR, NAME, challenge)

    addr = bytearray(ADDR)
    addr.reverse()

    with app_client.sign(BIP32_PATH,
                         {
                             "nonce": NONCE,
                             "gasPrice": Web3.to_wei(GAS_PRICE, "gwei"),
                             "gas": GAS_LIMIT,
                             "to": bytes(addr),
                             "value": Web3.to_wei(AMOUNT, "ether"),
                             "chainId": CHAIN_ID
                         }):
        if firmware.is_nano:
            end_text = "Accept"
        else:
            end_text = "Sign"

        scenario_navigator.review_approve(test_name=test_name, custom_screen_text=end_text)


def test_trusted_name_v1_non_mainnet(firmware: Firmware,
                                     backend: BackendInterface,
                                     scenario_navigator: NavigateWithScenario,
                                     test_name: str):
    app_client = EthAppClient(backend)
    challenge = common(firmware, app_client)

    chain_id = 5
    # pylint: disable=line-too-long
    icon = "400040000195000093001f8b08000000000002ff85ceb10dc4300805d01fb970e9113c4a467346cb288ce092223ace80382b892ef9cd93ac0f0678881c4616d980b4bb99aa0801a5874d844ff695b5d7f6c23ad79058f79c8df7e8c5dc7d9fff13ffc61d71d7bcf32549bcef5672c5a430bb1cd6073f68c3cd3d302cd3feea88f547d6a99eb8e87ecbcdecd255b8f869033cfd932feae0c09000020000"
    # pylint: enable=line-too-long
    app_client.provide_network_information("Goerli",
                                           "ETH",
                                           chain_id,
                                           bytes.fromhex(icon))

    app_client.provide_trusted_name_v1(ADDR, NAME, challenge)

    with app_client.sign(BIP32_PATH,
                         {
                             "nonce": NONCE,
                             "gasPrice": Web3.to_wei(GAS_PRICE, "gwei"),
                             "gas": GAS_LIMIT,
                             "to": ADDR,
                             "value": Web3.to_wei(AMOUNT, "ether"),
                             "chainId": chain_id
                         }):
        if firmware.is_nano:
            end_text = "Accept"
        else:
            end_text = "Sign"

        scenario_navigator.review_approve(test_name=test_name, custom_screen_text=end_text)


def test_trusted_name_v1_unknown_chain(firmware: Firmware,
                                       backend: BackendInterface,
                                       scenario_navigator: NavigateWithScenario,
                                       test_name: str):
    app_client = EthAppClient(backend)
    challenge = common(firmware, app_client)

    app_client.provide_trusted_name_v1(ADDR, NAME, challenge)

    with app_client.sign(BIP32_PATH,
                         {
                             "nonce": NONCE,
                             "gasPrice": Web3.to_wei(GAS_PRICE, "gwei"),
                             "gas": GAS_LIMIT,
                             "to": ADDR,
                             "value": Web3.to_wei(AMOUNT, "ether"),
                             "chainId": 9
                         }):
        if firmware.is_nano:
            end_text = "Accept"
        else:
            end_text = "Sign"

        scenario_navigator.review_approve(test_name=test_name, custom_screen_text=end_text)


def test_trusted_name_v1_name_too_long(firmware: Firmware, backend: BackendInterface):
    app_client = EthAppClient(backend)
    challenge = common(firmware, app_client)

    with pytest.raises(ExceptionRAPDU) as e:
        app_client.provide_trusted_name_v1(ADDR, "ledger" + "0"*25 + ".eth", challenge)
    assert e.value.status == StatusWord.INVALID_DATA


def test_trusted_name_v1_name_invalid_character(firmware: Firmware, backend: BackendInterface):
    app_client = EthAppClient(backend)
    challenge = common(firmware, app_client)

    with pytest.raises(ExceptionRAPDU) as e:
        app_client.provide_trusted_name_v1(ADDR, "l\xe8dger.eth", challenge)
    assert e.value.status == StatusWord.INVALID_DATA


def test_trusted_name_v1_uppercase(firmware: Firmware, backend: BackendInterface):
    app_client = EthAppClient(backend)
    challenge = common(firmware, app_client)

    with pytest.raises(ExceptionRAPDU) as e:
        app_client.provide_trusted_name_v1(ADDR, NAME.upper(), challenge)
    assert e.value.status == StatusWord.INVALID_DATA


def test_trusted_name_v1_name_non_ens(firmware: Firmware, backend: BackendInterface):
    app_client = EthAppClient(backend)
    challenge = common(firmware, app_client)

    with pytest.raises(ExceptionRAPDU) as e:
        app_client.provide_trusted_name_v1(ADDR, "ledger.hte", challenge)
    assert e.value.status == StatusWord.INVALID_DATA


def test_trusted_name_v2(firmware: Firmware,
                         backend: BackendInterface,
                         scenario_navigator: NavigateWithScenario,
                         test_name: str):
    app_client = EthAppClient(backend)
    challenge = common(firmware, app_client)

    app_client.provide_trusted_name_v2(ADDR,
                                       NAME,
                                       TrustedNameType.ACCOUNT,
                                       TrustedNameSource.ENS,
                                       CHAIN_ID,
                                       challenge=challenge)

    with app_client.sign(BIP32_PATH,
                         {
                             "nonce": NONCE,
                             "gasPrice": Web3.to_wei(GAS_PRICE, "gwei"),
                             "gas": GAS_LIMIT,
                             "to": ADDR,
                             "value": Web3.to_wei(AMOUNT, "ether"),
                             "chainId": CHAIN_ID
                         }):
        if firmware.is_nano:
            end_text = "Accept"
        else:
            end_text = "Sign"

        scenario_navigator.review_approve(test_name=test_name, custom_screen_text=end_text)


def test_trusted_name_v2_wrong_chainid(firmware: Firmware,
                                       backend: BackendInterface,
                                       scenario_navigator: NavigateWithScenario,
                                       test_name: str):
    app_client = EthAppClient(backend)
    challenge = common(firmware, app_client)

    app_client.provide_trusted_name_v2(ADDR,
                                       NAME,
                                       TrustedNameType.ACCOUNT,
                                       TrustedNameSource.ENS,
                                       CHAIN_ID,
                                       challenge=challenge)

    with app_client.sign(BIP32_PATH,
                         {
                             "nonce": NONCE,
                             "gasPrice": Web3.to_wei(GAS_PRICE, "gwei"),
                             "gas": GAS_LIMIT,
                             "to": ADDR,
                             "value": Web3.to_wei(AMOUNT, "ether"),
                             "chainId": CHAIN_ID + 1,
                         }):
        if firmware.is_nano:
            end_text = "Accept"
        else:
            end_text = "Sign"

        scenario_navigator.review_approve(test_name=test_name, custom_screen_text=end_text)


def test_trusted_name_v2_missing_challenge(firmware: Firmware, backend: BackendInterface):
    app_client = EthAppClient(backend)
    common(firmware, app_client, False)

    with pytest.raises(ExceptionRAPDU) as e:
        app_client.provide_trusted_name_v2(ADDR,
                                           NAME,
                                           TrustedNameType.ACCOUNT,
                                           TrustedNameSource.ENS,
                                           CHAIN_ID)
    assert e.value.status == StatusWord.INVALID_DATA


def test_trusted_name_v2_expired(firmware: Firmware, backend: BackendInterface, app_version: tuple[int, int, int]):
    app_client = EthAppClient(backend)
    challenge = common(firmware, app_client)

    # convert to list and reverse
    app_version = list(app_version)
    app_version.reverse()
    # simulate a previous version number by decrementing the first non-zero value
    for idx, v in enumerate(app_version):
        if v > 0:
            app_version[idx] -= 1
            break
    # reverse and convert back
    app_version.reverse()
    app_version = tuple(app_version)

    with pytest.raises(ExceptionRAPDU) as e:
        app_client.provide_trusted_name_v2(ADDR,
                                           NAME,
                                           TrustedNameType.ACCOUNT,
                                           TrustedNameSource.ENS,
                                           CHAIN_ID,
                                           challenge=challenge,
                                           not_valid_after=app_version)
    assert e.value.status == StatusWord.INVALID_DATA
