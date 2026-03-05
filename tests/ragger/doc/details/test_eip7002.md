# test_eip7002.py

## Overview

Tests for EIP-7002 ("Execution layer triggerable withdrawals"). This EIP allows
validators to trigger withdrawals from the execution layer (L1) instead of only
from the consensus layer (Beacon Chain). This provides more flexibility for
validator operations.

## Test Context

### EIP-7002 Withdrawal Requests

The withdrawal request mechanism uses a predeploy contract that accepts:

- **Validator Public Key**: BLS12-381 public key (48 bytes) identifying the validator
- **Amount**: Withdrawal amount in Gwei (0 = full exit)

### Predeploy Contract

**Address**: `0x00000961Ef480Eb55e80D19ad83579A64c007002`

This is the EIP-7002 predeploy contract on Ethereum mainnet. All withdrawal requests
are sent to this address.

**Etherscan**:
[0x00000961Ef480Eb55e80D19ad83579A64c007002](https://etherscan.io/address/0x00000961Ef480Eb55e80D19ad83579A64c007002)

### Test Validator

All tests use the same test validator public key:

`0xb8c426e9a7fa4dfa3ac1f20de1e151f35a6f30ca78944b9774ef8a25c954acce4570d876fc85f32698fbc7430c9fb9a4`

## Functions Tested

### test_eip7002_partial_withdrawal

**Purpose**: Request partial withdrawal from validator

Calls the predeploy contract to request a partial withdrawal of **13.37 Gwei**
from the validator. The validator remains active.

**Transaction Details**:

- To: `0x00000961Ef480Eb55e80D19ad83579A64c007002`
- Data: Validator pubkey (48 bytes) + amount (8 bytes)
- Amount: 13.37 Gwei (13,370,000,000 wei)

### test_eip7002_exit

**Purpose**: Request full exit of validator

Calls the predeploy contract to request a **full exit** (amount = 0 Gwei).
This triggers the validator to exit and withdraw all staked ETH.

**Transaction Details**:

- To: `0x00000961Ef480Eb55e80D19ad83579A64c007002`
- Data: Validator pubkey (48 bytes) + amount (8 bytes, value 0)
- Amount: 0 Gwei (full exit signal)

## Test Functions

Both tests follow the same pattern:

1. Construct withdrawal request data (validator pubkey + amount)
2. Create transaction to predeploy contract
3. Sign transaction on device with address review
4. Verify signature recovers to device address

## Coverage Summary

| Feature                | Coverage                       |
|:-----------------------|:-------------------------------|
| Partial Withdrawal     | Non-zero amount (13.37 Gwei)   |
| Full Exit              | Zero amount (exit validator)   |
| Predeploy Contract     | EIP-7002 system contract       |
| Validator Public Key   | BLS12-381 key (48 bytes)       |
| Transaction Signing    | EIP-1559 format                |

## Related Documentation

- [Test Overview](../test_overview.md) - EIP-7002 overview
- [Glossary](../glossary.md) - Gwei, validator concepts
- [test_eth2.md](test_eth2.md) - ETH2 deposit tests
- [README](../README.md) - Test infrastructure

### External Resources

- [EIP-7002 Specification](https://eips.ethereum.org/EIPS/eip-7002) - Execution layer
  triggerable withdrawals
- [EIP-7002 Predeploy Contract](https://etherscan.io/address/0x00000961Ef480Eb55e80D19ad83579A64c007002)
  - Etherscan
- [Validator Withdrawals](https://ethereum.org/en/staking/withdrawals/) - Ethereum.org
  guide
