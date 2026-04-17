import hashlib
import hmac as hmac_module
import struct
from typing import Optional

from bip_utils import Bip39SeedGenerator, Bip32Slip10Nist256p1
from ragger.navigator.navigation_scenario import NavigateWithScenario
from ragger.bip import pack_derivation_path
from ragger.bip.seed import SPECULOS_MNEMONIC
from ragger.navigator import NavInsID
from web3 import Web3

from client.client import EthAppClient
from client.status_word import StatusWord
from client.address_book import (
    AddressBookClient,
    AddressBookSubCommand,
    AddressBookResponseType,
    DEFAULT_BIP32_PATH,
    DEFAULT_CONTACT_NAME,
    DEFAULT_ADDRESS,
    DEFAULT_ACCOUNT_NAME,
    DEFAULT_SCOPE,
    DEFAULT_CHAIN_ID,
    GROUP_HANDLE_LENGTH,
    GID_SIZE,
    HMAC_PROOF_LENGTH,
)


# =============================================================================
# TX signing constants (shared across provide-contact tests)
# =============================================================================

NONCE     = 21
GAS_PRICE = 13
GAS_LIMIT = 21000
AMOUNT    = 1.22

# =============================================================================
# Constants (must match SDK address_book_crypto.c)
# =============================================================================

HMAC_KDF_SALT_IDENTITY       = b"AddressBook-Identity"
HMAC_KDF_SALT_LEDGER_ACCOUNT = b"AddressBook-LedgerAccount"
BLOCKCHAIN_FAMILY_ETHEREUM   = 1


# =============================================================================
# Crypto helpers
# =============================================================================

def bip32_path_to_list(path: str) -> list[int]:
    """Parse a BIP32 path string into a list of raw indices."""
    raw = pack_derivation_path(path)
    n = raw[0]
    return [struct.unpack('>I', raw[1 + i*4: 5 + i*4])[0] for i in range(n)]


def _derive_privkey(bip32_path: str) -> bytes:
    """Derive secp256r1 private key (SLIP-10) at the given BIP32 path."""
    seed_bytes = Bip39SeedGenerator(SPECULOS_MNEMONIC).Generate()
    ctx = Bip32Slip10Nist256p1.FromSeed(seed_bytes)
    for level in bip32_path_to_list(bip32_path):
        ctx = ctx.ChildKey(level)
    return ctx.PrivateKey().Raw().ToBytes()


def derive_hmac_key_identity(bip32_path: str) -> bytes:
    """KDF for Identity HMAC: SHA256("AddressBook-Identity" || privkey.d)"""
    privkey_d = _derive_privkey(bip32_path)
    h = hashlib.sha256()
    h.update(HMAC_KDF_SALT_IDENTITY)
    h.update(privkey_d)
    return h.digest()


def derive_hmac_key_ledger_account(bip32_path: str) -> bytes:
    """KDF for Ledger Account HMAC: SHA256("AddressBook-LedgerAccount" || privkey.d)"""
    privkey_d = _derive_privkey(bip32_path)
    h = hashlib.sha256()
    h.update(HMAC_KDF_SALT_LEDGER_ACCOUNT)
    h.update(privkey_d)
    return h.digest()


def compute_hmac_name(bip32_path: str,
                      gid: bytes,
                      contact_name: str) -> bytes:
    """Compute HMAC_NAME for an Identity contact.

    Mirrors address_book_compute_hmac_name() in C.
    Message: gid(32) | name_len(1) | name
    """
    assert len(gid) == GID_SIZE
    key        = derive_hmac_key_identity(bip32_path)
    name_bytes = contact_name.encode('utf-8')
    msg = gid + bytes([len(name_bytes)]) + name_bytes
    return hmac_module.new(key, msg, hashlib.sha256).digest()


def compute_hmac_rest(bip32_path: str,
                      gid: bytes,
                      scope: str,
                      address: bytes,
                      chain_id: int) -> bytes:
    """Compute HMAC_REST for an Identity contact.

    Mirrors address_book_compute_hmac_rest() in C.
    Message: gid(32) | scope_len(1) | scope | id_len(1) | address |
             family(1) [| chain_id(8) for FAMILY_ETHEREUM]
    """
    assert len(gid) == GID_SIZE
    key         = derive_hmac_key_identity(bip32_path)
    scope_bytes = scope.encode('utf-8')
    msg = (gid +
           bytes([len(scope_bytes)]) + scope_bytes +
           bytes([len(address)])  + address  +
           bytes([BLOCKCHAIN_FAMILY_ETHEREUM]))
    msg += chain_id.to_bytes(8, 'big')
    return hmac_module.new(key, msg, hashlib.sha256).digest()


