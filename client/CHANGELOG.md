# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Added

- Support for EIP-712 V2: `eip712_v2_send_schema`, `eip712_v2_send_values`, `eip712_v2_sign`,
  `eip712_v2_send_message_info` and `eip712_v2_send_field_desc`, with `eip712.json_to_tlv` to build their
  payloads from an EIP-712 JSON message
- `Eip712MessageInfo`, and `Eip712Path` values for the GCS fields of a typed message
- `domain_separator` and `primary_type_hash` keywords of `EnumValue` and `MapEntry`, for a typed
  message, whose `chain_id`, `contract_addr` and `selector` are then `None`
- `domain_separator` and `primary_type_hash` keywords of `Gating`, for a V2 message, whose `address`
  is then `None`

### Changed

- **Breaking:** `provide_transaction_info` and `provide_transaction_field_desc` renamed
  `provide_operation_info` and `provide_operation_field_desc`, after their APDUs, in `EthAppClient`
  and in `CommandBuilder`, with no alias
- **Breaking:** `InsType.PROVIDE_TRANSACTION_INFO` and `InsType.PROVIDE_TRANSACTION_FIELD_DESC`
  renamed `PROVIDE_OPERATION_INFO` and `PROVIDE_OPERATION_FIELD_DESC`

## [0.7.0] - 2026-10-01

### Added

- Support for Address Book (based on Ragger generic helpers)
- Support for Gated signing
- Support for TLV token information (`TokenInfo`, `provide_token_info`)
- Support for `TrustedName` serializable class (`provide_trusted_name`), with token, MAB and owner derivation path
- Support for GCS map entries, groups, separators, interoperable addresses (EIP-7930), network formatter,
  chain ID container path, visibility and constraints, and `PARAM_CALLDATA`
- Support for EIP-712 calldata filters
- Support for ETH2 withdrawal index (`set_eth2_withdrawal_index`)
- `async_mode` parameter for `perform_privacy_operation`
- `send_pki_certificate` function

### Changed

- Minimum Python version *3.11*
- Status Words aligned on Ragger global definitions
- EIP-712 command builder P1/P2 enums renamed with a `V1` prefix
- EIP-712 amount-join tokens are all sent upfront, their id decoupled from their list order
- `TxAuth7702` requires an explicit `chain_id` (`CHAIN_ID_ALL` for all chains)
- `ledger_pki` module renamed to `signing_partners`
- PKI certificate no longer sent beforehand when using a pre-computed signature

### Removed

- Support for Python *3.9* & *3.10*
- `provide_trusted_name_v1` and `provide_trusted_name_v2`, replaced by `provide_trusted_name`
- `tlv` module, replaced by Ragger TLV helpers

### Fixed

- EIP-712 type fields ordering
- Truncation of some EIP-712 struct names
- STRUCT_TYPE value for Safe signers

## [0.6.0] - 2025-09-05

### Added

- Support for Apex devices

## [0.5.0] - 2025-06-30

### Added

- Support for Python *3.11*, *3.12* & *3.13*
- Support for Tx Simulation
- Support for EIP7702
- Support for Dynamic Networks
- Support for LedgerPKI certificates
- Support for Generic Clear-Signing
- Support for ETH2 Public Key
- Support for Privacy Operation
- Full TLV module to automatically manage this kind of data structure in APDUs (`Class TlvSerializable`)

### Fixed

- Linter warnings/errors

### Removed

- Support for Python *3.7* & *3.8*

### Changed

- Minimum Python version *3.9*
- Rename `DOMAIN_NAME` by `TRUSTED_NAME`
- `(v,r,s)` returned by signature are now `int` instead of `bytes`
- Move Status Words definitions in a dedicated module
- Improve `GET_APP_CONFIGURATION` APDU

## [0.4.1] - 2024-04-15

### Added

- Add new function `send_raw`, allowing to send a raw payload APDU
- Add new error code definition

### Fixed

- Encoding of EIP-712 bytes elements

## [0.4.0] - 2024-04-03

### Added

- Changed `sync` functions of the client to not use an `async` syntax anymore

## [0.3.1] - 2024-03-12

### Fixed

- `recover_transaction` & `recover_message` util functions

## [0.3.0] - 2024-02-13

### Added

- New `provide_token_metadata` function

### Fixed

- Increased the delay between `autonext` callback calls for EIP-712 on Stax
- `recover_transaction` util function for non-legacy transactions

## [0.2.1] - 2023-12-01

### Fixed

- v0.2.0 version already published on pypi.org

## [0.2.0] - 2023-12-01

### Added

- New generic `sign` function that uses the Web3.py library

### Removed

- `sign_legacy` & `sign_1559` functions

### Fixed

- Now uses the proper signing key for the `SET_EXTERNAL_PLUGIN` APDU

### Changed

- `get_public_addr` now returns address as `bytes` instead of `str`

## [0.1.0] - 2023-10-30

### Added

- Update the ragger app client to support "set external plugin" APDU

## [0.0.1] - 2023-07-08

### Added

- Initial version
