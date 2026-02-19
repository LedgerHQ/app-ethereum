# test_eip712.py - Test Details

## Overview

Tests **EIP-712 structured data signing**, a standard for type-safe, human-readable signing of
complex data structures. This is widely used for permits, DEX orders, DAO votes, and other
off-chain signed data.

## Test Context

### What is EIP-712?

EIP-712 provides a standard for signing typed structured data that is:

- **Type-safe:** Each field has a defined type
- **Human-readable:** Users see field names and values
- **Verifiable:** Includes domain separator for context

📖 [EIP-712 Specification](https://eips.ethereum.org/EIPS/eip-712)

### Structure

Every EIP-712 message has:

#### Domain Separator

Provides context about the signing domain:

- `name`: Application name
- `version`: Contract version
- `chainId`: Network chain ID
- `verifyingContract`: Contract address that will verify the signature

#### Message Types

Custom types defining the structure:

```json
{
  "Person": [
    {"name": "name", "type": "string"},
    {"name": "wallet", "type": "address"}
  ]
}
```

#### Primary Type & Message

The main data being signed with its values.

---

## Test Scenarios

Tests use **50+ JSON input files** covering real-world use cases:

### DeFi Protocols

- **Uniswap:** Swap orders, liquidity permits
- **1inch:** Limit orders, aggregator orders
- **CowSwap:** Trade orders
- **Aave, Compound:** Delegation, permits
- **SushiSwap:** Order routing

### NFT Marketplaces

- **OpenSea:** Collection offers, item listings
- **Seaport:** Advanced orders with criteria

### DAO Governance

- **Snapshot:** Off-chain voting
- **Delegation:** Vote delegation

### Token Operations

- **ERC-2612 Permits:** Gasless approvals
- **Batch operations:** Multiple transfers

### Advanced Features

- **Array filtering:** Display specific array elements
- **Token amounts:** Format with proper decimals
- **NFT references:** Display NFT collection info
- **Datetime fields:** Format Unix timestamps
- **Trusted names:** Display ENS names
- **Nested structures:** Complex data hierarchies

---

## Key Test Functions

### Basic EIP-712 Tests

**Test files in:** `tests/ragger/eip712_input_files/*.json`

Each JSON file contains:

- Domain separator
- Message types
- Primary type
- Message data
- Expected display format

### Advanced Features

#### Array Filtering

Display only specific array elements (e.g., first 3 of 100 items)

#### Token Amount Formatting

Display token amounts with proper decimals and ticker:

- Raw: `1000000` (6 decimals)
- Displayed: `1.000000 USDC`

#### NFT Integration

Show NFT collection name instead of address:

- Raw: `0xbc4ca0eda7647a8ab7c2061c2e118a18a936f13d`
- Displayed: `Bored Ape Yacht Club`

#### Datetime Formatting

Convert Unix timestamps to readable dates:

- Raw: `1640000000`
- Displayed: `2021-12-20 13:33:20 UTC`

### Generic Clear Signing (GCS)

Tests integration between EIP-712 and GCS for enhanced field display with custom descriptors.

---

## Common Message Types Tested

### ERC-2612 Permit

Gasless token approval:

```text
Owner: 0x...
Spender: 0x...
Value: 1000.00 USDC
Deadline: 2024-12-31 23:59:59 UTC
```

### DEX Limit Order

Trade order on decentralized exchange:

```text
Maker: 0x...
Taker: 0x... (or ANY)
Token In: 1.5 ETH
Token Out: 3000.0 USDC
Expiry: 2024-12-25 12:00:00 UTC
```

### NFT Offer

Bid on OpenSea:

```text
Offerer: 0x...
Collection: Bored Ape Yacht Club
Token ID: #1234
Offer: 50 ETH
Expiry: 7 days
```

### DAO Vote

Snapshot governance:

```text
Voter: 0x...
Proposal: "Proposal #42: Treasury Allocation"
Choice: For
Voting Power: 1250.5 tokens
```

---

## Test Coverage Summary

| Category                  | Coverage                                           |
|---------------------------|----------------------------------------------------|
| **Message Types**         | 50+ real-world scenarios                           |
| **DeFi Protocols**        | Uniswap, 1inch, CowSwap, Aave, Compound            |
| **NFT Marketplaces**      | OpenSea, Seaport                                   |
| **Token Standards**       | ERC-20 permits, ERC-721, ERC-1155                  |
| **DAO Governance**        | Snapshot voting, delegation                        |
| **Field Types**           | Strings, numbers, addresses, bytes, arrays, structs|
| **Advanced Features**     | Array filtering, token formatting, NFT refs, dates |
| **Integration**           | Transaction simulation, GCS, trusted names         |

---

## Related Documentation

- [Main Test Overview](../test_overview.md#test_eip712py)
- [Quick Reference](../quick_reference.md)
- [Ethereum Glossary](../glossary.md) - Terminology and concepts reference
- Test input files: `tests/ragger/eip712_input_files/`
