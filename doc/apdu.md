# Ethereum application APDU specification

## About

This application describes the APDU messages interface to communicate with the Ethereum application.

The application covers the following functionalities:

- Retrieve a public Ethereum address given a BIP 32 path
- Sign a basic Ethereum transaction given a BIP 32 path
- Provide callbacks to validate the data associated to an Ethereum transaction

The application interface can be accessed over HID or BLE.

## APDUs list

The following APDUs are managed by the application. They are detailed in the following chapters.

| Description                         | Instruction |
|-------------------------------------|-------------|
| GET ETH PUBLIC ADDRESS              | 0x02        |
| SIGN ETH TRANSACTION                | 0x04        |
| GET APP CONFIGURATION               | 0x06        |
| SIGN ETH PERSONAL MESSAGE           | 0x08        |
| PROVIDE ERC 20 TOKEN INFORMATION    | 0x0A        |
| SIGN ETH EIP 712                    | 0x0C        |
| GET ETH2 PUBLIC KEY                 | 0x0E        |
| SET ETH2 WITHDRAWAL INDEX           | 0x10        |
| SET EXTERNAL PLUGIN                 | 0x12        |
| PROVIDE NFT INFORMATION             | 0x14        |
| SET PLUGIN                          | 0x16        |
| PERFORM PRIVACY OPERATION           | 0x18        |
| EIP712 SEND STRUCT DEFINITION       | 0x1A        |
| EIP712 SEND STRUCT IMPLEMENTATION   | 0x1C        |
| EIP712 FILTERING                    | 0x1E        |
| GET CHALLENGE                       | 0x20        |
| PROVIDE TRUSTED NAME                | 0x22        |
| PROVIDE ENUM VALUE                  | 0x24        |
| PROVIDE OPERATION INFO              | 0x26        |
| PROVIDE OPERATION FIELD DESCRIPTION | 0x28        |
| PROVIDE PROXY INFO                  | 0x2A        |
| PROVIDE NETWORK INFO                | 0x30        |
| PROVIDE TX SIMULATION               | 0x32        |
| SIGN EIP 7702 AUTHORIZATION         | 0x34        |
| PROVIDE SAFE ACCOUNT                | 0x36        |
| PROVIDE GATING                      | 0x38        |
| PROVIDE MAP ENTRY                   | 0x3A        |

### EIP-712 versions

A typed message is signed in one of two ways, V1 or V2, which never mix. A signing flow is a V1 one
from its first V1 command and a V2 one from its schema. Until the flow ends, by a signature, a
rejection or an error, the device refuses every command of the other version with `0x6980`:

- the V2 commands (`P2 = 02`) in a V1 flow;
- the V0 and V1 commands in a V2 flow: `EIP712 SEND STRUCT DEFINITION`,
  `EIP712 SEND STRUCT IMPLEMENTATION`, `EIP712 FILTERING` and `SIGN ETH EIP 712` with another `P2`.

## General purpose APDUs

### GET ETH PUBLIC ADDRESS

#### Description

This command returns the public key and Ethereum address for the given BIP 32 path.

The address can be optionally checked on the device before being returned.

#### Coding

_Command:_

| CLA | INS | P1                                                                         | P2                                                                | Lc       | Le       |
|-----|-----|----------------------------------------------------------------------------|-------------------------------------------------------------------|----------|----------|
| E0  | 02  | `00`: return address<br>`01`: display address and confirm before returning | `00`: do not return the chain code<br>`01`: return the chain code | variable | variable |

_Input data:_

| Description                                      | Length |
|--------------------------------------------------|--------|
| Number of BIP 32 derivations to perform (max 10) | 1      |
| First derivation index (big endian)              | 4      |
| ...                                              | 4      |
| Last derivation index (big endian)               | 4      |
| Chain ID (big endian) (optional)                 | 8      |

_Output data:_

| Description                    | Length |
|--------------------------------|--------|
| Public Key length              | 1      |
| Uncompressed Public Key        | var    |
| Ethereum address length        | 1      |
| ASCII-encoded Ethereum address | var    |
| Chain code if requested        | 32     |

### SIGN ETH TRANSACTION

#### Description

