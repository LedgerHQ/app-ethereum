# test_privacy_operation.py

## Overview

Tests for privacy operation commands. These are cryptographic operations used for
privacy-preserving protocols. The exact use case is internal to Ledger's privacy
features.

## Test Context

### Privacy Operation

The privacy operation generates a 32-byte cryptographic value that can be used for:

- Privacy-preserving protocols
- Secure multi-party computation
- Confidential transactions
- Internal Ledger features

The operation comes in two modes:

- **Public**: No additional parameters
- **Secret**: With a public key parameter

### Device Support

These tests only run on physical devices, not on Speculos emulator (the software
emulator does not support the privacy operation primitives).

## Functions Tested

### test_perform_privacy_operation_public

**Purpose**: Test public mode privacy operation

Performs a privacy operation without additional parameters. Returns a 32-byte
cryptographic value.

**Expected Output**:

- Status: OK
- Data: 32 bytes (256 bits)

**Note**: Skipped on Speculos (not supported).

### test_perform_privacy_operation_secret

**Purpose**: Test secret mode privacy operation with public key

Performs a privacy operation with a public key parameter. Returns a 32-byte
shared secret or derived value.

**Public Key**:
`5901c19a086d1be4b907ec0325bffa758c3eb78192c3df4afa2afd2736a39963`

**Expected Output**:

- Status: OK
- Data: 32 bytes (256 bits)

**Note**: Skipped on Speculos (not supported).

## Test Functions

Both tests follow the same pattern:

1. Check if running on Speculos (skip if yes)
2. Create EthAppClient
3. Call perform_privacy_operation (with or without pubkey)
4. Verify response status is OK
5. Verify response data is 32 bytes
6. Print hex-encoded result

## Coverage Summary

| Feature                | Coverage                    |
|:-----------------------|:----------------------------|
| Privacy Operation      | Public and secret modes     |
| Cryptographic Output   | 32-byte values              |
| Device Support         | Physical devices only       |
| Error Handling         | Status verification         |

## Related Documentation

- [Test Overview](../test_overview.md) - Privacy operations overview
- [README](../README.md) - Test infrastructure

### Notes

This is an internal Ledger feature. The specific cryptographic protocol and use
cases are not publicly documented. The tests verify the APDU command works correctly
and returns the expected data format.
