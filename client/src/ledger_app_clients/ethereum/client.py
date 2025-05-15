import struct
from enum import IntEnum
from typing import Optional
from hashlib import sha256
import rlp
from web3 import Web3
from eth_utils import keccak

from ragger.backend import BackendInterface
from ragger.firmware import Firmware
from ragger.error import ExceptionRAPDU
from ragger.utils import RAPDU

from .command_builder import CommandBuilder
from .eip712 import EIP712FieldType
from .keychain import sign_data, Key
from .tlv import format_tlv, FieldTag
from .response_parser import pk_addr
from .tx_simu import TxSimu
from .tx_auth_7702 import TxAuth7702


class StatusWord(IntEnum):
    OK = 0x9000
    ERROR_NO_INFO = 0x6a00
    INVALID_DATA = 0x6a80
    INSUFFICIENT_MEMORY = 0x6a84
    INVALID_INS = 0x6d00
    INVALID_P1_P2 = 0x6b00
    CONDITION_NOT_SATISFIED = 0x6985
    REF_DATA_NOT_FOUND = 0x6a88
    EXCEPTION_OVERFLOW = 0x6807
    NOT_IMPLEMENTED = 0x911c


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


class PKIPubKeyUsage(IntEnum):
    PUBKEY_USAGE_GENUINE_CHECK = 0x01
    PUBKEY_USAGE_EXCHANGE_PAYLOAD = 0x02
    PUBKEY_USAGE_NFT_METADATA = 0x03
    PUBKEY_USAGE_TRUSTED_NAME = 0x04
    PUBKEY_USAGE_BACKUP_PROVIDER = 0x05
    PUBKEY_USAGE_RECOVER_ORCHESTRATOR = 0x06
    PUBKEY_USAGE_PLUGIN_METADATA = 0x07
    PUBKEY_USAGE_COIN_META = 0x08
    PUBKEY_USAGE_SEED_ID_AUTH = 0x09
    PUBKEY_USAGE_TX_SIMU_SIGNER = 0x0a
    PUBKEY_USAGE_CALLDATA = 0x0b
    PUBKEY_USAGE_NETWORK = 0x0c


class SignMode(IntEnum):
    BASIC = 0x00
    STORE = 0x01
    START_FLOW = 0x02


class PKIClient:
    _CLA: int = 0xB0
    _INS: int = 0x06

    def __init__(self, client: BackendInterface) -> None:
        self._client = client

    def send_certificate(self, p1: PKIPubKeyUsage, payload: bytes) -> None:
        try:
            response = self.send_raw(p1, payload)
            assert response.status == StatusWord.OK
        except ExceptionRAPDU as err:
            if err.status == StatusWord.NOT_IMPLEMENTED:
                print("Ledger-PKI APDU not yet implemented. Legacy path will be used")

    def send_raw(self, p1: PKIPubKeyUsage, payload: bytes) -> RAPDU:
        header = bytearray()
        header.append(self._CLA)
        header.append(self._INS)
        header.append(p1)
        header.append(0x00)
        header.append(len(payload))
        return self._client.exchange_raw(header + payload)