def compute_hmac_proof_ledger_account(bip32_path: str,
                                      contact_name: str,
                                      chain_id: int) -> bytes:
    """Compute HMAC Proof of Registration for a Ledger Account.

    Mirrors address_book_compute_hmac_proof_ledger_account() in C.
    Message: name_len(1) | name | family(1) | chain_id(8)
    chain_id is always encoded as 8 bytes big-endian
    """
    key        = derive_hmac_key_ledger_account(bip32_path)
    name_bytes = contact_name.encode('utf-8')
    chain_id = chain_id.to_bytes(8, 'big')
    msg = bytes([len(name_bytes)]) + name_bytes + bytes([BLOCKCHAIN_FAMILY_ETHEREUM]) + chain_id
    return hmac_module.new(key, msg, hashlib.sha256).digest()


# =============================================================================
# Response checkers
# =============================================================================

def _check_response_generic(app_client: EthAppClient,
                            expected_type: AddressBookResponseType,
                            bip32_path: str = DEFAULT_BIP32_PATH,
                            # Identity-specific params
                            gid: Optional[bytes] = None,
                            contact_name: Optional[str] = None,
                            scope: Optional[str] = None,
                            address: Optional[bytes] = None,
                            chain_id: int = DEFAULT_CHAIN_ID) -> tuple:
    """Generic response verifier for all Address Book response types.

    Returns (formats by type):
        - REGISTER_IDENTITY:        1 + 64 + 32 + 32 = 129B (group_handle + hmac_name + hmac_rest)
        - EDIT_CONTACT_NAME:        1 + 32            = 33B  (hmac_name, None, None)
        - EDIT_IDENTIFIER:          1 + 32            = 33B  (hmac_rest, None, None)
        - EDIT_SCOPE:               1 + 32            = 33B  (hmac_rest, None, None)
        - REGISTER_LEDGER_ACCOUNT:  1 + 32            = 33B  (hmac_proof, None, None)
        - EDIT_LEDGER_ACCOUNT:      1 + 32            = 33B  (hmac_proof, None, None)
    """
    response = app_client.response()
    assert response and response.status == StatusWord.OK, \
        f"Unexpected status: {response.status}"
    data = response.data

    # Verify type byte
    actual_type = data[0]
    assert actual_type == expected_type, \
        f"Unexpected response type: 0x{actual_type:02x}, expected {expected_type.name}"

    # -------------------------------------------------------------------------
    # REGISTER_IDENTITY: type(1) | group_handle(64) | hmac_name(32) | hmac_rest(32)
    # -------------------------------------------------------------------------
    if expected_type == AddressBookResponseType.TYPE_REGISTER_IDENTITY:
        expected_len = 1 + GROUP_HANDLE_LENGTH + HMAC_PROOF_LENGTH + HMAC_PROOF_LENGTH
        assert len(data) == expected_len, \
            f"Expected {expected_len} bytes for REGISTER_IDENTITY, got {len(data)}"

        group_handle = data[1:1 + GROUP_HANDLE_LENGTH]
        gid_from_response = group_handle[:GID_SIZE]  # first 32B of group_handle
        offset = 1 + GROUP_HANDLE_LENGTH

        assert contact_name is not None, "contact_name required for REGISTER_IDENTITY"
        assert scope is not None and address is not None, \
            "scope and address required for REGISTER_IDENTITY HMAC_REST"

        device_hmac_name = data[offset:offset + HMAC_PROOF_LENGTH]
        offset += HMAC_PROOF_LENGTH
        expected_hmac_name = compute_hmac_name(bip32_path, gid_from_response, contact_name)
        assert device_hmac_name == expected_hmac_name, (
            f"HMAC_NAME mismatch:\n"
            f"  device:   {device_hmac_name.hex()}\n"
            f"  expected: {expected_hmac_name.hex()}"
        )

        device_hmac_rest = data[offset:offset + HMAC_PROOF_LENGTH]
        expected_hmac_rest = compute_hmac_rest(bip32_path, gid_from_response, scope, address, chain_id)
        assert device_hmac_rest == expected_hmac_rest, (
            f"HMAC_REST mismatch:\n"
            f"  device:   {device_hmac_rest.hex()}\n"
            f"  expected: {expected_hmac_rest.hex()}"
        )

        return (group_handle, device_hmac_name, device_hmac_rest)

    # -------------------------------------------------------------------------
    # All other types: type(1) | hmac(32) = 33B
    # -------------------------------------------------------------------------
    expected_len = 1 + HMAC_PROOF_LENGTH
    assert len(data) == expected_len, \
        f"Expected {expected_len} bytes for {expected_type.name}, got {len(data)}"

    device_hmac = data[1:1 + HMAC_PROOF_LENGTH]

    if expected_type == AddressBookResponseType.TYPE_EDIT_CONTACT_NAME:
        assert gid is not None and contact_name is not None, \
            "gid and contact_name required for EDIT_CONTACT_NAME"
        expected_hmac = compute_hmac_name(bip32_path, gid, contact_name)
        assert device_hmac == expected_hmac, (
            f"HMAC_NAME mismatch:\n"
            f"  device:   {device_hmac.hex()}\n"
            f"  expected: {expected_hmac.hex()}"
        )

    elif expected_type in (AddressBookResponseType.TYPE_EDIT_IDENTIFIER,
                           AddressBookResponseType.TYPE_EDIT_SCOPE):
        assert all(p is not None for p in [gid, scope, address]), \
            "gid, scope, and address required for HMAC_REST"
        expected_hmac = compute_hmac_rest(bip32_path, gid, scope, address, chain_id)
        assert device_hmac == expected_hmac, (
            f"HMAC_REST mismatch:\n"
            f"  device:   {device_hmac.hex()}\n"
            f"  expected: {expected_hmac.hex()}"
        )

    elif expected_type in (AddressBookResponseType.TYPE_REGISTER_LEDGER_ACCOUNT,
                           AddressBookResponseType.TYPE_EDIT_LEDGER_ACCOUNT):
        assert contact_name is not None, "contact_name required for Ledger Account HMAC"
        expected_hmac = compute_hmac_proof_ledger_account(bip32_path, contact_name, chain_id)
        assert device_hmac == expected_hmac, (
            f"HMAC_PROOF mismatch:\n"
            f"  device:   {device_hmac.hex()}\n"
            f"  expected: {expected_hmac.hex()}"
        )

    return (device_hmac, None, None)


