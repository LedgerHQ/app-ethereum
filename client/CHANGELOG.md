# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

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