class EthAppClient:
    def __init__(self, client: BackendInterface):
        self._client = client
        self._firmware = client.firmware
        self._cmd_builder = CommandBuilder()
        self._pki_client: Optional[PKIClient] = None
        self._pki_client = PKIClient(self._client)

    def _exchange_async(self, payload: bytes):
        return self._client.exchange_async_raw(payload)

    def _exchange(self, payload: bytes):
        return self._client.exchange_raw(payload)

    def response(self) -> Optional[RAPDU]:
        return self._client.last_async_response

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
        cert_apdu = ""
        if name_source == TrustedNameSource.CAL:
            if self._pki_client is not None:
                # pylint: disable=line-too-long
                if self._firmware == Firmware.NANOSP:
                    cert_apdu = "010101020102110400000002120100130200021401011604000000002010547275737465645F4E616D655F43414C300200093101043201213321024CCA8FAD496AA5040A00A7EB2F5CC3B85376D88BA147A7D7054A99C6405618873401013501031546304402202397E6D4F5A9C13532810619EB766CB41919F7A814935CA207429966E41CE80202202E43159A04BB0596A6B1F0DDE1A12931EA56156751586BEE8FDCAB54EEE36883"  # noqa: E501
                elif self._firmware == Firmware.NANOX:
                    cert_apdu = "010101020102110400000002120100130200021401011604000000002010547275737465645F4E616D655F43414C300200093101043201213321024CCA8FAD496AA5040A00A7EB2F5CC3B85376D88BA147A7D7054A99C64056188734010135010215473045022100BCAED6896A3413657F7F936F53FFF5BD7E30498B1109EAB878135F6DD4AAE555022056913EF42F166BEFEEDDB06C1F7AEEDE8712828F37B916E3E6DA7AE0809558E4"  # noqa: E501
                elif self._firmware == Firmware.STAX:
                    cert_apdu = "010101020102110400000002120100130200021401011604000000002010547275737465645F4E616D655F43414C300200093101043201213321024CCA8FAD496AA5040A00A7EB2F5CC3B85376D88BA147A7D7054A99C64056188734010135010415473045022100ABA9D58446EE81EB073DA91941989DD7E133556D58DE2BCBA59E46253DB448B102201DF8AE930A9E318B50576D8922503A5D3EC84C00C332A7C8FF7CD48708751840"  # noqa: E501
                elif self._firmware == Firmware.FLEX:
                    cert_apdu = "010101020102110400000002120100130200021401011604000000002010547275737465645F4E616D655F43414C300200093101043201213321024CCA8FAD496AA5040A00A7EB2F5CC3B85376D88BA147A7D7054A99C6405618873401013501051546304402206DC9F82C53F3B13400D3E343E3C8C81868E8C73B1EF2655D07891064B7AC3166022069A36E4059D75C93E488A5D58C02BCA9C80C081F77B31C5EDCF07F1A500C565A"  # noqa: E501
                else:
                    print(f"Invalid device '{self._firmware.name}'")
                # pylint: enable=line-too-long
            key_id = 9
            key = Key.CAL
        else:
            if self._pki_client is not None:
                # pylint: disable=line-too-long
                if self._firmware == Firmware.NANOSP:
                    cert_apdu = "01010102010211040000000212010013020002140101160400000000200C547275737465645F4E616D6530020007310104320121332102B91FBEC173E3BA4A714E014EBC827B6F899A9FA7F4AC769CDE284317A00F4F6534010135010315473045022100F394484C045418507E0F76A3231F233B920C733D3E5BB68AFBAA80A55195F70D022012BC1FD796CD2081D8355DEEFA051FBB9329E34826FF3125098F4C6A0C29992A"  # noqa: E501
                elif self._firmware == Firmware.NANOX:
                    cert_apdu = "01010102010211040000000212010013020002140101160400000000200C547275737465645F4E616D6530020007310104320121332102B91FBEC173E3BA4A714E014EBC827B6F899A9FA7F4AC769CDE284317A00F4F65340101350102154730450221009D97646C49EE771BE56C321AB59C732E10D5D363EBB9944BF284A3A04EC5A14102200633518E851984A7EA00C5F81EDA9DAA58B4A6C98E57DA1FBB9074AEFF0FE49F"  # noqa: E501
                elif self._firmware == Firmware.STAX:
                    cert_apdu = "01010102010211040000000212010013020002140101160400000000200C547275737465645F4E616D6530020007310104320121332102B91FBEC173E3BA4A714E014EBC827B6F899A9FA7F4AC769CDE284317A00F4F6534010135010415473045022100A57DC7AB3F0E38A8D10783C7449024D929C60843BB75E5FF7B8088CB71CB130C022045A03E6F501F3702871466473BA08CE1F111357ED9EF395959733477165924C4"  # noqa: E501
                elif self._firmware == Firmware.FLEX:
                    cert_apdu = "01010102010211040000000212010013020002140101160400000000200C547275737465645F4E616D6530020007310104320121332102B91FBEC173E3BA4A714E014EBC827B6F899A9FA7F4AC769CDE284317A00F4F6534010135010515473045022100D5BB77756C3D7C1B4254EA8D5351B94A89B13BA69C3631A523F293A10B7144B302201519B29A882BB22DCDDF6BE79A9CBA76566717FA877B7CA4B9CC40361A2D579E"  # noqa: E501
                else:
                    print(f"Invalid device '{self._firmware.name}'")
                # pylint: enable=line-too-long
            key_id = 7
            key = Key.TRUSTED_NAME

        if self._pki_client is not None and cert_apdu:
            self._pki_client.send_certificate(PKIPubKeyUsage.PUBKEY_USAGE_TRUSTED_NAME, bytes.fromhex(cert_apdu))

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

        if self._pki_client is None:
            print(f"Ledger-PKI Not supported on '{self._firmware.name}'")
        else:
            # pylint: disable=line-too-long
            if self._firmware == Firmware.NANOSP:
                cert_apdu = "01010102010210040102000011040000000212010013020002140101160400000000200A53657420506C7567696E30020003310107320121332103C055BC4ECF055E2D85085D35127A3DE6705C7F885055CD7071E87671BF191FE3340101350103154630440220401824348DA0E435C9BF16C3591665CFA1B7D8E729971BE884027E02BD3C35A102202289EE207B73D98E9E6110CC143EB929F03B99D54C63023C99561D3CE164D30F"  # noqa: E501
            elif self._firmware == Firmware.NANOX:
                cert_apdu = "01010102010211040000000212010013020002140101160400000000200A53657420506C7567696E30020003310107320121332103C055BC4ECF055E2D85085D35127A3DE6705C7F885055CD7071E87671BF191FE334010135010215473045022100E657DE255F954779E14D281E2E739D89DEF2E943B7FD4B4AFE49CF4FF7E1D84F022057F29C9AEA8FAA25C8438FDEE85C6DABF270E5CEC1655F17F2D9A6ADCD3ADC0E"  # noqa: E501
            elif self._firmware == Firmware.STAX:
                cert_apdu = "01010102010211040000000212010013020002140101160400000000200A53657420506C7567696E30020003310107320121332103C055BC4ECF055E2D85085D35127A3DE6705C7F885055CD7071E87671BF191FE334010135010415473045022100B8AF9667C190B60BF350D8F8CA66A4BCEA22BF47D757CB7F88F8D16C7794BCDC02205F7D6C8E9294F73744A82E1062B10FFEB809252682112E71A419EFC78227211B"  # noqa: E501
            elif self._firmware == Firmware.FLEX:
                cert_apdu = "01010102010211040000000212010013020002140101160400000000200A53657420506C7567696E30020003310107320121332103C055BC4ECF055E2D85085D35127A3DE6705C7F885055CD7071E87671BF191FE334010135010515473045022100F5069D8BCEDCF7CC55273266E3871B09FFCACD084B5753347A809DDDA67E6235022003CE65364BFA96B6FE7A9D8C13EC87B8E727E8B7BF4A63176F5D61AB8F97807E"  # noqa: E501
            else:
                print(f"Invalid device '{self._firmware.name}'")
                cert_apdu = ""
            # pylint: enable=line-too-long
            if cert_apdu:
                self._pki_client.send_certificate(PKIPubKeyUsage.PUBKEY_USAGE_PLUGIN_METADATA, bytes.fromhex(cert_apdu))

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

        if self._pki_client is None:
            print(f"Ledger-PKI Not supported on '{self._firmware.name}'")
        else:
            # pylint: disable=line-too-long
            if self._firmware == Firmware.NANOSP:
                cert_apdu = "0101010201021004010200001104000000021201001302000214010116040000000020084e46545f496e666f300200043101033201213321023cfb5fb31905f4bd39d9d535a40c26aab51c5d7d3219b28ac942b980fb206cfb34010135010315473045022100d43e142a6639b27a79bc4f021854df48f1bc1e828ac47b105578cb527b69f525022078f6e6b3eb9bb787a0a29e85531ce3512c2d6481e761e840db0fb6b0898911a1"  # noqa: E501
            elif self._firmware == Firmware.NANOX:
                cert_apdu = "0101010201021104000000021201001302000214010116040000000020084E46545F496E666F300200043101033201213321023CFB5FB31905F4BD39D9D535A40C26AAB51C5D7D3219B28AC942B980FB206CFB340101350102154730450221009BAE21BB8CBA6F95DDFF86AEEA991D63FA36A469A3071F61BDA8895F1A5F0AC3022061661F95D1513D3FDE81FFEA4B0C6D48ADCB27ED70915EE3ACD16A2A64CDE916"  # noqa: E501
            elif self._firmware == Firmware.STAX:
                cert_apdu = "0101010201021104000000021201001302000214010116040000000020084E46545F496E666F300200043101033201213321023CFB5FB31905F4BD39D9D535A40C26AAB51C5D7D3219B28AC942B980FB206CFB3401013501041546304402201DEE04EC830FFDE5C98A708EC6865605FC14FF6105A54BE5230F2B954C673B940220581A0A5E42A7779140963703E43B3BEABE4C69284EDEF00E76BB5875E0810C9B"  # noqa: E501
            elif self._firmware == Firmware.FLEX:
                cert_apdu = "0101010201021104000000021201001302000214010116040000000020084E46545F496E666F300200043101033201213321023CFB5FB31905F4BD39D9D535A40C26AAB51C5D7D3219B28AC942B980FB206CFB340101350105154730450221009ABCC7056D54C1B5DBB353178B13850C20521EE6884AA415AA61B329DB1D87F602204E308F273B8D18080184695438577F770524F717E5D08EE20ECBF1BC599F3538"  # noqa: E501
            else:
                print(f"Invalid device '{self._firmware.name}'")
                cert_apdu = ""
            # pylint: enable=line-too-long
            if cert_apdu:
                self._pki_client.send_certificate(PKIPubKeyUsage.PUBKEY_USAGE_NFT_METADATA, bytes.fromhex(cert_apdu))

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

        if self._pki_client is None:
            print(f"Ledger-PKI Not supported on '{self._firmware.name}'")
        else:
            # pylint: disable=line-too-long
            if self._firmware == Firmware.NANOSP:
                cert_apdu = "01010102010211040000000212010013020002140101160400000000200B45524332305F546F6B656E300200063101083201213321024CCA8FAD496AA5040A00A7EB2F5CC3B85376D88BA147A7D7054A99C64056188734010135010310040102000015473045022100C15795C2AE41E6FAE6B1362EE1AE216428507D7C1D6939B928559CC7A1F6425C02206139CF2E133DD62F3E00F183E42109C9853AC62B6B70C5079B9A80DBB9D54AB5"  # noqa: E501
            elif self._firmware == Firmware.NANOX:
                cert_apdu = "01010102010211040000000212010013020002140101160400000000200B45524332305F546F6B656E300200063101083201213321024CCA8FAD496AA5040A00A7EB2F5CC3B85376D88BA147A7D7054A99C64056188734010135010215473045022100E3B956F93FBFF0D41908483888F0F75D4714662A692F7A38DC6C41A13294F9370220471991BECB3CA4F43413CADC8FF738A8CC03568BFA832B4DCFE8C469080984E5"  # noqa: E501
            elif self._firmware == Firmware.STAX:
                cert_apdu = "01010102010211040000000212010013020002140101160400000000200B45524332305F546F6B656E300200063101083201213321024CCA8FAD496AA5040A00A7EB2F5CC3B85376D88BA147A7D7054A99C6405618873401013501041546304402206731FCD3E2432C5CA162381392FD17AD3A41EEF852E1D706F21A656AB165263602204B89FAE8DBAF191E2D79FB00EBA80D613CB7EDF0BE960CB6F6B29D96E1437F5F"  # noqa: E501
            elif self._firmware == Firmware.FLEX:
                cert_apdu = "01010102010211040000000212010013020002140101160400000000200B45524332305F546F6B656E300200063101083201213321024CCA8FAD496AA5040A00A7EB2F5CC3B85376D88BA147A7D7054A99C64056188734010135010515473045022100B59EA8B958AA40578A6FBE9BBFB761020ACD5DBD8AA863C11DA17F42B2AFDE790220186316059EFA58811337D47C7F815F772EA42BBBCEA4AE123D1118C80588F5CB"  # noqa: E501
            else:
                print(f"Invalid device '{self._firmware.name}'")
                cert_apdu = ""
            # pylint: enable=line-too-long
            if cert_apdu:
                self._pki_client.send_certificate(PKIPubKeyUsage.PUBKEY_USAGE_COIN_META, bytes.fromhex(cert_apdu))

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

    def _prepare_network_info(self,
                              name: str,
                              ticker: str,
                              chain_id: int,
                              icon: Optional[bytes] = None) -> bytes:

        payload = format_tlv(FieldTag.STRUCT_TYPE, 8)
        payload += format_tlv(FieldTag.STRUCT_VERSION, 1)
        payload += format_tlv(FieldTag.BLOCKCHAIN_FAMILY, 1)
        payload += format_tlv(FieldTag.CHAIN_ID, chain_id.to_bytes(8, 'big'))
        payload += format_tlv(FieldTag.NETWORK_NAME, name.encode('utf-8'))
        payload += format_tlv(FieldTag.TICKER, ticker.encode('utf-8'))
        if icon:
            # Network Icon
            payload += format_tlv(FieldTag.NETWORK_ICON_HASH, sha256(icon).digest())
        # Append the data Signature
        payload += format_tlv(FieldTag.DER_SIGNATURE, sign_data(Key.NETWORK, payload))
        return payload

    def provide_network_information(self,
                                    name: str,
                                    ticker: str,
                                    chain_id: int,
                                    icon: Optional[bytes] = None) -> None:

        if self._pki_client is None:
            print(f"Ledger-PKI Not supported on '{self._firmware.name}'")
        else:
            # pylint: disable=line-too-long
            if self._firmware == Firmware.NANOSP:
                cert_apdu = "0101010201021104000000021201001302000214010116040000000020076E6574776F726B3002000A31010C32012133210304AF5CF32094F855E93235E9EB43F48E9B436C2E1DFAEA58ECAFA68AAFB1D27C3401013501031546304402207BDE0D30C0A573ECF2F8377F30D9A064CFDD8C85E3B328E4C0867A5C7E95EC7E02204B3546342571AE54E7524DD66D4F801E3E4F4F2EA9008F943661F62E48053E92"  # noqa: E501
            elif self._firmware == Firmware.NANOX:
                cert_apdu = "0101010201021104000000021201001302000214010116040000000020076E6574776F726B3002000A31010C32012133210304AF5CF32094F855E93235E9EB43F48E9B436C2E1DFAEA58ECAFA68AAFB1D27C34010135010215473045022100E1CE7DAC7A23B9422B0764F8FE80246B2CF0780227289FBFB203985DA6326A9502205F90B43600407A094DD6DEBCEB92F34DA3EDA9EB5463568440BDC21635FF2044"  # noqa: E501
            elif self._firmware == Firmware.STAX:
                cert_apdu = "0101010201021104000000021201001302000214010116040000000020076E6574776F726B3002000A31010C32012133210304AF5CF32094F855E93235E9EB43F48E9B436C2E1DFAEA58ECAFA68AAFB1D27C34010135010415463044022044C595C3E98100D4ECA75A73BF294090FF94948E80EE1430624C886B15BE862302200994E1D98CA72B78D57808B5FD236F439376AFC7C651B55D4AFBFB5AF4C15E00"  # noqa: E501
            elif self._firmware == Firmware.FLEX:
                cert_apdu = "0101010201021104000000021201001302000214010116040000000020076E6574776F726B3002000A31010C32012133210304AF5CF32094F855E93235E9EB43F48E9B436C2E1DFAEA58ECAFA68AAFB1D27C34010135010515463044022008D276684F1A1CC3A89DB0B15120860414FF6A60E227FCAA29ED8F2096C982460220343FE956D443CEA33A2F8BD9DD1EAD783ACFF86088CF01BCE63C224DC815D7F0"  # noqa: E501
            else:
                print(f"Invalid device '{self._firmware.name}'")
                cert_apdu = ""
            # pylint: enable=line-too-long
            if cert_apdu:
                self._pki_client.send_certificate(PKIPubKeyUsage.PUBKEY_USAGE_NETWORK, bytes.fromhex(cert_apdu))

        # Add the network info
        payload = self._prepare_network_info(name, ticker, chain_id, icon)
        chunks = self._cmd_builder.provide_network_information(payload, icon)
        for chunk in chunks[:-1]:
            response = self._exchange(chunk)
            assert response.status == StatusWord.OK
        response = self._exchange(chunks[-1])
        assert response.status == StatusWord.OK

    def provide_enum_value(self, payload: bytes) -> RAPDU:
        if self._pki_client is None:
            print(f"Ledger-PKI Not supported on '{self._firmware.name}'")
        else:
            # pylint: disable=line-too-long
            if self._firmware == Firmware.NANOSP:
                cert_apdu = "01010102010211040000000212010013020002140101160400000000200863616C6C646174613002000831010B32012133210381C0821E2A14AC2546FB0B9852F37CA2789D7D76483D79217FB36F51DCE1E7B434010135010315463044022076DD2EAB72E69D440D6ED8290C8C37E39F54294C23FF0F8520F836E7BE07455C02201D9A8A75223C1ADA1D9D00966A12EBB919D0BBF2E66F144C83FADCAA23672566"  # noqa: E501
            elif self._firmware == Firmware.NANOX:
                cert_apdu = "01010102010211040000000212010013020002140101160400000000200863616C6C646174613002000831010B32012133210381C0821E2A14AC2546FB0B9852F37CA2789D7D76483D79217FB36F51DCE1E7B434010135010215463044022077FF9625006CB8A4AD41A4B04FF2112E92A732BD263CCE9B97D8E7D2536D04300220445B8EE3616FB907AA5E68359275E94D0A099C3E32A4FC8B3669C34083671F2F"  # noqa: E501
            elif self._firmware == Firmware.STAX:
                cert_apdu = "01010102010211040000000212010013020002140101160400000000200863616C6C646174613002000831010B32012133210381C0821E2A14AC2546FB0B9852F37CA2789D7D76483D79217FB36F51DCE1E7B434010135010415473045022100A88646AD72CA012D5FDAF8F6AE0B7EBEF079212768D57323CB5B57CADD9EB20D022005872F8EA06092C9783F01AF02C5510588FB60CBF4BA51FB382B39C1E060BB6B"  # noqa: E501
            elif self._firmware == Firmware.FLEX:
                cert_apdu = "01010102010211040000000212010013020002140101160400000000200863616C6C646174613002000831010B32012133210381C0821E2A14AC2546FB0B9852F37CA2789D7D76483D79217FB36F51DCE1E7B43401013501051546304402205305BDDDAD0284A2EAC2A9BE4CEF6604AE9415C5F46883448F5F6325026234A3022001ED743BCF33CCEB070FDD73C3D3FCC2CEE5AB30A5C3EB7D2A8D21C6F58D493F"  # noqa: E501
            # pylint: enable=line-too-long
            self._pki_client.send_certificate(PKIPubKeyUsage.PUBKEY_USAGE_CALLDATA, bytes.fromhex(cert_apdu))

        chunks = self._cmd_builder.provide_enum_value(payload)
        for chunk in chunks[:-1]:
            self._exchange(chunk)
        return self._exchange(chunks[-1])

    def provide_transaction_info(self, payload: bytes) -> RAPDU:
        if self._pki_client is None:
            print(f"Ledger-PKI Not supported on '{self._firmware.name}'")
        else:
            # pylint: disable=line-too-long
            if self._firmware == Firmware.NANOSP:
                cert_apdu = "01010102010211040000000212010013020002140101160400000000200863616C6C646174613002000831010B32012133210381C0821E2A14AC2546FB0B9852F37CA2789D7D76483D79217FB36F51DCE1E7B434010135010315463044022076DD2EAB72E69D440D6ED8290C8C37E39F54294C23FF0F8520F836E7BE07455C02201D9A8A75223C1ADA1D9D00966A12EBB919D0BBF2E66F144C83FADCAA23672566"  # noqa: E501
            elif self._firmware == Firmware.NANOX:
                cert_apdu = "01010102010211040000000212010013020002140101160400000000200863616C6C646174613002000831010B32012133210381C0821E2A14AC2546FB0B9852F37CA2789D7D76483D79217FB36F51DCE1E7B434010135010215463044022077FF9625006CB8A4AD41A4B04FF2112E92A732BD263CCE9B97D8E7D2536D04300220445B8EE3616FB907AA5E68359275E94D0A099C3E32A4FC8B3669C34083671F2F"  # noqa: E501
            elif self._firmware == Firmware.STAX:
                cert_apdu = "01010102010211040000000212010013020002140101160400000000200863616C6C646174613002000831010B32012133210381C0821E2A14AC2546FB0B9852F37CA2789D7D76483D79217FB36F51DCE1E7B434010135010415473045022100A88646AD72CA012D5FDAF8F6AE0B7EBEF079212768D57323CB5B57CADD9EB20D022005872F8EA06092C9783F01AF02C5510588FB60CBF4BA51FB382B39C1E060BB6B"  # noqa: E501
            elif self._firmware == Firmware.FLEX:
                cert_apdu = "01010102010211040000000212010013020002140101160400000000200863616C6C646174613002000831010B32012133210381C0821E2A14AC2546FB0B9852F37CA2789D7D76483D79217FB36F51DCE1E7B43401013501051546304402205305BDDDAD0284A2EAC2A9BE4CEF6604AE9415C5F46883448F5F6325026234A3022001ED743BCF33CCEB070FDD73C3D3FCC2CEE5AB30A5C3EB7D2A8D21C6F58D493F"  # noqa: E501
            else:
                print(f"Invalid device '{self._firmware.name}'")
                cert_apdu = ""
            # pylint: enable=line-too-long
            if cert_apdu:
                self._pki_client.send_certificate(PKIPubKeyUsage.PUBKEY_USAGE_CALLDATA, bytes.fromhex(cert_apdu))

        chunks = self._cmd_builder.provide_transaction_info(payload)
        for chunk in chunks[:-1]:
            self._exchange(chunk)
        return self._exchange(chunks[-1])

    def opt_in_tx_simulation(self):
        return self._exchange_async(self._cmd_builder.opt_in_tx_simulation())

    def provide_tx_simulation(self, simu_params: TxSimu) -> RAPDU:

        if self._pki_client is None:
            print(f"Ledger-PKI Not supported on '{self._firmware.name}'")
        else:
            # pylint: disable=line-too-long
            if self._firmware == Firmware.NANOSP:
                cert_apdu = "01010102010211040000000212010013020002140101160400000000200A573343204C65646765723002000531010A320121332103DA154C97512390BDFDD0E2178523EF7B2486B7A7DD5B507079D2F3641AEF550134010135010315473045022100EABC9E26361DD551E30F52604884E5D0DAEF9EDD63848C45DA0B446DEE870BF002206AC308F46D04E5B23CC8D62F9C062E3AF931E3EEF9C2509AFA768A891CA8F10B"  # noqa: E501
            elif self._firmware == Firmware.NANOX:
                cert_apdu = "01010102010211040000000212010013020002140101160400000000200A573343204C65646765723002000531010A320121332103DA154C97512390BDFDD0E2178523EF7B2486B7A7DD5B507079D2F3641AEF550134010135010215473045022100B9EC810718F85C110CF60E7C5A8A6A4F783F3E0918E93861A8FCDCE7CFF4D405022047BD29E3F2B8EFD3FC7451FA19EE3147C38BEF83246DC396E7A10B2D2A44DB30"  # noqa: E501
            elif self._firmware == Firmware.STAX:
                cert_apdu = "01010102010211040000000212010013020002140101160400000000200A573343204C65646765723002000531010A320121332103DA154C97512390BDFDD0E2178523EF7B2486B7A7DD5B507079D2F3641AEF550134010135010415473045022100C490C9DD99F7D1A39D3AE9D448762DEAA17694C9BCF00454503498D2BA883DFE02204AEAD903C2B3A106206D7C2B1ACAA6DD20B6B41EE6AEF38F060F05EC4D7813BB"  # noqa: E501
            elif self._firmware == Firmware.FLEX:
                cert_apdu = "01010102010211040000000212010013020002140101160400000000200A573343204C65646765723002000531010A320121332103DA154C97512390BDFDD0E2178523EF7B2486B7A7DD5B507079D2F3641AEF550134010135010515473045022100FFDE724191BAA18250C7A93404D57CE13465797979EAF64239DB221BA679C5A402206E02953F47D32299F82616713D3DBA39CF8A09EF948B23446A6DDE610F80D066"  # noqa: E501
            else:
                print(f"Invalid device '{self._firmware.name}'")
                cert_apdu = ""
            # pylint: enable=line-too-long
            if cert_apdu:
                self._pki_client.send_certificate(PKIPubKeyUsage.PUBKEY_USAGE_TX_SIMU_SIGNER, bytes.fromhex(cert_apdu))

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
        if self._pki_client is None:
            print(f"Ledger-PKI Not supported on '{self._firmware.name}'")
        else:
            # pylint: disable=line-too-long
            if self._firmware == Firmware.NANOSP:
                cert_apdu = "01010102010211040000000212010013020002140101160400000000200863616C6C646174613002000831010B32012133210381C0821E2A14AC2546FB0B9852F37CA2789D7D76483D79217FB36F51DCE1E7B434010135010315463044022076DD2EAB72E69D440D6ED8290C8C37E39F54294C23FF0F8520F836E7BE07455C02201D9A8A75223C1ADA1D9D00966A12EBB919D0BBF2E66F144C83FADCAA23672566"  # noqa: E501
            elif self._firmware == Firmware.NANOX:
                cert_apdu = "01010102010211040000000212010013020002140101160400000000200863616C6C646174613002000831010B32012133210381C0821E2A14AC2546FB0B9852F37CA2789D7D76483D79217FB36F51DCE1E7B434010135010215463044022077FF9625006CB8A4AD41A4B04FF2112E92A732BD263CCE9B97D8E7D2536D04300220445B8EE3616FB907AA5E68359275E94D0A099C3E32A4FC8B3669C34083671F2F"  # noqa: E501
            elif self._firmware == Firmware.STAX:
                cert_apdu = "01010102010211040000000212010013020002140101160400000000200863616C6C646174613002000831010B32012133210381C0821E2A14AC2546FB0B9852F37CA2789D7D76483D79217FB36F51DCE1E7B434010135010415473045022100A88646AD72CA012D5FDAF8F6AE0B7EBEF079212768D57323CB5B57CADD9EB20D022005872F8EA06092C9783F01AF02C5510588FB60CBF4BA51FB382B39C1E060BB6B"  # noqa: E501
            elif self._firmware == Firmware.FLEX:
                cert_apdu = "01010102010211040000000212010013020002140101160400000000200863616C6C646174613002000831010B32012133210381C0821E2A14AC2546FB0B9852F37CA2789D7D76483D79217FB36F51DCE1E7B43401013501051546304402205305BDDDAD0284A2EAC2A9BE4CEF6604AE9415C5F46883448F5F6325026234A3022001ED743BCF33CCEB070FDD73C3D3FCC2CEE5AB30A5C3EB7D2A8D21C6F58D493F"  # noqa: E501
            # pylint: enable=line-too-long
            self._pki_client.send_certificate(PKIPubKeyUsage.PUBKEY_USAGE_CALLDATA, bytes.fromhex(cert_apdu))
        chunks = self._cmd_builder.provide_proxy_info(payload)
        for chunk in chunks[:-1]:
            self._exchange(chunk)
        return self._exchange(chunks[-1])

    def sign_eip7702_authorization(self, bip32_path: str, auth_params: TxAuth7702) -> RAPDU:
        chunks = self._cmd_builder.sign_eip7702_authorization(bip32_path, auth_params.serialize())
        for chunk in chunks[:-1]:
            self._exchange(chunk)
        return self._exchange_async(chunks[-1])