def check_identity_response(app_client: EthAppClient,
                            contact_name: str,
                            scope: str,
                            address: bytes = DEFAULT_ADDRESS,
                            bip32_path: str = DEFAULT_BIP32_PATH,
                            chain_id: int = DEFAULT_CHAIN_ID) -> tuple[bytes, bytes, bytes]:
    """Verify the Register Identity response and return (group_handle, hmac_name, hmac_rest).

    group_handle (64B) must be stored by the wallet and re-sent in all subsequent
    Edit operations for this contact.
    """
    group_handle, hmac_name, hmac_rest = _check_response_generic(
        app_client,
        AddressBookResponseType.TYPE_REGISTER_IDENTITY,
        bip32_path,
        contact_name=contact_name,
        scope=scope,
        address=address,
        chain_id=chain_id,
    )
    print("✓ Register Identity: group_handle, HMAC_NAME and HMAC_REST verified")
    return group_handle, hmac_name, hmac_rest


def check_ledger_account_response(app_client: EthAppClient,
                                  contact_name: str = DEFAULT_ACCOUNT_NAME,
                                  bip32_path: str = DEFAULT_BIP32_PATH,
                                  chain_id: int = DEFAULT_CHAIN_ID) -> bytes:
    """Verify the Register Ledger Account response and return the HMAC proof."""
    hmac_proof, _, _ = _check_response_generic(
        app_client,
        AddressBookResponseType.TYPE_REGISTER_LEDGER_ACCOUNT,
        bip32_path,
        contact_name=contact_name,
        chain_id=chain_id,
    )
    print("✓ HMAC Proof of Registration (Ledger Account) verified")
    return hmac_proof


def check_edit_contact_name_response(app_client: EthAppClient,
                                     contact_name: str,
                                     group_handle: bytes,
                                     bip32_path: str = DEFAULT_BIP32_PATH) -> bytes:
    """Verify the Edit Contact Name response and return the new HMAC_NAME."""
    hmac_name, _, _ = _check_response_generic(
        app_client,
        AddressBookResponseType.TYPE_EDIT_CONTACT_NAME,
        bip32_path,
        gid=group_handle[:GID_SIZE],
        contact_name=contact_name,
    )
    print("✓ Edit Contact Name: HMAC_NAME verified")
    return hmac_name


