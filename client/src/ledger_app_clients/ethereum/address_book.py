from enum import IntEnum

from ragger.backend import BackendInterface
from ragger.bip import pack_derivation_path

from .tlv import TlvSerializable, FieldTag


class AddressBookResponseType(IntEnum):
    TYPE_REGISTER_IDENTITY = 0x11
    TYPE_EDIT_CONTACT_NAME = 0x12
    TYPE_REGISTER_LEDGER_ACCOUNT = 0x13
    TYPE_EDIT_LEDGER_ACCOUNT = 0x14
    TYPE_EDIT_IDENTIFIER = 0x15
    TYPE_EDIT_SCOPE = 0x16
    TYPE_PROVIDE_CONTACT = 0x20


class AddressBookSubCommand(IntEnum):
    SUB_CMD_REGISTER_IDENTITY = 0x01
    SUB_CMD_EDIT_CONTACT_NAME = 0x02
    SUB_CMD_EDIT_IDENTIFIER = 0x03
    SUB_CMD_EDIT_SCOPE = 0x04
    SUB_CMD_REGISTER_LEDGER_ACCOUNT = 0x11
    SUB_CMD_EDIT_LEDGER_ACCOUNT = 0x12
    SUB_CMD_PROVIDE_CONTACT = 0x20


GROUP_HANDLE_LENGTH = 64  # group_handle = gid(32) | MAC(K_group, gid)(32)
GID_SIZE = 32  # first half of group_handle, used in HMAC messages
HMAC_PROOF_LENGTH = 32
DEFAULT_BIP32_PATH = "m/44'/60'/0'/0/0"
DEFAULT_CONTACT_NAME = "Alice"
DEFAULT_CHAIN_ID = 1  # Ethereum Mainnet
DEFAULT_SCOPE = "Eth Address 1"
DEFAULT_ADDRESS = bytes.fromhex("6b175474e89094c44da98b954eedeac495271d0f")
DEFAULT_ACCOUNT_NAME = "ETH main address"


