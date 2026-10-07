# test_eth2.py - Test Details

## Overview

Tests **Ethereum 2.0 beacon chain deposit** functionality for validator deposits.

## Test Context

### Beacon Chain Deposit Contract

**Contract Address:** `0x00000000219ab540356cBB839Cbe05303d7705Fa`

- 🔍 [View on Etherscan](https://etherscan.io/address/0x00000000219ab540356cBB839Cbe05303d7705Fa)
- **Type:** System contract for ETH2 staking
- **Purpose:** Receives validator deposits (32 ETH minimum)
- **Network:** Ethereum Mainnet (Chain ID: 1)

### Validator Deposit Requirements

| Requirement              | Value                      |
|--------------------------|----------------------------|
| Minimum deposit          | 32 ETH                     |
| Validator public key     | 48 bytes (BLS12-381)       |
| Withdrawal credentials   | 32 bytes                   |
| Signature                | 96 bytes (BLS signature)   |

### BLS Keys for ETH2

**Curve:** BLS12-381 (different from account keys)

**Why different?**

- Supports signature aggregation (important for consensus)
- More efficient for validator operations
- Better suited for beacon chain requirements

💡 For more on cryptographic curves, see the [Glossary](../glossary.md)

### Withdrawal Credentials

**Format:** `0x01` prefix + execution address

```text
0x01 + 00...00 + <20-byte execution address>
```

**Purpose:**

- Specifies where to withdraw staked ETH later
- `0x01` prefix indicates withdrawal to execution layer address
- Uses `BLS_WITHDRAWAL_PREFIX` constant

📖 [ETH2 Staking Documentation](https://ethereum.org/en/staking/)

---

## Test Functions

### `test_eth2_deposit`

**Purpose:** Test full beacon chain validator deposit transaction.

**What It Tests:**

- Create 32 ETH deposit transaction
- Display validator public key (BLS)
- Proper withdrawal credentials generation
- Transaction to deposit contract
- Signature verification

**Transaction Details:**

| Parameter       | Value                  | Purpose                          |
|-----------------|------------------------|----------------------------------|
| To              | Deposit contract       | `0x00000...7705Fa`               |
| Value           | 32 ETH (or custom)     | Validator deposit amount         |
| Chain ID        | 1                      | Ethereum Mainnet                 |
| Data            | Validator info         | Public key, credentials, etc.    |

**Data Field Contains:**

1. Validator public key (48 bytes)
2. Withdrawal credentials (32 bytes)
3. Deposit amount (8 bytes, in Gwei)
4. BLS signature (96 bytes)

---

## Staking Process Overview

### 1. Generate Validator Keys

- Validator private key (BLS12-381)
- Validator public key (48 bytes)
- Withdrawal key

### 2. Create Deposit Transaction

- Send 32 ETH to deposit contract
- Include validator public key
- Specify withdrawal credentials

### 3. Activation

- Wait for deposit to be processed by beacon chain
- Validator activates after ~12-24 hours
- Begin validating and earning rewards

### 4. Withdrawal (Eventually)

- Use withdrawal credentials to reclaim staked ETH
- Requires withdrawal/exit mechanism

---

## Security Considerations

### What Users Should Verify

✅ **Deposit Amount:** Typically 32 ETH  
✅ **Deposit Contract Address:** Must be the official contract  
✅ **Validator Public Key:** Matches your generated validator  
✅ **Withdrawal Credentials:** Contains your withdrawal address

⚠️ **Important:**

- Deposits are irreversible
- Funds locked until withdrawals enabled
- Wrong withdrawal credentials = lost access

---

## Test Coverage Summary

| Test Function       | Focus                              |
|---------------------|------------------------------------|
| `test_eth2_deposit` | Full validator deposit transaction |

**Features Tested:**

- ✅ 32 ETH deposit creation
- ✅ BLS public key display
- ✅ Withdrawal credentials generation
- ✅ Deposit contract interaction
- ✅ Signature verification

---

## Related Documentation

- [Main Test Overview](../test_overview.md#test_eth2py)
- [Quick Reference](../quick_reference.md)
- [Ethereum Glossary](../glossary.md) - Terminology and concepts reference
- [test_eip7002.md](test_eip7002.md) - Validator withdrawal requests
- [test_eip7251.md](test_eip7251.md) - Validator consolidation
