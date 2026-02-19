# test_configuration_cmd.py

## Overview

Tests for app configuration commands. These tests verify settings management and
version information retrieval. Settings control various security and display
features of the Ethereum app.

## Test Context

### App Settings

The Ethereum app provides several configurable settings:

- **TRANSACTION_CHECKS**: Enable transaction simulation warnings (not on Nano)
- **BLIND_SIGNING**: Allow signing of arbitrary contract data
- **NONCE**: Display transaction nonce during review
- **VERBOSE_EIP712**: Show detailed token info in EIP-712 messages
- **DEBUG_DATA**: Show raw data during transaction review
- **DISPLAY_HASH**: Show transaction hash after signing

### Configuration Commands

The app provides APDUs for:

- **Get Configuration**: Retrieve app version, flags, and capabilities
- **Get Version**: Retrieve app name and version string
- **Settings Navigation**: Toggle settings through UI

## Functions Tested

### test_settings

**Purpose**: Test navigation through settings and UI verification

Parametrized test that navigates to various settings and captures screenshots
for visual regression testing.

**Test Cases**:

- `tx_checks`: Transaction check setting
- `blind_sign`: Blind signing setting
- `nonce`: Nonce display setting
- `eip712_token`: Verbose EIP-712 setting
- `debug_token`: Debug data setting
- `hash`: Hash display setting
- `multiple1`: BLIND_SIGNING + DEBUG_DATA
- `multiple2`: BLIND_SIGNING + VERBOSE_EIP712
- `multiple3`: BLIND_SIGNING + TRANSACTION_CHECKS

Each test navigates to the setting(s) and verifies the UI display is correct.

**Note**: TX_CHECKS is skipped on Nano devices (only Stax/Flex).

### test_check_version

**Purpose**: Verify app version and name retrieval

Queries the device for app name and version, verifies it matches the expected
version string.

**Expected Output**:

- Name: "Ethereum"
- Version: Semantic version (e.g., "1.12.3")

## Test Functions

- `test_settings`: Parametrized with 9 setting combinations
  - Navigates through settings UI
  - Captures screenshots for regression testing
  - Skips TX_CHECKS on Nano devices

- `test_check_version`: Queries and verifies version
  - Sends GET_APP_CONFIGURATION APDU
  - Compares version string with expected

## Coverage Summary

| Feature               | Coverage                        |
|:----------------------|:--------------------------------|
| Settings Management   | All available settings          |
| Multiple Settings     | Combinations of 2-3 settings    |
| Version Query         | App name and version            |
| UI Navigation         | Settings menu traversal         |
| Device Compatibility  | Nano, Stax, Flex differences    |

## Related Documentation

- [Test Overview](../test_overview.md) - Configuration overview
- [README](../README.md) - Test infrastructure

### External Resources

- [APDU Specification](../../doc/ethapp.adoc) - Ethereum app APDUs
