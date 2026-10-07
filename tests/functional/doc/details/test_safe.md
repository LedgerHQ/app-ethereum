# test_safe.py

## Overview

Tests for Safe (formerly Gnosis Safe) account descriptors. Safe is the most trusted
multi-signature wallet on Ethereum, managing over $100 billion in digital assets.
Safe accounts require multiple signers to authorize transactions.

## Test Context

### Safe Account Types

Two types of Safe accounts are tested:

- **SAFE Account**: The actual Safe multisig contract that holds assets
- **SIGNER Account**: Individual addresses that are authorized signers for a Safe

### Safe Descriptor Fields

- **Role**: PROPOSER or SIGNER (for LES - Ledger Enterprise Service)
- **Threshold**: Minimum number of signatures required
- **Signers**: List of authorized signer addresses

### Addresses Used

The tests use generated test addresses (repeated hex patterns like "0x111...111").
No real Safe contracts on mainnet are directly referenced.

## Functions Tested

### test_safe_descriptor

**Purpose**: Verify Safe descriptor APDU handling

Tests providing two descriptors:

1. **SIGNER descriptor** with 15 signers (maximum supported)
2. **SAFE descriptor** with:
   - Safe address: `0x5a321744667052affa8386ed49e00ef223cbffc3`
   - Role: PROPOSER
   - Threshold: 5 signatures required
   - References 15 signers from the first descriptor

### test_signer_descriptor_error

**Purpose**: Verify error handling for malformed signer descriptors

Tests that invalid signer configurations are properly rejected.

## Test Functions

- `test_safe_descriptor`: Safe and signer descriptor flow
  - Provides SIGNER descriptor (no UI navigation)
  - Provides SAFE descriptor with address review
- `test_signer_descriptor_error`: Invalid descriptor handling

## Coverage Summary

| Feature                        | Coverage                    |
|:-------------------------------|:----------------------------|
| Safe Descriptors               | SAFE and SIGNER types       |
| Multi-signature Configuration  | Threshold and signers       |
| Error Handling                 | Invalid descriptors         |
| Challenge-Response             | Secure descriptor provision |

## Related Documentation

- [Test Overview](../test_overview.md) - Safe functionality overview
- [Glossary](../glossary.md) - Ethereum concepts
- [README](../README.md) - Test infrastructure

### External Resources

- [Safe (Gnosis Safe)](https://safe.global/) - Official Safe website
- [Safe Contracts](https://github.com/safe-global/safe-contracts) - Smart contracts
- [What is Safe](https://help.safe.global/en/articles/40868-what-is-safe) - User guide
