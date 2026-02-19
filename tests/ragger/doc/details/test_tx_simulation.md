# test_tx_simulation.py

## Overview

Tests for Transaction Check feature. This security feature allows the Ledger
device to receive risk assessment information from Ledger's backend service before
signing a transaction. The app displays warnings for suspicious or malicious
transactions.

## Test Context

### Transaction Check

When enabled, the Ledger app can receive simulation data including:

- **Risk Level**: 0 (benign), 1 (warning), 2 (threat)
- **Category**: Type of risk detected
- **Tiny URL**: Link to more information (typically ledger.com)
- **Check Type**: TRANSACTION, MESSAGE, or UNKNOWN

### Settings

- **TRANSACTION_CHECKS**: Must be enabled to receive simulations
- **TX_CHECKS_OPT_IN**: User must opt-in to enable the feature
- **TX_CHECKS_ENABLE**: Feature is fully enabled

### Risk Configurations

Tests use four risk configurations:

- **benign**: No risk detected (risk=0, category=0)
- **warning**: Potential issue detected (risk=1, category=4)
- **threat**: Malicious activity detected (risk=2, category=2)
- **issue**: No simulation provided

## Functions Tested

### test_tx_simulation_opt_in

**Purpose**: Test the opt-in flow for transaction check

User must explicitly opt-in to enable the transaction checks feature. Verifies that
the TX_CHECKS_OPT_IN and TX_CHECKS_ENABLE flags are set after opt-in.

### test_tx_simulation_disabled

**Purpose**: Verify error when simulation provided but feature disabled

When TRANSACTION_CHECKS setting is disabled, providing simulation data should return
NOT_IMPLEMENTED error.

### test_tx_simulation_enabled

**Purpose**: Verify simulation accepted when feature enabled

When TRANSACTION_CHECKS setting is enabled, simulation data is accepted successfully.

### test_tx_simulation_sign

**Purpose**: Test transaction signing with various risk levels

Parametrized test that signs a simple ETH transfer with four configurations:

- **benign**: Transaction proceeds normally (no warning UI)
- **warning**: UI displays warning (category 4)
- **threat**: UI displays threat alert (category 2)
- **issue**: No simulation provided, transaction proceeds

**Transaction Details**:

- To: `0x5a321744667052affa8386ed49e00ef223cbffc3`
- Value: 1.22 ETH
- Chain: Goerli (5)

### test_tx_simulation_no_simu

**Purpose**: Test signing without providing simulation

Even with TRANSACTION_CHECKS enabled, transactions can proceed without simulation
data. The device allows the transaction but may show a notification.

### test_tx_simulation_nft

**Purpose**: Test NFT transfer with simulation

Tests simulation for ERC-721 NFT transfers. Uses the NFT plugin test suite with
simulation data.

### test_tx_simulation_blind_sign

**Purpose**: Test blind signing with simulation

Tests simulation for transactions requiring blind signing (arbitrary smart contract
calls). Uses various risk configurations.

### test_tx_simulation_eip191

**Purpose**: Test EIP-191 message signing with simulation

Tests simulation for personal message signing with different risk levels.

### test_tx_simulation_eip712

**Purpose**: Test EIP-712 structured data signing with simulation

Tests simulation for EIP-712 typed data signatures.

### test_tx_simulation_eip712_v0

**Purpose**: Test legacy EIP-712 v0 signing with simulation

Tests simulation for older EIP-712 v0 format signatures.

### test_tx_simulation_gcs

**Purpose**: Test GCS (Generic Clear Signing) with simulation

Tests simulation for transactions using the GCS framework (POAP NFT minting example).

## Test Functions

Most tests follow this pattern:

1. Enable TRANSACTION_CHECKS setting if needed
2. Construct transaction/message parameters
3. Provide simulation data (risk, category, URL)
4. Sign on device with appropriate UI flow
5. Verify simulation affects UI (warnings/threats displayed)

## Coverage Summary

| Feature                     | Coverage                          |
|:----------------------------|:----------------------------------|
| Opt-in Flow                 | User consent for feature          |
| Settings Management         | Enable/disable checks             |
| Risk Levels                 | Benign, warning, threat           |
| Transaction Types           | ETH transfer, NFT, smart contract |
| Message Signing             | EIP-191, EIP-712                  |
| GCS Integration             | Generic Clear Signing             |
| Error Handling              | Disabled feature, missing data    |
| UI Display                  | Risk warnings and alerts          |

**Note**: Transaction check is not yet supported on Nano devices (only Stax/Flex).

## Related Documentation

- [Test Overview](../test_overview.md) - Transaction check overview
- [Glossary](../glossary.md) - Transaction concepts
- [test_sign.md](test_sign.md) - Basic transaction signing
- [test_blind_sign.md](test_blind_sign.md) - Blind signing
- [test_eip191.md](test_eip191.md) - Message signing
- [test_eip712.md](test_eip712.md) - Structured data signing
- [README](../README.md) - Test infrastructure

### External Resources

- [Ledger Security](https://www.ledger.com/academy/security) - Security features
- [Clear Signing](https://www.ledger.com/academy/what-is-clear-signing) - Ledger
  Academy
