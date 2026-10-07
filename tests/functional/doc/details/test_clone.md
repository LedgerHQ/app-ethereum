# test_clone.py

## Overview

Tests for Ethereum clone chains. Clone chains are Ethereum-compatible blockchains
that use the same address derivation and transaction format but with different
chain IDs. This test verifies the app can sign transactions on clone chains.

## Test Context

### Clone Chains

Clone chains are Ethereum-compatible networks that:

- Use standard Ethereum transaction format
- Have unique chain IDs
- May use different derivation paths (m/44'/coin_type'/...)
- Are built on Ethereum codebase (forks or EVM-compatible)

### ThunderCore

ThunderCore is an EVM-compatible blockchain focused on high throughput and low fees.

- **Chain ID**: 108
- **Derivation Path**: `m/44'/1001'/0'/0/0`
- **Native Token**: TT (Thunder Token)

### Library Mode

Clone chain support requires the app to be in "library mode" which enables
additional chain configurations.

## Functions Tested

### test_clone_thundercore

**Purpose**: Sign transaction on ThunderCore network

Tests a simple ETH-style transfer on ThunderCore using the correct derivation
path and chain ID.

**Transaction Details**:

- To: `0x5a321744667052affa8386ed49e00ef223cbffc3`
- Value: 0.31415 TT (Thunder Token)
- Chain ID: 108 (ThunderCore mainnet)
- BIP32 Path: `m/44'/1001'/0'/0/0`

**Note**: This test requires special setup (`needs_setup='lib_mode'`).

## Test Functions

- `test_clone_thundercore`: Full transaction signing flow
  - Creates legacy transaction (type 0)
  - Signs with ThunderCore derivation path
  - Verifies signature on device
  - Compares with expected screenshots

## Coverage Summary

| Feature                 | Coverage                      |
|:------------------------|:------------------------------|
| Clone Chain Support     | ThunderCore (chain 108)       |
| Custom Derivation       | m/44'/1001'/... path          |
| Transaction Format      | Legacy Ethereum format        |
| Library Mode            | Clone configuration loading   |

## Related Documentation

- [Test Overview](../test_overview.md) - Clone chains overview
- [test_sign.md](test_sign.md) - Standard transaction signing
- [Glossary](../glossary.md) - Chain ID, derivation path
- [README](../README.md) - Test infrastructure

### External Resources

- [ThunderCore](https://www.thundercore.com/) - Official website
- [ThunderCore Explorer](https://viewblock.io/thundercore) - Block explorer
- [EVM Chains](https://chainlist.org/) - List of EVM-compatible chains
