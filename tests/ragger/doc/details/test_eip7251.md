# test_eip7251.py

## Overview

Tests for EIP-7251 ("Increase the MAX_EFFECTIVE_BALANCE"). This EIP allows validators
to have an effective balance higher than 32 ETH (up to 2048 ETH) and enables
consolidation of multiple validators. This reduces validator set size and improves
network efficiency.

## Test Context

### EIP-7251 Consolidation Requests

The consolidation request mechanism uses a predeploy contract that accepts:

- **Source Public Key**: BLS12-381 public key (48 bytes) of validator to consolidate
  from
- **Target Public Key**: BLS12-381 public key (48 bytes) of validator to consolidate
  into

### Predeploy Contract

**Address**: `0x0000BBdDc7CE488642fb579F8B00f3a590007251`

This is the EIP-7251 predeploy contract on Ethereum mainnet. All consolidation
requests are sent to this address.

**Etherscan**:
[0x0000BBdDc7CE488642fb579F8B00f3a590007251](https://etherscan.io/address/0x0000BBdDc7CE488642fb579F8B00f3a590007251)

### Operations

- **Consolidate**: Merge two different validators (source → target)
- **Compound**: Increase a single validator's balance (source = target)

## Functions Tested

### test_eip7251_consolidate

**Purpose**: Consolidate two different validators

Calls the predeploy contract to consolidate two validators:

**Source Validator**:
`0xb26add12e8fd4bfc463ba29ed255afb478a1391bb39b1f5a97cfbd8300391ec9387b126e9cb3eaafdd910a7c6b1eeb72`

**Target Validator**:
`0xa5565b3a6a3818346d4610a6c771b45f55ba94e1cceb6525648b148c40cfafcc93611b6acf28ac5408860188ca01cff6`

This merges the source validator's stake into the target validator.

**Transaction Details**:

- To: `0x0000BBdDc7CE488642fb579F8B00f3a590007251`
- Data: Source pubkey (48 bytes) + Target pubkey (48 bytes)
- Total data: 96 bytes

### test_eip7251_compound

**Purpose**: Compound (increase balance of) a single validator

Calls the predeploy contract with the **same public key** as both source and target.
This signals to increase the validator's effective balance rather than merge with
another validator.

**Validator**:
`0xa09a2c66b3cc6253bd83e2d3aa698fcc760c8d03b4782870e0b2638dffefcc7bc9e053215b004faa6e15ac76cac10bc2`

**Transaction Details**:

- To: `0x0000BBdDc7CE488642fb579F8B00f3a590007251`
- Data: Validator pubkey (48 bytes) + Same pubkey (48 bytes)
- Total data: 96 bytes (same key repeated)

## Test Functions

Both tests follow the same pattern:

1. Construct consolidation request data (source pubkey + target pubkey)
2. Create transaction to predeploy contract
3. Sign transaction on device with address review
4. Verify signature recovers to device address

## Coverage Summary

| Feature                   | Coverage                         |
|:--------------------------|:---------------------------------|
| Validator Consolidation   | Two different validators         |
| Validator Compounding     | Same validator (self-reference)  |
| Predeploy Contract        | EIP-7251 system contract         |
| Public Keys               | BLS12-381 keys (48 bytes each)   |
| Transaction Signing       | EIP-1559 format                  |

## Related Documentation

- [Test Overview](../test_overview.md) - EIP-7251 overview
- [Glossary](../glossary.md) - Validator, effective balance
- [test_eth2.md](test_eth2.md) - ETH2 deposit tests
- [test_eip7002.md](test_eip7002.md) - Withdrawal requests
- [README](../README.md) - Test infrastructure

### External Resources

- [EIP-7251 Specification](https://eips.ethereum.org/EIPS/eip-7251) - Increase the
  MAX_EFFECTIVE_BALANCE
- [EIP-7251 Predeploy Contract](https://etherscan.io/address/0x0000BBdDc7CE488642fb579F8B00f3a590007251)
  - Etherscan
- [Validator Consolidation](https://ethereum.org/en/roadmap/single-slot-finality/) -
  Ethereum.org roadmap
