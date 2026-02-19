# Ethereum Ledger App - Test Suite Overview

This document provides a comprehensive overview of the Ethereum Ledger application functionalities
tested using the Ragger framework with Speculos emulator.

## Table of Contents

1. [Basic Operations](#1-basic-operations)
2. [Transaction Signing](#2-transaction-signing)
3. [Token Standards](#3-token-standards)
4. [Message Signing](#4-message-signing)
5. [Ethereum 2.0 / Staking Operations](#5-ethereum-20--staking-operations)
6. [Advanced Features](#6-advanced-features)
7. [External Chains](#7-external-chains)
8. [Privacy Operations](#8-privacy-operations)

---

## 1. Basic Operations

### test_get_address.py

**Purpose:** Test public key and address retrieval functionality.

**Key Features Tested:**

- **Public Key Retrieval**: Get Ethereum (`secp256k1`) public keys with and without chaincode
- **Multi-chain Support**: Test address retrieval for different chain IDs (None, 1, 2, 5, 137)
- **Dynamic Networks**: Provide custom network information (name, ticker, icon) for display
- **ETH2 Public Keys**: Get Ethereum 2.0 (`BLS12-381`) public keys for staking
- **User Approval/Rejection**: Test both approve and reject flows for address verification

**Test Cases:**

- `test_get_pk_rejected`: User rejects the address display request
- `test_get_pk`: User approves address display with various configurations
- `test_get_eth2_pk`: Retrieve Ethereum 2.0 BLS public key

---

### test_configuration_cmd.py

**Purpose:** Test application configuration and settings management.

**Key Features Tested:**

- **Settings Toggle**: Test enabling/disabling various settings through UI navigation
- **Version Check**: Verify app name and version information
- **Multiple Settings**: Test combinations of settings

**Settings Tested:**

- Transaction Checks (Not supported yet on Nano)
- Blind Signing
- Nonce Display
- Verbose EIP-712
- Debug Data Display
- Transaction Hash Display
- EIP-7702 Authorization

📖 **[Detailed documentation](details/test_configuration_cmd.md)**

---

## 2. Transaction Signing

### test_sign.py

**Purpose:** Core transaction signing functionality for standard Ethereum transfers.

**Key Features Tested:**

- **Legacy Transactions**: Type 0 transactions with `gasPrice`
- **EIP-2930 Transactions**: Type 1 transactions with `accessList`
- **EIP-1559 Transactions**: Type 2 transactions with `maxFeePerGas` and `maxPriorityFeePerGas`
- **Multiple Paths**: Test signing with different BIP32 derivation paths
- **Dynamic Networks**: Custom network information display
- **Transaction Rejection**: User rejection flow
- **Error Handling**: Invalid transaction parameters
- **Data Field**: Transactions with arbitrary data payloads
- **Various Gas Parameters**: Different gas prices and limits

**Test Cases:**

- Simple transfers with various amounts
- Transactions with different nonce values
- Multi-chain transactions
- Transaction hash display mode
- Self-transfers (same sender and recipient)

---

### test_blind_sign.py

**Purpose:** Test blind signing functionality for unrecognized smart contract interactions.

**Key Features Tested:**

- **Blind Signing Mode**: Sign transactions that cannot be fully parsed
- **Risk Warnings**: Display appropriate warning screens
- **User Decision**: Accept or reject risky transactions
- **Non-zero Transfers**: Blind sign with ETH value transfers
- **Nonce Display**: Show transaction nonce in blind signing mode
- **Debug Data Display**: Show raw function selectors and parameters
- **Proxy Support**: Handle proxy contract interactions
- **Gating Integration**: Display third-party security information
- **Transaction Simulation**: Show transaction effects in blind signing
- **Error Handling**: Proper error when blind signing is disabled

**Test Cases:**

- `test_blind_sign`: Main blind signing flow with multiple scenarios
- `test_blind_sign_nonce`: Blind signing with nonce display enabled
- `test_blind_sign_reject_in_risk_review`: Reject during risk warning
- `test_sign_parameter_selector`: Debug mode with raw data display
- `test_blind_sign_not_enabled_error`: Error when feature is disabled

📖 **[Detailed documentation with parameters and configurations](details/test_blind_sign.md)**

---

## 3. Token Standards

### test_erc20.py

**Purpose:** Test ERC-20 token operations using the internal plugin.

**Key Features Tested:**

- **Token Metadata**: Provide token information (ticker, address, decimals, chain)
- **Token Transfer**: `transfer()` function for ERC-20 tokens
- **Token Approval**: `approve()` function for spending allowances
- **Extra Data Handling**: Transactions with additional data beyond standard ABI
- **Transaction Simulation**: Display token transfer effects
- **Gating Integration**: Third-party security checks for token operations
- **Error Handling**: Invalid token metadata

**Test Cases:**

- Token metadata provisioning (USDC example)
- Basic token transfers
- Token approval operations
- Transfers/approvals with extra data
- Integration with transaction check
- Integration with gating descriptors

---

### test_nft.py

**Purpose:** Test NFT (Non-Fungible Token) operations for ERC-721 and ERC-1155 standards.

**Key Features Tested:**

- **ERC-721 Support**:
  - `safeTransferFrom()` - Transfer single NFT
  - `setApprovalForAll()` - Approve operator for all NFTs
- **ERC-1155 Support**:
  - `safeTransferFrom()` - Transfer single token type
  - `safeBatchTransferFrom()` - Transfer multiple token types in one transaction
  - `setApprovalForAll()` - Approve operator for all tokens
- **NFT Collection Metadata**: Provide collection name and address
- **Plugin Integration**: Use NFT plugins for parsing
- **Transaction Simulation**: Display NFT transfer effects
- **Gating Integration**: Security checks for NFT operations
- **Proxy Contracts**: Handle NFT transfers through proxies

**NFT Collections Tested:**

- Crypto Kitties (ERC-721)
- Bored Ape Yacht Club (ERC-721)
- Azuki (ERC-721)
- OpenSea Shared Storefront (ERC-1155)

---

## 4. Message Signing

### test_eip191.py

**Purpose:** Test EIP-191 personal message signing (`personal_sign`).

**Key Features Tested:**

- **ASCII Messages**: Human-readable text messages
- **Non-ASCII Messages**: Raw bytes/hex messages
- **Long Messages**: Multi-screen message display (e.g., OpenSea terms)
- **Signature Verification**: Recover signer address from signature
- **Transaction Simulation**: Optional threat detection for message signing
- **User Rejection**: Test rejection flow

**Test Cases:**

- `test_personal_sign_metamask`: Simple text message
- `test_personal_sign_non_ascii`: Raw bytes message
- `test_personal_sign_opensea`: Long multi-line message
- `test_personal_sign_reject`: User rejection flow

---

### test_eip712.py

**Purpose:** Test EIP-712 structured data signing with advanced features.

**Key Features Tested:**

- **Structured Data Signing**: Type-safe structured message signing
- **Domain Separator**: Verify domain information (name, version, chainId, verifyingContract)
- **Complex Types**: Support for nested structs, arrays, and custom types
- **Array Filtering**: Display specific array elements
- **Token Amounts**: Format token values with proper decimals
- **NFT References**: Display NFT information in messages
- **Datetime Fields**: Format Unix timestamps as human-readable dates
- **Trusted Names**: Display ENS names or other trusted identifiers
- **Raw Data Display**: Verbose mode for seeing raw field values
- **Generic Clear Signing (GCS)**: Advanced structured data with custom field descriptors
- **Transaction Simulation**: Threat detection for EIP-712 messages
- **Proxy Integration**: Handle messages from proxy contracts
- **Gating Integration**: Third-party security checks
- **V0 Legacy Support**: Old EIP-712 implementation compatibility

**Test Input Files:**

Over 50 test scenarios covering:

- Simple mail messages
- Token permits (ERC-2612)
- Token approvals
- DEX orders (Uniswap, 1inch, CowSwap, etc.)
- NFT orders (OpenSea, Seaport)
- DAO governance votes
- Multi-sig operations
- DeFi protocol interactions
- Complex nested structures
- Edge cases and error conditions

---

## 5. Ethereum 2.0 / Staking Operations

### test_eth2.py

**Purpose:** Test Ethereum 2.0 beacon chain deposit functionality.

**Key Features Tested:**

- **ETH2 Deposits**: Create deposits to the beacon chain deposit contract
- **BLS Public Keys**: Display validator public keys
- **Withdrawal Credentials**: Generate proper withdrawal credentials with `BLS_WITHDRAWAL_PREFIX`
- **Deposit Amount**: Handle 32 ETH validator deposits (or custom amounts)
- **Signature Verification**: Verify transaction signature

**Test Cases:**

- `test_eth2_deposit`: Full deposit transaction to beacon chain contract

---

### test_eip7002.py

**Purpose:** Test EIP-7002 execution layer withdrawal requests.

**Key Features Tested:**

- **Partial Withdrawals**: Request partial withdrawal with specific amount
- **Full Exit**: Request full validator exit (0 amount)
- **Validator Public Key Display**: Show the validator being affected
- **Withdrawal Amount**: Display requested withdrawal amount in Gwei
- **Predeploy Contract**: Interaction with EIP-7002 system contract

**Predeploy Contract Address:**

`0x00000961Ef480Eb55e80D19ad83579A64c007002`

- 🔍 [View on Etherscan](https://etherscan.io/address/0x00000961Ef480Eb55e80D19ad83579A64c007002)
- **Type**: System predeploy contract (standardized in EIP-7002)
- **Purpose**: Allows validators to request partial withdrawals or full exits from the execution layer
- **Activation**: Available after the corresponding network fork

**Test Cases:**

- `test_eip7002_partial_withdrawal`: Request 13.37 Gwei withdrawal
- `test_eip7002_exit`: Request full validator exit

📖 **[Detailed documentation](details/test_eip7002.md)**

---

### test_eip7251.py

**Purpose:** Test EIP-7251 validator consolidation requests.

**Key Features Tested:**

- **Validator Consolidation**: Merge two validators' balances
- **Validator Compounding**: Compound a validator with itself
- **Public Key Display**: Show source and target validator public keys
- **Predeploy Contract**: Interaction with EIP-7251 system contract

**Predeploy Contract Address:**

`0x0000BBdDc7CE488642fb579F8B00f3a590007251`

- 🔍 [View on Etherscan](https://etherscan.io/address/0x0000BBdDc7CE488642fb579F8B00f3a590007251)
- **Type**: System predeploy contract (standardized in EIP-7251)
- **Purpose**: Enables consolidation of multiple validators or compounding a single validator
- **Activation**: Available after the corresponding network fork

**Test Cases:**

- `test_eip7251_consolidate`: Consolidate two different validators
- `test_eip7251_compound`: Compound a validator with itself

📖 **[Detailed documentation](details/test_eip7251.md)**

---

## 6. Advanced Features

### test_gcs.py

**Purpose:** Test Generic Clear Signing (GCS) framework for advanced smart contract interactions.

**Key Features Tested:**

- **Custom Field Descriptors**: Define how to parse and display any smart contract data
- **Parameter Types**:
  - Raw data (hex display)
  - Token amounts (with decimals)
  - Token metadata
  - NFT references
  - Addresses with trusted names
  - Enums (human-readable values)
  - Datetime fields
  - Network/chain information
  - Nested calldata
- **Data Paths**: Navigate complex nested structures and arrays
- **Field Formatting**: Custom labels and display formatting
- **Container Support**: Handle structs and arrays
- **Transaction Info**: Provide context about the transaction
- **Proxy Contracts**: Parse implementation contract data
- **Transaction Simulation**: Integrate with threat detection
- **Filtering**: Hide/show specific fields

**Test Scenarios:**

- Over 50 complex smart contract interactions
- DeFi protocols (Uniswap, Aave, Compound, etc.)
- NFT marketplaces
- Multi-sig wallets
- DAO governance
- Batch operations
- Complex nested structures

📖 **[Detailed documentation](details/test_gcs.md)**

---

### test_tx_simulation.py

**Purpose:** Test transaction check and threat detection integration.

**Key Features Tested:**

- **Opt-in Flow**: User enables transaction checks
- **Risk Levels**:
  - Benign: Safe transaction
  - Warning: Potentially risky
  - Threat: Dangerous transaction
- **Risk Categories**: Different types of threats
- **URL Display**: Link to detailed security analysis
- **Integration with Other Features**:
  - Standard transactions
  - Blind signing
  - ERC-20 transfers
  - NFT transfers
  - EIP-191 messages
  - EIP-712 structured data
  - GCS interactions
- **Block on Threat**: Prevent dangerous transactions
- **Issue Handling**: Handle service unavailability

**Test Cases:**

- Opt-in configuration
- Simulations with different risk levels
- Integration across all transaction types

📖 **[Detailed documentation](details/test_tx_simulation.md)**

---

### test_trusted_name.py

**Purpose:** Test Trusted Name Service integration (ENS and similar services).

**Key Features Tested:**

- **ENS Names**: Display Ethereum Name Service names instead of addresses
- **Other Name Services**: Support for other naming systems
- **Challenge-Response**: Secure attestation of name-to-address mapping
- **Sources**:
  - CAL (Crypto Asset List)
  - DNS
  - Ledger backend services
- **Verbose Mode**: Toggle between name and full address display
- **Multiple Name Types**:
  - Account names (addresses)
  - Token names
  - NFT collection names
- **EIP-712 Integration**: Named entities in structured data
- **GCS Integration**: Named entities in generic clear signing

**Test Cases:**

- V1 and V2 name descriptor formats
- Address-to-name resolution
- Verbose display toggle
- Integration with transactions
- Integration with EIP-712 messages

📖 **[Detailed documentation](details/test_trusted_name.md)**

---

### test_gating.py

**Purpose:** Test security gating descriptors from third-party services.

**Key Features Tested:**

- **Security Messages**: Display messages from security providers
- **URL Display**: Link to detailed security analysis
- **Integration**:
  - Blind signing transactions
  - EIP-712 messages
  - Proxy contracts
- **Contract Address**: Associate security info with specific contracts

---

### test_safe.py

**Purpose:** Test Safe (Gnosis Safe) multi-signature wallet integration.

**About Safe / Gnosis Safe:**

Safe (formerly known as "Gnosis Safe") is the most popular multi-signature wallet on Ethereum.
The name "Gnosis" comes from ancient Greek *γνῶσις* (gnōsis) meaning "knowledge". Gnosis was
founded in 2015 as a blockchain company, and their Safe product became the industry standard
for multi-sig wallets. In 2022, the product rebranded to "Safe", though "Gnosis Safe" remains
widely used.

- 🔍 [Safe Website](https://safe.global/)
- 🔍 [Safe on Ethereum](https://etherscan.io/accounts/label/safe-multisig)

**Key Features Tested:**

- **Safe Account Information**: Display Safe wallet context
- **Signer List**: Show multiple signers (up to 15)
- **Safe Address**: Display the Safe wallet address
- **Multisig Role**: Indicate user's role (proposer, signer)
- **Threshold Display**: Show required number of signatures
- **Challenge-Response**: Secure attestation
- **Error Handling**: Invalid signer configurations

**Test Cases:**

- `test_safe_descriptor`: Valid Safe with multiple signers
- `test_signer_descriptor_error`: Invalid configuration handling

📖 **[Detailed documentation](details/test_safe.md)**

---

### test_eip7702.py

**Purpose:** Test EIP-7702 set code authorization (account abstraction feature).

**Key Features Tested:**

- **Authorization Signing**: Sign authorization to delegate account code
- **Whitelist**: Only allow whitelisted implementation contracts
- **Chain-specific Whitelists**: Different contracts for different chains
- **Revocation**: Special case for revoking authorization (`0x00...00` address)
- **Nonce Support**: Handle authorization nonces
- **Settings Toggle**: Enable/disable EIP-7702 feature
- **Error Handling**:
  - Non-whitelisted contracts
  - Wrong chain for contract
  - Feature disabled
- **Max Values**: Test with maximum nonce and chain ID

**Test Cases:**

- Whitelisted contracts
- Chain-specific whitelisting
- All-chain whitelisted contracts
- Revocation authorization
- Error cases (wrong chain, not whitelisted, disabled)
- Maximum values

📖 **[Detailed documentation](details/test_eip7702.md)**

---

## 7. External Chains

### test_clone.py

**Purpose:** Test support for Ethereum clone chains using library mode.

**Key Features Tested:**

- **Clone Chain Support**: Ethereum-compatible chains with different chain IDs
- **Library Mode**: App running as a library for other apps
- **Standard Transactions**: Basic transfers on clone chains

**Example Chains:**

- ThunderCore (chain ID 108)

**Requirements:**

- Tests marked with `@pytest.mark.needs_setup('lib_mode')`
- Require special test environment setup

📖 **[Detailed documentation](details/test_clone.md)**

---

## 8. Privacy Operations

### test_privacy_operation.py

**Purpose:** Test privacy-preserving cryptographic operations.

**Key Features Tested:**

- **Public Operation**: Generate 32-byte public randomness
- **Secret Operation**: ECDH-style operation with provided public key
- **Hardware-only**: Tests skipped on Speculos (require actual hardware)

**Use Cases:**

- Privacy-preserving protocols
- Zero-knowledge proof systems
- Secure multi-party computation

📖 **[Detailed documentation](details/test_privacy_operation.md)**

---

## Test Coverage Summary

The test suite covers:

- ✅ **Basic Operations**: Address retrieval, settings, configuration
- ✅ **Transaction Types**: Legacy, EIP-1559, EIP-2930
- ✅ **Token Standards**: ERC-20, ERC-721, ERC-1155
- ✅ **Message Signing**: EIP-191 (personal_sign), EIP-712 (structured data)
- ✅ **Ethereum 2.0**: Deposits, withdrawals, consolidations
- ✅ **Advanced Features**: GCS, transaction check, trusted names, Safe
- ✅ **Security Features**: Blind signing, gating, threat detection
- ✅ **Account Abstraction**: EIP-7702 authorizations
- ✅ **Multi-chain**: Dynamic networks, clone chains
- ✅ **Privacy**: Hardware cryptographic operations

### Test Statistics

- **Total Test Files**: 19
- **Test Scenarios**: 100+ individual test functions
- **Input Files**: 50+ EIP-712 test cases
- **Device Types**: 5 (Nano S Plus, Nano X, Stax, Flex, Apex)
- **Lines of Test Code**: ~7,500+
