# Change Log

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](http://keepachangelog.com/)
and this project adheres to [Semantic Versioning](http://semver.org/).

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
