# Quick Reference Guide

## Test Files at a Glance

| Test File                    | Main Purpose           | Key Features                                           |
|------------------------------|------------------------|--------------------------------------------------------|
| `test_get_address.py`        | Public key retrieval   | ETH1 addresses, ETH2 BLS keys, multi-chain, chaincode  |
| `test_configuration_cmd.py`  | App settings           | Settings toggles, version check, multiple settings     |
| `test_sign.py`               | Standard transactions  | Legacy, EIP-1559, EIP-2930, multi-chain, rejection     |
| `test_blind_sign.py`         | Unrecognized contracts | Blind signing, warnings, debug mode, proxy support     |
| `test_erc20.py`              | ERC-20 tokens          | Transfer, approve, token metadata, check               |
| `test_nft.py`                | NFT operations         | ERC-721, ERC-1155, transfer, approve, batch operations |
| `test_eip191.py`             | Personal messages      | ASCII/non-ASCII messages, long messages, check         |
| `test_eip712.py`             | Structured data        | Complex types, arrays, tokens, NFTs, datetime, GCS     |
| `test_eth2.py`               | Beacon deposits        | ETH2 deposits, BLS keys, withdrawal credentials        |
| `test_eip7002.py`            | Withdrawal requests    | Partial withdrawals, full exit, validator operations   |
| `test_eip7251.py`            | Validator operations   | Consolidation, compounding, validator management       |
| `test_gcs.py`                | Generic Clear Signing  | Advanced parsing, custom fields, complex contracts     |
| `test_tx_simulation.py`      | Threat detection       | Risk levels, security checks, opt-in, integration      |
| `test_trusted_name.py`       | Name services          | ENS, address-to-name, verbose mode, integration        |
| `test_gating.py`             | Security descriptors   | Third-party security messages, integration             |
| `test_safe.py`               | Multi-sig wallets      | Safe integration, signer lists, threshold              |
| `test_eip7702.py`            | Account abstraction    | Set code authorization, whitelist, revocation          |
| `test_clone.py`              | External chains        | Clone chains, library mode, ThunderCore                |
| `test_privacy_operation.py`  | Cryptographic ops      | Privacy operations, ECDH, hardware-only                |

## Feature Coverage Matrix

| Feature                   | Test Files                                                                                                        |
|---------------------------|-------------------------------------------------------------------------------------------------------------------|
| **Address Display**       | `test_get_address.py`                                                                                             |
| **Settings Management**   | `test_configuration_cmd.py`                                                                                       |
| **Simple Transfers**      | `test_sign.py`                                                                                                    |
| **Contract Interactions** | `test_sign.py`, `test_blind_sign.py`, `test_gcs.py`                                                               |
| **Token Transfers**       | `test_erc20.py`                                                                                                   |
| **NFT Operations**        | `test_nft.py`                                                                                                     |
| **Message Signing**       | `test_eip191.py`, `test_eip712.py`                                                                                |
| **Staking**               | `test_eth2.py`, `test_eip7002.py`, `test_eip7251.py`                                                              |
| **Transaction Simulation**| `test_tx_simulation.py`, `test_eip191.py`, `test_eip712.py`, `test_blind_sign.py`, `test_erc20.py`, `test_nft.py` |
| **Trusted Names**         | `test_trusted_name.py`, `test_eip712.py`, `test_gcs.py`                                                           |
| **Security Gating**       | `test_gating.py`, `test_blind_sign.py`, `test_eip712.py`, `test_erc20.py`, `test_nft.py`                          |
| **Multi-sig**             | `test_safe.py`                                                                                                    |
| **Account Abstraction**   | `test_eip7702.py`                                                                                                 |
| **Proxy Contracts**       | `test_blind_sign.py`, `test_nft.py`, `test_eip712.py`, `test_gcs.py`                                              |
| **Multi-chain**           | `test_sign.py`, `test_get_address.py`, `test_clone.py`                                                            |

## Test Count by Category