The application supports signing legacy or [EIP-2718](https://eips.ethereum.org/EIPS/eip-2718) transactions for Type 2
([EIP-1559](https://eips.ethereum.org/EIPS/eip-1559)) and Type 4 ([EIP-7702](https://eips.ethereum.org/EIPS/eip-7702)).
Type 4 transactions are only supported by builds compiled with `EIP7702_SUPPORT=1`.

This command signs an Ethereum transaction after having the user validate the following parameters:

- Gas price
- Gas limit
- Recipient address
- Value

The input data is the RLP encoded transaction (as per [pyethereum's
transactions.py](https://github.com/ethereum/pyethereum/blob/develop/ethereum/transactions.py#L22)), without v/r/s
present, streamed to the device in 255 bytes maximum data chunks.

#### Coding

_Command:_

| CLA | INS | P1                                                                            | P2                                                                 | Lc       | Le       |
|-----|-----|-------------------------------------------------------------------------------|--------------------------------------------------------------------|----------|----------|
| E0  | 04  | `00`: first transaction data block<br>`80`: subsequent transaction data block | `00`: process & start flow<br>`01`: store only<br>`02`: start flow | variable | variable |

_Input data (first transaction data block):_

If P2 == 0x02, then no data is provided.

| Description                                      | Length   |
|--------------------------------------------------|----------|
| Number of BIP 32 derivations to perform (max 10) | 1        |
| First derivation index (big endian)              | 4        |
| ...                                              | 4        |
| Last derivation index (big endian)               | 4        |
| RLP transaction chunk                            | variable |

_Input data (other transaction data block):_

| Description           | Length   |
|-----------------------|----------|
| RLP transaction chunk | variable |

_Output data:_

If P2 == 0x01, then no data is returned.

| Description | Length |
|-------------|--------|
| v           | 1      |
| r           | 32     |
| s           | 32     |

### GET APP CONFIGURATION

#### Description

This command returns specific application configuration.

#### Coding

_Command:_

| CLA | INS | P1  | P2  | Lc  | Le  |
|-----|-----|-----|-----|-----|-----|
| E0  | 06  | 00  | 00  | 00  | 04  |

_Input data:_

None

_Output data:_

| Description                                                                                                                                                                                                 | Length |
|-------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|--------|
| Flags<br>`0x01`: arbitrary data signature enabled by user<br>`0x02`: ERC 20 Token information needs to be provided externally<br>`0x10`: Transaction Check enabled<br>`0x20`: Transaction Check Opt-In done | 01     |
| Application major version                                                                                                                                                                                   | 01     |
| Application minor version                                                                                                                                                                                   | 01     |
| Application patch version                                                                                                                                                                                   | 01     |

### SIGN ETH PERSONAL MESSAGE

#### Description

This command signs an Ethereum message following the [personal_sign
specification](https://github.com/ethereum/go-ethereum/pull/2940) after having the user validate the Keccak-256 hash of
the message being signed.

This command has been supported since firmware version 1.0.8.

The input data is the message to sign, streamed to the device in 255 bytes maximum data chunks.

#### Coding

_Command:_

| CLA | INS | P1                                                                    | P2  | Lc       | Le       |
|-----|-----|-----------------------------------------------------------------------|-----|----------|----------|
| E0  | 08  | `00`: first message data block<br>`80`: subsequent message data block | 00  | variable | variable |

_Input data (first message data block):_

| Description                                      | Length   |
|--------------------------------------------------|----------|
| Number of BIP 32 derivations to perform (max 10) | 1        |
| First derivation index (big endian)              | 4        |
| ...                                              | 4        |
| Last derivation index (big endian)               | 4        |
| Message length                                   | 4        |
| Message chunk                                    | variable |

_Input data (other transaction data block):_

| Description   | Length   |
|---------------|----------|
| Message chunk | variable |

_Output data:_

| Description | Length |
|-------------|--------|
| v           | 1      |
| r           | 32     |
| s           | 32     |

### PROVIDE ERC 20 TOKEN INFORMATION

#### Description

This command provides a trusted description of an [ERC-20](https://eips.ethereum.org/EIPS/eip-20) token to associate a
contract address with a ticker and number of decimals.

It shall be run immediately before performing a transaction involving a contract calling this contract address to
display the proper token information to the user if necessary, as marked in GET APP CONFIGURATION flags.

Two formats are supported, selected by P1:

- **P1 = 00 (legacy)**: raw binary payload, signed with a dedicated secp256k1 key.
- **P1 = 01 (TLV)**: TLV dynamic descriptor payload as defined in
  [DYNAMIC_TOKEN_DESCRIPTOR](tlv_structs.md#dynamic_token_descriptor), signed via the Ledger PKI (key usage
  `COIN_META`). Supports multi-APDU chunking via P2.

##### Legacy format (P1 = 00)

The signature is computed on:

```text
ticker || address || number of decimals (uint4be) || chainId (uint4be)
```

signed by the following secp256k1 public key:
`045e6c1020c14dc46442fe89f97c0b68cdb15976dc24f24c316e7b30fe4e8cc76b1489150c21514ebf440ff5dea5393d83de5358cd098fce8fd0f81daa94979183`

##### TLV format (P1 = 01)

The signature covers the SHA-256 hash of all TLV fields except the SIGNATURE tag itself, verified via the Ledger PKI
with key usage `COIN_META`.

#### Coding

_Command:_

| CLA | INS | P1                  | P2                                         | LC       |
|-----|-----|---------------------|--------------------------------------------|----------|
| E0  | 0A  | `00`: legacy format | 00                                         | variable |
| E0  | 0A  | `01`: TLV format    | `01`: first chunk<br>`00`: following chunk | variable |

_Input data:_

##### If P1 == legacy format

| Description                             | Length   |
|-----------------------------------------|----------|
| Length of ERC 20 ticker                 | 1        |
| ERC 20 ticker                           | variable |
| ERC 20 contract address                 | 20       |
| Number of decimals (big endian encoded) | 4        |
| Chain ID (big endian encoded)           | 4        |
| Token information signature             | variable |

##### If P1 == TLV format

###### If P2 == first chunk

| Description                                                                | Length   |
|----------------------------------------------------------------------------|----------|
| Payload length                                                             | 2        |
| [DYNAMIC_TOKEN_DESCRIPTOR struct](tlv_structs.md#dynamic_token_descriptor) | variable |

###### If P2 == following chunk

| Description                                                                | Length   |
|----------------------------------------------------------------------------|----------|
| [DYNAMIC_TOKEN_DESCRIPTOR struct](tlv_structs.md#dynamic_token_descriptor) | variable |

_Output data:_

Returned only when the last (or only) chunk has been processed.

| Description                                       | Length |
|---------------------------------------------------|--------|
| Asset index where the information has been stored | 1      |

### SIGN ETH EIP 712

#### Description

This command signs an Ethereum message following the [EIP-712 specification](https://eips.ethereum.org/EIPS/eip-712).

For V0 implementation, the domain hash and message hash are provided to the device, which displays them and returns the
signature.

This command has been supported since app version 1.5.0.

The full implementation uses all the JSON data and does all the hashing on the device, it has been supported since app
version 1.9.19. This command should come last, after all the EIP712 SEND STRUCT DEFINITION & SEND STRUCT IMPLEMENTATION.

The V2 implementation computes the hashes and signs after the schema and the values have been
delivered with EIP712 SEND STRUCT DEFINITION and EIP712 SEND STRUCT IMPLEMENTATION (V2 branches,
`P2 = 02`). It carries no input data: the BIP 32 path is part of the
[EIP712_VALUES struct](tlv_structs.md#eip712_values).

#### Coding

_Command:_

| CLA | INS | P1  | P2                                                                                            | Lc       | Le       |
|-----|-----|-----|-----------------------------------------------------------------------------------------------|----------|----------|
| E0  | 0C  | 00  | `00`: V0 (legacy) implementation<br>`01`: V1 (full) implementation<br>`02`: V2 implementation | variable | variable |

Any other P2 is refused with `0x6B00`.

_Input data:_

##### If P2 == V0 or V1 implementation

| Description                                      | Length |
|--------------------------------------------------|--------|
| Number of BIP 32 derivations to perform (max 10) | 1      |
| First derivation index (big endian)              | 4      |
| ...                                              | 4      |
| Last derivation index (big endian)               | 4      |
| Domain hash _(only for V0)_                      | 32     |
| Message hash _(only for V0)_                     | 32     |

##### If P2 == V2 implementation

None. `P1` must be `00` and `Lc` must be `0`: the command is `e0 0c 00 02 00`. It starts the review of
the message sent with the previous V2 commands, and returns the signature once the user approves it.
If the user rejects the review, it answers `0x6985`.

The device refuses the command, and shows nothing, with:

- `0x6A86` if `P1` is not `00`;
- `0x6A80` if `Lc` is not `0`;
- `0x6980` when the schema or the values were not received, and when the message was already
  signed;
- `0x6A80` if the descriptors received are not the announced ones: any that is missing, added,
  altered or out of order;
- `0x6A88` if a call they named is not described, or not completely;
- `0x6A80` if the device cannot hash the message or build the review, see below.

When the message info announced field descriptors, the device has to have received all of them first
([PROVIDE OPERATION FIELD DESCRIPTION](#provide-operation-field-description), `P2 = 02`), along with the
description of every call they named. The review is then the one of the described message, see
[Reviewing a described message](eip712.md#reviewing-a-described-message). It is not blind
signing: it needs no setting and shows no warning.

Without announced descriptors, the review shows every value raw, see
[Reviewing a message without descriptors](eip712.md#reviewing-a-message-without-descriptors).
It is blind signing: it needs the "Blind signing" or the "Raw messages" setting of the app, and the
device refuses the command with neither. It shows the warning. The name of each field it shows has
to be printable ASCII with a visible character, see [Names](eip712.md#names).

_Output data:_

| Description | Length |
|-------------|--------|
| v           | 1      |
| r           | 32     |
| s           | 32     |

### GET ETH2 PUBLIC KEY

#### Description

This command returns an Ethereum 2 BLS12-381 public key derived following [EIP-2333
specification](https://eips.ethereum.org/EIPS/eip-2333).

This command has been supported since firmware version 1.6.0.

#### Coding

_Command:_

| CLA | INS | P1                                                                               | P2  | Lc       | Le       |
|-----|-----|----------------------------------------------------------------------------------|-----|----------|----------|
| E0  | 0E  | `00`: return public key<br>`01`: display public key and confirm before returning | 00  | variable | variable |

_Input data:_

| Description                                      | Length |
|--------------------------------------------------|--------|
| Number of BIP 32 derivations to perform (max 10) | 1      |
| First derivation index (big endian)              | 4      |
| ...                                              | 4      |
| Last derivation index (big endian)               | 4      |

_Output data:_

| Description | Length |
|-------------|--------|
| Public key  | 48     |

### SET ETH2 WITHDRAWAL INDEX

#### Description

This command sets the index of the Withdrawal key used as withdrawal credentials in an ETH2 deposit contract call
signature. The path of the Withdrawal key is defined as `m/12381/3600/index/0` according to
[EIP-2334](https://eips.ethereum.org/EIPS/eip-2334).

The default index used is 0 if this method isn't called before the deposit contract transaction is sent to the device to
be signed.

This command has been supported since firmware version 1.5.0.

#### Coding

_Command:_

| CLA | INS | P1  | P2  | Lc       | Le       |
|-----|-----|-----|-----|----------|----------|
| E0  | 10  | 00  | 00  | variable | variable |

_Input data:_

| Description                       | Length |
|-----------------------------------|--------|
| Withdrawal key index (big endian) | 4      |

_Output data:_

None

### SET EXTERNAL PLUGIN

#### Description

This command provides the name of a trusted binding of a plugin with a contract address and a supported method selector.
This plugin will be called to interpret contract data in the following transaction signing command.

It shall be run immediately before performing a transaction involving a contract supported by this plugin to display the
proper information to the user if necessary.

The function returns an error sw (0x6984) if the plugin requested is not installed on the device, 0x9000 otherwise.

The signature is computed on:

```text
len(pluginName) || pluginName || contractAddress || methodSelector
```

signed by the following secp256k1 public key:
`0482bbf2f34f367b2e5bc21847b6566f21f0976b22d3388a9a5e446ac62d25cf725b62a2555b2dd464a4da0ab2f4d506820543af1d242470b1b1a969a27578f353`

#### Coding

_Command:_

| CLA | INS | P1  | P2  | Lc       | Le  |
|-----|-----|-----|-----|----------|-----|
| E0  | 12  | 00  | 00  | variable | 00  |

_Input data:_

| Description           | Length   |
|-----------------------|----------|
| Length of plugin name | 1        |
| plugin name           | variable |
| contract address      | 20       |
| method selector       | 4        |
| signature             | variable |

_Output data:_

None

### PROVIDE NFT INFORMATION

#### Description

This command provides a trusted description of an NFT to associate a contract address with a collectionName.

It shall be run immediately before performing a transaction involving a contract calling this contract address to
display the proper nft information to the user if necessary, as marked in GET APP CONFIGURATION flags.

The signature is computed on:

```text
type || version || len(collectionName) || collectionName || address || chainId || keyId || algorithmId
```

#### Coding

_Command:_

| CLA | INS | P1  | P2  | Lc       | Le  |
|-----|-----|-----|-----|----------|-----|
| E0  | 14  | 00  | 00  | variable | 00  |

_Input data:_

| Description            | Length   |
|------------------------|----------|
| Type                   | 1        |
| Version                | 1        |
| Collection Name Length | 1        |
| Collection Name        | variable |
| Address                | 20       |
| Chain ID               | 8        |
| KeyID                  | 1        |
| Algorithm ID           | 1        |
| Signature Length       | 1        |
| Signature              | variable |

_Output data:_

| Description                                       | Length |
|---------------------------------------------------|--------|
| Asset index where the information has been stored | 1      |

### SET PLUGIN

#### Description

This command provides the name of a trusted binding of a plugin with a contract address and a supported method selector.
This plugin will be called to interpret contract data in the following transaction signing command.

It can be used to set both internal and external plugins.

It shall be run immediately before performing a transaction involving a contract supported by this plugin to display the
proper information to the user if necessary.

The function returns an error sw (0x6984) if the plugin requested is not installed on the device, 0x9000 otherwise.

The plugin names `ERC20`, `ERC721` and `ERC1155` are reserved. Additional plugin names might be added to this list in
the future.

The signature is computed on:

```text
type || version || len(pluginName) || pluginName || address || selector || chainId || keyId || algorithmId
```

#### Coding

_Command:_

| CLA | INS | P1  | P2  | Lc       | Le  |
|-----|-----|-----|-----|----------|-----|
| E0  | 16  | 00  | 00  | variable | 00  |

_Input data:_

| Description        | Length   |
|--------------------|----------|
| Type               | 1        |
| Version            | 1        |
| Plugin Name Length | 1        |
| Plugin Name        | variable |
| Address            | 20       |
| Selector           | 4        |
| Chain ID           | 8        |
| KeyID              | 1        |
| Algorithm ID       | 1        |
| Signature Length   | 1        |
| Signature          | variable |

_Output data:_

None

### PERFORM PRIVACY OPERATION

#### Description

This command performs privacy operations as defined in
[EIP-1024](https://ethereum-magicians.org/t/eip-1024-cross-client-encrypt-decrypt/505).

It can return the public encryption key on Curve25519 for a given Ethereum account or the shared secret (generated by
the scalar multiplication of the remote public key by the account private key on Curve25519) used to decrypt private
data encrypted for a given Ethereum account.

All data can be optionally checked on the device before being returned.

#### Coding

_Command:_

| CLA | INS | P1                                                                   | P2                                                                       | Lc       | Le       |
|-----|-----|----------------------------------------------------------------------|--------------------------------------------------------------------------|----------|----------|
| E0  | 18  | `00`: return data<br>`01`: display data and confirm before returning | `00`: return the public encryption key<br>`01`: return the shared secret | variable | variable |

_Input data:_

| Description                                                          | Length |
|----------------------------------------------------------------------|--------|
| Number of BIP 32 derivations to perform (max 10)                     | 1      |
| First derivation index (big endian)                                  | 4      |
| ...                                                                  | 4      |
| Last derivation index (big endian)                                   | 4      |
| Third party public key on Curve25519, if returning the shared secret | 32     |

_Output data:_

| Description                            | Length |
|----------------------------------------|--------|
| Public encryption key or shared secret | 32     |

### EIP712 SEND STRUCT DEFINITION

#### Description

This command sends the message definition with all its types.
These commands should come before the EIP712 SEND STRUCT IMPLEMENTATION ones.

This description is the V1 one. The V2 schema is a single TLV payload, see
[the V2 branch](#if-p2--v2-schema).

#### Coding

_Command:_

| CLA | INS | P1                                                                        | P2                                                                   | LC       | Le       |
|-----|-----|---------------------------------------------------------------------------|----------------------------------------------------------------------|----------|----------|
| E0  | 1A  | `00`: unused (V1)<br>`01`: first chunk (V2)<br>`00`: following chunk (V2) | `00`: struct name (V1)<br>`FF`: struct field (V1)<br>`02`: V2 schema | variable | variable |

_Input data:_

##### If P2 == struct name

| Description | Length (byte) |
|-------------|---------------|
| Name        | LC            |

##### If P2 == struct field

| Description                 | Length (byte) | Mandatory |
|-----------------------------|---------------|-----------|
| TypeDesc (type description) | 1             | Yes       |
| TypeNameLength              | 1             | No        |
| TypeName                    | variable      | No        |
| TypeSize                    | 1             | No        |
| ArrayLevelCount             | 1             | No        |
| ArrayLevels                 | variable      | No        |
| KeyNameLength               | 1             | Yes       |
| KeyName                     | variable      | Yes       |

###### TypeDesc

From MSB to LSB:

| Description                          | Length (bit) |
|--------------------------------------|--------------|
| TypeArray (is it an array?)          | 1            |
| TypeSize (is a type size specified?) | 1            |
| Unused                               | 2            |
| Type                                 | 4            |

How to interpret Type from its value:

| Value | Type                 |
|-------|----------------------|
| 0     | custom (struct type) |
| 1     | int                  |
| 2     | uint                 |
| 3     | address              |
| 4     | bool                 |
| 5     | string               |
| 6     | fixed-sized bytes    |
| 7     | dynamic-sized bytes  |

###### TypeName

_Only present if the Type is set to custom._

Indicates the name of the struct that will be the type of the field.

###### TypeSize

_Only present if the TypeSize bit is set in TypeDesc._

Indicates the byte size of the field. (Ex: 8 for an int64)

###### ArrayLevelCount

_Only present if the TypeArray bit is set in TypeDesc._

Indicates how many array levels that field has (Ex: 3 for `int16[2][][4]`).

###### ArrayLevels

_Only present if the TypeArray bit is set in TypeDesc._

Types of array level:

| Byte value | Type                     |
|------------|--------------------------|
| 0          | Dynamic sized (`type[]`) |
| 1          | Fixed size (`type[N]`)   |

Each fixed-sized array level is followed by a byte indicating its size (number of elements).

##### If P2 == V2 schema

With this P2, P1 is the chunk flag: `01` first chunk, `00` following chunk.

###### If P1 == first chunk

| Description                                          | Length (byte) |
|------------------------------------------------------|---------------|
| struct size (BE)                                     | 2             |
| [EIP712_SCHEMA struct](tlv_structs.md#eip712_schema) | variable      |

###### If P1 == following chunk

| Description                                          | Length (byte) |
|------------------------------------------------------|---------------|
| [EIP712_SCHEMA struct](tlv_structs.md#eip712_schema) | variable      |

The device locks the schema once it has received and accepted a full `EIP712_SCHEMA`. It answers:

- `0x6A86` for a `P1` other than `01` and `00`;
- `0x6980` for a first chunk when the app is not idle (this includes a second schema in the same
  session), and for a following chunk when no incomplete schema is in progress;
- `0x6A80` for a payload that is malformed or that the device refuses, see
  [EIP712_SCHEMA](tlv_structs.md#eip712_schema).

_Output data:_

None

### EIP712 SEND STRUCT IMPLEMENTATION

#### Description

This command sends the message implementation with all its values.
These commands should come after the EIP712 SEND STRUCT DEFINITION ones.

This description is the V1 one. The V2 values are a single TLV payload, see
[the V2 branch](#if-p2--v2-values).

#### Coding

_Command:_

| CLA | INS | P1                                                                                                                        | P2                                                                                       | LC       | Le       |
|-----|-----|---------------------------------------------------------------------------------------------------------------------------|------------------------------------------------------------------------------------------|----------|----------|
| E0  | 1C  | `00`: complete send (V1)<br>`01`: partial send, more to come (V1)<br>`01`: first chunk (V2)<br>`00`: following chunk (V2) | `00`: root struct (V1)<br>`0F`: array (V1)<br>`FF`: struct field (V1)<br>`02`: V2 values | variable | variable |

_Input data:_

##### If P2 == root struct

| Description | Length (byte) |
|-------------|---------------|
| Name        | LC            |

Sets the name of the upcoming root structure all the following fields will be a part of until we set another root
structure.

##### If P2 == array

| Description | Length (byte) |
|-------------|---------------|
| Array size  | 1             |

Sets the size of the upcoming array the following N fields will be a part of.

##### If P2 == struct field

| Description  | Length (byte) |
|--------------|---------------|
| Value length | 2 (BE)        |
| Value        | variable      |

Sets the raw value of the next field in order in the current root structure.
Raw as in, an integer in the JSON file represented as "128" would only be 1 byte long (`0x80`)
instead of 3 as an array of ASCII characters, same for addresses and so on.

##### If P2 == V2 values

With this P2, P1 is the chunk flag: `01` first chunk, `00` following chunk.

###### If P1 == first chunk

| Description                                          | Length (byte) |
|------------------------------------------------------|---------------|
| struct size (BE)                                     | 2             |
| [EIP712_VALUES struct](tlv_structs.md#eip712_values) | variable      |

###### If P1 == following chunk

| Description                                          | Length (byte) |
|------------------------------------------------------|---------------|
| [EIP712_VALUES struct](tlv_structs.md#eip712_values) | variable      |

This call requires the `EIP712_SCHEMA` (see EIP712 SEND STRUCT DEFINITION, V2 branch) to be
locked, and the device accepts the values once per session. It answers:

- `0x6A86` for a `P1` other than `01` and `00`;
- `0x6980` when no schema was received, and for a first chunk when the values
  were already received;
- `0x6A80` for a payload that is malformed or that the device refuses, see
  [EIP712_VALUES](tlv_structs.md#eip712_values), and for a following chunk that continues no first
  chunk.

It may be followed by an [EIP712_MESSAGE_INFO struct](eip712.md#eip712_message_info), sent with
PROVIDE OPERATION INFO and `P2 = 02`, before EIP712 SIGN.

_Output data:_

None

### EIP712 FILTERING

#### Description

This command provides a trusted way of deciding what information from the JSON data to show and replace some values by
more meaningful ones.

This mode can be overridden by the in-app setting to fully clear-sign [EIP-712](https://eips.ethereum.org/EIPS/eip-712)
messages.

This command is part of V1. A V2 message has no filtering command: its field descriptors do the same job, see
[EIP712_FIELD_DESCRIPTOR](tlv_structs.md#eip712_field_descriptor).

For the signatures:

- The chain ID used for the signature must be 8 bytes wide.
- The schema hash = sha224sum of the value of _types_ at the root of the JSON data (stripped of all spaces and newlines)

##### Activation

Full filtering is disabled by default and has to be changed with this APDU (default behaviour is basic filtering handled
by the app itself).

Field substitution will be ignored if the full filtering is not activated.

This command should come before the domain & message implementations. If activated, fields will be by default hidden
unless they receive a field name substitution.

##### Discarded filter path

This command gives the app the absolute path of the upcoming filter which will be discarded (because it targets a field
within an empty array).

The next filter should be marked as discarded (with P1) to be able to use this given filter path.

##### Message info

This command should come right after the implementation of the domain has been sent with **SEND STRUCT IMPLEMENTATION**,
just before sending the message implementation.
The first byte is used as a magic number so that a signature of one type cannot be valid as another type.

The signature is computed on:

```text
183 || chain ID (BE) || contract address || schema hash || filters count || display name
```

##### Amount-join token

This command should come before the corresponding **SEND STRUCT IMPLEMENTATION** and are only usable for message fields
(and not domain ones).
The first byte is used as a magic number so that a signature of one type cannot be valid as another type.

The signature is computed on:

```text
11 || chain ID (BE) || contract address || schema hash || field path || join identifier
```

##### Amount-join value

This command should come before the corresponding **SEND STRUCT IMPLEMENTATION** and are only usable for message fields
(and not domain ones).
The first byte is used as a magic number so that a signature of one type cannot be valid as another type.

A join identifier of 0xFF indicates the token address is in the _verifyingContract_ field of the EIP712Domain so the app
won't receive an amount-join token filtering APDU. This enables support for Permit
([ERC-2612](https://eips.ethereum.org/EIPS/eip-2612)) messages.

The signature is computed on:

```text
22 || chain ID (BE) || contract address || schema hash || field path || display name || join identifier
```

##### Date / Time

This command should come before the corresponding **SEND STRUCT IMPLEMENTATION** and are only usable for message fields
(and not domain ones).
The first byte is used as a magic number so that a signature of one type cannot be valid as another type.

The signature is computed on:

```text
33 || chain ID (BE) || contract address || schema hash || field path || display name
```

##### Trusted name

This command should come before the corresponding **SEND STRUCT IMPLEMENTATION** and are only usable for message fields
(and not domain ones).
The first byte is used as a magic number so that a signature of one type cannot be valid as another type.

The signature is computed on:

```text
44 || chain ID (BE) || contract address || schema hash || field path || display name || name types || name sources
```

##### Calldata info

This command should come before the first other calldata filter of the same calldata index.
The first byte is used as a magic number so that a signature of one type cannot be valid as another type.

The signature is not computed on a field path, because this is not tied to a field within the message and is valid as
long as the app receives it during the [EIP-712](https://eips.ethereum.org/EIPS/eip-712) flow and before any other
calldata filter.

The signature is computed on:

```text
55 || chain ID (BE) || contract address || schema hash || calldata index || filter flags
```

##### Calldata value

This command should come before the corresponding **SEND STRUCT IMPLEMENTATION** and are only usable for message fields
(and not domain ones).
The first byte is used as a magic number so that a signature of one type cannot be valid as another type.

The signature is computed on:

```text
66 || chain ID (BE) || contract address || schema hash || field path || calldata index
```

##### Calldata callee

This command should come before the corresponding **SEND STRUCT IMPLEMENTATION** and are only usable for message fields
(and not domain ones).
The first byte is used as a magic number so that a signature of one type cannot be valid as another type.

The signature is computed on:

```text
77 || chain ID (BE) || contract address || schema hash || field path || calldata index
```

##### Calldata chain ID

This command should come before the corresponding **SEND STRUCT IMPLEMENTATION** and are only usable for message fields
(and not domain ones).
The first byte is used as a magic number so that a signature of one type cannot be valid as another type.

The signature is computed on:

```text
88 || chain ID (BE) || contract address || schema hash || field path || calldata index
```

##### Calldata selector

This command should come before the corresponding **SEND STRUCT IMPLEMENTATION** and are only usable for message fields
(and not domain ones).
The first byte is used as a magic number so that a signature of one type cannot be valid as another type.

The signature is computed on:

```text
99 || chain ID (BE) || contract address || schema hash || field path || calldata index
```

##### Calldata amount

This command should come before the corresponding **SEND STRUCT IMPLEMENTATION** and are only usable for message fields
(and not domain ones).
The first byte is used as a magic number so that a signature of one type cannot be valid as another type.

The signature is computed on:

```text
110 || chain ID (BE) || contract address || schema hash || field path || calldata index
```

##### Calldata spender

This command should come before the corresponding **SEND STRUCT IMPLEMENTATION** and are only usable for message fields
(and not domain ones).
The first byte is used as a magic number so that a signature of one type cannot be valid as another type.

The signature is computed on:

```text
121 || chain ID (BE) || contract address || schema hash || field path || calldata index
```

##### Show raw field

This command should come before the corresponding **SEND STRUCT IMPLEMENTATION** and are only usable for message fields
(and not domain ones).
The first byte is used as a magic number so that a signature of one type cannot be valid as another type.

The signature is computed on:

```text
72 || chain ID (BE) || contract address || schema hash || field path || display name
```

#### Coding

_Command:_

| CLA | INS | P1                                | P2                                                                                                                                                                                                                                                                                                                                                                       | LC       | Le       |
|-----|-----|-----------------------------------|--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|----------|----------|
| E0  | 1E  | `00`: standard<br>`01`: discarded | `00`: activation<br>`01`: discarded filter path<br>`0F`: message info<br>`F4`: calldata spender<br>`F5`: calldata amount<br>`F6`: calldata selector<br>`F7`: calldata chain ID<br>`F8`: calldata callee<br>`F9`: calldata value<br>`FA`: calldata info<br>`FB`: trusted name<br>`FC`: date/time<br>`FD`: amount-join token<br>`FE`: amount-join value<br>`FF`: raw field | variable | variable |

_Input data:_

##### If P2 == activation

None

##### If P2 == discarded filter path

| Description | Length (byte) |
|-------------|---------------|
| Path length | 1             |
| Path        | variable      |

##### If P2 == message info

| Description         | Length (byte) |
|---------------------|---------------|
| Display name length | 1             |
| Display name        | variable      |
| Filters count       | 1             |
| Signature length    | 1             |
| Signature           | variable      |

##### If P2 == calldata info

The calldata index is there to differentiate calldatas if the message contains multiple and if they are presented in a
way where the filters would be intertwined (the app would start receiving filters about a second calldata before
receiving all the filters from the first one).

This command is not counted towards the `Filters count` of the `message info` command, but other calldata commands are.

| Description          | Type                    | Length (byte) |
|----------------------|-------------------------|---------------|
| Calldata index       | uint8                   | 1             |
| Value filter flag    | bool                    | 1             |
| Callee filter flag   | `CalldataParamPresence` | 1             |
| Chain ID filter flag | bool                    | 1             |
| Selector filter flag | bool                    | 1             |
| Amount filter flag   | bool                    | 1             |
| Spender filter flag  | `CalldataParamPresence` | 1             |
| Signature length     | bool                    | 1             |
| Signature            | bool                    | variable      |

`enum CalldataParamPresence`

| Name                                   | Integer value |
|----------------------------------------|---------------|
| None                                   | 0             |
| Present (filtered message field)       | 1             |
| Present (domain's `verifyingContract`) | 2             |

##### If P2 == calldata value

| Description      | Length (byte) |
|------------------|---------------|
| Calldata index   | 1             |
| Signature length | 1             |
| Signature        | variable      |

##### If P2 == calldata callee

| Description      | Length (byte) |
|------------------|---------------|
| Calldata index   | 1             |
| Signature length | 1             |
| Signature        | variable      |

##### If P2 == calldata chain ID

| Description      | Length (byte) |
|------------------|---------------|
| Calldata index   | 1             |
| Signature length | 1             |
| Signature        | variable      |

##### If P2 == calldata selector

| Description      | Length (byte) |
|------------------|---------------|
| Calldata index   | 1             |
| Signature length | 1             |
| Signature        | variable      |

##### If P2 == calldata amount

| Description      | Length (byte) |
|------------------|---------------|
| Calldata index   | 1             |
| Signature length | 1             |
| Signature        | variable      |

##### If P2 == calldata spender

| Description      | Length (byte) |
|------------------|---------------|
| Calldata index   | 1             |
| Signature length | 1             |
| Signature        | variable      |

##### If P2 == trusted name

| Description         | Length (byte) |
|---------------------|---------------|
| Display name length | 1             |
| Display name        | variable      |
| Name types count    | 1             |
| Name types          | variable      |
| Name sources count  | 1             |
| Name sources        | variable      |
| Signature length    | 1             |
| Signature           | variable      |

##### If P2 == date / time

| Description         | Length (byte) |
|---------------------|---------------|
| Display name length | 1             |
| Display name        | variable      |
| Signature length    | 1             |
| Signature           | variable      |

##### If P2 == amount-join token

| Description      | Length (byte) |
|------------------|---------------|
| Join identifier  | 1             |
| Signature length | 1             |
| Signature        | variable      |

##### If P2 == amount-join value

| Description         | Length (byte) |
|---------------------|---------------|
| Display name length | 1             |
| Display name        | variable      |
| Join identifier     | 1             |
| Signature length    | 1             |
| Signature           | variable      |

##### If P2 == show raw field

| Description         | Length (byte) |
|---------------------|---------------|
| Display name length | 1             |
| Display name        | variable      |
| Signature length    | 1             |
| Signature           | variable      |

_Output data:_

None

### GET CHALLENGE

#### Description

Sends a random 32-bit long value. Can prevent replay of signed payloads when the challenge is included in said payload.

#### Coding

_Command:_

| CLA | INS | P1  | P2  | LC  |
|-----|-----|-----|-----|-----|
| E0  | 20  | 00  | 00  | 00  |

_Input data:_

None

_Output data:_

| Description          | Length |
|----------------------|--------|
| Challenge value (BE) | 4      |

### PROVIDE TRUSTED NAME

#### Description

This command provides a trusted name (like an ENS domain) to be displayed during transactions in place of the address it
is associated to. It shall be run just before a transaction/message involving the associated address that would be
displayed on the device.

The signature is computed on the TLV payload (minus the signature obviously).

#### Coding

_Command:_

| CLA | INS | P1                                         | P2  | LC  |
|-----|-----|--------------------------------------------|-----|-----|
| E0  | 22  | `01`: first chunk<br>`00`: following chunk | 00  | 00  |

_Input data:_

##### If P1 == first chunk

| Description                                        | Length (byte) |
|----------------------------------------------------|---------------|
| Payload length                                     | 2             |
| [TRUSTED_NAME struct](tlv_structs.md#trusted_name) | variable      |

##### If P1 == following chunk

| Description                                        | Length (byte) |
|----------------------------------------------------|---------------|
| [TRUSTED_NAME struct](tlv_structs.md#trusted_name) | variable      |

_Output data:_

None

### PROVIDE ENUM VALUE

#### Description

This command provides the app with a signed enum value, which labels a numeric value of an
operation (see [ENUM_VALUE](tlv_structs.md#enum_value)). It comes before the FIELD descriptor that
uses it, and may come before the [PROVIDE OPERATION INFO](#provide-operation-info) of the operation (or,
for a typed message, its EIP712_MESSAGE_INFO).

The value is attached to a function of a contract for a transaction (its chain, its contract and its
selector), and to a primary type of a domain for a typed message (the domain separator and the hash
of the primary type): its tags tell which. A call a typed message names is a transaction, keyed by
its function.

#### Coding

_Command:_

| CLA | INS | P1                                         | P2  | LC  |
|-----|-----|--------------------------------------------|-----|-----|
| E0  | 24  | `01`: first chunk<br>`00`: following chunk | 00  | variable |

Any other P1 or P2 is refused with `0x6A86`.

_Input data:_

##### If P1 == first chunk

| Description                                    | Length (byte) |
|------------------------------------------------|---------------|
| struct size (BE)                               | 2             |
| [ENUM_VALUE struct](tlv_structs.md#enum_value) | variable      |

##### If P1 == following chunk

| Description                                    | Length (byte) |
|------------------------------------------------|---------------|
| [ENUM_VALUE struct](tlv_structs.md#enum_value) | variable      |

_Output data:_

None

### PROVIDE OPERATION INFO

#### Description

This command provides the [OPERATION_INFO struct](tlv_structs.md#operation_info) of what is signed:
a transaction or an EIP-712 message.

##### If P2 == transaction

The struct, in its `TRANSACTION_INFO` context, describes the transaction being signed.

##### If P2 == V2 message

The struct, in its [EIP712_MESSAGE_INFO](eip712.md#eip712_message_info) context, has a domain
separator and a primary type hash instead of a chain, a contract and a selector. It describes the
message of the EIP-712 V2 flow in progress:

- it has to come after the values (EIP712 SEND STRUCT IMPLEMENTATION, V2 branch), and before
  EIP712 SIGN; otherwise `0x6980` is returned;
- there is at most one per message, a second one is refused with `0x6980`;
- it is bound to the message, and refused with `0x6A80` if its domain separator is not the one of
  the message's domain, if its primary type hash is not the one of the message's type, or if the
  domain has a `chainId` that is not a chain or a `verifyingContract` that is not a contract. A
  signature that does not verify, a missing required tag, a tag of a transaction or a version other
  than `01` are refused with the same status.

The info announces the field descriptors of the message through its `FIELDS_HASH`. The device
receives them with [PROVIDE OPERATION FIELD DESCRIPTION](#provide-operation-field-description) (`P2 = 02`),
after the info. When the `FIELDS_HASH` is the hash of no descriptors, the message is reviewed raw,
as it is when no info is sent.

#### Coding

_Command:_

| CLA | INS | P1                                         | P2                                      | LC  |
|-----|-----|--------------------------------------------|-----------------------------------------|-----|
| E0  | 26  | `01`: first chunk<br>`00`: following chunk | `00`: transaction<br>`02`: V2 message   | variable |

Any other P1 or P2 is refused with `0x6A86`.

_Input data:_

##### If P1 == first chunk

| Description                                                                | Length (byte) |
|----------------------------------------------------------------------------|---------------|
| struct size (BE)                                                           | 2             |
| [OPERATION_INFO struct](tlv_structs.md#operation_info) (P2 == transaction) | variable      |
| [OPERATION_INFO struct](tlv_structs.md#operation_info) (P2 == V2 message)  | variable      |

##### If P1 == following chunk

| Description                                                                | Length (byte) |
|----------------------------------------------------------------------------|---------------|
| [OPERATION_INFO struct](tlv_structs.md#operation_info) (P2 == transaction) | variable      |
| [OPERATION_INFO struct](tlv_structs.md#operation_info) (P2 == V2 message)  | variable      |

_Output data:_

None

### PROVIDE OPERATION FIELD DESCRIPTION

#### Description

This command provides a field description of a transaction or of an EIP-712 message.

See [FIELD struct](tlv_structs.md#field) for the full field layout and parameter types.

##### If P2 == transaction

The field describes the transaction being signed, or a call it made, after its
[PROVIDE OPERATION INFO](#provide-operation-info). The same goes for the calls a typed message names, described
as transactions: they have a `PROVIDE OPERATION INFO` (`P2 = 00`) of their own, and their fields follow
it.

##### If P2 == V2 message

The field describes the message of the EIP-712 V2 flow in progress, with the paths of the
[EIP712_FIELD_DESCRIPTOR](tlv_structs.md#eip712_field_descriptor). It is accepted when:

- the message info the message came with announced field descriptors, that is when its `FIELDS_HASH`
  is not the one of no descriptors;
- the message is complete and the review has not started: after the values and the message info,
  and before EIP712 SIGN;
- no call is being described: the fields of a call that a descriptor named are sent with `P2 = 00`,
  after the info of that call, and the ones of the message resume when it is complete.

Otherwise `0x6980` is returned. A field that cannot be applied to the message, whose path leads to
no value for example, or whose path selects more than 16 values, is refused with `0x6A80`. The
descriptors are hashed as they come: whether they are the announced ones is checked by EIP712 SIGN.

The 50-byte `Send` descriptor of the [example](eip712.md#receiving-descriptors) fits in one
command, with the size of the struct in front of it:

```text
e0 28 01 02 34                                    CLA INS P1 P2 Lc
00 32                                             struct size, 50
00 01 01 01 04 53 65 6e 64 02 01 02 03 24 00 01   FIELD
01 01 11 00 01 01 01 01 01 02 01 20 03 06 00 01
01 01 01 02 02 0c 00 01 01 01 01 05 02 01 14 04
01 01
```

#### Coding

_Command:_

| CLA | INS | P1                                         | P2                                      | LC  |
|-----|-----|--------------------------------------------|-----------------------------------------|-----|
| E0  | 28  | `01`: first chunk<br>`00`: following chunk | `00`: transaction<br>`02`: V2 message   | variable |

Any other P1 or P2 is refused with `0x6A86`.

_Input data:_

##### If P1 == first chunk

| Description                          | Length (byte) |
|--------------------------------------|---------------|
| struct size (BE)                     | 2             |
| [FIELD struct](tlv_structs.md#field) | variable      |

##### If P1 == following chunk

| Description                          | Length (byte) |
|--------------------------------------|---------------|
| [FIELD struct](tlv_structs.md#field) | variable      |

_Output data:_

None

### PROVIDE PROXY INFO

#### Description

This command provides the app with the knowledge that a contract address is a proxy to another contract on a specific
chain.
This can also (optionally) be restricted to a specific function within the contract.
Without a function, it also applies to the `verifyingContract` of an EIP-712 domain, see
[EIP712_MESSAGE_INFO](eip712.md#eip712_message_info).

#### Coding

_Command:_

| CLA | INS | P1                                         | P2  | LC  |
|-----|-----|--------------------------------------------|-----|-----|
| E0  | 2A  | `01`: first chunk<br>`00`: following chunk | 00  | 00  |

_Input data:_

##### If P1 == first chunk

| Description                                    | Length (byte) |
|------------------------------------------------|---------------|
| struct size (BE)                               | 2             |
| [PROXY_INFO struct](tlv_structs.md#proxy_info) | variable      |

##### If P1 == following chunk

| Description                                    | Length (byte) |
|------------------------------------------------|---------------|
| [PROXY_INFO struct](tlv_structs.md#proxy_info) | variable      |

_Output data:_

None

### PROVIDE NETWORK INFO

#### Description

This command handles the dynamic networks configuration, allowing to access funds without needing to update the
application for each new network.

This configuration must be sent before any access to a network and stays valid until the app exits.
Several networks can be configured. Sending a configuration for an already known chain ID replaces it.

The configuration is sent in TLV (Tag-Length-Value) mode, whereas the icon itself is send as raw bytes in dedicated
chunk(s).
The configuration doesn't include the icon itself, but only its hash (`sha256`), and it is signed.

The TLV description is available in [NETWORK_INFO struct](tlv_structs.md#network_info).

#### Coding

_Command:_

| CLA | INS | P1                                         | P2                                                | LC       |
|-----|-----|--------------------------------------------|---------------------------------------------------|----------|
| E0  | 30  | `01`: first chunk<br>`00`: following chunk | `00`: Network configuration<br>`01`: Network icon | variable |
| E0  | 30  | 00                                         | `02`: Get info                                    | 0        |

_Input data:_

##### If P2 == Network configuration

###### If P1 == first chunk

| Description    | Length (byte) |
|----------------|---------------|
| Payload length | 2             |
| TLV payload    | variable      |

###### If P1 == following chunk

| Description | Length (byte) |
|-------------|---------------|
| TLV payload | variable      |

##### If P2 == Network Icon

| Description | Length (byte) |
|-------------|---------------|
| Icon data   | variable      |

> [!NOTE]
>
> - No need to specify the full size because it is already included in the data header
> - The icon is expected as a 64x64 bitmap on Stax and Flex, 48x48 on Apex and 14x14 on Nano devices; any
>   other size is rejected.
> - The bitmap uses 1, 2 or 4 bits per pixel, raw or compressed (gzlib or RLE). Its size cannot exceed the raw
>   bitmap size, `(width × height × bpp + 7) / 8` bytes.
> - Depending on its size, it may be split on several chunks.
> - The data correspond the hex string generated by the script `<SDK_PATH>/lib_nbgl/tools/icon2glyph.py`, with parameter
>   `--hexbitmap`

_Output data:_

##### If P2 == Get Info

| Description        | Length (byte) |
|--------------------|---------------|
| Number of networks | 1             |
| Networks chain_id  | 8             |
| ...                | 8             |
| Networks chain_id  | 8             |

### PROVIDE TX SIMULATION

#### Description

This command handles the Transaction Simulation information, allowing to evaluate and verify the risk.

There are 2 sub-commands:

- One command to request the Opt-In flow, to inform the user a new parameter is available
- One command to send the payload data with the simulation information

> [!NOTE]
>
> - The Opt-In should be sent only once

The Transaction Simulation information must be sent before any transaction to be verified and confirmed.
It will be displayed in the review flow only if the corresponding setting is _Enabled_.

The information is sent in TLV (Tag-Length-Value) mode.

The TLV description is available in [TX_SIMULATION struct](tlv_structs.md#tx_simulation).

#### Coding

_Command:_

| CLA | INS | P1                          | P2                                         | LC       | Le       |
|-----|-----|-----------------------------|--------------------------------------------|----------|----------|
| E0  | 32  | `00`: TX Simulation Payload | `01`: first chunk<br>`00`: following chunk | variable | variable |
| E0  | 32  | `01`: TX Simulation Opt-In  | 00                                         | 00       | 00       |

_Input data:_

##### If P1 == TX Simulation Payload

###### If P2 == first chunk

| Description    | Length (byte) |
|----------------|---------------|
| Payload length | 2             |
| TLV payload    | variable      |

###### If P2 == following chunk

| Description | Length (byte) |
|-------------|---------------|
| TLV payload | variable      |

##### If P1 == TX Simulation Opt-In

None

_Output data:_

##### Response if P1 == TX Simulation Payload

None

##### Response if P1 == TX Simulation Opt-In

| Description                      | Length |
|----------------------------------|--------|
| Transaction Check setting status | 1      |

### SIGN EIP 7702 AUTHORIZATION

#### Description

This command computes the signature for an element of an [EIP-7702](https://eips.ethereum.org/EIPS/eip-7702)
Authorization list on the given account, for the given delegate, chain ID and nonce.

The user is prompted to confirm the operation before the signature is issued.

This command is only available in builds compiled with `EIP7702_SUPPORT=1`; other builds answer with an invalid
instruction status word.

#### Coding

_Command:_

| CLA | INS | P1                                         | P2  | LC  |
|-----|-----|--------------------------------------------|-----|-----|
| E0  | 34  | `01`: first chunk<br>`00`: following chunk | 00  | 00  |

_Input data:_

##### If P1 == first chunk

| Description                                      | Length (byte) |
|--------------------------------------------------|---------------|
| Number of BIP 32 derivations to perform (max 10) | 1             |
| First derivation index (BE)                      | 4             |
| ...                                              | 4             |
| Last derivation index (BE)                       | 4             |
| struct size (BE)                                 | 2             |
| [AUTH_7702 struct](tlv_structs.md#auth_7702)     | variable      |

##### If P1 == following chunk

| Description                                  | Length (byte) |
|----------------------------------------------|---------------|
| [AUTH_7702 struct](tlv_structs.md#auth_7702) | variable      |

_Output data:_

| Description                                              | Length |
|----------------------------------------------------------|--------|
| Signature parity (0 even, 1 odd) - use as is in EIP 7702 | 1      |
| r                                                        | 32     |
| s                                                        | 32     |

### PROVIDE SAFE ACCOUNT

#### Description

This command provides the Safe Account descriptors.

The descriptors are then presented to the user to validate the Transaction Signers.

There are 2 sub-commands:

- One command to send the Safe descriptor
- One command to send the Signers descriptors

> [!NOTE]
> The `Safe descriptor` should be sent only once, and **before** the Signers descriptors

The information is sent in TLV (Tag-Length-Value) mode.

#### Coding

_Command:_

| CLA | INS | P1                                         | P2                                               | Lc       | Le       |
|-----|-----|--------------------------------------------|--------------------------------------------------|----------|----------|
| E0  | 36  | `01`: first chunk<br>`00`: following chunk | `00`: Safe descriptor<br>`01`: Signer descriptor | variable | variable |

_Input data:_

##### If P2 == Safe Account info descriptor

###### If P1 == first chunk

| Description                                              | Length (byte) |
|----------------------------------------------------------|---------------|
| Payload length                                           | 2             |
| [SAFE_DESCRIPTOR struct](tlv_structs.md#safe_descriptor) | variable      |

###### If P1 == following chunk

| Description                                              | Length (byte) |
|----------------------------------------------------------|---------------|
| [SAFE_DESCRIPTOR struct](tlv_structs.md#safe_descriptor) | variable      |

##### If P2 == Signer info descriptor

###### If P1 == first chunk

| Description                                                  | Length (byte) |
|--------------------------------------------------------------|---------------|
| Payload length                                               | 2             |
| [SIGNER_DESCRIPTOR struct](tlv_structs.md#signer_descriptor) | variable      |

###### If P1 == following chunk

| Description                                                  | Length (byte) |
|--------------------------------------------------------------|---------------|
| [SIGNER_DESCRIPTOR struct](tlv_structs.md#signer_descriptor) | variable      |

_Output data:_

None

### PROVIDE GATING

#### Description

This command provides the Gating descriptor.

The descriptor is then presented to the user before the Transaction.

The information is sent in TLV (Tag-Length-Value) mode.

#### Coding

_Command:_

| CLA | INS | P1                                         | P2  | Lc       | Le       |
|-----|-----|--------------------------------------------|-----|----------|----------|
| E0  | 38  | `01`: first chunk<br>`00`: following chunk | 00  | variable | variable |

_Input data:_

##### If P1 == first chunk

| Description                                                  | Length (byte) |
|--------------------------------------------------------------|---------------|
| Payload length                                               | 2             |
| [GATING_DESCRIPTOR struct](tlv_structs.md#gating_descriptor) | variable      |

##### If P1 == following chunk

| Description                                                  | Length (byte) |
|--------------------------------------------------------------|---------------|
| [GATING_DESCRIPTOR struct](tlv_structs.md#gating_descriptor) | variable      |

_Output data:_

None

### PROVIDE MAP ENTRY

#### Description

This command provides the app with a key/value entry for a specific function of a contract on a chain (or primary type of
a domain). The entry is signed by CAL and stored on the device until a new signing session starts. When the Generic
Transaction Parser encounters a `MAP_REF` parameter, the device looks up the stored entries and resolves the display
value from the matching `MAP_ENTRY`.

The entry comes before the FIELD descriptor that uses it, and may come before the
[PROVIDE OPERATION INFO](#provide-operation-info) of the operation (or, for a typed message, its EIP712_MESSAGE_INFO). It is
attached to a function of a contract for a transaction, and to a primary type of a domain for a typed message: its tags
tell which. A call a typed message names is a transaction, keyed by its function.

The descriptor is sent in TLV (Tag-Length-Value) mode.

#### Coding

_Command:_

| CLA | INS | P1                                         | P2  | Lc       |
|-----|-----|--------------------------------------------|-----|----------|
| E0  | 3A  | `01`: first chunk<br>`00`: following chunk | 00  | variable |

Any other P1 or P2 is refused with `0x6A86`.

_Input data:_

##### If P1 == first chunk

| Description                                  | Length (byte) |
|----------------------------------------------|---------------|
| Payload length                               | 2             |
| [MAP_ENTRY struct](tlv_structs.md#map_entry) | variable      |

##### If P1 == following chunk

| Description                                  | Length (byte) |
|----------------------------------------------|---------------|
| [MAP_ENTRY struct](tlv_structs.md#map_entry) | variable      |

_Output data:_

None
