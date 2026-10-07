# test_address_book.py

## Overview

Tests for the Address Book feature. This feature allows a Ledger device to securely associate
human-readable names with blockchain addresses or device-managed accounts. The device
generates HMAC-SHA256 proofs (`HMAC_PROOF`, `HMAC_REST`) that the wallet stores alongside
each contact record; no secrets ever leave the device.

A contact is identified by a `group_handle` (64 bytes) returned at registration. Multiple
addresses can be linked to the same group and therefore share a single contact name. All Edit
operations are authenticated by re-presenting the relevant HMAC proof(s).

For the full protocol specification and cryptographic design see the
[SDK documentation][sdk-api].

## Test Constants

```python
DEFAULT_BIP32_PATH = "m/44'/60'/0'/0/0"
LEDGER_ACCOUNT_BIP32_PATH = "m/44'/60'/0'/0/1"

DEFAULT_CONTACT_NAME = "Alice"
DEFAULT_ACCOUNT_NAME = "ETH main address"
SECONDARY_ACCOUNT_NAME = "ETH secondary account"
DEFAULT_SCOPE = "Eth Address 1"
SECONDARY_SCOPE = "Eth Address 2"
DEFAULT_CHAIN_ID = 1  # Ethereum Mainnet

DEFAULT_ADDRESS = 0x6B175474E89094C44DA98B954EEDEAC495271D0F  # DAI
SECONDARY_ADDRESS = 0xDAC17F958D2EE523A2206206994597C13D831EC7  # USDT
```

---

## Test Functions

### Registration

#### test_address_book_identity_register

**Purpose**: Verify the full Register Identity flow.

Registers contact "Alice" for `DEFAULT_ADDRESS` (DAI token contract used as a dummy address)
on Ethereum mainnet. Checks that the device returns a 129-byte response:
`type(1) | group_handle(64) | HMAC_PROOF(32) | HMAC_REST(32)`.
Both HMACs are independently re-derived from the same inputs and compared byte-for-byte.

**Response format**: `type(1) | group_handle(64) | HMAC_PROOF(32) | HMAC_REST(32)` = 129 bytes

---

#### test_address_book_ledger_account_register

**Purpose**: Verify the Register Ledger Account flow.

Registers "ETH main address" at `DEFAULT_BIP32_PATH`. The device derives the Ethereum address
internally and returns `type(1) | HMAC_PROOF(32)` = 33 bytes.

**Response format**: `type(1) | HMAC_PROOF(32)` = 33 bytes

---

### Edit Flows

#### test_address_book_identity_edit_identifier

**Purpose**: Verify that Edit Identifier produces a new `HMAC_REST` after an address change.

Changes the registered address from `DEFAULT_ADDRESS` to `0xa0b8...eb48` (USDC contract).
The new `HMAC_REST` covers `(gid, scope, new_address, chain_id)`. The old `HMAC_REST` is
invalidated and would be rejected by Provide Contact.

#### test_address_book_identity_edit_contact_name

**Purpose**: Verify that Edit Contact Name produces a new `HMAC_PROOF` after a rename.

Renames "Alice" to "Bob". The new `HMAC_PROOF` covers `(gid, "Bob")`. The old `HMAC_PROOF`
for "Alice" is invalidated for all addresses in the group simultaneously.

#### test_address_book_identity_edit_scope

**Purpose**: Verify that Edit Scope produces a new `HMAC_REST` after a scope change.

Changes the scope from `"Eth Address 1"` to `"Eth Savings"`. The new `HMAC_REST` covers
`(gid, new_scope, address, chain_id)`. The old `HMAC_REST` is no longer valid.

#### test_address_book_ledger_account_edit

**Purpose**: Verify that Edit Ledger Account produces a new `HMAC_PROOF` after a rename.

Renames "ETH main address" to "ETH savings". The new `HMAC_PROOF` covers
`(new_name, blockchain_family, chain_id)`.

---

### Provide Contact — ETH Transaction Tests

#### test_address_book_simple_tx

**Purpose**: Verify that the contact name replaces the raw address in a basic ETH TX review.

1. Register "Alice" for `DEFAULT_ADDRESS`
2. Provide Contact (synchronous, no UI — device stores the binding)
3. Sign a 1.22 ETH transfer to `DEFAULT_ADDRESS` — "To" field shows "Alice"

