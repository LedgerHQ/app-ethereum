# test_erc20.py - Test Details

## Overview

Tests the **ERC-20 token** operations using the internal plugin for token transfers and approvals.

## Test Context

### Token Used for Testing

**Token:** USDC (USD Coin)

**Contract Address:** `0xA0b86991c6218b36c1d19D4a2e9Eb0cE3606eB48`

- 🔍 [View on Etherscan](https://etherscan.io/address/0xA0b86991c6218b36c1d19D4a2e9Eb0cE3606eB48)
- **Type:** ERC-20 Stablecoin
- **Decimals:** 6
- **Chain ID:** 1 (Ethereum Mainnet)
- **Issuer:** Circle

### Functions Tested

#### `transfer(address recipient, uint256 amount)`

- **Function Selector:** `0xa9059cbb`
- **Recipient:** `0xd8dA6BF26964aF9D7eEd9e03E53415D37aA96045`
- **Purpose:** Transfer tokens to another address

#### `approve(address spender, uint256 amount)`

- **Function Selector:** `0x095ea7b3`
- **Spender:** `0xd8dA6BF26964aF9D7eEd9e03E53415D37aA96045`
- **Purpose:** Authorize another address to spend tokens on your behalf

📖 [ERC-20 Standard](https://eips.ethereum.org/EIPS/eip-20)

### Transaction Parameters

| Parameter       | Transfer Value    | Approve Value   | Purpose                              |
|-----------------|-------------------|-----------------|--------------------------------------|
| Chain ID        | `1`               | `1`             | Ethereum Mainnet                     |
| Nonce           | `1337`            | `1337`          | Transaction sequence number          |
| Gas Limit       | `94,548`          | `36,007`        | Maximum gas allowed                  |
| Max Fee         | `2.55 gwei`       | `2.62 gwei`     | Maximum total fee per gas (EIP-1559) |
| Priority Fee    | `0 gwei`          | `1.3 gwei`      | Tip to validators (EIP-1559)         |
| ETH Value       | `0`               | `0`             | No ETH sent (token operation only)   |
| To              | USDC contract     | USDC contract   | Token contract address               |

💡 For explanations of these terms, see the [Glossary](../glossary.md)

---

## Test Functions

### `test_provide_erc20_token`

**Purpose:** Test token metadata provisioning.

**What It Tests:**

- App can receive and store token information (ticker, address, decimals, chain)
- Enables app to display human-readable token amounts

**Expected Result:** Success status

### `test_provide_erc20_token_error`

**Purpose:** Test error handling for invalid token metadata.

**What It Tests:**

- App rejects malformed token metadata
- Proper validation of token information

**Expected Result:** `INVALID_DATA` error

### `test_transfer_erc20`

**Purpose:** Test basic ERC-20 token transfer.

**What It Tests:**

- Transfer 10 USDC to recipient
- App displays amount with correct decimals (10.000000 USDC)
- Standard ERC-20 transfer flow

### `test_approve_erc20`

**Purpose:** Test ERC-20 token approval.

**What It Tests:**

- Approve 5 USDC spending allowance
- App displays spender address and amount
- Standard ERC-20 approve flow

### Extra Data Tests

Some dApps append extra data to ERC-20 function calls for tracking purposes. This data is not
part of the ERC-20 standard and is not processed by the smart contract.

#### `test_transfer_erc20_extra_data`

**What It Tests:**

- Transfer with ASCII extra data appended (`cpis_1RnzUSEXxObdZZOcn8gPzPPS`)
- App handles extra data gracefully

#### `test_transfer_erc20_extra_data_nonascii`

**What It Tests:**

- Transfer with non-ASCII extra data (hex: `deadcafe0042`)
- App truncates or ignores non-displayable data

#### `test_transfer_erc20_extra_data_toolong`

**What It Tests:**

- Transfer with excessively long extra data (>100 chars)
- App rejects transaction to prevent display issues

**Expected Result:** `INVALID_DATA` error

#### `test_transfer_erc20_extra_data_nonascii_truncated`

**What It Tests:**

- Transfer with long non-ASCII extra data (64 bytes)
- App handles truncation properly

#### `test_approve_erc20_extra_data`

**What It Tests:**

- Approve with ASCII extra data appended
- Extra data handling for approve operations

#### `test_approve_erc20_extra_data_nonascii`

**What It Tests:**

- Approve with non-ASCII extra data
- Extra data handling for approve operations

---

## Test Coverage Summary

| Test Function                                       | Focus                                |
|-----------------------------------------------------|--------------------------------------|
| `test_provide_erc20_token`                          | Token metadata provisioning          |
| `test_provide_erc20_token_error`                    | Error handling for invalid metadata  |
| `test_transfer_erc20`                               | Basic token transfer                 |
| `test_approve_erc20`                                | Token spending approval              |
| `test_transfer_erc20_extra_data`                    | Transfer with ASCII extra data       |
| `test_transfer_erc20_extra_data_nonascii`           | Transfer with binary extra data      |
| `test_transfer_erc20_extra_data_toolong`            | Reject overly long extra data        |
| `test_transfer_erc20_extra_data_nonascii_truncated` | Handle truncated binary data         |
| `test_approve_erc20_extra_data`                     | Approve with ASCII extra data        |
| `test_approve_erc20_extra_data_nonascii`            | Approve with binary extra data       |

---

## Related Documentation

- [Main Test Overview](../test_overview.md#test_erc20py)
- [Quick Reference](../quick_reference.md)
- [Ethereum Glossary](../glossary.md) - Terminology and concepts reference
