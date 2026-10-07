# test_nft.py - Test Details

## Overview

Tests **NFT (Non-Fungible Token)** operations for ERC-721 and ERC-1155 standards using NFT plugins.

## Test Context

### ERC-721 Collections Tested

#### 1. Bored Ape Yacht Club (BAYC)

**Contract Address:** `0xbc4ca0eda7647a8ab7c2061c2e118a18a936f13d`

- 🔍 [View on Etherscan](https://etherscan.io/address/0xbc4ca0eda7647a8ab7c2061c2e118a18a936f13d)
- **Chain:** Ethereum Mainnet (Chain ID: 1)
- **Type:** ERC-721
- **Description:** One of the most iconic NFT collections

#### 2. y00ts

**Contract Address:** `0x670fd103b1a08628e9557cd66b87ded841115190`

- 🔍 [View on Polygonscan](https://polygonscan.com/address/0x670fd103b1a08628e9557cd66b87ded841115190)
- **Chain:** Polygon (Chain ID: 137)
- **Type:** ERC-721

#### 3. goerlirocks

**Contract Address:** `0x2909cf13e458a576cdd9aab6bd6617051a92dacf`

- 🔍 [View on Goerli Etherscan](https://goerli.etherscan.io/address/0x2909cf13e458a576cdd9aab6bd6617051a92dacf)
- **Chain:** Goerli Testnet (Chain ID: 5)
- **Type:** ERC-721
- **Note:** Test collection on testnet

### ERC-1155 Collections Tested

#### 1. OpenSea Shared Storefront

**Contract Address:** `0x495f947276749ce646f68ac8c248420045cb7b5e`

- 🔍 [View on Etherscan](https://etherscan.io/address/0x495f947276749ce646f68ac8c248420045cb7b5e)
- **Chain:** Ethereum Mainnet (Chain ID: 1)
- **Type:** ERC-1155
- **Description:** OpenSea's shared contract for creating NFTs without deploying own contract

#### 2. OpenSea Collections (Polygon)

**Contract Address:** `0x2953399124f0cbb46d2cbacd8a89cf0599974963`

- 🔍 [View on Polygonscan](https://polygonscan.com/address/0x2953399124f0cbb46d2cbacd8a89cf0599974963)
- **Chain:** Polygon (Chain ID: 137)
- **Type:** ERC-1155

### Addresses Used in Tests

| Address Type | Value                                        | Purpose                    |
|--------------|----------------------------------------------|----------------------------|
| FROM         | `0x1122334455667788990011223344556677889900` | NFT sender (test account)  |
| TO           | `0x0099887766554433221100998877665544332211` | NFT recipient              |

### Transaction Parameters

| Parameter       | Value             | Purpose                              |
|-----------------|-------------------|--------------------------------------|
| Chain ID        | `1` / `137` / `5` | Network (Mainnet/Polygon/Goerli)     |
| Nonce           | `21`              | Transaction sequence number          |
| Gas Limit       | `132239`          | Maximum gas allowed                  |
| Max Fee         | `100 gwei`        | Maximum total fee per gas (EIP-1559) |
| Priority Fee    | `10 gwei`         | Tip to validators (EIP-1559)         |
| ETH Value       | `0`               | No ETH sent (NFT operation only)     |

💡 For explanations of these terms, see the [Glossary](../glossary.md)

---

## ERC-721 Functions Tested

### `safeTransferFrom(address from, address to, uint256 tokenId, bytes data)`

- Transfer NFT with data parameter
- Ensures recipient can receive NFTs

### `safeTransferFrom(address from, address to, uint256 tokenId)`

- Transfer NFT without data parameter (most common)

### `transferFrom(address from, address to, uint256 tokenId)`

- Basic NFT transfer (legacy, no safety check)

### `approve(address to, uint256 tokenId)`

- Approve another address to transfer specific NFT

### `setApprovalForAll(address operator, bool approved)`

- Approve/revoke operator for ALL user's NFTs in collection

📖 [ERC-721 Standard](https://eips.ethereum.org/EIPS/eip-721)

---

## ERC-1155 Functions Tested

### `safeTransferFrom`

`safeTransferFrom(address from, address to, uint256 id, uint256 amount, bytes data)`

- Transfer single token type
- Can transfer multiple copies (amount > 1)

### `safeBatchTransferFrom`

`safeBatchTransferFrom(address from, address to, uint256[] ids, uint256[] amounts, bytes data)`

- Transfer multiple token types in one transaction
- Efficient batch operation

### `setApprovalForAll(address operator, bool approved)`

- Approve/revoke operator for ALL user's tokens

📖 [ERC-1155 Standard](https://eips.ethereum.org/EIPS/eip-1155)

---

## Test Functions

### `test_nft_erc721`

**Purpose:** Test all ERC-721 NFT operations across different collections.

**What It Tests:**

- All ERC-721 functions (safeTransferFrom, transferFrom, approve, setApprovalForAll)
- Multiple NFT collections (BAYC, y00ts, goerlirocks)
- Multi-chain support (Mainnet, Polygon, Goerli)
- NFT plugin integration

**Test Matrix:**

- 3 collections × 5 functions = 15 test combinations per run

### `test_nft_erc721_reject`

**Purpose:** Test user rejection flow for NFT transfers.

**What It Tests:**

- User can reject NFT operations
- Proper error handling on rejection

**Expected Result:** `CONDITION_NOT_SATISFIED` error

### `test_nft_erc1155`

**Purpose:** Test all ERC-1155 multi-token operations.

**What It Tests:**

- safeTransferFrom (single token type)
- safeBatchTransferFrom (multiple token types)
- setApprovalForAll
- Multiple collections (OpenSea Shared Storefront, OpenSea Polygon)
- Batch operations with multiple token IDs and amounts

**Test Matrix:**

- Multiple collections × multiple functions

### `test_nft_erc1155_reject`

**Purpose:** Test user rejection flow for ERC-1155 operations.

**What It Tests:**

- User can reject ERC-1155 operations
- Proper error handling on rejection

**Expected Result:** `CONDITION_NOT_SATISFIED` error

---

## Test Coverage Summary

| Test Function            | Focus                                    |
|--------------------------|------------------------------------------|
| `test_nft_erc721`        | All ERC-721 operations and collections   |
| `test_nft_erc721_reject` | Rejection flow for ERC-721               |
| `test_nft_erc1155`       | All ERC-1155 operations and collections  |
| `test_nft_erc1155_reject`| Rejection flow for ERC-1155              |

**Collections Coverage:**

- ✅ ERC-721: BAYC (Mainnet), y00ts (Polygon), goerlirocks (Goerli)
- ✅ ERC-1155: OpenSea Shared Storefront (Mainnet), OpenSea Collections (Polygon)

**Operations Coverage:**

- ✅ Single NFT transfers
- ✅ Batch transfers (ERC-1155)
- ✅ Approvals (single and all tokens)
- ✅ Multi-chain support

---

## Related Documentation

- [Main Test Overview](../test_overview.md#test_nftpy)
- [Quick Reference](../quick_reference.md)
- [Ethereum Glossary](../glossary.md) - Terminology and concepts reference