#### test_address_book_simple_tx_reject

**Purpose**: Verify user rejection after the contact name is displayed.

Same setup as `test_address_book_simple_tx`. The user rejects at the TX review screen.
Expects `CONDITIONS_NOT_SATISFIED` (0x6984).

#### test_address_book_simple_tx_chain_id_mismatch

**Purpose**: Verify that a contact bound to chain 1 is not resolved when signing on chain 137.

Contact registered for `chain_id=1` (Ethereum mainnet). A TX is then signed on Polygon
(`chain_id=137`). Because `get_address_book_contact(137, addr)` returns `NULL`, the raw
address is shown instead of the contact name.

#### test_address_book_identity_multi_address

**Purpose**: Verify that two addresses share a contact name and that a rename propagates
to both atomically.

1. Register `DEFAULT_ADDRESS` as "Alice" → new group: `(group_handle, HMAC_NAME_alice, HMAC_REST_1)`
2. Register `SECONDARY_ADDRESS` under the same group → device echoes `group_handle + HMAC_NAME_alice`, returns new `HMAC_REST_2`
3. Rename "Alice" → "Bob" → `HMAC_NAME_bob` (the old `HMAC_NAME_alice` is now invalid)
4. Provide Contact for `SECONDARY_ADDRESS` with `HMAC_NAME_bob + HMAC_REST_2` → sign TX → "To" shows "Bob"
5. Provide Contact for `DEFAULT_ADDRESS` with `HMAC_NAME_bob + HMAC_REST_1` → sign TX → "To" shows "Bob"

#### test_address_book_simple_tx_after_edit_identifier

**Purpose**: Verify that Provide Contact works with the new address after Edit Identifier.

The old `HMAC_REST` is bound to `DEFAULT_ADDRESS` and is invalid for the new address. After
Edit Identifier, the new `HMAC_REST` must be used in Provide Contact. The contact name is
still shown during the TX review.

#### test_address_book_simple_tx_after_edit_scope

**Purpose**: Verify that Provide Contact works with the new scope after Edit Scope.

The old `HMAC_REST` is bound to `DEFAULT_SCOPE` and is invalid for the new scope. After
Edit Scope, the new `HMAC_REST` must be used in Provide Contact. The contact name is still
shown during the TX review.

---

### Provide Ledger Account Contact Tests

#### test_address_book_simple_tx_ledger_account

**Purpose**: Verify that "From" and "To" both display account names when two Ledger Accounts
are registered.

1. Register "ETH main address" at `DEFAULT_BIP32_PATH` (the signing account)
2. Provide Ledger Account Contact for the signing account
3. Register "ETH secondary account" at `LEDGER_ACCOUNT_BIP32_PATH` (the recipient)
4. Derive the recipient address at `LEDGER_ACCOUNT_BIP32_PATH`
5. Provide Ledger Account Contact for the recipient
6. Sign TX from `DEFAULT_BIP32_PATH` to the derived address
   → "From" shows "ETH main address", "To" shows "ETH secondary account"

#### test_address_book_simple_tx_ledger_account_rename

**Purpose**: Verify that the new name is shown in TX review after Edit Ledger Account.

After renaming "ETH main address" to "ETH savings", the old `HMAC_PROOF` is invalid. The
new `HMAC_PROOF` must be used in Provide Ledger Account Contact. The "From" field then shows
"ETH savings".

---

### EIP-712 Integration Tests

#### test_address_book_eip712_calldata_empty_send

**Purpose**: Verify that the contact name is shown in an EIP-712 Safe empty-calldata review.

Registers the Safe's `to` address as a contact, then signs a GCS Safe EIP-712 message. The
"To" field displays the contact name instead of the raw address.

**EIP-712 message**: `safe_empty.json` — a Safe empty-calldata send

#### test_address_book_eip712_typed_field

**Purpose**: Verify that the contact name is shown in an EIP-712 `trusted_name`-filtered field.

Registers `0x1111...1111` as a contact, then signs an EIP-712 message containing that address
in a field annotated with `type: trusted_name`. The field displays the contact name.

**EIP-712 message**: custom `Root.validator` field (address type, trusted_name filter)

---

### GCS Integration Tests

#### test_address_book_gcs_empty_tx

**Purpose**: Verify that the contact name is shown in a GCS empty-ETH batch send.