- **Basic Operations**: 2 files, ~15 tests
- **Transaction Signing**: 2 files, ~30 tests
- **Token Standards**: 2 files, ~25 tests
- **Message Signing**: 2 files, ~60+ tests
- **Ethereum 2.0**: 3 files, ~5 tests
- **Advanced Features**: 6 files, ~100+ tests
- **External Chains**: 1 file, ~1 test
- **Privacy**: 1 file, ~2 tests

**Total**: 19 files, 200+ test cases

## Common Test Patterns

### Standard Transaction Flow

```python
def test_something(scenario_navigator):
    app_client = EthAppClient(scenario_navigator.backend)

    # Build transaction
    tx_params = {...}

    # Sign with navigation
    with app_client.sign(BIP32_PATH, tx_params):
        scenario_navigator.review_approve()

    # Verify signature
    vrs = ResponseParser.signature(app_client.response().data)
    assert recover_transaction(tx_params, vrs) == DEVICE_ADDR
```

### Message Signing Flow

```python
def test_message(scenario_navigator):
    app_client = EthAppClient(scenario_navigator.backend)

    # Sign message
    with app_client.personal_sign(BIP32_PATH, msg):
        scenario_navigator.review_approve()

    # Verify signature
    vrs = ResponseParser.signature(app_client.response().data)
    assert recover_message(msg, vrs) == DEVICE_ADDR
```

### With Security Features

```python
def test_with_security(scenario_navigator):
    app_client = EthAppClient(scenario_navigator.backend)

    # Provide security context
    app_client.provide_tx_simulation(simu_params)
    app_client.provide_trusted_name(trusted_name)
    app_client.provide_gating(gating_params)

    # Sign with warnings
    with app_client.sign(BIP32_PATH, tx_params):
        scenario_navigator.review_approve_with_warning()
```

## Device Support

Some features are device-specific:

- **Transaction Checks**: Only Stax/Flex/Apex (not Nano)
- **Touch Navigation**: Stax/Flex/Apex
- **Button Navigation**: Nano devices
- **Privacy Operations**: Hardware only (not Speculos)

## Test Fixtures

### Common Fixtures

- `backend` - Speculos or hardware backend
- `navigator` - Low-level UI navigation
- `scenario_navigator` - High-level navigation scenarios
- `test_name` - Unique test identifier for screenshots
- `default_screenshot_path` - Path for screenshot comparison

### Parametrized Fixtures

- `with_chaincode` - Test with/without chaincode (True/False)
- `chain` - Test with different chain IDs (1, 2, 5, 137, etc.)
- `reject` - Test approve/reject flows (True/False)
- `amount` - Test with different ETH amounts
- `display_hash` - Test with/without hash display
- `verbose_raw` - Test verbose display mode
- `filtering` - Test with/without filtering

## EIP Standards Covered

- **EIP-155**: Replay protection (chain ID in signature)
- **EIP-191**: Personal message signing
- **EIP-712**: Structured data signing
- **EIP-1559**: Fee market change (maxFeePerGas)
- **EIP-2612**: Token permits
- **EIP-2930**: Access lists
- **EIP-7002**: Execution layer withdrawals
- **EIP-7251**: Increase maxEB
- **EIP-7702**: Set EOA code

## Smart Contract Standards

- **ERC-20**: Fungible tokens
- **ERC-721**: Non-fungible tokens (NFTs)
- **ERC-1155**: Multi-token standard
- **ERC-2612**: Permit (gasless approvals)

## Important Notes

1. **Screenshots**: Tests use visual regression testing - screenshots are compared
2. **Golden Run**: First run creates "golden" screenshots for comparison
3. **Device Types**: Some tests automatically adapt to device capabilities
4. **Markers**: Some tests require special setup (e.g., `@pytest.mark.needs_setup('lib_mode')`)
5. **Skipped Tests**: Some tests skip on Speculos (hardware-only features)

## Finding Test Details

For detailed information about any test file:

1. Open the test file (e.g., `tests/ragger/test_sign.py`)
2. Look at the docstrings and function names
3. Check the `common()` helper functions for shared logic
4. Review fixture parameters for test variations
5. See [test_overview.md](test_overview.md) for comprehensive descriptions
6. Check [details/](details/) directory for in-depth parameter documentation