def check_edit_identifier_response(app_client: EthAppClient,
                                   address: bytes,
                                   group_handle: bytes,
                                   scope: str = DEFAULT_SCOPE,
                                   bip32_path: str = DEFAULT_BIP32_PATH,
                                   chain_id: int = DEFAULT_CHAIN_ID) -> bytes:
    """Verify the Edit Identifier response and return the new HMAC_REST."""
    hmac_rest, _, _ = _check_response_generic(
        app_client,
        AddressBookResponseType.TYPE_EDIT_IDENTIFIER,
        bip32_path,
        gid=group_handle[:GID_SIZE],
        scope=scope,
        address=address,
        chain_id=chain_id,
    )
    print("✓ Edit Identifier: HMAC_REST verified")
    return hmac_rest


def check_edit_scope_response(app_client: EthAppClient,
                              scope: str,
                              group_handle: bytes,
                              address: bytes = DEFAULT_ADDRESS,
                              bip32_path: str = DEFAULT_BIP32_PATH,
                              chain_id: int = DEFAULT_CHAIN_ID) -> bytes:
    """Verify the Edit Scope response and return the new HMAC_REST."""
    hmac_rest, _, _ = _check_response_generic(
        app_client,
        AddressBookResponseType.TYPE_EDIT_SCOPE,
        bip32_path,
        gid=group_handle[:GID_SIZE],
        scope=scope,
        address=address,
        chain_id=chain_id,
    )
    print("✓ Edit Scope: HMAC_REST verified")
    return hmac_rest


def check_edit_ledger_account_response(app_client: EthAppClient,
                                       contact_name: str,
                                       bip32_path: str = DEFAULT_BIP32_PATH,
                                       chain_id: int = DEFAULT_CHAIN_ID) -> bytes:
    """Verify the Edit Ledger Account response and return the new HMAC proof."""
    hmac_proof, _, _ = _check_response_generic(
        app_client,
        AddressBookResponseType.TYPE_EDIT_LEDGER_ACCOUNT,
        bip32_path,
        contact_name=contact_name,
        chain_id=chain_id,
    )
    print("✓ HMAC Proof of Registration (Edit Ledger Account) verified")
    return hmac_proof


# =============================================================================
# Test helpers
# =============================================================================

def _common_register_identity(scenario_navigator: NavigateWithScenario,
                              app_client: EthAppClient,
                              addr_book: AddressBookClient,
                              contact_name: str = DEFAULT_CONTACT_NAME,
                              scope: str = DEFAULT_SCOPE,
                              address: bytes = DEFAULT_ADDRESS,
                              do_compare: bool = True) -> tuple[bytes, bytes, bytes]:
    """Common helper to register an Identity contact.

    Args:
        scenario_navigator: Test navigator
        app_client: Ethereum app client
        addr_book: Address book client
        contact_name: Contact name
        scope: Optional contact scope (address name)
        address: Address bytes (20 bytes for Ethereum)
        do_compare: If False, uses "/register" test suffix to skip snapshot comparison

    Returns:
        Tuple of (group_handle, hmac_name, hmac_rest).
        group_handle (64B) must be passed to all subsequent Edit operations.
    """
    apdu = addr_book.prepare_register_identity(
        address,
        scope=scope,
    )

    with app_client.provide_address_book(addr_book,
                                         apdu,
                                         AddressBookSubCommand.SUB_CMD_REGISTER_IDENTITY):
        scenario_navigator.address_review_approve(
            custom_screen_text="Approve",
            do_comparison=do_compare,
        )

    return check_identity_response(app_client, contact_name, scope, address=address)


def _common_register_ledger_account(scenario_navigator: NavigateWithScenario,
                                    app_client: EthAppClient,
                                    addr_book: AddressBookClient,
                                    do_compare: bool = True) -> bytes:
    """Common helper to register a Ledger Account.

    Args:
        scenario_navigator: Test navigator
        app_client: Ethereum app client
        addr_book: Address book client
        contact_name: Account name
        do_compare: If False, uses "/register" test suffix to skip snapshot comparison

    Returns:
        HMAC proof
    """
    apdu = addr_book.prepare_register_ledger_account()

    instructions = []
    if scenario_navigator.backend.device.is_nano:
        instructions += [
            NavInsID.RIGHT_CLICK,
            NavInsID.RIGHT_CLICK,
            NavInsID.BOTH_CLICK,
        ]
    else:
        instructions += [
            NavInsID.USE_CASE_HOME_SETTINGS,
            NavInsID.LEFT_HEADER_TAP,
            NavInsID.USE_CASE_CHOICE_CONFIRM,
        ]

    with app_client.provide_address_book(addr_book, apdu,
                                         AddressBookSubCommand.SUB_CMD_REGISTER_LEDGER_ACCOUNT):
        if do_compare:
            scenario_navigator.navigator.navigate_and_compare(scenario_navigator.screenshot_path,
                                                              scenario_navigator.test_name,
                                                              instructions)
        else:
            scenario_navigator.navigator.navigate(instructions)

    return check_ledger_account_response(app_client)


