# Ethereum Glossary

Quick reference for Ethereum concepts used in tests. For detailed explanations, see
[Ethereum.org Developer Documentation](https://ethereum.org/en/developers/docs/).

---

## Ether Units

Ethereum uses different denominations for expressing amounts:

| Unit     | Value in Wei                  | Common Use                 |
|----------|-------------------------------|----------------------------|
| **Wei**  | 1 wei                         | Smallest unit (10^-18 ETH) |
| **Gwei** | 1,000,000,000 wei             | Gas prices (10^-9 ETH)     |
| **ETH**  | 1,000,000,000,000,000,000 wei | Main currency (1 ETH)      |

**Conversion formulas:**

- 1 ETH = 10^18 wei
- 1 Gwei = 10^9 wei
- 1 ETH = 10^9 Gwei

📖 [More on Ether units](https://ethereum.org/en/developers/docs/intro-to-ether/#denominations)

---

## Transaction Parameters

### Basic Parameters

#### To

- Recipient address (contract or account)
- For contract interactions, this is the contract address

#### Value

- Amount of ETH to send with the transaction (in wei)
- Can be 0 for contract calls that don't require payment

#### Data

- Arbitrary data payload (in bytes)
- For contract interactions: encoded function call (selector + parameters)
- For simple transfers: usually empty

#### Nonce

- Transaction sequence number for the sending account
- Prevents replay attacks
- Must be sequential (0, 1, 2, 3, ...)
- Each account maintains its own nonce counter

📖 [More on transactions](https://ethereum.org/en/developers/docs/transactions/)

### Gas Parameters

#### What is Gas?

**Gas** is a unit of measurement that represents the computational effort required to execute operations
on the Ethereum network.

**Why Gas Exists:**

- **Metering computation**: Every operation (addition, storage write, etc.) costs a specific amount of gas
- **Preventing abuse**: Limits infinite loops and spam by making computation costly
- **Resource allocation**: Ensures fair distribution of network resources
- **Incentivizing validators**: Compensates validators for executing transactions

**Key Concept - Gas vs Ether:**

Gas is **NOT** a currency. Think of it like this:

- **Gas** = Units of computational work (like liters/gallons for fuel)
- **Gas Price** = Cost per unit of gas (in wei or gwei)
- **Transaction Fee** = Gas Used × Gas Price (paid in ETH)

**Example:**

```text
A simple ETH transfer costs:     21,000 gas
A complex smart contract call:   100,000+ gas

If gas price = 50 gwei:
- Simple transfer fee = 21,000 × 50 gwei = 0.00105 ETH
- Complex call fee = 100,000 × 50 gwei = 0.005 ETH
```

**Common Operations and Their Gas Costs:**

| Operation                | Approximate Gas Cost |
|--------------------------|----------------------|
| ETH transfer             | 21,000               |
| ERC-20 transfer          | ~65,000              |
| Uniswap swap             | ~100,000-150,000     |
| NFT mint                 | ~50,000-100,000      |
| Smart contract creation  | 200,000+             |

📖 [Ethereum Gas Explained](https://ethereum.org/en/developers/docs/gas/)

---

#### Gas Limit

- Maximum number of gas units the transaction is allowed to consume
- Acts as a safety cap to prevent overspending
- If actual gas used exceeds this limit, transaction reverts (but you still pay for gas used until failure)
- If transaction uses less than limit, unused gas is refunded
- Measured in gas units (not wei or gwei)

**Example:**

```text
You set Gas Limit = 100,000
Actual usage = 65,000 gas
→ Transaction succeeds, 35,000 gas refunded

You set Gas Limit = 50,000
Actual usage would be 65,000 gas
→ Transaction fails at 50,000, you pay for 50,000 gas used
```

**Legacy Gas Model (Type 0 transactions):**

#### Gas Price

- Price per gas unit in wei
- Total cost = Gas Used × Gas Price
- Set by user, higher price = faster inclusion

**EIP-1559 Gas Model (Type 2 transactions):**

#### Max Fee Per Gas

- Maximum total price per gas unit you're willing to pay (in wei or gwei)
- Includes base fee + priority fee
- Actual fee may be lower

#### Max Priority Fee Per Gas

- Tip to validators/miners (in wei or gwei)
- Also called "priority fee" or "tip"
- Incentivizes transaction inclusion

**How EIP-1559 works:**

```text
Actual fee = Base Fee + Priority Fee
Base Fee = Set by protocol (burned)
Priority Fee = Min(Max Priority Fee, Max Fee - Base Fee)
```

📖 [More on gas and fees](https://ethereum.org/en/developers/docs/gas/)  
📖 [EIP-1559 specification](https://eips.ethereum.org/EIPS/eip-1559)

### Network Parameters

#### Chain ID

- Unique identifier for each Ethereum network
- Common chain IDs:
  - `1` - Ethereum Mainnet
  - `5` - Goerli Testnet
  - `11155111` - Sepolia Testnet
  - `137` - Polygon
  - `42161` - Arbitrum One
- Prevents replay attacks across different networks

📖 [Chainlist.org](https://chainlist.org/) - Complete list of chain IDs

---

## Cryptographic Concepts

### BIP32 Derivation Path

- Hierarchical deterministic wallet path
- Format: `m/purpose'/coin_type'/account'/change/address_index`
- Example: `m/44'/60'/0'/0/0`
  - `44'` - BIP44 purpose (hardened)
  - `60'` - Ethereum coin type (hardened)
  - `0'` - First account (hardened)
  - `0` - External chain (non-hardened)
  - `0` - First address (non-hardened)
- The `'` mark indicates hardened derivation

📖 [BIP32 specification](https://github.com/bitcoin/bips/blob/master/bip-0032.mediawiki)  
📖 [BIP44 specification](https://github.com/bitcoin/bips/blob/master/bip-0044.mediawiki)

### Elliptic Curve Cryptography

#### secp256k1

- Elliptic curve used for Ethereum account keys
- Same curve as Bitcoin
- Provides 128-bit security level

#### BLS12-381

- Elliptic curve used for Ethereum 2.0 validator keys
- Supports signature aggregation
- Used in beacon chain consensus

📖 [More on Ethereum accounts](https://ethereum.org/en/developers/docs/accounts/)

---

## Smart Contract Concepts

### Function Selector

- First 4 bytes of the keccak256 hash of the function signature
- Example: `balanceOf(address)` → `0x70a08231`
- Used to identify which function to call in contract data

### ABI (Application Binary Interface)

- Standard for encoding/decoding contract function calls
- Defines function signatures, parameter types, return types
- Tests use ABI encoding for contract interactions

📖 [More on smart contracts](https://ethereum.org/en/developers/docs/smart-contracts/)  
📖 [Contract ABI specification](https://docs.soliditylang.org/en/latest/abi-spec.html)

### Proxy Contracts

- Contracts that delegate calls to implementation contracts
- Allows contract upgrades without changing address
- Common patterns: Transparent proxy, UUPS proxy
- Tests verify app can handle proxy interactions

📖 [More on proxy patterns](https://ethereum.org/en/developers/docs/smart-contracts/upgrading/)

---

## Token Standards

### ERC-20

- Standard for fungible tokens
- Common functions: `transfer()`, `approve()`, `balanceOf()`
- Examples: USDC, DAI, USDT

📖 [EIP-20 specification](https://eips.ethereum.org/EIPS/eip-20)

### ERC-721

- Standard for non-fungible tokens (NFTs)
- Each token has unique identifier
- Examples: CryptoPunks, Bored Ape Yacht Club

📖 [EIP-721 specification](https://eips.ethereum.org/EIPS/eip-721)

### ERC-1155

- Multi-token standard (fungible + non-fungible)
- Efficient batch operations
- Examples: OpenSea Shared Storefront

📖 [EIP-1155 specification](https://eips.ethereum.org/EIPS/eip-1155)

---

## Message Signing

### EIP-191 (Personal Sign)

- Standard for signing arbitrary messages
- Format: `\x19Ethereum Signed Message:\n{length}{message}`
- Used by MetaMask and other wallets
- Prevents message from being valid transaction

📖 [EIP-191 specification](https://eips.ethereum.org/EIPS/eip-191)

### EIP-712 (Typed Structured Data)

- Standard for signing typed structured data
- Type-safe, human-readable signing
- Used for permits, orders, votes, etc.
- Includes domain separator for context

📖 [EIP-712 specification](https://eips.ethereum.org/EIPS/eip-712)

---

## Ethereum 2.0 / Consensus Layer

### Validator

- Entity that participates in Ethereum consensus
- Requires 32 ETH deposit
- Uses BLS12-381 keys (different from account keys)

### Beacon Chain

- Ethereum's proof-of-stake consensus layer
- Coordinates validators
- Separate from execution layer (where transactions happen)

### Deposit Contract

- Address: `0x00000000219ab540356cBB839Cbe05303d7705Fa`
- Receives validator deposits (32 ETH)
- Bridge between execution and consensus layers

📖 [More on Ethereum staking](https://ethereum.org/en/staking/)

---

## Test-Specific Terms

### Blind Signing

- Signing transactions/messages without full parsing
- Used when app doesn't recognize contract or function
- Requires user acknowledgment of risk

### Transaction Check

- Off-chain analysis of transaction effects
- Detects potential threats (scams, phishing)
- Displays warnings or blocks dangerous transactions

### Generic Clear Signing (GCS)

- Framework for parsing any smart contract interaction
- Uses external descriptors to format display
- Allows clear signing without app updates

### Gating Descriptor

- Third-party security information about contracts
- Displayed to users during signing
- Provides additional context and warnings

### Trusted Names

- Human-readable names for addresses
- Examples: ENS names, token tickers
- Verified through challenge-response protocol

---

## Additional Resources

- [Ethereum.org Developer Docs](https://ethereum.org/en/developers/docs/)
- [Ethereum Yellow Paper](https://ethereum.github.io/yellowpaper/paper.pdf) (Technical specification)
- [EIPs (Ethereum Improvement Proposals)](https://eips.ethereum.org/)
- [Solidity Documentation](https://docs.soliditylang.org/)
- [Web3.py Documentation](https://web3py.readthedocs.io/) (Used in tests)
