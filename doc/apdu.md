# APDU Documentation

  - [GET](#get)
    - [GET APP CONFIGURATION](#get-app-configuration)
    - [GET ETH PUBLIC ADDRESS](#get-eth-public-address)
    - [GET ETH2 PUBLIC KEY](#get-eth2-public-key)
  - [SIGN](#sign)
    - [SIGN ETH TRANSACTION](#sign-eth-transaction)
    - [SIGN ETH PERSONAL MESSAGE](#sign-eth-personal-message)
    - [SIGN ETH EIP 712](#sign-eth-eip-712)
  - [SET](#set)
    - [SET EXTERNAL PLUGIN](#set-external-plugin)
    - [SET PLUGIN](#set-plugin)
    - [SET ETH2 WITHDRAWAL INDEX](#set-eth2-withdrawal-index)
  - [Provide](#provide)
    - [PROVIDE ERC 20 TOKEN INFORMATION](#provide-erc-20-token-information)
    - [PROVIDE NFT INFORMATION](#provide-nft-information)

<br/>
<br/>
<br/>

## Global Instruction

|Name|INS|
|----|---|
|[GET PUBLIC ADDRESS](#get-eth-public-address)|0x02|
|[SIGN](#sign-eth-transaction)|0x04|
|[GET APP CONFIGURATION](#get-app-configuration)|0x06|
|[SIGN PERSONAL MESSAGE](#sign-eth-personal-message)|0x08|
|[PROVIDE ERC 20 TOKEN INFORMATION](#provide-erc-20-token-information)|0x0A|
|[SIGN EIP 712 MESSAGE](#sign-eth-eip-712)|0x0C|
|[GET ETH2 PUBLIC KEY](#get-eth2-public-key)|0x0E|
|[SET ETH2 WITHDRAWAL INDEX](#set-eth2-withdrawal-index)|0x10|
|[SET EXTERNAL PLUGIN](#set-external-plugin)|0x12|
|[PROVIDE NFT INFORMATION](#provide-nft-information)|0x14|
|[SET PLUGIN](#set-plugin)|0x16|

<br/>
<br/>

## GET

### GET APP CONFIGURATION
<details>

<summary>Description </summary>

This command returns specific application configuration

|CLA|INS|P1|P2|Lc|Le|
|---|---|--|--|--|--|
|E0|06|00|00|00|04|

:inbox_tray: input data

None

:outbox_tray: output data

|Description|Length|
|-----------|------|
|0x01 : arbitrary data signature enabled by user<br/>0x02 : ERC 20 Token information needs to be provided externally|1|
|Application major version|1|
|Application minor version|1|
|Application patch version|1|
</details>

<br/>

### GET ETH PUBLIC ADDRESS

<details>

<summary>Description </summary>

This command returns the public key and Ethereum address for the given BIP 32 path.  
The address can be optionally checked on the device before being returned.  

Usefull link:
- [HD Wallet by ledger](https://www.ledger.com/academy/crypto/what-are-hierarchical-deterministic-hd-wallets)
- [BIP-044](https://github.com/bitcoin/bips/blob/master/bip-0044.mediawiki)

|CLA|INS|P1                                               |P2                              |Lc        |Le        |
|---|---|-------------------------------------------------|--------------------------------|----------|----------|
|E0 |02 |00 : return address                              |00: do not return the chain code| variable | variable |
|   |   |01 : display address and confirm before returning|01 : return the chain code|     |          |          |

:inbox_tray: input data

|Description|Length|
|-----------|------|
|Number of BIP 32 derivations to perform (max 10)| 1|
|First derivation index (big endian)| 4|
| ... | 4|
| Last derivation index (big endian) | 4|

:outbox_tray: output data

|Description|Length|
|-----------|------|
|Public Key length|1|
|Uncompressed address length|var|
|Ethereum address length|1|
|Ethereum address|var|
|Chain code if requested|32|

Exemple:  
With path `"44'/60'/1'/0/0"`  

CLA: E0  
INS: 02  
P1  : 00  
P2  : 00  
Lc  : 11  (17 in hex)  
Le  :
  - 04  (number BIP 32 derivations)
  - 80 00 00 2c
  - 80 00 00 3C
  - 00 00 00 00
  - 00 00 00 00  

|CLA|INS|P1|P2|Lc|Le - BIP number|First derivation|Second derivation|Third derivation|Fourth derivation|
|-|-|-|-|-|-|-|-|-|-|
|  |  |  |  |  |  |`44'`   |`60'`   |`0`     |`0`     |
|E0|02|00|00|11|04|8000002C|8000003C|00000000|00000000|

-> E0 02 00 00 11 04 8000002C 8000003C 00000000 00000000

</details>

<br/>

### GET ETH2 PUBLIC KEY

<details>

<summary>Description </summary>

This command returns an Ethereum 2 BLS12-381 public key derived following EIP 2333 specification (https://eips.ethereum.org/EIPS/eip-2333)

This command has been supported since firmware version 1.6.0

|CLA|INS|P1|P2|Lc|Le|
|---|---|--|--|--|--|
|E0|0E|00 : return public key|00|variable|variable|
|||01 : display public key and confirm before returning||||
|||||||

:inbox_tray: input data

|Description|Length|
|-----------|------|
|Number of BIP 32 derivations to perform (max 10)|1|
|First derivation index (big endian)|4|
|...|4|
|Last derivation index (big endian)|4|

:outbox_tray: output data

|Description|Length|
|-----------|------|
|Public key|48|
</details>


<br/>
<br/>
<br/>

## SIGN

### SIGN ETH TRANSACTION

<details>

<summary>Description </summary>

This command signs an Ethereum transaction after having the user validate the following parameters

  - Gas price 
  - Gas limit
  - Recipient address
  - Value

The input data is the RLP encoded transaction, without v/r/s present, streamed to the device in 255 bytes maximum data chunks.

|CLA|INS|P1|P2|Lc|Le|
|---|---|--|--|--|--|
|E0|04|00 : first transaction data block|00|variable|variable|
|||80 : subsequent transaction data block||||
|||||||

:inbox_tray: input data (first transaction data block)

|Description|Length|
|-----------|------|
|Number of BIP 32 derivations to perform (max 10)|1|
|First derivation index (big endian)|4|
|...|4|
|Last derivation index (big endian)|4|
|RLP transaction chunk|variable|

:inbox_tray: input data  (other transaction data block)

|Description|Length|
|-----------|------|
|RLP transaction chunk|variable|

:outbox_tray: output data

|Description|Length|
|-----------|------|
|v|1|
|r|32|
|s|32|

</details>

<br/>

### SIGN ETH PERSONAL MESSAGE

<details>

<summary>Description </summary>

This command signs an Ethereum message following the personal_sign specification (https://github.com/ethereum/go-ethereum/pull/2940) after having the user validate the SHA-256 hash of the message being signed. 

This command has been supported since firmware version 1.0.8

The input data is the message to sign, streamed to the device in 255 bytes maximum data chunks


|CLA|INS|P1|P2|Lc|Le|
|---|---|--|--|--|--|
|E0|08|00 : first message data block|00|variable|variable|
|||80 : subsequent message data block||||
|||||||

:inbox_tray: input data (first message data block)

|Description|Length|
|-----------|------|
|Number of BIP 32 derivations to perform (max 10)|1|
|First derivation index (big endian)|4|
|...|4|
|Last derivation index (big endian)|4|
|Message length|4|
|Message chunk|variable|

:inbox_tray: input data (other transaction data block)

|Description|Length|
|-----------|------|
|Message chunk|variable|

:outbox_tray: output data

|Description|Length|
|-----------|------|
|v|1|
|r|32|
|s|32|

</details>

<br/>

### SIGN ETH EIP 712

<details>

<summary>Description </summary>

This command signs an Ethereum message following the EIP 712 specification (https://github.com/ethereum/EIPs/blob/master/EIPS/eip-712.md)

For implementation version 0, the domain hash and message hash are provided to the device, which displays them and returns the signature

This command has been supported since firmware version 1.5.0

|CLA|INS|P1|P2|Lc|Le|
|---|---|--|--|--|--|
|E0|0C|00|implementation version: 00|variable|variable|

:inbox_tray: input data

|Description|Length|
|-----------|------|
|Number of BIP 32 derivations to perform (max 10)|1|
|First derivation index (big endian)|4|
|...|4|
|Last derivation index (big endian)|4|
|Domain hash|32|
|Message hash|32|

:outbox_tray: output data

|Description|Length|
|-----------|------|
|v|1|
|r|32|
|s|32|

</details>

<br/>
<br/>
<br/>

## SET

### SET EXTERNAL PLUGIN

<details>

<summary>Description </summary>

This commands provides the name of a trusted binding of a plugin with a contract address and a supported method selector. This plugin will be called to interpret contract data in the following transaction signing command.

It shall be run immediately before performing a transaction involving a contract supported by this plugin to display the proper information to the user if necessary.

The function returns an error sw (0x6984) if the plugin requested is not installed on the device, 0x9000 otherwise.

The signature is computed on

len(pluginName) || pluginName || contractAddress || methodSelector

signed by the following secp256k1 public key 0482bbf2f34f367b2e5bc21847b6566f21f0976b22d3388a9a5e446ac62d25cf725b62a2555b2dd464a4da0ab2f4d506820543af1d242470b1b1a969a27578f353

|CLA|INS|P1|P2|Lc|Le|
|---|---|--|--|--|--|
|E0|12|00|00|variable|00|

:inbox_tray: input data

|Description|Length|
|-----------|------|
|Length of plugin name|1|
|plugin name|variable|
|contract address|20|
|method selector|4|
|signature|variable|

:outbox_tray: output data

None

</details>

<br/>

### SET PLUGIN

<details>

<summary>Description </summary>

This commands provides the name of a trusted binding of a plugin with a contract address and a supported method selector. This plugin will be called to interpret contract data in the following transaction signing command.

It can be used to set both internal and external plugins.

It shall be run immediately before performing a transaction involving a contract supported by this plugin to display the proper information to the user if necessary.

The function returns an error sw (0x6984) if the plugin requested is not installed on the device, 0x9000 otherwise.

The plugin names `ERC20`, `ERC721` and `ERC1155` are reserved. Additional plugin names might be added to this list in the future.

The signature is computed on

type || version || len(pluginName) || pluginName || address || selector || chainId || keyId || algorithmId || len(signature) || signature



|CLA|INS|P1|P2|Lc|Le|
|---|---|--|--|--|--|
|E0|16|00|00|variable|00|

:inbox_tray: input data

|Description|Length|
|-----------|------|
|Type|1|
|Version|1|
|Plugin Name Length|1|
|Plugin Name|variable|
|Address|20|
|Selector|4|
|Chain ID|8|
|KeyID|1|
|Algorithm|1|
|Signature Length|1|
|Signature|variable|

:outbox_tray: output data

None

</details>

<br/>

### SET ETH2 WITHDRAWAL INDEX

<details>

<summary>Description </summary>

This command sets the index of the Withdrawal key used as withdrawal credentials in an ETH2 deposit contract call signature. The path of the Withdrawal key is defined as m/12381/3600/index/0 according to EIP 2334 (https://eips.ethereum.org/EIPS/eip-2334)

The default index used is 0 if this method isn’t called before the deposit contract transaction is sent to the device to be signed

This command has been supported since firmware version 1.5.0

|CLA|INS|P1|P2|Lc|Le|
|---|---|--|--|--|--|
|E0|10|00|00|variable|variable|

:inbox_tray: input data

|Description|Length|
|-----------|------|
|Withdrawal key index (big endian)|4|

:outbox_tray: output data

None

</details>

<br/>
<br/>
<br/>

## Provide

### PROVIDE ERC 20 TOKEN INFORMATION

<details>

<summary>Description </summary>

This commands provides a trusted description of an ERC 20 token to associate a contract address with a ticker and number of decimals.

It shall be run immediately before performing a transaction involving a contract calling this contract address to display the proper token information to the user if necessary, as marked in GET APP CONFIGURATION flags.

The signature is computed on

ticker || address || number of decimals (uint4be) || chainId (uint4be)

signed by the following secp256k1 public key 0482bbf2f34f367b2e5bc21847b6566f21f0976b22d3388a9a5e446ac62d25cf725b62a2555b2dd4This command returns an Ethereum 2 BLS12-381 public key derived following EIP 2333 specification (https://eips.ethereum.org/EIPS/eip-2333)

This command has been supported since firmware version 1.6.064a4da0ab2f4d506820543af1d242470b1b1a969a27578f353

|CLA|INS|P1|P2|Lc|Le|
|---|---|--|--|--|--|
|E0|0A|00|00|variable|00|

:inbox_tray: input data

|Description|Length|
|-----------|------|
|Length of ERC 20 ticker|1|
|ERC 20 ticker|variable|
|ERC 20 contract address|20|
|Number of decimals (big endian encoded)|4|
|Chain ID (big endian encoded)|4|
|Token information signature|variable|

:outbox_tray: output data

none

</details>

<br/>

### PROVIDE NFT INFORMATION

<details>

<summary>Description </summary>

This commands provides a trusted description of an NFT to associate a contract address with a collectionName.

It shall be run immediately before performing a transaction involving a contract calling this contract address to display the proper nft information to the user if necessary, as marked in GET APP CONFIGURATION flags.

The signature is computed on:

type || version || len(collectionName) || collectionName || address || chainId || keyId || algorithmId

|CLA|INS|P1|P2|Lc|Le|
|---|---|--|--|--|--|
|E0|14|00|00|variable|00|

:inbox_tray: input data

|Description|Length|
|-----------|------|
|Type|1|
|Version|1|
|Collection Name Length|1|
|Collection Name|variable|
|Address|20|
|Chain ID|8|
|KeyID|1|
|Algorithm ID|1|
|Signature Length|1|
|Signature|variable|

:outbox_tray: output data

None

</details>

<br/>
<br/>
<br/>

[//]: # (## Command name)
[//]: # ()
[//]: # (<details>)
[//]: # ()
[//]: # (<summary>Description </summary>)
[//]: # ()
[//]: # ()
[//]: # (|CLA|INS|P1|P2|Lc|Le|)
[//]: # (|---|---|--|--|--|--|)
[//]: # (|||||||)
[//]: # (|||||||)
[//]: # (|||||||)
[//]: # ()
[//]: # (:inbox_tray: input data)
[//]: # ()
[//]: # (|Description|Length|)
[//]: # (|-----------|------|)
[//]: # (|||)
[//]: # (|||)
[//]: # (|||)
[//]: # ()
[//]: # (:outbox_tray: output data)
[//]: # ()
[//]: # (|Description|Length|)
[//]: # (|-----------|------|)
[//]: # (|||)
[//]: # (|||)
[//]: # (|||)
[//]: # ()
[//]: # (</details>)