# =============================================================================
# Tests — Register Identity
# =============================================================================

def test_address_book_register_identity(scenario_navigator: NavigateWithScenario) -> None:
    """Test Register Identity: bind a name + scope to an Ethereum address.

    Verifies that the device returns two valid HMACs (HMAC_NAME and HMAC_REST)
    that can be independently re-derived from the same inputs.
    """
    backend  = scenario_navigator.backend
    app_client = EthAppClient(backend)
    addr_book  = AddressBookClient(backend)

    _common_register_identity(scenario_navigator, app_client, addr_book)


# =============================================================================
# Tests — Edit Identifier
# =============================================================================

def test_address_book_edit_identifier(scenario_navigator: NavigateWithScenario) -> None:
    """Test Edit Identifier: change the address of an existing contact.

    Flow:
      1. Register Identity → receive (hmac_name, hmac_rest_old)
      2. Edit Identifier: address_old → address_new, providing hmac_rest_old
      3. Verify the returned new HMAC_REST covers (gid, scope, address_new)
    """
    backend    = scenario_navigator.backend
    app_client = EthAppClient(backend)
    addr_book  = AddressBookClient(backend)

    new_address = bytes.fromhex("a0b86991c6218b36c1d19d4a2e9eb0ce3606eb48")

    # Step 1: Register Identity (skip snapshot comparison)
    group_handle, hmac_name_old, hmac_rest_old = _common_register_identity(
        scenario_navigator, app_client, addr_book,
        do_compare=False,
    )

    # Step 2: Edit Identifier (address_old → address_new)
    apdu = addr_book.prepare_edit_identifier(new_address, hmac_name_old, hmac_rest_old, group_handle)
    with app_client.provide_address_book(addr_book, apdu,
                                         AddressBookSubCommand.SUB_CMD_EDIT_IDENTIFIER):
        scenario_navigator.address_review_approve(custom_screen_text="Approve")
    check_edit_identifier_response(app_client, new_address, group_handle)


# =============================================================================
# Tests — Edit Contact Name
# =============================================================================

def test_address_book_edit_contact_name(scenario_navigator: NavigateWithScenario) -> None:
    """Test Edit Contact Name: rename an existing contact.

    Flow:
      1. Register Identity → receive (hmac_name_old, hmac_rest)
      2. Edit Contact Name, providing group_handle + hmac_name_old
         (no address, scope, or network needed)
      3. Verify the returned HMAC_NAME covers (gid, "Bob")
    """
    backend    = scenario_navigator.backend
    app_client = EthAppClient(backend)
    addr_book  = AddressBookClient(backend)

    new_name = "Bob"

    # Step 1: Register Identity (skip snapshot comparison)
    group_handle, hmac_name_old, _ = _common_register_identity(
        scenario_navigator, app_client, addr_book,
        do_compare=False,
    )

    # Step 2: Edit Contact Name (Alice → Bob)
    apdu = addr_book.prepare_edit_contact_name(DEFAULT_CONTACT_NAME, new_name, hmac_name_old, group_handle)
    with app_client.provide_address_book(addr_book, apdu,
                                         AddressBookSubCommand.SUB_CMD_EDIT_CONTACT_NAME):
        scenario_navigator.address_review_approve(custom_screen_text="Approve")
    check_edit_contact_name_response(app_client, new_name, group_handle)


# =============================================================================
# Tests — Edit Scope
# =============================================================================

