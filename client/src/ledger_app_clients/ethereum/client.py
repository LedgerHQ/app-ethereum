import struct
from enum import IntEnum
from typing import Optional
import rlp
from web3 import Web3
from eth_utils import keccak

from ragger.backend import BackendInterface
from ragger.utils import RAPDU

from .command_builder import CommandBuilder
from .eip712 import EIP712FieldType
from .keychain import sign_data, Key
from .tlv import format_tlv, FieldTag
from .response_parser import pk_addr
from .tx_simu import TxSimu
from .tx_auth_7702 import TxAuth7702
from .status_word import StatusWord
from .ledger_pki import PKIClient, PKIPubKeyUsage
from .dynamic_networks import DynamicNetwork
from .safe import SafeAccount, AccountType


class TrustedNameType(IntEnum):
    ACCOUNT = 0x01
    CONTRACT = 0x02
    NFT = 0x03


class TrustedNameSource(IntEnum):
    LAB = 0x00
    CAL = 0x01
    ENS = 0x02
    UD = 0x03
    FN = 0x04
    DNS = 0x05


class SignMode(IntEnum):
    BASIC = 0x00
    STORE = 0x01
    START_FLOW = 0x02


class EthAppClient:
    def __init__(self, backend: BackendInterface):
        self._backend = backend
        self.device = backend.device
        self._cmd_builder = CommandBuilder()
        self.pki_client = PKIClient(self._backend)

    def _exchange_async(self, payload: bytes):
        return self._backend.exchange_async_raw(payload)

    def _exchange(self, payload: bytes):
        return self._backend.exchange_raw(payload)

    def response(self) -> Optional[RAPDU]:
        return self._backend.last_async_response

    def send_raw(self, cla: int, ins: int, p1: int, p2: int, payload: bytes):
        header = bytearray()
        header.append(cla)
        header.append(ins)
        header.append(p1)
        header.append(p2)
        header.append(len(payload))
        return self._exchange(header + payload)

    def send_raw_async(self, cla: int, ins: int, p1: int, p2: int, payload: bytes):
        header = bytearray()
        header.append(cla)
        header.append(ins)
        header.append(p1)
        header.append(p2)
        header.append(len(payload))
        return self._exchange_async(header + payload)

    def get_app_configuration(self):
        return self._exchange(self._cmd_builder.get_app_configuration())

    def eip712_send_struct_def_struct_name(self, name: str):
        return self._exchange_async(self._cmd_builder.eip712_send_struct_def_struct_name(name))

    def eip712_send_struct_def_struct_field(self,
                                            field_type: EIP712FieldType,
                                            type_name: str,
                                            type_size: int,
                                            array_levels: list,
                                            key_name: str):
        return self._exchange_async(self._cmd_builder.eip712_send_struct_def_struct_field(
                          field_type,
                          type_name,
                          type_size,
                          array_levels,
                          key_name))

    def eip712_send_struct_impl_root_struct(self, name: str):
        return self._exchange_async(self._cmd_builder.eip712_send_struct_impl_root_struct(name))

    def eip712_send_struct_impl_array(self, size: int):
        return self._exchange_async(self._cmd_builder.eip712_send_struct_impl_array(size))

    def eip712_send_struct_impl_struct_field(self, raw_value: bytes):
        chunks = self._cmd_builder.eip712_send_struct_impl_struct_field(bytearray(raw_value))
        for chunk in chunks[:-1]:
            self._exchange(chunk)
        return self._exchange_async(chunks[-1])

    def eip712_sign_new(self, bip32_path: str):
        return self._exchange_async(self._cmd_builder.eip712_sign_new(bip32_path))

    def eip712_sign_legacy(self,
                           bip32_path: str,
                           domain_hash: bytes,
                           message_hash: bytes):
        return self._exchange_async(self._cmd_builder.eip712_sign_legacy(bip32_path,
                                                                         domain_hash,
                                                                         message_hash))

    def eip712_filtering_activate(self):
        return self._exchange_async(self._cmd_builder.eip712_filtering_activate())

    def eip712_filtering_discarded_path(self, path: str):
        return self._exchange(self._cmd_builder.eip712_filtering_discarded_path(path))

    def eip712_filtering_message_info(self, name: str, filters_count: int, sig: bytes):
        return self._exchange_async(self._cmd_builder.eip712_filtering_message_info(name,
                                                                                    filters_count,
                                                                                    sig))

    def eip712_filtering_amount_join_token(self, token_idx: int, sig: bytes, discarded: bool):
        return self._exchange_async(self._cmd_builder.eip712_filtering_amount_join_token(token_idx,
                                                                                         sig,
                                                                                         discarded))

    def eip712_filtering_amount_join_value(self, token_idx: int, name: str, sig: bytes, discarded: bool):
        return self._exchange_async(self._cmd_builder.eip712_filtering_amount_join_value(token_idx,
                                                                                         name,
                                                                                         sig,
                                                                                         discarded))

    def eip712_filtering_datetime(self, name: str, sig: bytes, discarded: bool):
        return self._exchange_async(self._cmd_builder.eip712_filtering_datetime(name, sig, discarded))

    def eip712_filtering_trusted_name(self,
                                      name: str,
                                      name_type: list[int],
                                      name_source: list[int],
                                      sig: bytes,
                                      discarded: bool):
        return self._exchange_async(self._cmd_builder.eip712_filtering_trusted_name(name,
                                                                                    name_type,
                                                                                    name_source,
                                                                                    sig,
                                                                                    discarded))

    def eip712_filtering_raw(self, name: str, sig: bytes, discarded: bool):
        return self._exchange_async(self._cmd_builder.eip712_filtering_raw(name, sig, discarded))

    def serialize_tx(self, tx_params: dict) -> tuple[bytes, bytes]:
        """Computes the serialized TX and its hash"""

        tx = Web3().eth.account.create().sign_transaction(tx_params).raw_transaction
        prefix = bytes()
        suffix = []
        if tx[0] in [0x01, 0x02, 0x04]:
            prefix = tx[:1]
            tx = tx[len(prefix):]
        else:  # legacy
            if "chainId" in tx_params:
                suffix = [int(tx_params["chainId"]), bytes(), bytes()]
        decoded_tx = rlp.decode(tx)[:-3]  # remove already computed signature
        encoded_tx = prefix + rlp.encode(decoded_tx + suffix)
        tx_hash = keccak(encoded_tx)
        return encoded_tx, tx_hash

    def sign(self,
             bip32_path: str,
             tx_params: dict,
             mode: SignMode = SignMode.BASIC):
        tx, _ = self.serialize_tx(tx_params)
        chunks = self._cmd_builder.sign(bip32_path, tx, mode)
        for chunk in chunks[:-1]:
            self._exchange(chunk)
        return self._exchange_async(chunks[-1])

    def get_challenge(self):
        return self._exchange(self._cmd_builder.get_challenge())

    def get_public_addr(self,
                        display: bool = True,
                        chaincode: bool = False,
                        bip32_path: str = "m/44'/60'/0'/0/0",
                        chain_id: Optional[int] = None):
        return self._exchange_async(self._cmd_builder.get_public_addr(display,
                                                                      chaincode,
                                                                      bip32_path,
                                                                      chain_id))

    def get_eth2_public_addr(self,
                             display: bool = True,
                             bip32_path: str = "m/12381/3600/0/0"):
        return self._exchange_async(self._cmd_builder.get_eth2_public_addr(display,
                                                                           bip32_path))

    def perform_privacy_operation(self,
                                  display: bool = True,
                                  bip32_path: str = "m/44'/60'/0'/0/0",
                                  pubkey: bytes = bytes()):
        return self._exchange(self._cmd_builder.perform_privacy_operation(display,
                                                                          bip32_path,
                                                                          pubkey))

    def _provide_trusted_name_common(self, payload: bytes, name_source: TrustedNameSource) -> RAPDU:
        payload += format_tlv(FieldTag.STRUCT_TYPE, 3)  # TrustedName
        if name_source == TrustedNameSource.CAL:
            key_id = 9
            key = Key.CAL
        else:
            key_id = 7
            key = Key.TRUSTED_NAME

        self.pki_client.send_certificate(PKIPubKeyUsage.PUBKEY_USAGE_TRUSTED_NAME, name_source == TrustedNameSource.CAL)

        payload += format_tlv(FieldTag.SIGNER_KEY_ID, key_id)  # test key
        payload += format_tlv(FieldTag.SIGNER_ALGO, 1)  # secp256k1
        payload += format_tlv(FieldTag.DER_SIGNATURE,
                              sign_data(key, payload))
        chunks = self._cmd_builder.provide_trusted_name(payload)
        for chunk in chunks[:-1]:
            self._exchange(chunk)
        return self._exchange(chunks[-1])

    def provide_trusted_name_v1(self, addr: bytes, name: str, challenge: int) -> RAPDU:
        payload = format_tlv(FieldTag.STRUCT_VERSION, 1)
        payload += format_tlv(FieldTag.CHALLENGE, challenge)
        payload += format_tlv(FieldTag.COIN_TYPE, 0x3c)  # ETH in slip-44
        payload += format_tlv(FieldTag.TRUSTED_NAME, name)
        payload += format_tlv(FieldTag.ADDRESS, addr)
        return self._provide_trusted_name_common(payload, TrustedNameSource.ENS)

    def provide_trusted_name_v2(self,
                                addr: bytes,
                                name: str,
                                name_type: TrustedNameType,
                                name_source: TrustedNameSource,
                                chain_id: int,
                                nft_id: Optional[int] = None,
                                challenge: Optional[int] = None,
                                not_valid_after: Optional[tuple[int, int, int]] = None) -> RAPDU:
        payload = format_tlv(FieldTag.STRUCT_VERSION, 2)
        payload += format_tlv(FieldTag.TRUSTED_NAME, name)
        payload += format_tlv(FieldTag.ADDRESS, addr)
        payload += format_tlv(FieldTag.TRUSTED_NAME_TYPE, name_type)
        payload += format_tlv(FieldTag.TRUSTED_NAME_SOURCE, name_source)
        payload += format_tlv(FieldTag.CHAIN_ID, chain_id)
        if nft_id is not None:
            payload += format_tlv(FieldTag.TRUSTED_NAME_NFT_ID, nft_id)
        if challenge is not None:
            payload += format_tlv(FieldTag.CHALLENGE, challenge)
        if not_valid_after is not None:
            assert len(not_valid_after) == 3
            payload += format_tlv(FieldTag.NOT_VALID_AFTER, struct.pack("BBB", *not_valid_after))
        return self._provide_trusted_name_common(payload, name_source)

    def set_plugin(self,
                   plugin_name: str,
                   contract_addr: bytes,
                   selector: bytes,
                   chain_id: int,
                   type_: int = 1,
                   version: int = 1,
                   key_id: int = 2,
                   algo_id: int = 1,
                   sig: Optional[bytes] = None) -> RAPDU:

        # Send ledgerPKI certificate
        self.pki_client.send_certificate(PKIPubKeyUsage.PUBKEY_USAGE_PLUGIN_METADATA)

        if sig is None:
            # Temporarily get a command with an empty signature to extract the payload and
            # compute the signature on it
            tmp = self._cmd_builder.set_plugin(type_,
                                               version,
                                               plugin_name,
                                               contract_addr,
                                               selector,
                                               chain_id,
                                               key_id,
                                               algo_id,
                                               bytes())
            # skip APDU header & empty sig
            sig = sign_data(Key.SET_PLUGIN, tmp[5:-1])
        return self._exchange(self._cmd_builder.set_plugin(type_,
                                                           version,
                                                           plugin_name,
                                                           contract_addr,
                                                           selector,
                                                           chain_id,
                                                           key_id,
                                                           algo_id,
                                                           sig))

    def provide_nft_metadata(self,
                             collection: str,
                             addr: bytes,
                             chain_id: int,
                             type_: int = 1,
                             version: int = 1,
                             key_id: int = 1,
                             algo_id: int = 1,
                             sig: Optional[bytes] = None) -> RAPDU:

        # Send ledgerPKI certificate
        self.pki_client.send_certificate(PKIPubKeyUsage.PUBKEY_USAGE_NFT_METADATA)

        if sig is None:
            # Temporarily get a command with an empty signature to extract the payload and
            # compute the signature on it
            tmp = self._cmd_builder.provide_nft_information(type_,
                                                            version,
                                                            collection,
                                                            addr,
                                                            chain_id,
                                                            key_id,
                                                            algo_id,
                                                            bytes())
            # skip APDU header & empty sig
            sig = sign_data(Key.NFT, tmp[5:-1])
        return self._exchange(self._cmd_builder.provide_nft_information(type_,
                                                                        version,
                                                                        collection,
                                                                        addr,
                                                                        chain_id,
                                                                        key_id,
                                                                        algo_id,
                                                                        sig))

    def set_external_plugin(self,
                            plugin_name: str,
                            contract_address: bytes,
                            method_selelector: bytes,
                            sig: Optional[bytes] = None) -> RAPDU:

        # Send ledgerPKI certificate
        self.pki_client.send_certificate(PKIPubKeyUsage.PUBKEY_USAGE_COIN_META)

        if sig is None:
            # Temporarily get a command with an empty signature to extract the payload and
            # compute the signature on it
            tmp = self._cmd_builder.set_external_plugin(plugin_name, contract_address, method_selelector, bytes())

            # skip APDU header & empty sig
            sig = sign_data(Key.CAL, tmp[5:])
        return self._exchange(self._cmd_builder.set_external_plugin(plugin_name,
                                                                    contract_address,
                                                                    method_selelector,
                                                                    sig))

    def personal_sign(self, path: str, msg: bytes):
        chunks = self._cmd_builder.personal_sign(path, msg)
        for chunk in chunks[:-1]:
            self._exchange(chunk)
        return self._exchange_async(chunks[-1])

    def provide_token_metadata(self,
                               ticker: str,
                               addr: bytes,
                               decimals: int,
                               chain_id: int,
                               sig: Optional[bytes] = None) -> RAPDU:

        # Send ledgerPKI certificate
        self.pki_client.send_certificate(PKIPubKeyUsage.PUBKEY_USAGE_COIN_META)

        if sig is None:
            # Temporarily get a command with an empty signature to extract the payload and
            # compute the signature on it
            tmp = self._cmd_builder.provide_erc20_token_information(ticker,
                                                                    addr,
                                                                    decimals,
                                                                    chain_id,
                                                                    bytes())
            # skip APDU header & empty sig
            sig = sign_data(Key.CAL, tmp[6:])
        return self._exchange(self._cmd_builder.provide_erc20_token_information(ticker,
                                                                                addr,
                                                                                decimals,
                                                                                chain_id,
                                                                                sig))

    def provide_network_information(self, network_params: DynamicNetwork) -> None:
        # Send ledgerPKI certificate
        self.pki_client.send_certificate(PKIPubKeyUsage.PUBKEY_USAGE_NETWORK)

        # Add the network info
        chunks = self._cmd_builder.provide_network_information(network_params.serialize(),
                                                               network_params.icon)
        for chunk in chunks[:-1]:
            response = self._exchange(chunk)
            assert response.status == StatusWord.OK
        response = self._exchange(chunks[-1])
        assert response.status == StatusWord.OK

    def provide_enum_value(self, payload: bytes) -> RAPDU:
        # Send ledgerPKI certificate
        self.pki_client.send_certificate(PKIPubKeyUsage.PUBKEY_USAGE_CALLDATA)

        chunks = self._cmd_builder.provide_enum_value(payload)
        for chunk in chunks[:-1]:
            self._exchange(chunk)
        return self._exchange(chunks[-1])

    def provide_transaction_info(self, payload: bytes) -> RAPDU:
        # Send ledgerPKI certificate
        self.pki_client.send_certificate(PKIPubKeyUsage.PUBKEY_USAGE_CALLDATA)

        chunks = self._cmd_builder.provide_transaction_info(payload)
        for chunk in chunks[:-1]:
            self._exchange(chunk)
        return self._exchange(chunks[-1])

    def opt_in_tx_simulation(self):
        return self._exchange_async(self._cmd_builder.opt_in_tx_simulation())

    def provide_tx_simulation(self, simu_params: TxSimu) -> RAPDU:
        # Send ledgerPKI certificate
        self.pki_client.send_certificate(PKIPubKeyUsage.PUBKEY_USAGE_TX_SIMU_SIGNER)

        if not simu_params.from_addr:
            with self.get_public_addr(False):
                pass
            response = self.response()
            assert response
            _, from_addr, _ = pk_addr(response.data)
            simu_params.from_addr = from_addr

        chunks = self._cmd_builder.provide_tx_simulation(simu_params.serialize())
        for chunk in chunks[:-1]:
            self._exchange(chunk)
        return self._exchange(chunks[-1])

    def provide_proxy_info(self, payload: bytes) -> RAPDU:
        # Send ledgerPKI certificate
        self.pki_client.send_certificate(PKIPubKeyUsage.PUBKEY_USAGE_TRUSTED_NAME)

        chunks = self._cmd_builder.provide_proxy_info(payload)
        for chunk in chunks[:-1]:
            self._exchange(chunk)
        return self._exchange(chunks[-1])

    def sign_eip7702_authorization(self, bip32_path: str, auth_params: TxAuth7702):
        chunks = self._cmd_builder.sign_eip7702_authorization(bip32_path, auth_params.serialize())
        for chunk in chunks[:-1]:
            self._exchange(chunk)
        return self._exchange_async(chunks[-1])

    def provide_safe_account(self, safe_params: SafeAccount):
        # Send ledgerPKI certificate - only for SAFE accounts
        if safe_params.account_type == AccountType.SAFE:
            self.pki_client.send_certificate(PKIPubKeyUsage.PUBKEY_USAGE_SAFE_ACCOUNT)

        chunks = self._cmd_builder.provide_safe_account(safe_params.serialize(), safe_params.account_type)
        for chunk in chunks[:-1]:
            self._exchange(chunk)
        return self._exchange_async(chunks[-1])
