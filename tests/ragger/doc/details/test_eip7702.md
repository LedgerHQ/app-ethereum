# test_eip7702.py

## Overview

Tests for EIP-7702 ("Set EOA account code") authorization signatures. EIP-7702 allows
Externally Owned Accounts (EOAs) to temporarily delegate their execution to a smart
contract during a transaction. This enables EOAs to act like smart contract wallets
without permanently converting them.

## Test Context

### EIP-7702 Authorization

An EIP-7702 authorization specifies:

- **Delegate Address**: The contract address to delegate execution to
- **Nonce**: Authorization nonce (prevents replay)
- **Chain ID**: Network where authorization is valid

The Ledger app maintains a whitelist of approved delegate contracts for security.

### Whitelist & Security

The app whitelists specific delegate addresses per chain ID to prevent malicious
delegation. Some contracts are whitelisted for all chains (chain ID 0).

### Test Addresses

- **TEST_ADDRESS_1**: `0x0101010101010101010101010101010101010101` (chain 1)
- **TEST_ADDRESS_2**: `0x0202020202020202020202020202020202020202` (chain 2, 0)
- **TEST_ADDRESS_MAX**: `0xffffffffffffffffffffffffffffffffffffffff` (max values)
- **TEST_ADDRESS_NO_WHITELIST**: `0x4242424242424242424242424242424242424242`
  (not whitelisted)
- **SimplEIP7702Account**: `0x4Cd241E8d1510e30b2076397afc7508Ae59C66c9`
  (whitelisted for all chains)
- **ADDRESS_REVOCATION**: `0x0000000000000000000000000000000000000000` (revokes
  delegation)

## Functions Tested

### test_eip7702_in_whitelist

**Purpose**: Sign authorization for whitelisted delegate on correct chain

Delegates to TEST_ADDRESS_1 on chain 1 with nonce 1337.

### test_eip7702_in_whitelist_all_chain_whitelisted

**Purpose**: Test delegate whitelisted for all chains

Uses SimplEIP7702Account (`0x4Cd2...66c9`) which works on any chain ID.

### test_eip7702_in_whitelist_all_chain_param

**Purpose**: Test with chain ID 0 (all chains parameter)

Delegates to TEST_ADDRESS_2 with chain ID 0.

### test_eip7702_in_whitelist_max

**Purpose**: Test maximum values

Uses TEST_ADDRESS_MAX (`0xfff...fff`) with maximum nonce and chain ID values.

### test_eip7702_in_whitelist_wrong_chain

**Purpose**: Verify rejection of delegate on wrong chain

TEST_ADDRESS_2 is whitelisted for chain 2 but not chain 1 - should reject.

### test_eip7702_not_in_whitelist

**Purpose**: Verify rejection of non-whitelisted delegate

TEST_ADDRESS_NO_WHITELIST (`0x424...242`) is not whitelisted - should reject.

### test_eip7702_not_enabled

**Purpose**: Verify rejection when EIP-7702 setting is disabled

Authorization should fail if the EIP-7702 setting is not enabled on device.

### test_eip7702_revocation

**Purpose**: Sign revocation authorization

Delegates to ADDRESS_REVOCATION (`0x000...000`) which revokes any active delegation.

## Test Functions

All tests follow the pattern:

1. Enable EIP-7702 setting (if applicable)
2. Create authorization with delegate address, nonce, chain ID
3. Sign authorization on device
4. Verify signature recovers to device address (success tests)
5. Verify rejection with COMMAND_NOT_ALLOWED (error tests)

## Coverage Summary

| Feature                     | Coverage                        |
|:----------------------------|:--------------------------------|
| Authorization Signing       | Valid delegate addresses        |
| Whitelist Enforcement       | Per-chain and all-chain         |
| Security Controls           | Non-whitelisted rejection       |
| Maximum Values              | Max nonce, chain ID, address    |
| Revocation                  | Address 0x00...00               |
| Settings                    | EIP-7702 enable/disable         |
| Error Cases                 | Wrong chain, disabled, invalid  |

## Related Documentation

- [Test Overview](../test_overview.md) - EIP-7702 overview
- [Glossary](../glossary.md) - Nonce, chain ID concepts
- [README](../README.md) - Test infrastructure

### External Resources

- [EIP-7702 Specification](https://eips.ethereum.org/EIPS/eip-7702) - Set EOA account
  code
- [EIP-7702 Discussion](https://ethereum-magicians.org/t/eip-7702-set-eoa-account-code/19923)
  - Ethereum Magicians forum
