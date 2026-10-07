# test_get_address.py - Test Details

## Overview

Tests **public key and address retrieval** functionality for Ethereum accounts, including multi-chain
support and Ethereum 2.0 BLS keys.

## Test Context

### Cryptographic Key Types

#### Ethereum Accounts (secp256k1)

- **Curve:** secp256k1 (same as Bitcoin)
- **Public Key:** 65 bytes (uncompressed)
- **Address:** 20 bytes (derived from public key)
- **Use:** Standard Ethereum transactions

#### Ethereum 2.0 Validators (BLS12-381)

- **Curve:** BLS12-381
- **Public Key:** 48 bytes
- **Use:** Beacon chain validator keys
- **Feature:** Supports signature aggregation

💡 For more on elliptic curves, see the [Glossary](../glossary.md)

### Derivation Paths Tested

| Path                | Purpose                          |
|---------------------|----------------------------------|
| `m/44'/60'/0'/0/0`  | Standard Ethereum account #1     |
| `m/44'/60'/0'/0/1`  | Standard Ethereum account #2     |
| `m/44'/60'/1'/0/0`  | Alternative account path         |

### Chain IDs Tested

| Chain ID | Network               |
|----------|-----------------------|
| None     | Generic (no network)  |
| 1        | Ethereum Mainnet      |
| 2        | Expanse               |
| 5        | Goerli Testnet        |
| 137      | Polygon               |

---

## Test Functions

### `test_get_pk_rejected`

**Purpose:** Test user rejection of address display.

**What It Tests:**

- User can reject address verification request
- Proper error handling

**Expected Result:** `CONDITION_NOT_SATISFIED` error

### `test_get_pk`

**Purpose:** Test address retrieval with various configurations.

**What It Tests:**

- Retrieve public key and address
- With/without chaincode
- Different chain IDs (1, 2, 5, 137, None)
- User approval flow
- Address display on device

**Test Matrix:**

- Multiple derivation paths × Multiple chain IDs × Display options

### `test_get_eth2_pk`

**Purpose:** Test Ethereum 2.0 BLS public key retrieval.

**What It Tests:**

- BLS12-381 public key generation
- Different from standard secp256k1 keys
- Used for beacon chain validators
- 48-byte public key format

**Derivation Path:** `m/12381/3600/0/0` (ETH2 validator path)

---

## Dynamic Network Support

Some tests provide custom network information to display on the device:

### Network Configuration

- **Network Name:** Custom network name (e.g., "Custom Network")
- **Network Ticker:** Token symbol (e.g., "CUST")
- **Chain ID:** Network identifier
- **Icon:** Optional network icon

**Use Case:** Supporting networks not pre-configured in the app.

---

## Test Coverage Summary

| Test Function          | Focus                                 |
|------------------------|---------------------------------------|
| `test_get_pk_rejected` | User rejection flow                   |
| `test_get_pk`          | Address retrieval with various configs|
| `test_get_eth2_pk`     | ETH2 BLS public keys                  |

**Key Features:**

- ✅ Standard Ethereum addresses (secp256k1)
- ✅ ETH2 validator keys (BLS12-381)
- ✅ Multi-chain support
- ✅ Dynamic network configuration
- ✅ User approval/rejection flows

---

## Related Documentation

- [Main Test Overview](../test_overview.md#test_get_addresspy)
- [Quick Reference](../quick_reference.md)
- [Ethereum Glossary](../glossary.md) - Terminology and concepts reference
