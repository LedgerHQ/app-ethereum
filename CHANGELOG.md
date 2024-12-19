# Change Log

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](http://keepachangelog.com/)
and this project adheres to [Semantic Versioning](http://semver.org/).

## [1.15.0](../../compare/1.14.0...1.15.0) - 2025-xx-xx

## [1.14.0](../../compare/1.13.0...1.14.0) - 2024-12-19

### Added

- (network) Zero
- Generic clear-signing support

### Changed

- Renamed "Address" field in ERC-20 approval flow to "Approve to"
- Blind-signing flow now shows transaction hash (keccak-256 of the RLP without v/r/s)
- Blind-signing flow now hides the amount if it is 0

### Fixed

- Version comparison in trusted name feature
- Key ID & public key used for trusted names coming from CAL
- PKI key usage in trusted name feature

## [1.13.0](../../compare/1.12.2...1.13.0) - 2024-11-26

### Added

- (clone) IoTeX
- (network) Defi Oracle Meta
- (network) IoTeX
- (network) IoTeX Testnet
- (network) Neo X Mainnet
- (network) Neo X Testnet
- (network) Bitlayer
- (network) Bitlayer Testnet
- Dynamic network handling, can get new networks at runtime from the CAL instead of from the hardcoded list
- Support for Ethermint's non-standard EIP-712 verifyingContract

### Changed

- Improved error handling in swap mode
- Provide NFT info APDU does not require a loaded NFT (721/1155) internal plugin anymore

### Fixed

- Potential overflow on the UI buffer used for amounts
- RLP parsing issue with legacy transactions

## [1.12.2](../../compare/1.12.1...1.12.2) - 2024-10-24

### Fixed

- Token swap with calldata

## [1.12.1](../../compare/1.12.0...1.12.1) - 2024-10-02

### Fixed

- Review of EIP-191 messages getting stuck and not responding to APDUs
- (clone) Ethereum Classic, gave it back the Ethereum derivation path

## [1.12.0](../../compare/1.11.3...1.12.0) - 2024-09-27

### Added

- Ledger PKI support
- Added support for swap with calldata (Thorswap / LiFi / ...)
- (network) PulseChain Testnet
- The app now provides the derivation path to its plugins
- Support for Trusted Name V2 payloads
- EIP-712 filtering on trusted names

### Removed

- (clone) ApothemNetwork
- (clone) Binance Smart Chain
- (clone) BTTC
- (clone) Conflux eSpace
- (clone) Cube
- (clone) KardiaChain
- (clone) Meter
- (clone) MultiVAC
- (clone) OKXChain
- (clone) POA
- (clone) Polygon
- (clone) Shyft

### Fixed

- (network) Apothemnetwork ticker
- Missing error handling on EIP-712, which could lead to a crash of the app
- EIP-712 filtering on fields within an empty array (requires client support with a new APDU)
- EIP-712 amount-join filtering with missing token information
- EIP-712 UI-code overflow on Stax which could lead to a crash of the app

### Changed

- (clone) Astar Polkadot EVM, removed Ethereum derivation path
- (clone) EnergyWebChain, removed Ethereum derivation path
- (clone) Ethereum Classic, removed Ethereum derivation path
- (clone) Moonbeam, removed Ethereum derivation path
- (clone) Moonriver, removed Ethereum derivation path
- (clone) Oasys, removed Ethereum derivation path
- (clone) Shiden EVM, removed Ethereum derivation path
- (clone) Songbird, removed Ethereum derivation path
- (clone) TecraCoin, removed Ethereum derivation path
- (clone) TecraTestnet, removed Ethereum derivation path
- (clone) Volta, removed Ethereum derivation path
- (clone) XDC Network, removed Ethereum derivation path
- Now the app sends back its response immediately instead of after the Transaction/Message signed/rejected screen on Stax & Flex
- Added blind-signing friction to EIP-712 v0 & unfiltered flows
- EIP-712 unfiltered flow now defaults to raw/verbose mode on Stax & Flex, but adds a skip button

## [1.11.3](../../compare/1.11.2...1.11.3) - 2024-09-04

### Changed

- Replaced MATIC by POL ticker for Polygon network

## [1.11.2](../../compare/1.11.1...1.11.2) - 2024-08-13

### Added

- Blind-signing setting

### Changed

- Simplified blind-signing warnings on Flex & Stax
- Restored blind-signing warning screen from < 1.11.0 on Nano devices

## [1.11.1](../../compare/1.11.0...1.11.1) - 2024-07-26

### Fixed

- (network/clone) Wanchain
- Refusal of EIP-712 messages after another transaction or message

## [1.11.0](../../compare/1.10.4...1.11.0) - 2024-07-24

### Added

- (network) Base Sepolia
- (network) Blast
- (network) Blast Sepolia
- (network) Mantle
- (network) Mantle Sepolia
- (network) Arbitrum Sepolia
- (network) Linea Sepolia
- (network) OP Sepolia
- (network) Etherlink Mainnet
- (network) ZetaChain
- (network) Astar zkEVM
- (network) Lisk
- (network) Lisk Sepolia
- (network) ZKsync
- (network) BOB
- (network) Electroneum
- New EIP-712 filtering modes (datetime, amount-join)
- New blind-signing warning flow before every blind-signed transaction flow
- New "From" field in transactions containing the wallet's derived address
- Ledger Flex support

### Removed

- (clone) Flare
- (clone) Flare Coston
- (clone) Eth Goerli
- (clone) Eth Ropsten
- Wallet ID support
- U2F support
- Blind-signing setting

### Changed

- Renamed Optimism to OP Mainnet
- Can now store up to 5 assets information (instead of 2)
- Can now buffer & show multiple EIP-712 fields on one page for NBGL devices
- Renamed the "Address" field in transactions to "To"

### Fixed

- Handling of EIP-712 empty arrays within nested structs

## [1.10.4](../../compare/1.10.3...1.10.4) - 2024-03-08

### Added

- Addresses in EIP-712 messages can now be displayed as a token ticker or a trusted domain name if a match is found
- Stax app now has icons of the other supported EVM chains
- (network) Bitcichain
- (network) Core
- (network) Bitrock Mainnet
- (network) Numbers Protocol
- (network) Linea
- (network) Holesky

### Removed

- Starkware support
- (clone) kUSD
- (clone) Tobalaba

### Changed

- Can now clear-sign NFT operations on other EVM chains without a clone app
- Can now swap on other EVM chains without a clone app
- Improved RAM usage
- Now shows an explicit ??? ticker when it is unknown instead of falling back to the native chain ticker

### Fixed

- Refusal of transactions with very large chain IDs even within specs
- Refusal of 10 character-long token tickers
- (network) Wanchain chain ID
- (network) Sepolia chain ID

## [1.10.3](../../compare/1.10.2...1.10.3) - 2023-07-27

### Added

- (network) LUKSO mainnet & testnet
- (network) Chiado
- (network) PulseChain
- (network) Neon EVM mainnet & devnet
- (network) Venidium
- (network) Telos EVM mainnet
- (network) OKBChain mainnet
- (network) Polygon zkEVM
- (network) Base
- (network) Sepolia
- ENS on chains that share the Ethereum derivation path
- Ledger Stax support

### Changed

- (network) xDai renamed to Gnosis

### Fixed

- Missing context cleanup between plugin calls
- Miscellaneous swap issues
- Improper EIP-712 array handling

## [1.10.2](../../compare/1.10.1...1.10.2) - 2023-04-24

### Added

- (clone) ID4Good
- (network) Cronos
- (network) Scroll
- (network) KCC
- (network) Rootstock
- (network) Evmos
- (network) Metis Andromeda
- (network) Kava EVM
- (network) Klaytn Cypress
- (network) Syscoin
- (network) Velas EVM
- (network) Boba Network
- (network) Energi
- Domain names support (LNX / LNS+)

### Changed

- Starknet blind signing wording

### Fixed

- Missing 44'/60' derivation path for XDC Network
- Small visual glitch with EIP-712 verbose mode with the "Review struct" page
- Possible overflow with very large transactions
- EnergyWebChain ticker
- Arbitrum ticker
- Error handling on EIP-191 APDUs
- Swap transactions handling

## [1.10.1](../../compare/1.10.0...1.10.1) - 2022-11-09

### Fixed

- App/device crash with fast button clicks on slow APDU transport on the new EIP-712 signature UI

## [1.10.0](../../compare/1.9.20...1.10.0) - 2022-10-26

### Changed

- EIP-712 signatures are now computed on-device and display their content (clear-signing) (LNX & LNS+)

## [1.9.20](../../compare/1.9.19...1.9.20) - 2022-10-10

### Added

- (clone) XDCNetwork
- (clone) Meter
- (clone) Multivac
- (clone) Tecra
- (clone) ApothemNetwork

### Changed

- EIP-191 improvements, now lets the user see the entire message one chunk at a time
  (255 characters for LNX & LNS+, 99 for LNS)

### Fixed

- Allow swap with variants

### Removed

- Compound support (will become its own plugin)

## [1.9.19](../../compare/1.9.18...1.9.19) - 2022-06-15

### Added

- (clone) OKXChain
- (clone) Cube
- (clone) Astar EVM
- (clone) Shiden EVM

### Changed

- EIP-191 signatures now show (up to 99 characters on LNS and 255 on LNX & LNS+) the actual data
  contained in the message (clear-signing)

### Fixed

- Bug with huge swap amounts

## [1.9.18](../../compare/1.9.17...1.9.18) - 2022-04-25

### Added

- Easier way of adding a chain into the Makefile
- EIP 1024 support
- (clone) Conflux chain
- (clone) Moonbeam chain
- (clone) KardiaChain
- (clone) BitTorrent Chain
- (clone) Wethio chain

### Changed

- More uniform naming between the ERC-721 & ERC-1155 screens

### Fixed

- CI (mostly Zemu tests)
- App crashing when trying to approve an NFT transaction without having received the NFT information beforehand
- App refusing to approve an NFT transaction with a long collection name

## [1.9.17](../../compare/1.9.16...1.9.17) - 2022-01-14

### Added

- Support for Non-Fungible Tokens (ERC-721 & ERC-1155)

## [1.9.16](../../compare/1.9.14...1.9.16) - 2022-01-13

### Added

- Shyft variant

## [1.9.14](../../compare/1.9.13...1.9.14) - 2021-11-30

### Added

- Added Moonriver BIP44 1285

### Fixed

- Fixed stark order signature on LNS

## [1.9.13](../../compare/1.9.12...1.9.13) - 2021-11-17

### Changed

- Small improvement in app size

## [1.9.12](../../compare/1.9.11...1.9.12) - 2021-11-12

### Fixed

- Fixed stark order signature on LNX

## [1.9.11](../../compare/1.9.10...1.9.11) - 2021-10-12

### Added

- Provide network ticker to plugins (especially helpful for Paraswap plugin)
- Polygon variant

## [1.9.10](../../compare/1.9.9...1.9.10) - 2021-10-08

### Added

- Add new app: Moonriver

## [1.9.9](../../compare/1.9.8...1.9.9) - 2021-10-08

### Changed

- Rollback the revert in wording change of "Contract data" in "Blind signing" that was introduced in v1.9.8

## [1.9.8](../../compare/1.9.7...1.9.8) - 2021-10-06

### Changed

- Revert wording change of "Contract data" in "Blind signing" from v1.9.5

### Added

- Goerli now has its own standalone app, with hardcoded deversifi tokens

## [1.9.7](../../compare/1.9.6...1.9.7) - 2021-9-30

### Fixed

- Fixed a bug where amounts displayed where wrong when the amount was huge (>=2^87)

## [1.9.6](../../compare/1.9.5...1.9.6) - 2021-9-29

### Fixed

- Fixed a bug where fees displayed were wrong on Starkware transactions

## [1.9.5](../../compare/1.9.4...1.9.5) - 2021-9-27

### Changed

- "Contract Data" is now replaced with "Blind sign", which carries more meaning for regular users.

### Added

- When blind signing is disabled in settings, and a transaction with smart conract interactions is sent to the app,
  a new warning screen pops to let the user know that the setting must be enabled to sign this kind of transactions.

## [1.9.4](../../compare/1.9.3...1.9.4) - 2021-9-14

### Added

- Added Arbitrum network

## [1.9.3](../../compare/1.9.2...1.9.3) - 2021-9-03

### Added

- Added better display for bigger chainIDs.
- Added support for Songbird.
- Added support for Celo.

### Changed

- Small refactor of `getEthDisplayableAddress` helper
- Improve Zemu tests to get parallelization
- Increased plugin interface to version 2
- Remove support for Theta and Flare

## [1.9.2](../../compare/1.9.0...1.9.2) - 2021-8-11

### Added

- Added support for bigger chainIDs.

### Fixed

- Fixed BSC icon colors.
- Fixed theta tokens.

## [1.9.0](../../compare/1.8.8...1.9.0) - 2021-8-05

### Added

- Added support for EIP-1559 and EIP-2930 style transactions.

## [1.8.8](../../compare/1.8.7...1.8.8) - 2021-7-21

### Added

- Added support for BSC.
- Add support for Lido plugin

## [1.8.7](../../compare/1.8.6...1.8.7) - 2021-7-9

### Added

Plugins can now check the address of the transaction sender.
Remove `m/44'/60'` derivation path authorisation for Theta app.

### Fixed

`additional_screens` was introduced previously but wasn't properly initialized in some cases.

## [1.8.6](../../compare/1.8.5...1.8.6) - 2021-7-5

### Added

Display the name of the network when signing a transaction, or the chain ID if the network is not known
When the network is known, amounts and fees are displayed in the network unit instead of ETH.

### Fixed

Fix some compilation warning

## [1.8.5](../../compare/1.7.9...1.8.5) - 2021-6-8

### Added

- Added support for external plugins.

## [1.7.9](../../compare/1.7.8...1.7.9) - 2021-6-2

### Added

- Added support for Flare Network and Theta Chain.

## [1.7.8](../../compare/1.7.7...1.7.8) - 2021-5-20

### Fixed

- Fixed a bug where transaction would sometimes not get properly signed.

## [1.7.7](../../compare/1.7.6...1.7.7) - 2021-5-19

### Special

- Version bump needed for deployment reasons, nothing changed.

## [1.7.6](../../compare/1.7.5...1.7.6) - 2021-5-14

### Special

- Version bump needed for deployment reasons, nothing changed.

## [1.7.7](../../compare/1.7.6...1.7.7) - 2021-5-19

- N/A

## [1.7.6](../../compare/1.7.6...1.7.6) - 2021-5-14

- N/A

## [1.7.5](../../compare/1.7.4...1.7.5) - 2021-5-10

### Fixed

- Fixed a bug with cx_ecfp_scalar_mult

## [1.7.4](../../compare/1.7.3...1.7.4) - 2021-5-6

### Fixed

- Fixed a bug that prevented using Ethereum sidechains

## [1.7.3](../../compare/1.7.2...1.7.3) - 2021-5-5

### Added

- Enable Ethereum 2 deposit on Nano S 2.0.0

## [1.7.2](../../compare/1.7.1...1.7.2) - 2021-5-5

### Added

- Improve Ethereum 2 deposit security:
  - Display the validator address on screen when depositing.
  - Abort signing when the account index of the withdrawal key is higher than INDEX_MAX.
  - Check that the destination field of the transaction is Ethereum 2 deposit contract.

## [1.7.1](../../compare/1.7.0...1.7.1) - 2021-5-5

### Added

- Support for Berlin hard fork: EIP2718 (transaction types) and EIP2930 (access list transactions)
- Display ChainID when transacting on chains which are not ethereum (BSC, Polygon, etc)

## [1.7.0](../../compare/1.6.6...1.7.0) - 2021-4-30

### Added

- Wallet ID feature now available on Nano X

## [1.6.6](../../compare/1.6.5...1.6.6) - 2021-4-16

### Added

- Improved Starkware support

## [1.6.5](../../compare/1.6.4...1.6.5) - 2021-2-12

### Added

- Add a setting to enable nonce display when approving transactions

## [1.6.4](../../compare/1.6.3...1.6.4) - 2021-1-12

### Fixed

- "warning" icon wasn't correctly displayed

## [1.6.3](../../compare/1.6.2...1.6.3) - 2020-12-10

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