class AddressBookClient(TlvSerializable):
    """
    Client for managing Address Book on Ledger devices.
    """
    _CLA: int = 0xB0
    _INS: int = 0x10

    def __init__(self, backend: BackendInterface) -> None:
        """Initialize the Address Book client with a backend."""
        self._backend = backend

    def send_async_raw(self, p1: AddressBookSubCommand, payload: bytes):
        """Send an address book APDU, splitting into chunks when needed.

        CMD_VERIFY_SIGNED_ADDRESS payloads exceed 255 bytes, so they are
        transported as multiple APDUs using a P2-based chunking scheme:
          - First chunk  (P2=0x00): 2-byte big-endian total length + first slice
          - Next  chunks (P2=0x80): subsequent slices
        All intermediate chunks are sent synchronously (device responds 9000).
        The last chunk is sent asynchronously to allow the UI flow to proceed.

        All other commands fit in a single short APDU (P2=0x00, no framing).
        """
        MAX_CHUNK = 255
        P2_FIRST = 0x00
        P2_NEXT = 0x80

        if p1 not in (AddressBookSubCommand.SUB_CMD_EDIT_IDENTIFIER,
                      AddressBookSubCommand.SUB_CMD_EDIT_SCOPE,
                      AddressBookSubCommand.SUB_CMD_PROVIDE_CONTACT):
            header = bytes([self._CLA, self._INS, p1, P2_FIRST, len(payload)])
            return self._backend.exchange_async_raw(header + payload)

        # Prepend 2-byte big-endian total length (mirrors the firmware framing).
        framed = len(payload).to_bytes(2, 'big') + payload
        chunks = [framed[i:i + MAX_CHUNK] for i in range(0, len(framed), MAX_CHUNK)]

        # Send all but the last chunk synchronously.
        for i, chunk in enumerate(chunks[:-1]):
            p2 = P2_FIRST if i == 0 else P2_NEXT
            header = bytes([self._CLA, self._INS, p1, p2, len(chunk)])
            self._backend.exchange_raw(header + chunk)

        # Last chunk triggers the UI flow on the device.
        p2 = P2_FIRST if len(chunks) == 1 else P2_NEXT
        last = chunks[-1]
        header = bytes([self._CLA, self._INS, p1, p2, len(last)])
        return self._backend.exchange_async_raw(header + last)

    def send_sync_raw(self, p1: AddressBookSubCommand, payload: bytes):
        """Send an address book APDU synchronously (no UI expected).

        Uses the same 2-byte big-endian length framing and P2-based chunking
        as send_async_raw, but every chunk — including the last — is sent
        synchronously via exchange_raw.

        Returns the RAPDU from the final chunk.
        """
        MAX_CHUNK = 255
        P2_FIRST = 0x00
        P2_NEXT = 0x80

        framed = len(payload).to_bytes(2, 'big') + payload
        chunks = [framed[i:i + MAX_CHUNK] for i in range(0, len(framed), MAX_CHUNK)]

        rapdu = None
        for i, chunk in enumerate(chunks):
            p2 = P2_FIRST if i == 0 else P2_NEXT
            header = bytes([self._CLA, self._INS, p1, p2, len(chunk)])
            rapdu = self._backend.exchange_raw(header + chunk)
        return rapdu

    def prepare_edit_ledger_account(self,
                                    new_account_name: str,
                                    hmac_proof: bytes,
                                    old_account_name: str = DEFAULT_ACCOUNT_NAME,
                                    derivation_path: str = DEFAULT_BIP32_PATH,
                                    chain_id: int = DEFAULT_CHAIN_ID) -> bytes:
        """
        Prepare TLV payload for editing the name of an existing Ledger Account.

        Args:
            old_account_name: Current name of the account
            new_account_name: New name to assign to the account
            derivation_path: BIP32 path used to derive the HMAC key on device
            chain_id: Chain ID for the network
            hmac_proof: HMAC Proof of Registration from the previous registration

        Returns:
            Complete TLV payload bytes.
        """
        assert old_account_name and len(old_account_name) <= HMAC_PROOF_LENGTH, \
            f"Old account name required (max {HMAC_PROOF_LENGTH} chars)"
        assert new_account_name and len(new_account_name) <= HMAC_PROOF_LENGTH, \
            f"New account name required (max {HMAC_PROOF_LENGTH} chars)"
        assert derivation_path, "Derivation path is required"
        assert chain_id > 0, "Chain ID must be greater than 0"
        assert len(hmac_proof) == HMAC_PROOF_LENGTH, f"HMAC proof must be {HMAC_PROOF_LENGTH} bytes"

        path_bytes = pack_derivation_path(derivation_path)

        payload: bytes = self.serialize_field(FieldTag.STRUCT_TYPE,
                                              AddressBookResponseType.TYPE_EDIT_LEDGER_ACCOUNT)
        payload += self.serialize_field(FieldTag.STRUCT_VERSION, 1)
        payload += self.serialize_field(FieldTag.CONTACT_NAME, new_account_name.encode('utf-8'))
        payload += self.serialize_field(FieldTag.PREVIOUS_CONTACT_NAME, old_account_name.encode('utf-8'))
        payload += self.serialize_field(FieldTag.DERIVATION_PATH, path_bytes)
        payload += self.serialize_field(FieldTag.CHAIN_ID, chain_id)
        payload += self.serialize_field(FieldTag.HMAC_PROOF, hmac_proof)
        payload += self.serialize_field(FieldTag.BLOCKCHAIN_FAMILY, 1)  # Ethereum
        return payload

    def prepare_register_ledger_account(self,
                                        contact_name: str = DEFAULT_ACCOUNT_NAME,
                                        derivation_path: str = DEFAULT_BIP32_PATH,
                                        chain_id: int = DEFAULT_CHAIN_ID) -> bytes:
        """
        Prepare APDU for registering a Ledger account.

        Args:
            contact_name: Name for this contact
            derivation_path: BIP32 derivation path as string (e.g., "m/44'/60'/0'/0/0")
            chain_id: Chain ID for the network

        Returns:
            Complete APDU bytes ready to send
        """
        assert contact_name, "Contact name is required"
        assert derivation_path, "Derivation path is required"
        assert chain_id > 0, "Chain ID must be greater than 0"

        # Encode derivation path using ragger's pack_derivation_path
        path_bytes = pack_derivation_path(derivation_path)

        # Build TLV payload
        payload: bytes = self.serialize_field(FieldTag.STRUCT_TYPE,
                                              AddressBookResponseType.TYPE_REGISTER_LEDGER_ACCOUNT)
        payload += self.serialize_field(FieldTag.STRUCT_VERSION, 1)
        payload += self.serialize_field(FieldTag.CONTACT_NAME, contact_name.encode('utf-8'))
        payload += self.serialize_field(FieldTag.DERIVATION_PATH, path_bytes)
        payload += self.serialize_field(FieldTag.CHAIN_ID, chain_id)
        payload += self.serialize_field(FieldTag.BLOCKCHAIN_FAMILY, 1)  # Ethereum
        return payload

    def prepare_register_identity(self,
                                  address: bytes,
                                  contact_name: str = DEFAULT_CONTACT_NAME,
                                  derivation_path: str = DEFAULT_BIP32_PATH,
                                  chain_id: int = DEFAULT_CHAIN_ID,
                                  scope: str = DEFAULT_SCOPE) -> bytes:
        """
        Prepare APDU for registering a Contact.

        Args:
            contact_name: Name of the contact (max 32 chars, printable ASCII)
            address: Unique address for the contact (e.g. 20-byte Ethereum address)
            derivation_path: BIP32 path used to derive the HMAC key on device
            chain_id: Chain ID for the network
            scope: Scope/namespace for the address (max 32 chars)

        Returns:
            Complete TLV payload bytes.
        """
        assert contact_name and len(contact_name) <= HMAC_PROOF_LENGTH, \
            f"Contact name required (max {HMAC_PROOF_LENGTH} chars)"
        assert len(address) > 0, "Identifier is required"
        assert derivation_path, "Derivation path is required"
        assert chain_id > 0, "Chain ID must be greater than 0"

        path_bytes = pack_derivation_path(derivation_path)

        payload: bytes = self.serialize_field(FieldTag.STRUCT_TYPE,
                                              AddressBookResponseType.TYPE_REGISTER_IDENTITY)
        payload += self.serialize_field(FieldTag.STRUCT_VERSION, 1)
        payload += self.serialize_field(FieldTag.CONTACT_NAME, contact_name.encode('utf-8'))
        payload += self.serialize_field(FieldTag.SCOPE, scope.encode('utf-8'))
        payload += self.serialize_field(FieldTag.IDENTIFIER, address)
        payload += self.serialize_field(FieldTag.DERIVATION_PATH, path_bytes)
        payload += self.serialize_field(FieldTag.CHAIN_ID, chain_id)
        payload += self.serialize_field(FieldTag.BLOCKCHAIN_FAMILY, 1)  # Ethereum
        return payload

    def prepare_edit_identifier(self,
                                new_address: bytes,
                                hmac_name_proof: bytes,
                                hmac_rest_proof: bytes,
                                group_handle: bytes,
                                old_address: bytes = DEFAULT_ADDRESS,
                                contact_name: str = DEFAULT_CONTACT_NAME,
                                derivation_path: str = DEFAULT_BIP32_PATH,
                                chain_id: int = DEFAULT_CHAIN_ID,
                                scope: str = DEFAULT_SCOPE) -> bytes:
        """
        Prepare TLV payload for editing the address of an existing Identity contact.

        Args:
            new_address: New address bytes (e.g. 20-byte Ethereum address)
            hmac_name_proof: HMAC_NAME from the original Register Identity response
            hmac_rest_proof: HMAC_REST from the original Register Identity response
            group_handle: 64-byte group handle received from the Register Identity response
            old_address: Current (old) address bytes
            contact_name: Name of the contact (unchanged, for display)
            derivation_path: BIP32 path used to derive the HMAC key on device
            chain_id: Chain ID for the network
            scope: Scope/namespace for the address (unchanged)

        Returns:
            Complete TLV payload bytes.
        """
        assert contact_name and len(contact_name) <= HMAC_PROOF_LENGTH, \
            f"Contact name required (max {HMAC_PROOF_LENGTH} chars)"
        assert len(group_handle) == GROUP_HANDLE_LENGTH, f"group_handle must be {GROUP_HANDLE_LENGTH} bytes"
        assert len(new_address) > 0, "New address is required"
        assert len(old_address) > 0, "Old address is required"
        assert derivation_path, "Derivation path is required"
        assert chain_id > 0, "Chain ID must be greater than 0"
        assert len(hmac_name_proof) == HMAC_PROOF_LENGTH, f"HMAC_PROOF must be {HMAC_PROOF_LENGTH} bytes"
        assert len(hmac_rest_proof) == HMAC_PROOF_LENGTH, f"HMAC_REST proof must be {HMAC_PROOF_LENGTH} bytes"

        path_bytes = pack_derivation_path(derivation_path)

        payload: bytes = self.serialize_field(FieldTag.STRUCT_TYPE,
                                              AddressBookResponseType.TYPE_EDIT_IDENTIFIER)
        payload += self.serialize_field(FieldTag.STRUCT_VERSION, 1)
        payload += self.serialize_field(FieldTag.CONTACT_NAME, contact_name.encode('utf-8'))
        payload += self.serialize_field(FieldTag.SCOPE, scope.encode('utf-8'))
        payload += self.serialize_field(FieldTag.IDENTIFIER, new_address)
        payload += self.serialize_field(FieldTag.PREVIOUS_IDENTIFIER, old_address)
        payload += self.serialize_field(FieldTag.GROUP_HANDLE, group_handle)
        payload += self.serialize_field(FieldTag.DERIVATION_PATH, path_bytes)
        payload += self.serialize_field(FieldTag.CHAIN_ID, chain_id)
        payload += self.serialize_field(FieldTag.HMAC_PROOF, hmac_name_proof)
        payload += self.serialize_field(FieldTag.HMAC_REST, hmac_rest_proof)
        payload += self.serialize_field(FieldTag.BLOCKCHAIN_FAMILY, 1)  # Ethereum
        return payload

    def prepare_edit_contact_name(self,
                                  old_contact_name: str,
                                  new_contact_name: str,
                                  hmac_name_proof: bytes,
                                  group_handle: bytes,
                                  derivation_path: str = DEFAULT_BIP32_PATH) -> bytes:
        """
        Prepare TLV payload for editing the name of an existing Identity contact.

        Only needs group_handle + names + path + HMAC_NAME — no address, scope,
        or network required (HMAC_NAME covers only gid + name).

        Args:
            old_contact_name: Current (old) name of the contact
            new_contact_name: New name to assign to the contact
            hmac_name_proof: HMAC_NAME from the original Register Identity response
            group_handle: 64-byte group handle received from the Register Identity response
            derivation_path: BIP32 path used to derive the HMAC key on device

        Returns:
            Complete TLV payload bytes.
        """
        assert old_contact_name and len(old_contact_name) <= HMAC_PROOF_LENGTH, \
            f"Previous contact name required (max {HMAC_PROOF_LENGTH} chars)"
        assert new_contact_name and len(new_contact_name) <= HMAC_PROOF_LENGTH, \
            f"New contact name required (max {HMAC_PROOF_LENGTH} chars)"
        assert len(group_handle) == GROUP_HANDLE_LENGTH, f"group_handle must be {GROUP_HANDLE_LENGTH} bytes"
        assert derivation_path, "Derivation path is required"
        assert len(hmac_name_proof) == HMAC_PROOF_LENGTH, f"HMAC_PROOF must be {HMAC_PROOF_LENGTH} bytes"

        path_bytes = pack_derivation_path(derivation_path)

        payload: bytes = self.serialize_field(FieldTag.STRUCT_TYPE, AddressBookResponseType.TYPE_EDIT_CONTACT_NAME)
        payload += self.serialize_field(FieldTag.STRUCT_VERSION, 1)
        payload += self.serialize_field(FieldTag.CONTACT_NAME, new_contact_name.encode('utf-8'))
        payload += self.serialize_field(FieldTag.PREVIOUS_CONTACT_NAME, old_contact_name.encode('utf-8'))
        payload += self.serialize_field(FieldTag.GROUP_HANDLE, group_handle)
        payload += self.serialize_field(FieldTag.DERIVATION_PATH, path_bytes)
        payload += self.serialize_field(FieldTag.HMAC_PROOF, hmac_name_proof)
        return payload

    def prepare_edit_scope(self,
                           old_scope: str,
                           new_scope: str,
                           hmac_name_proof: bytes,
                           hmac_rest_proof: bytes,
                           group_handle: bytes,
                           address: bytes = DEFAULT_ADDRESS,
                           contact_name: str = DEFAULT_CONTACT_NAME,
                           derivation_path: str = DEFAULT_BIP32_PATH,
                           chain_id: int = DEFAULT_CHAIN_ID) -> bytes:
        """
        Prepare TLV payload for editing the scope of an existing contact.

        Args:
            old_scope: Current (old) scope of the contact
            new_scope: New scope to assign to the contact
            hmac_name_proof: HMAC_NAME from the original Register Identity response
            hmac_rest_proof: HMAC_REST from the original Register Identity response
            group_handle: 64-byte group handle received from the Register Identity response
            contact_name: Name of the contact (unchanged, for display)
            address: Raw address bytes (e.g. 20-byte Ethereum address)
            derivation_path: BIP32 path used to derive the HMAC key on device
            chain_id: Chain ID for the network

        Returns:
            Complete TLV payload bytes.
        """
        assert contact_name and len(contact_name) <= HMAC_PROOF_LENGTH, \
            f"Contact name required (max {HMAC_PROOF_LENGTH} chars)"
        assert len(group_handle) == GROUP_HANDLE_LENGTH, f"group_handle must be {GROUP_HANDLE_LENGTH} bytes"
        assert old_scope and len(old_scope) <= HMAC_PROOF_LENGTH, \
            f"Previous scope required (max {HMAC_PROOF_LENGTH} chars)"
        assert new_scope and len(new_scope) <= HMAC_PROOF_LENGTH, \
            f"New scope required (max {HMAC_PROOF_LENGTH} chars)"
        assert len(address) > 0, "Identifier is required"
        assert derivation_path, "Derivation path is required"
        assert chain_id > 0, "Chain ID must be greater than 0"
        assert len(hmac_name_proof) == HMAC_PROOF_LENGTH, f"HMAC_PROOF must be {HMAC_PROOF_LENGTH} bytes"
        assert len(hmac_rest_proof) == HMAC_PROOF_LENGTH, f"HMAC_REST proof must be {HMAC_PROOF_LENGTH} bytes"

        path_bytes = pack_derivation_path(derivation_path)

        payload: bytes = self.serialize_field(FieldTag.STRUCT_TYPE, AddressBookResponseType.TYPE_EDIT_SCOPE)
        payload += self.serialize_field(FieldTag.STRUCT_VERSION, 1)
        payload += self.serialize_field(FieldTag.CONTACT_NAME, contact_name.encode('utf-8'))
        payload += self.serialize_field(FieldTag.SCOPE, new_scope.encode('utf-8'))
        payload += self.serialize_field(FieldTag.IDENTIFIER, address)
        payload += self.serialize_field(FieldTag.PREVIOUS_SCOPE, old_scope.encode('utf-8'))
        payload += self.serialize_field(FieldTag.GROUP_HANDLE, group_handle)
        payload += self.serialize_field(FieldTag.DERIVATION_PATH, path_bytes)
        payload += self.serialize_field(FieldTag.CHAIN_ID, chain_id)
        payload += self.serialize_field(FieldTag.HMAC_PROOF, hmac_name_proof)
        payload += self.serialize_field(FieldTag.HMAC_REST, hmac_rest_proof)
        payload += self.serialize_field(FieldTag.BLOCKCHAIN_FAMILY, 1)  # Ethereum
        return payload

    def prepare_provide_contact(self,
                                address: bytes,
                                group_handle: bytes,
                                hmac_name: bytes,
                                hmac_rest: bytes,
                                contact_name: str = DEFAULT_CONTACT_NAME,
                                scope: str = DEFAULT_SCOPE,
                                derivation_path: str = DEFAULT_BIP32_PATH,
                                chain_id: int = DEFAULT_CHAIN_ID) -> bytes:
        """
        Prepare TLV payload for the Provide Contact command (P1=0x20).

        Delivers a previously registered contact to the device so that the
        application can substitute the contact name for the raw address
        during transaction review. No UI is displayed; the device responds
        with 9000 and no data on success.

        Args:
            address:         Raw 20-byte Ethereum address
            group_handle:    64-byte group handle from the Register Identity response
            hmac_name:       HMAC_PROOF (32 B) from the Register Identity response
            hmac_rest:       HMAC_REST  (32 B) from the Register Identity response
            contact_name:    Human-readable name bound to the address
            scope:           Scope/namespace for the address (e.g. "Eth Address 1")
            derivation_path: BIP32 path used to derive the HMAC key on device
            chain_id:        Chain ID for the network

        Returns:
            Complete TLV payload bytes.
        """
        assert len(address) > 0, "Address is required"
        assert len(group_handle) == GROUP_HANDLE_LENGTH, \
            f"group_handle must be {GROUP_HANDLE_LENGTH} bytes"
        assert len(hmac_name) == HMAC_PROOF_LENGTH, \
            f"HMAC_PROOF must be {HMAC_PROOF_LENGTH} bytes"
        assert len(hmac_rest) == HMAC_PROOF_LENGTH, \
            f"HMAC_REST must be {HMAC_PROOF_LENGTH} bytes"
        assert derivation_path, "Derivation path is required"
        assert chain_id > 0, "Chain ID must be greater than 0"

        path_bytes = pack_derivation_path(derivation_path)

        payload: bytes = self.serialize_field(FieldTag.STRUCT_TYPE,
                                              AddressBookResponseType.TYPE_PROVIDE_CONTACT)
        payload += self.serialize_field(FieldTag.STRUCT_VERSION, 1)
        payload += self.serialize_field(FieldTag.CONTACT_NAME, contact_name.encode('utf-8'))
        payload += self.serialize_field(FieldTag.SCOPE, scope.encode('utf-8'))
        payload += self.serialize_field(FieldTag.IDENTIFIER, address)
        payload += self.serialize_field(FieldTag.GROUP_HANDLE, group_handle)
        payload += self.serialize_field(FieldTag.DERIVATION_PATH, path_bytes)
        payload += self.serialize_field(FieldTag.CHAIN_ID, chain_id)
        payload += self.serialize_field(FieldTag.BLOCKCHAIN_FAMILY, 1)  # Ethereum
        payload += self.serialize_field(FieldTag.HMAC_PROOF, hmac_name)
        payload += self.serialize_field(FieldTag.HMAC_REST, hmac_rest)
        return payload