def test_address_book_edit_scope(scenario_navigator: NavigateWithScenario) -> None:
    """Test Edit Scope: change the scope of an existing contact.

    Flow:
      1. Register Identity
         → receive (hmac_name, hmac_rest_old)
      2. Edit Scope, providing hmac_rest_old
      3. Verify the returned HMAC_REST covers (gid, "Eth Savings", address, ...)
    """
    backend    = scenario_navigator.backend
    app_client = EthAppClient(backend)
    addr_book  = AddressBookClient(backend)

    new_scope = "Eth Savings"

    # Step 1: Register Identity (skip snapshot comparison)
    group_handle, hmac_name_old, hmac_rest_old = _common_register_identity(
        scenario_navigator, app_client, addr_book,
        do_compare=False,
    )

    # Step 2: Edit Scope
    apdu = addr_book.prepare_edit_scope(
        DEFAULT_SCOPE, new_scope, hmac_name_old, hmac_rest_old, group_handle
    )
    with app_client.provide_address_book(addr_book, apdu,
                                         AddressBookSubCommand.SUB_CMD_EDIT_SCOPE):
        scenario_navigator.address_review_approve(custom_screen_text="Approve")
    check_edit_scope_response(app_client, new_scope, group_handle)


# =============================================================================
# Tests — Register Ledger Account
# =============================================================================

def test_address_book_register_ledger_account(scenario_navigator: NavigateWithScenario) -> None:
    """Test Register Ledger Account: bind a name to a BIP32 derivation path."""
    app_client = EthAppClient(scenario_navigator.backend)
    addr_book  = AddressBookClient(scenario_navigator.backend)

    _common_register_ledger_account(scenario_navigator, app_client, addr_book)


# =============================================================================
# Tests — Edit Ledger Account
# =============================================================================

# =============================================================================
# Tests — Provide Contact
# =============================================================================

def test_address_book_provide_contact(scenario_navigator: NavigateWithScenario) -> None:
    """Test Provide Contact: contact name replaces raw address in TX review.

    Flow:
      1. Register Identity (no screenshot comparison)
         → receive (group_handle, hmac_name, hmac_rest)
      2. Provide Contact (synchronous, no UI)
         → device stores the contact and responds 9000 with no data
      3. Sign a TX to the same address
         → "To" field shows the contact name instead of the raw address
    """
    backend    = scenario_navigator.backend
    app_client = EthAppClient(backend)
    addr_book  = AddressBookClient(backend)

    # Step 1: Register Identity
    group_handle, hmac_name, hmac_rest = _common_register_identity(
        scenario_navigator, app_client, addr_book,
        do_compare=False,
    )

    # Step 2: Provide Contact (synchronous, no UI)
    apdu = addr_book.prepare_provide_contact(
        DEFAULT_ADDRESS,
        group_handle,
        hmac_name,
        hmac_rest,
    )
    response = app_client.provide_address_book(addr_book, apdu,
                                               AddressBookSubCommand.SUB_CMD_PROVIDE_CONTACT, False)
    assert response.status == StatusWord.OK
    assert len(response.data) == 0, \
        f"Provide Contact must return no data, got {response.data.hex()}"

    # Step 3: Sign TX to the same address — contact name shown in review
    tx_params: dict = {
        "nonce": NONCE,
        "gasPrice": Web3.to_wei(GAS_PRICE, "gwei"),
        "gas": GAS_LIMIT,
        "to": DEFAULT_ADDRESS,
        "value": Web3.to_wei(AMOUNT, "ether"),
        "chainId": DEFAULT_CHAIN_ID
    }
    with app_client.sign(DEFAULT_BIP32_PATH, tx_params):
        scenario_navigator.review_approve()


def test_address_book_edit_ledger_account(scenario_navigator: NavigateWithScenario) -> None:
    """Test Edit Ledger Account: rename an existing account.

    Flow:
      1. Register Ledger Account "ETH main address" → receive hmac_proof_old
      2. Rename → "ETH savings", providing hmac_proof_old
      3. Verify the new HMAC proof matches the proof for "ETH savings"
    """
    app_client = EthAppClient(scenario_navigator.backend)
    addr_book  = AddressBookClient(scenario_navigator.backend)

    new_name = "ETH savings"

    # Step 1: Register Ledger Account (skip snapshot comparison)
    hmac_proof_old = _common_register_ledger_account(
        scenario_navigator, app_client, addr_book, do_compare=False,
    )

    # Step 2: Edit Ledger Account
    apdu = addr_book.prepare_edit_ledger_account(new_name, hmac_proof_old)
    with app_client.provide_address_book(addr_book, apdu,
                                         AddressBookSubCommand.SUB_CMD_EDIT_LEDGER_ACCOUNT):
        scenario_navigator.address_review_approve(custom_screen_text="Approve")
    check_edit_ledger_account_response(app_client, new_name)
