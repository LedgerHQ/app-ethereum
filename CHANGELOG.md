# Change Log

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](http://keepachangelog.com/)
and this project adheres to [Semantic Versioning](http://semver.org/).

## [1.8.8](https://github.com/ledgerhq/app-ethereum/compare/1.8.7...1.8.8) - 2021-7-21

### Added

- Added support for BSC.
- Add support for Lido plugin

## [1.8.7](https://github.com/ledgerhq/app-ethereum/compare/1.8.6...1.8.7) - 2021-7-9

### Added

Plugins can now check the address of the transaction sender.
Remove `m/44'/60'` derivation path authorisation for Theta app.

### Fixed

`additional_screens` was introduced previously but wasn't properly initialized in some cases.

## [1.8.6](https://github.com/ledgerhq/app-ethereum/compare/1.8.5...1.8.6) - 2021-7-5

### Added

Display the name of the network when signing a transaction, or the chain ID if the network is not known
When the network is known, amounts and fees are displayed in the network unit instead of ETH.

### Fixed

Fix some compilation warning

## [1.8.5](https://github.com/ledgerhq/app-ethereum/compare/1.7.9...1.8.5) - 2021-6-8

### Added

- Added support for external plugins.

## [1.7.9](https://github.com/ledgerhq/app-ethereum/compare/1.7.8...1.7.9) - 2021-6-2

### Added

- Added support for Flare Network and Theta Chain.

## [1.7.8](https://github.com/ledgerhq/app-ethereum/compare/1.7.7...1.7.8) - 2021-5-20

### Fixed

- Fixed a bug where transaction would sometimes not get properly signed.

## [1.7.7](https://github.com/ledgerhq/app-ethereum/compare/1.7.6...1.7.7) - 2021-5-19

### Special

- Version bump needed for deployment reasons, nothing changed.

## [1.7.6](https://github.com/ledgerhq/app-ethereum/compare/1.7.5...1.7.6) - 2021-5-14

### Special

- Version bump needed for deployment reasons, nothing changed.

## [1.7.7](https://github.com/ledgerhq/app-ethereum/compare/1.7.6...1.7.7) - 2021-5-19

- N/A

## [1.7.6](https://github.com/ledgerhq/app-ethereum/compare/1.7.6...1.7.6) - 2021-5-14

- N/A

## [1.7.5](https://github.com/ledgerhq/app-ethereum/compare/1.7.4...1.7.5) - 2021-5-10

### Fixed

- Fixed a bug with cx_ecfp_scalar_mult

## [1.7.4](https://github.com/ledgerhq/app-ethereum/compare/1.7.3...1.7.4) - 2021-5-6

### Fixed

- Fixed a bug that prevented using Ethereum sidechains

## [1.7.3](https://github.com/ledgerhq/app-ethereum/compare/1.7.2...1.7.3) - 2021-5-5

### Added

- Enable Ethereum 2 deposit on Nano S 2.0.0

## [1.7.2](https://github.com/ledgerhq/app-ethereum/compare/1.7.1...1.7.2) - 2021-5-5

### Added

- Improve Ethereum 2 deposit security:
  - Display the validator address on screen when depositing.
  - Abort signing when the account index of the withdrawal key is higher than INDEX_MAX.
  - Check that the destination field of the transaction is Ethereum 2 deposit contract.

## [1.7.1](https://github.com/ledgerhq/app-ethereum/compare/1.7.0...1.7.1) - 2021-5-5

### Added

- Support for Berlin hard fork: EIP2718 (transaction types) and EIP2930 (access list transactions)
- Display ChainID when transacting on chains which are not ethereum (BSC, Polygon, etc)

## [1.7.0](https://github.com/ledgerhq/app-ethereum/compare/1.6.6...1.7.0) - 2021-4-30

### Added

- Wallet ID feature now available on Nano X

## [1.6.6](https://github.com/ledgerhq/app-ethereum/compare/1.6.5...1.6.6) - 2021-4-16

### Added

- Improved Starkware support

## [1.6.5](https://github.com/ledgerhq/app-ethereum/compare/1.6.4...1.6.5) - 2021-2-12

### Added

- Add a setting to enable nonce display when approving transactions

## [1.6.4](https://github.com/ledgerhq/app-ethereum/compare/1.6.3...1.6.4) - 2021-1-12

### Fixed

- "warning" icon wasn't correctly displayed

## [1.6.3](https://github.com/ledgerhq/app-ethereum/compare/1.6.2...1.6.3) - 2020-12-10

### Added

- Changelog file

### Removed

- unused `prepare_full_output` and `btchip_bagl_confirm_full_output` functions removed

### Changed

- More errors, less THROWs
- Cleanup args parsing when called as a library

### Fixed

- Most compilation warnings fixed
- Ensure `os_lib_end` is called when errors are encountered in library mode
- Fix pin validation check
