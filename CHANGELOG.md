# Change Log

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](http://keepachangelog.com/)
and this project adheres to [Semantic Versioning](http://semver.org/).

## [1.9.11](https://github.com/ledgerhq/app-ethereum/compare/1.9.10...1.9.11) - 2021-10-12

### Added

- Provide network ticker to plugins (especialy helpful for Paraswap plugin)

## [1.9.10](https://github.com/ledgerhq/app-ethereum/compare/1.9.9...1.9.10) - 2021-10-08

### Added

- Add new app: Moonriver

## [1.9.9](https://github.com/ledgerhq/app-ethereum/compare/1.9.8...1.9.9) - 2021-10-08

### Changed

- Rollback the revert in wording change of "Contract data" in "Blind signing" that was introduced in v1.9.8

## [1.9.8](https://github.com/ledgerhq/app-ethereum/compare/1.9.7...1.9.8) - 2021-10-06

### Changed

- Revert wording change of "Contract data" in "Blind signing" from v1.9.5

### Added

- Goerli now has its own standalone app, with hardcoded deversifi tokens

## [1.9.7](https://github.com/ledgerhq/app-ethereum/compare/1.9.6...1.9.7) - 2021-9-30

### Fixed

- Fixed a bug where amounts displayed where wrong when the amount was huge (>=2^87)

## [1.9.6](https://github.com/ledgerhq/app-ethereum/compare/1.9.5...1.9.6) - 2021-9-29

### Fixed

- Fixed a bug where fees displayed were wrong on Starkware transactions

## [1.9.5](https://github.com/ledgerhq/app-ethereum/compare/1.9.4...1.9.5) - 2021-9-27

### Changed

- "Contract Data" is now replaced with "Blind sign", which carries more meaning for regular users.

### Added

- When blind signing is disabled in settings, and a transaction with smart conract interactions is sent to the app, a new warning screen pops to let the user know that the setting must be enabled to sign this kind of transactions.

## [1.9.4](https://github.com/ledgerhq/app-ethereum/compare/1.9.3...1.9.4) - 2021-9-14

### Added

- Added Arbitrum network

## [1.9.3](https://github.com/ledgerhq/app-ethereum/compare/1.9.2...1.9.3) - 2021-9-03

### Added

- Added better display for bigger chainIDs.
- Added support for Songbird.
- Added support for Celo.

### Changed

- Small refactor of `getEthDisplayableAddress` helper
- Improve Zemu tests to get parallelization
- Increased plugin interface to version 2
- Remove support for Theta and Flare

## [1.9.2](https://github.com/ledgerhq/app-ethereum/compare/1.9.0...1.9.2) - 2021-8-11

### Added

- Added support for bigger chainIDs.

### Fixed

- Fixed BSC icon colors.
- Fixed theta tokens.

## [1.9.0](https://github.com/ledgerhq/app-ethereum/compare/1.8.8...1.9.0) - 2021-8-05

### Added

- Added support for EIP-1559 and EIP-2930 style transactions.

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
