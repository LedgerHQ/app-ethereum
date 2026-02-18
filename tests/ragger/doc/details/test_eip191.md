# test_eip191.py - Test Details

## Overview

Tests **EIP-191 personal message signing** (`personal_sign`), a standard for signing arbitrary
human-readable messages with Ethereum accounts.

## Test Context

### What is EIP-191?

EIP-191 defines a standard for signing messages that prevents them from being valid Ethereum
transactions. The signature format adds a prefix to prevent confusion between signed messages
and signed transactions.

**Message Format:**

```text
\x19Ethereum Signed Message:\n{length}{message}
```

**Why the prefix?**

- Prevents signed messages from being used as valid transactions
- Clearly distinguishes message signatures from transaction signatures
- Security feature to prevent replay attacks

📖 [EIP-191 Specification](https://eips.ethereum.org/EIPS/eip-191)

### Use Cases

- **Authentication:** Sign in to dApps with wallet
- **Proof of ownership:** Prove you control an address
- **Terms acceptance:** Accept terms of service
- **Message verification:** Cryptographically sign off-chain data

### Derivation Path

All tests use: `m/44'/60'/0'/0/0` (standard Ethereum account path)

💡 For explanations of BIP32 paths, see the [Glossary](../glossary.md)

---

## Test Functions

### `test_personal_sign_metamask`

**Purpose:** Test signing a simple ASCII message.

**Message:**

```text
Example `personal_sign` message
```

**What It Tests:**

- Basic personal_sign functionality
- ASCII message display
- Signature verification
- Common use case (short, readable message)

### `test_personal_sign_non_ascii`

**Purpose:** Test signing a non-ASCII (binary) message.

**Message:** Binary data (hex format)

**What It Tests:**

- Non-displayable message handling
- Binary data signing
- Message shown as hex on device
- Edge case for non-text data

### `test_personal_sign_opensea`

**Purpose:** Test signing a long multi-line message (real-world example).

**Message:**

```text
Welcome to OpenSea!

Click to sign in and accept the OpenSea Terms of Service: https://opensea.io/tos

This request will not trigger a blockchain transaction or cost any gas fees.

Your authentication status will reset after 24 hours.

Wallet address:
0x9876543210987654321098765432109876543210

Nonce:
2dbdec17-ea48-40d7-836f-a1cb548f2735
```

**What It Tests:**

- Multi-screen message display
- Real-world authentication flow
- Long message handling (multiple screens on device)
- URLs in messages
- Structured message with multiple sections

**Real-World Usage:**

This is the actual message format used by OpenSea for wallet authentication. Users sign this
message to prove they own the wallet address without paying gas fees.

### `test_personal_sign_reject`

**Purpose:** Test user rejection flow.

**What It Tests:**

- User can reject message signing
- Proper error handling on rejection

**Expected Result:** `CONDITION_NOT_SATISFIED` error

---

## Message Display on Device

### Short Messages (< 1 screen)

Displayed as single screen with full text.

### Long Messages (multiple screens)

- Paginated display
- User scrolls through screens
- Common for terms of service, authentication messages

### Non-ASCII Messages

- Displayed as hexadecimal
- Prefixed with warning about non-displayable content

---

## Security Considerations

### What Users Should Verify

✅ **Check the message content carefully**

- Signing a message can authorize actions
- Some dApps use signatures for authentication
- Malicious messages could grant unwanted permissions

⚠️ **Be cautious with:**

- Messages requesting token approvals
- Messages with unclear intent
- Messages from untrusted sources

### Safe Use Cases

✅ Authentication / Sign in  
✅ Proof of ownership  
✅ Off-chain voting  
✅ Terms of service acceptance

---

## Test Coverage Summary

| Test Function                    | Focus                           |
|----------------------------------|---------------------------------|
| `test_personal_sign_metamask`    | Simple ASCII message            |
| `test_personal_sign_non_ascii`   | Binary/hex message              |
| `test_personal_sign_opensea`     | Long multi-line message         |
| `test_personal_sign_reject`      | User rejection flow             |

**Message Types Covered:**

- ✅ Short ASCII messages
- ✅ Binary/non-ASCII data
- ✅ Long multi-screen messages
- ✅ Real-world authentication flows (OpenSea)
- ✅ User rejection

---

## Related Documentation

- [Main Test Overview](../test_overview.md#test_eip191py)
- [Quick Reference](../quick_reference.md)
- [Ethereum Glossary](../glossary.md) - Terminology and concepts reference
