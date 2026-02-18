# test_sign.py - Test Details

## Overview

Tests **standard Ethereum transaction signing** functionality for simple ETH transfers across
different transaction types and network configurations.

## Test Context

### Test Addresses

| Address      | Value                                        | Purpose                      |
|--------------|----------------------------------------------|------------------------------|
| ADDR         | `0x0011223344556677889900112233445566778899` | Main recipient               |
| ADDR2        | `0x5a321744667052affa8386ed49e00ef223cbffc3` | Alternative recipient        |
| ADDR3        | `0xdac17f958d2ee523a2206206994597c13d831ec7` | USDT contract (Mainnet)      |
| ADDR4        | `0xb2bb2b958afa2e96dab3f3ce7162b87daea39017` | Alternative recipient        |

### Common Transaction Parameters

| Parameter        | Value 1            | Value 2            | Purpose                     |
|------------------|--------------------|--------------------|-----------------------------|
| Chain ID         | `1`                | `56`               | Ethereum Mainnet / BSC      |
| Nonce            | `21` / `68`        | Various            | Transaction sequence number |
| Gas Limit        | `21,000`           | `21,000`           | Standard ETH transfer cost  |
| Gas Price        | `13 gwei`          | `5 gwei`           | Legacy transaction gas price|
| Max Fee          | `145 gwei`         | -                  | EIP-1559 max fee per gas    |
| Priority Fee     | `1.5 gwei`         | -                  | EIP-1559 priority fee       |
| ETH Amount       | `1.22 ETH`         | `0.31415 ETH`      | ETH sent with transaction   |
| Derivation Path  | `m/44'/60'/0'/0/0` | `m/44'/60'/1'/0/0` | Different account paths     |

💡 For explanations of these terms, see the [Glossary](../glossary.md)

### Transaction Types Tested

#### Type 0 - Legacy Transactions

- Uses `gasPrice` parameter
- Pre-EIP-1559 format
- Still widely supported

#### Type 1 - EIP-2930 Access List Transactions

- Adds `accessList` parameter
- Optimizes gas for contracts that access specific storage slots
- Not commonly used in tests (minimal difference for simple transfers)

#### Type 2 - EIP-1559 Transactions

- Uses `maxFeePerGas` and `maxPriorityFeePerGas` instead of `gasPrice`
- Dynamic fee market
- More predictable fees
- Current standard

📖 [EIP-1559 Explained](https://eips.ethereum.org/EIPS/eip-1559)  
📖 [EIP-2930 Access Lists](https://eips.ethereum.org/EIPS/eip-2930)

---

## Test Functions

### Basic Transaction Tests

#### `test_legacy`

**Purpose:** Test legacy (Type 0) transaction signing.

**What It Tests:**

- Basic ETH transfer using legacy format
- Gas price calculation
- Transaction with 1.22 ETH
- Chain ID 1 (Ethereum Mainnet)

#### `test_legacy_send_error`

**Purpose:** Test error handling for invalid transactions.

**What It Tests:**

- Overflow detection in transaction parameters
- Proper error codes for invalid data

**Expected Result:** `EXCEPTION_OVERFLOW` error

#### `test_sign_legacy_send_bsc`

**Purpose:** Test legacy transaction on BSC (Binance Smart Chain).

**What It Tests:**

- Multi-chain support (Chain ID 56)
- Different derivation path (`m/44'/60'/1'/0/0`)
- Transfer 0.31415 ETH
- 5 gwei gas price

#### `test_sign_legacy_chainid`

**Purpose:** Test legacy transaction with explicit chain ID handling.

**What It Tests:**

- Legacy format with post-EIP-155 chain ID encoding
- Chain ID 1 with account path 1

#### `test_1559`

**Purpose:** Test EIP-1559 (Type 2) transaction signing.

**What It Tests:**

- Modern gas fee model (maxFeePerGas, maxPriorityFeePerGas)
- Transfer 1.22 ETH
- Max fee: 145 gwei, Priority fee: 1.5 gwei
- Default account path

### Display and Settings Tests

#### `test_sign_simple`

**Purpose:** Test transaction signing with hash display toggle.

**What It Tests:**

- Transaction with nonce 68
- Transfer 0.31415 ETH
- Transaction hash display mode (on/off via fixture)

**Test Configurations:**

- With hash display enabled
- With hash display disabled

#### `test_sign_limit_nonce`

**Purpose:** Test transaction with maximum nonce value.

**What It Tests:**

- Nonce = `0xFFFFFFFFFFFFFF` (maximum 7-byte value)
- Edge case handling for large nonces
- Display of large numbers

#### `test_sign_nonce_display`

**Purpose:** Test nonce display setting.

**What It Tests:**

- Enable nonce display setting
- Nonce value shown on device screen
- Transaction with nonce 253

### Network Configuration Tests

#### `test_sign_custom_network`

**Purpose:** Test dynamic network configuration.

**What It Tests:**

- Providing custom network information (name, ticker, icon)
- Network display on device
- Chain ID 1337 (custom test network)

#### `test_sign_with_network_config`

**Purpose:** Test transaction with pre-configured network.

**What It Tests:**

- Using provided network configuration
- Network name display

### Data Field Tests

#### `test_legacy_data_param_len`

**Purpose:** Test transaction with data field (smart contract interaction).

**What It Tests:**

- Transaction with arbitrary data payload
- Data field of specific length
- Still treated as simple transaction (not contract call)

### Edge Cases

#### `test_sign_self_transfer`

**Purpose:** Test sending ETH to same address (self-transfer).

**What It Tests:**

- Sender == Recipient
- Warning display for self-transfers
- Valid but unusual transaction

---

## Test Coverage Summary

| Category                | Tests Covered                                                      |
|-------------------------|--------------------------------------------------------------------|
| **Transaction Types**   | Legacy (Type 0), EIP-1559 (Type 2)                                 |
| **Networks**            | Ethereum Mainnet, BSC, Custom networks                             |
| **Gas Models**          | Legacy gas price, EIP-1559 dynamic fees                            |
| **Display Options**     | Transaction hash display, Nonce display                            |
| **Edge Cases**          | Maximum nonce, Self-transfers, Data field                          |
| **Error Handling**      | Overflow errors, Invalid parameters                                |
| **Multi-account**       | Multiple BIP32 derivation paths                                    |

---

## Related Documentation

- [Main Test Overview](../test_overview.md#test_signpy)
- [Quick Reference](../quick_reference.md)
- [Ethereum Glossary](../glossary.md) - Terminology and concepts reference