Registers `0xd8dA...6045` as a contact, then signs a GCS `batchExecute` transaction. The
"Destination" field is resolved through `ParamCalldata` and displays the contact name.

**Contract**: `0x2cc8...5f3` batch contract, function `batchExecute`

#### test_address_book_gcs_trusted_name_field

**Purpose**: Verify that the contact name is shown in a GCS `ParamTrustedName` field.

Registers `0x1111...1111` as a contact, then signs a GCS ERC-20 `transfer` where `_to` is
annotated as `ParamTrustedName`. The "To" field displays the contact name.

**Token**: USDT (`0xdac1...ec7`), function `transfer(address _to, uint256 _value)`

#### test_address_book_gcs_combined_ab_and_tn

**Purpose**: Verify the combined display when both Address Book and ENS resolve the same address.

Registers `0x1111...1111` as a contact (Address Book) **and** provides an ENS trusted name
`alice.eth` for the same address. The GCS review shows the Address Book name with scope as
the primary display; the ENS name appears as additional context in the detail view.

---

### Cache Update in Place

These tests verify that `on_edit_*_applied()` callbacks update the in-memory contact cache
without requiring a re-provide from the wallet.

#### test_address_book_identity_cache_invalidation_after_edit_contact_name

**Purpose**: Verify that Edit Contact Name updates the cached entry in place so that the new
name is shown in the next TX review without a re-provide.

1. Register "Alice" → `(group_handle, hmac_name_old, hmac_rest)`
2. Provide Contact → "Alice" cached for `DEFAULT_ADDRESS`
3. Edit Contact Name "Alice" → "Alice Renamed" → `on_edit_contact_name_applied()` updates the
   cache entry in place
4. Sign TX to `DEFAULT_ADDRESS` (no re-provide) → "To" shows "Alice Renamed"

#### test_address_book_ledger_account_cache_invalidation_after_rename

**Purpose**: Verify that Edit Ledger Account updates the cached entry in place so that the
new name is shown in the next TX review without a re-provide.

1. Register "ETH main address" → `hmac_proof_old`
2. Provide Ledger Account Contact → "ETH main address" cached
3. Edit Ledger Account → "ETH savings renamed" → `on_edit_ledger_account_applied()` updates
   the cache entry in place
4. Sign TX (no re-provide) → "From" shows "ETH savings renamed"

---

### Security Tests

#### test_address_book_provide_contact_invalid_hmac

**Purpose**: Verify that the device rejects Provide Contact when either HMAC is corrupted.

Both `HMAC_PROOF` and `HMAC_REST` are individually bit-flipped (XOR 0xFF) and sent to the
device. Each case must return `SECURITY_CONDITION_NOT_SATISFIED` (0x6982).

---

## Test Coverage Matrix

