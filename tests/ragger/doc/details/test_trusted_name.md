# test_trusted_name.py

## Overview

Tests for Trusted Name feature. This feature allows the Ledger device to display
human-readable names (like ENS domains) instead of raw Ethereum addresses, improving
user experience and security. Names are provided by trusted sources and verified
using challenge-response authentication.

## Test Context

### Trusted Name Versions

Two protocol versions are tested:

- **Version 1**: Basic trusted name with coin type
- **Version 2**: Enhanced with type, source, and chain ID

### Trusted Name Fields

- **Address**: The Ethereum address (`0x0011223344556677889900112233445566778899`)
- **Name**: Human-readable name (`ledger.eth`)
- **Type**: ACCOUNT or CONTRACT (v2 only)
- **Source**: ENS, CAL (Crypto Assets List), or other (v2 only)
- **Chain ID**: Network where name is valid (v2 only)
- **Challenge**: Random value for challenge-response security

### Name Services

- **ENS (Ethereum Name Service)**: Domain names ending in `.eth`
- **CAL (Crypto Assets List)**: Ledger's curated contract registry

## Functions Tested

### test_trusted_name_v1

**Purpose**: Basic trusted name flow (version 1)

Provides trusted name "ledger.eth" for address and signs a transaction. The device
displays the name instead of the raw address during review.

**Transaction Details**:

- To: `0x0011223344556677889900112233445566778899` (displayed as "ledger.eth")
- Value: 1.22 ETH
- Chain: Mainnet (1)

### test_trusted_name_v1_verbose

**Purpose**: Verbose trusted name display

Similar to v1 but with verbose UI display mode.

### test_trusted_name_v1_wrong_challenge

**Purpose**: Verify challenge-response security

Provides trusted name with wrong challenge value. Should be rejected with
INVALID_DATA error.

### test_trusted_name_v1_wrong_addr

**Purpose**: Verify address mismatch detection

Transaction uses different address than trusted name. Name should not be displayed.

### test_trusted_name_v1_non_mainnet

**Purpose**: Test trusted name on non-mainnet chains

Tests trusted name on Goerli testnet (chain 5) with dynamic network information.

### test_trusted_name_v1_unknown_chain

**Purpose**: Test on unsupported chain

Uses chain ID 9 (unsupported). Transaction should still proceed but may show warnings.

### test_trusted_name_v1_name_too_long

**Purpose**: Verify name length validation

Tests that names longer than 30 characters are rejected with INVALID_DATA error.

**Test Name**: `ledger0000000000000000000000000.eth` (35 characters)

### test_trusted_name_v1_name_invalid_character

**Purpose**: Verify character validation

Tests that non-ASCII characters (e.g., `è`) are rejected with INVALID_DATA error.

**Test Name**: `lèdger.eth` (contains è)

### test_trusted_name_v1_uppercase

**Purpose**: Verify lowercase requirement

ENS names must be lowercase. Uppercase should be rejected with INVALID_DATA error.

**Test Name**: `LEDGER.ETH`

### test_trusted_name_v1_name_non_ens

**Purpose**: Verify ENS TLD requirement

Only `.eth` TLD is supported. Other TLDs should be rejected with INVALID_DATA error.

**Test Name**: `ledger.hte`

### test_trusted_name_v2

**Purpose**: Enhanced trusted name flow (version 2)

Uses v2 protocol with full metadata (type=ACCOUNT, source=ENS, chain_id=1).

### test_trusted_name_v2_wrong_chainid

**Purpose**: Verify chain ID matching

Trusted name specifies chain 2 but transaction uses chain 1. Should be rejected.

### test_trusted_name_v2_missing_challenge

**Purpose**: Verify challenge requirement

V2 without challenge should be rejected with INVALID_DATA error.

### test_trusted_name_v2_expired

**Purpose**: Verify challenge expiration

Challenge expires after some operations. Using expired challenge should fail.

### test_trusted_name_mab_idle

**Purpose**: Test MAB (Mutual Authentication Block) for CAL

MAB provides cryptographic proof for Crypto Assets List names. Tests idle state.

### test_trusted_name_mab_wrong_owner

**Purpose**: Verify MAB owner validation

Tests that MAB with wrong owner key is rejected.

## Test Functions

Most tests follow this pattern:

1. Request challenge from device
2. Provide trusted name with challenge (and optional metadata)
3. Create transaction to the address
4. Verify name is displayed during review (success cases)
5. Verify proper error handling (failure cases)

## Coverage Summary

| Feature                   | Coverage                          |
|:--------------------------|:----------------------------------|
| Protocol Versions         | V1 and V2                         |
| Name Sources              | ENS, CAL (with MAB)               |
| Name Validation           | Length, characters, format, TLD   |
| Challenge Security        | Challenge-response, expiration    |
| Chain Support             | Mainnet, testnets, unknown chains |
| Address Matching          | Correct and incorrect addresses   |
| Error Handling            | All validation failures           |

## Related Documentation

- [Test Overview](../test_overview.md) - Trusted name overview
- [Glossary](../glossary.md) - ENS concepts
- [README](../README.md) - Test infrastructure

### External Resources

- [ENS (Ethereum Name Service)](https://ens.domains/) - Official ENS website
- [ENS Documentation](https://docs.ens.domains/) - How ENS works
- [Ledger CAL](https://github.com/LedgerHQ/ledger-live-common/tree/master/src/data/cal)
  - Crypto Assets List
