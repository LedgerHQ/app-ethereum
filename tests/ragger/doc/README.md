# Ethereum Ledger App - Test Documentation

This directory contains comprehensive documentation for the Ethereum Ledger application test suite.

## Documentation Files

- **[test_overview.md](test_overview.md)** - Complete overview of all test functionalities
- **[quick_reference.md](quick_reference.md)** - Quick reference guide for test files
- **[glossary.md](glossary.md)** - Ethereum concepts and terminology reference
- **[details/](details/)** - In-depth documentation for individual test files with parameters and configurations

## About the Tests

The Ethereum Ledger app test suite uses:

- **Ragger**: Python testing framework for Ledger applications
- **Speculos**: Ledger device emulator for automated testing
- **pytest**: Python testing framework

## Test Categories

1. **Basic Operations** - Address retrieval, settings, configuration
2. **Transaction Signing** - Standard and blind transaction signing
3. **Token Standards** - ERC-20, ERC-721, ERC-1155
4. **Message Signing** - EIP-191 and EIP-712
5. **Ethereum 2.0** - Staking operations, withdrawals, consolidations
6. **Advanced Features** - Generic Clear Signing (GCS), transaction check, trusted names
7. **External Chains** - Clone chain support
8. **Privacy Operations** - Hardware cryptographic operations

## Quick Start

To understand what functionality is tested:

1. Read [test_overview.md](test_overview.md) for comprehensive documentation
2. Check [quick_reference.md](quick_reference.md) for a quick lookup table
3. Explore [details/](details/) for in-depth test configuration and parameter details

---

## Test Infrastructure

### Ragger Framework

All tests use the **Ragger** testing framework which provides:

- **Backends**: Support for Speculos emulator and physical devices
- **Navigation**: Automated UI interaction (button presses, swipes, taps)
- **Screenshot Comparison**: Visual regression testing
- **Scenario Management**: Reusable test scenarios
- **Device Support**: Nano S Plus, Nano X, Stax, Flex, Apex

### Speculos Emulator

The **Speculos** emulator simulates Ledger devices for testing:

- Accurate ARM processor emulation
- Display rendering
- Button/touch input simulation
- APDU communication
- No actual cryptographic security (faster for testing)

### Test Structure

Each test typically follows this pattern:

1. **Setup**: Initialize app client and backend
2. **Provide Context**: Supply network info, token metadata, security descriptors
3. **Create Transaction/Message**: Build the data to sign
4. **Navigate UI**: Simulate user interaction
5. **Verify**: Check signature validity and expected behavior

### Common Fixtures

Common pytest fixtures used across tests:

- `backend`: Speculos or hardware backend
- `navigator`: UI navigation helper
- `scenario_navigator`: High-level navigation scenarios
- `test_name`: Unique identifier for screenshots
- Various parameter fixtures for test variations

---

## Running Tests

For detailed instructions on running tests, including command examples, device options, and
advanced usage, see [../usage.md](../usage.md).
