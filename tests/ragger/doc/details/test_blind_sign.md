# test_blind_sign.py - Test Details

## Overview

Tests the **blind signing** functionality when the app cannot parse transaction details.

## Test Context

### Smart Contract Interaction

**Contract Type:** ERC-20 Token (DAI Stablecoin)

**Contract Address:** `0x6b175474e89094c44da98b954eedeac495271d0f` (Maker: Dai Stablecoin)

- 🔍 [View on Etherscan](https://etherscan.io/address/0x6b175474e89094c44da98b954eedeac495271d0f)

**Function Called:** `balanceOf(address)`

- Function Selector: `0x70a08231`
- Parameter: `0xd8dA6BF26964aF9D7eEd9e03E53415D37aA96045` (queried address)
- Purpose: Read the token balance of an address

**Why Blind Signing?**

This function is intentionally **not supported** by the internal plugin, forcing the app into blind
signing mode. Users must acknowledge they cannot verify the transaction details.

### Transaction Parameters

| Parameter       | Value                 | Purpose                                    |
|-----------------|-----------------------|--------------------------------------------|
| Chain ID        | `1`                   | Ethereum Mainnet                           |
| Nonce           | `235`                 | Transaction sequence number                |
| Gas Limit       | `44001`               | Maximum gas allowed                        |
| Max Fee         | `100 gwei`            | Maximum total fee per gas (EIP-1559)       |
| Priority Fee    | `10 gwei`             | Tip to validators (EIP-1559)               |
| ETH Value       | `0.0` or `1.2`        | ETH sent with transaction (parameterized)  |
| Derivation Path | `m/44'/60'/0'/0/0`    | Standard Ethereum account path             |

💡 For explanations of these terms, see the [Glossary](../glossary.md)

### Proxy Contract Testing

When testing proxy support (`with_proxy=True`):

**Proxy Address:** `0xCcCCccccCCCCcCCCCCCcCcCccCcCCCcCcccccccC`

**Implementation Address:** `0x6b175474e89094c44da98b954eedeac495271d0f` (same as DAI contract)

## Test Functions

### `test_blind_sign`

**Purpose:** Main blind signing test covering approval/rejection with various ETH amounts.

**What It Tests:**

- Blind signing approval and rejection flows
- Transactions with zero value (`0.0 ETH`) and non-zero value (`1.2 ETH`)
- Optional: Transaction simulation, security gating, and proxy contract support

**Test Configurations:**

Runs 3 combinations:

- ✅ Approve with 0.0 ETH
- ✅ Approve with 1.2 ETH
- ✅ Reject with 0.0 ETH
- ⏭️ Reject with 1.2 ETH (skipped - redundant)

**Optional Features:**

- Transaction simulation (`simu_params`)
- Security gating warnings (`gating_params`)
- Proxy contract testing (`with_proxy=True`)

### `test_blind_sign_nonce`

**Purpose:** Test blind signing with nonce display enabled.

**What It Tests:**

- Blind signing with nonce display setting enabled
- Transaction with 1.2 ETH value
- Nonce value (`235`) is shown on device screen

**Settings:** `BLIND_SIGNING` + `NONCE` display

### `test_blind_sign_reject_in_risk_review`

**Purpose:** Test rejection during the initial risk warning screen.

**What It Tests:**

- Users can reject a blind signing transaction at the first warning screen
- Transaction with 0.0 ETH value
- Early rejection before seeing full transaction details

**Expected Result:** `CONDITION_NOT_SATISFIED` error

### `test_sign_parameter_selector`

**Purpose:** Test debug mode that displays raw function selectors and parameters.

**What It Tests:**

- Debug data display showing function selector (`0x70a08231`)
- Parameter display (each 32-byte chunk shown separately)
- Transaction with 0.0 ETH value

**Settings:** `BLIND_SIGNING` + `DEBUG_DATA` display

**Debug Information Shown:**

- Function Selector: `0x70a08231` (first 4 bytes)
- Parameters: 1 parameter (address) displayed in 32-byte format

### `test_blind_sign_not_enabled_error`

**Purpose:** Test error handling when blind signing is required but not enabled.

**What It Tests:**

- App rejects unparsable transactions when blind signing setting is disabled
- Transaction with 0.0 ETH value
- Error protection for users

**Settings:** Blind signing **NOT enabled** (default state)

**Expected Result:** `INVALID_DATA` error - transaction is rejected

---

## Test Coverage Summary

| Test Function                            | Focus                                     |
|------------------------------------------|-------------------------------------------|
| `test_blind_sign`                        | Main approval/rejection flows             |
| `test_blind_sign_nonce`                  | Nonce display in blind signing            |
| `test_blind_sign_reject_in_risk_review`  | Early rejection at warning screen         |
| `test_sign_parameter_selector`           | Debug mode for raw data display           |
| `test_blind_sign_not_enabled_error`      | Error when feature disabled               |

---

## Related Documentation

- [Main Test Overview](../test_overview.md#test_blind_signpy)
- [Quick Reference](../quick_reference.md)
- [Ethereum Glossary](../glossary.md) - Terminology and concepts reference
