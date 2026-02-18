# test_gcs.py

## Overview

Tests for GCS (Generic Clear Signing) feature. GCS is an advanced system that allows
the Ledger device to clearly display details of complex smart contract interactions
without requiring a dedicated plugin. The app receives field descriptors that
explain how to parse and display transaction data.

## Test Context

### GCS Architecture

GCS uses a declarative approach to describe transaction fields:

- **Fields**: Named parameters to display (e.g., "From", "NFTs", "Amount")
- **Value Types**: Type families (ADDRESS, UINT, BYTES, etc.)
- **Data Paths**: How to extract values from transaction data
- **Formatters**: How to display values (trusted name, token amount, datetime, etc.)

### Field Types

GCS supports various parameter types:

- **ParamRaw**: Raw value display
- **ParamTrustedName**: Address with trusted name resolution
- **ParamNFT**: NFT identifier (collection + token ID)
- **ParamTokenAmount**: Token amount with decimals
- **ParamDatetime**: Unix timestamp formatting
- **ParamEnum**: Enumerated values
- **ParamCalldata**: Nested function call data

### Transaction Info

Each GCS transaction includes:

- **Chain ID**: Network identifier
- **Contract Address**: Target contract
- **Selector**: Function selector (first 4 bytes)
- **Instructions Hash**: Hash of field descriptors
- **Display Name**: Human-readable operation name

## Functions Tested

### test_gcs_nft

**Purpose**: NFT batch transfer with GCS

Tests ERC-1155 `safeBatchTransferFrom` with clear signing.

**Contract**: OpenSea Shared Storefront
`0x495f947276749ce646f68ac8c248420045cb7b5e`

**Function**: `safeBatchTransferFrom`

Displays:

- From address (with trusted name "gerard.eth")
- To address
- NFT collection and token IDs
- Transfer amounts
- Additional data

**Etherscan**:
[OpenSea Storefront](https://etherscan.io/address/0x495f947276749ce646f68ac8c248420045cb7b5e)

### test_gcs_poap

**Purpose**: POAP NFT minting with GCS

Tests minting a POAP (Proof of Attendance Protocol) NFT.

**Contract**: PoapBridge `0x0bb4D3e88243F4A057Db77341e6916B0e449b158`

**Function**: `mintToken`

Displays event ID, claim ID, recipient, expiration timestamp, and signature.

**Etherscan**:
[POAP Bridge](https://etherscan.io/address/0x0bb4D3e88243F4A057Db77341e6916B0e449b158)

### test_gcs_formatter

**Purpose**: Test various GCS formatters

Parametrized test covering different display formats:

- Datetime formatting (unix timestamps)
- Token amounts with decimals
- Enum values
- Nested arrays and tuples

### test_gcs_constraints

**Purpose**: Test field validation constraints

Verifies that GCS can enforce constraints on field values (e.g., must equal specific
value, must be in range).

### test_gcs_1inch

**Purpose**: DEX swap on 1inch with GCS

Tests token swap transaction on 1inch aggregator.

**Contract**: 1inch Router `0x1111111254EEB25477B68fb85Ed929f73A960582`

Displays input token, output token, amounts, slippage protection.

**Etherscan**:
[1inch Router](https://etherscan.io/address/0x1111111254EEB25477B68fb85Ed929f73A960582)

### test_gcs_proxy

**Purpose**: Proxy contract interaction with GCS

Tests interactions through proxy contracts (EIP-1967, minimal proxies).

### test_gcs_4226

**Purpose**: EIP-4626 vault operations with GCS

Tests deposit/withdraw operations on tokenized vaults (EIP-4626 standard).

### test_gcs_nested_createProxyWithNonce

**Purpose**: Safe proxy creation with GCS

Tests Safe `createProxyWithNonce` function for deploying new Safe multisig wallets.

Displays singleton address, initializer data, salt nonce.

### test_gcs_nested_execTransaction_send

**Purpose**: Safe multisig ETH transfer with GCS

Tests Safe `execTransaction` for sending ETH from multisig.

Displays:

- Destination address
- ETH value
- Operation type (CALL, DELEGATECALL)
- Signatures

### test_gcs_nested_execTransaction_addOwnerWithThreshold

**Purpose**: Safe owner management with GCS

Tests Safe `execTransaction` calling `addOwnerWithThreshold` to add a new multisig
signer.

Displays nested function call details.

### test_gcs_nested_execTransaction_changeThreshold

**Purpose**: Safe threshold change with GCS

Tests Safe `execTransaction` calling `changeThreshold` to modify the number of
required signatures.

Displays nested function call with new threshold value.

## Test Functions

Most GCS tests follow this pattern:

1. Encode ABI function call
2. Create transaction parameters
3. Define field descriptors (Fields array)
4. Compute instructions hash
5. Provide transaction info
6. Provide field descriptors
7. Provide additional metadata (NFT, trusted names, etc.)
8. Sign transaction with clear display
9. Verify signature

## Coverage Summary

| Feature               | Coverage                               |
|:----------------------|:---------------------------------------|
| Field Types           | All parameter types                    |
| Value Types           | ADDRESS, UINT, BYTES, etc.             |
| Formatters            | Trusted name, token, NFT, datetime     |
| Data Extraction       | Arrays, tuples, dynamic/static types   |
| Nested Calls          | Safe execTransaction with nested calls |
| Token Operations      | ERC-20, ERC-721, ERC-1155              |
| DeFi Protocols        | 1inch, EIP-4626 vaults                 |
| NFT Operations        | Minting, transfers, batch operations   |
| Proxy Patterns        | EIP-1967, minimal proxies              |
| Safe Integration      | Multisig operations                    |

## Related Documentation

- [Test Overview](../test_overview.md) - GCS overview
- [Glossary](../glossary.md) - Smart contract concepts
- [test_nft.md](test_nft.md) - NFT tests
- [test_safe.md](test_safe.md) - Safe descriptor tests
- [README](../README.md) - Test infrastructure
- [GCS Documentation](../../doc/gcs.md) - Generic Clear Signing specification

### External Resources

- [OpenSea](https://opensea.io/) - NFT marketplace
- [POAP](https://poap.xyz/) - Proof of Attendance Protocol
- [1inch](https://1inch.io/) - DEX aggregator
- [Safe](https://safe.global/) - Multisig wallet
- [EIP-4626](https://eips.ethereum.org/EIPS/eip-4626) - Tokenized vaults