| Test | Reg. Identity | Edit Name | Edit Identifier | Edit Scope | Reg. Account | Edit Account | Provide Contact | Provide Account | Description |
| ---- | :---: | :---: | :---: | :---: | :---: | :---: | :---: | :---: | --- |
| `test_address_book_identity_register` | ✓ | | | | | | | | `HMAC_PROOF` + `HMAC_REST` returned and verified |
| `test_address_book_identity_edit_identifier` | ✓ | | ✓ | | | | | | New `HMAC_REST` after address change |
| `test_address_book_identity_edit_contact_name` | ✓ | ✓ | | | | | | | New `HMAC_PROOF` after rename |
| `test_address_book_identity_edit_scope` | ✓ | | | ✓ | | | | | New `HMAC_REST` after scope change |
| `test_address_book_ledger_account_register` | | | | | ✓ | | | | `HMAC_PROOF` returned and verified |
| `test_address_book_ledger_account_edit` | | | | | ✓ | ✓ | | | New `HMAC_PROOF` after rename |
| `test_address_book_simple_tx` | ✓ | | | | | | ✓ | | "To" shows contact name in basic ETH TX |
| `test_address_book_simple_tx_reject` | ✓ | | | | | | ✓ | | User rejection → 0x6984 |
| `test_address_book_simple_tx_chain_id_mismatch` | ✓ | | | | | | ✓ | | Cross-chain contact not resolved → raw address shown |
| `test_address_book_identity_multi_address` | ✓ ×2 | ✓ | | | | | ✓ ×2 | | Two addresses share a group; rename propagates atomically |
| `test_address_book_simple_tx_ledger_account` | | | | | ✓ ×2 | | | ✓ ×2 | "From" and "To" both show account names |
| `test_address_book_eip712_calldata_empty_send` | ✓ | | | | | | ✓ | | Contact name shown in EIP-712 Safe calldata review |
| `test_address_book_eip712_typed_field` | ✓ | | | | | | ✓ | | Contact name shown in EIP-712 `trusted_name` field |
| `test_address_book_gcs_empty_tx` | ✓ | | | | | | ✓ | | Contact name shown in GCS empty-ETH batch send |
| `test_address_book_gcs_trusted_name_field` | ✓ | | | | | | ✓ | | Contact name shown in GCS `ParamTrustedName` field |
| `test_address_book_gcs_combined_ab_and_tn` | ✓ | | | | | | ✓ | | Combined Address Book + ENS display in GCS |
| `test_address_book_simple_tx_after_edit_identifier` | ✓ | | ✓ | | | | ✓ | | Post Edit Identifier: Provide Contact with new address → name shown |
| `test_address_book_simple_tx_after_edit_scope` | ✓ | | | ✓ | | | ✓ | | Post Edit Scope: Provide Contact with new scope → name shown |
| `test_address_book_simple_tx_ledger_account_rename` | | | | | ✓ | ✓ | | ✓ | Post Edit Ledger Account: new name in "From" TX field |
| `test_address_book_identity_cache_invalidation_after_edit_contact_name` | ✓ | ✓ | | | | | ✓ | | Cache updated in place after rename — no re-provide needed |
| `test_address_book_ledger_account_cache_invalidation_after_rename` | | | | | ✓ | ✓ | | ✓ | Cache updated in place after account rename — no re-provide needed |
| `test_address_book_provide_contact_invalid_hmac` | ✓ | | | | | | ✓ | | Corrupted `HMAC_PROOF` or `HMAC_REST` → 0x6982 |

**Column abbreviations:**

- **Reg. Identity** — Register Identity (P1=0x01)
- **Edit Name** — Edit Contact Name (P1=0x02)
- **Edit Identifier** — Edit Identifier (P1=0x03)
- **Edit Scope** — Edit Scope (P1=0x04)
- **Reg. Account** — Register Ledger Account (P1=0x11)
- **Edit Account** — Edit Ledger Account (P1=0x12)
- **Provide Contact** — Provide Contact (P1=0x20)
- **Provide Account** — Provide Ledger Account Contact (P1=0x21)

---

## Coverage Summary

| Area                           | Coverage                                                   |
|--------------------------------|------------------------------------------------------------|
| Identity registration          | ✅ Single address, multi-address group                     |
| Identity edit flows            | ✅ Name, Identifier, Scope                                 |
| Ledger Account registration    | ✅ Single account                                          |
| Ledger Account edit            | ✅ Rename                                                  |
| Provide Contact                | ✅ Post-registration, post-edit, cross-chain mismatch      |
| Provide Ledger Account Contact | ✅ Post-registration, post-rename                          |
| Cache update in place          | ✅ Contact/account rename — cache updated in place         |
| Security — HMAC rejection      | ✅ Corrupted `HMAC_PROOF` and `HMAC_REST` → 0x6982         |
| Security — user rejection      | ✅ TX review rejected → 0x6984                             |
| TX display — ETH               | ✅ Basic transfer, reject, chain mismatch, multi-address   |
| TX display — EIP-712           | ✅ Safe calldata, typed `trusted_name` field               |
| TX display — GCS               | ✅ Batch empty send, `ParamTrustedName`, combined with ENS |

## Related Documentation

- [Test Overview](../test_overview.md) — Address Book section in Advanced Features
- [Glossary](../glossary.md) — Ethereum concepts
- [README](../README.md) — Test infrastructure
- [SDK API Reference][sdk-api] — Address Book API reference
- [SDK Protocol Spec][sdk-spec] — Full protocol specification with sequence diagrams

[sdk-api]: https://github.com/LedgerHQ/ledger-secure-sdk/blob/main/app_features/address_book/doc/mainpage.dox
[sdk-spec]: https://github.com/LedgerHQ/ledger-secure-sdk/blob/main/app_features/address_book/doc/address_book_spec.md
