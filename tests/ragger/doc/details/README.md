# Detailed Test Documentation

This directory contains in-depth documentation for each test file, with focus on contracts,
addresses, functions tested, and high-level test context.

## Available Detailed Documentation

### Basic Operations

- **[test_get_address.md](test_get_address.md)** - Public key and address retrieval
  - Ethereum accounts (secp256k1), ETH2 validator keys (BLS12-381)
  - Multi-chain support, dynamic networks

### Transaction Signing

- **[test_sign.md](test_sign.md)** - Standard Ethereum transaction signing
  - Legacy (Type 0), EIP-1559 (Type 2) transactions
  - Multi-chain, display options, edge cases

- **[test_blind_sign.md](test_blind_sign.md)** - Blind signing for unparsable transactions
  - DAI contract balanceOf calls
  - Debug mode, nonce display, proxy support

### Token Standards

- **[test_erc20.md](test_erc20.md)** - ERC-20 token operations
  - USDC contract: `0xA0b86991c6218b36c1d19D4a2e9Eb0cE3606eB48`
  - transfer(), approve() functions
  - Extra data handling

- **[test_nft.md](test_nft.md)** - NFT operations (ERC-721 & ERC-1155)
  - BAYC, y00ts, OpenSea collections
  - Multi-chain NFT support

### Message Signing

- **[test_eip191.md](test_eip191.md)** - Personal message signing
  - ASCII and binary messages
  - OpenSea authentication example

- **[test_eip712.md](test_eip712.md)** - Structured data signing
  - 50+ test scenarios
  - DeFi protocols, NFT marketplaces, DAO votes

### Ethereum 2.0 / Staking

- **[test_eth2.md](test_eth2.md)** - Beacon chain validator deposits
  - Deposit contract: `0x00000000219ab540356cBB839Cbe05303d7705Fa`
  - 32 ETH deposits, BLS keys

- **[test_eip7002.md](test_eip7002.md)** - Validator withdrawal requests (EIP-7002)
  - Predeploy contract: `0x00000961Ef480Eb55e80D19ad83579A64c007002`
  - Partial withdrawals, full validator exit

- **[test_eip7251.md](test_eip7251.md)** - Validator consolidation (EIP-7251)
  - Predeploy contract: `0x0000BBdDc7CE488642fb579F8B00f3a590007251`
  - Consolidate validators, compound balance

### Advanced Features

- **[test_gcs.md](test_gcs.md)** - Generic Clear Signing framework
  - OpenSea, POAP, 1inch, Safe operations
  - Field descriptors, formatters, nested calls

- **[test_tx_simulation.md](test_tx_simulation.md)** - Transaction security checks
  - Risk levels: benign, warning, threat
  - Works with all transaction types

- **[test_trusted_name.md](test_trusted_name.md)** - ENS and trusted name resolution
  - Display "ledger.eth" instead of addresses
  - ENS, CAL (Crypto Assets List)
  - Challenge-response security

- **[test_safe.md](test_safe.md)** - Safe (Gnosis Safe) multi-signature wallets
  - Safe and Signer descriptors
  - Multi-sig threshold configuration

- **[test_eip7702.md](test_eip7702.md)** - EOA delegation authorizations (EIP-7702)
  - Delegate contract execution to EOAs
  - Whitelist enforcement per chain

### Configuration & Compatibility

- **[test_configuration_cmd.md](test_configuration_cmd.md)** - App settings and version
  - Blind signing, transaction checks, nonce display
  - App version query

- **[test_clone.md](test_clone.md)** - Clone chain support
  - ThunderCore (chain 108)
  - Custom derivation paths

- **[test_privacy_operation.md](test_privacy_operation.md)** - Hardware crypto operations
  - Privacy-preserving cryptographic primitives
  - Physical devices only

## Documentation Focus

Each detailed test documentation emphasizes:

1. **Test Context** - What is being tested and why
2. **Smart Contracts** - Addresses with Etherscan/explorer links
3. **Functions Called** - Function selectors and parameters
4. **Test Functions** - High-level purpose of each test
5. **Test Coverage** - Summary of what's covered

For general Ethereum concepts (gas, wei, nonce, etc.), see the [Glossary](../glossary.md).
