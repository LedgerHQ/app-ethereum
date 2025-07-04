# TLV structures

## TRANSACTION_INFO

| Name               | Tag  | Payload type | Description                                          | Optional | Source / value                                             |
|--------------------|------|--------------|------------------------------------------------------|----------|------------------------------------------------------------|
| VERSION            | 0x00 | uint8        | struct version                                       |          | constant: `0x0`                                            |
| CHAIN_ID           | 0x01 | uint64       | EIP-155 chain ID                                     |          | `$.context.contract.deployments.[<deployment id>].chainId` |
| CONTRACT_ADDR      | 0x02 | uint8[20]    | EVM contract address                                 |          | `$.context.contract.deployments.[<deployment id>].address` |
| SELECTOR           | 0x03 | uint8[4]     | selector (4 bytes form)                              |          | `$.display.formats.<format id>`                            |
| FIELDS_HASH        | 0x04 | uint8[32]    | SHA3-256 hash of all the FIELD structs               |          | computed by CAL                                            |
| OPERATION_TYPE     | 0x05 | char[]       | Will be appended to "Review ..." in the first screen |          | `$.display.formats.<selector>.intent`                      |
| CREATOR_NAME       | 0x06 | char[]       |                                                      | x        | `$.metadata.owner`                                         |
| CREATOR_LEGAL_NAME | 0x07 | char[]       |                                                      | x        | `$.metadata.info.legalName`                                |
| CREATOR_URL        | 0x08 | char[]       | website of the dApp or company behind it             | x        | `$.metadata.info.url`                                      |
| CONTRACT_NAME      | 0x09 | char[]       |                                                      | x        | `$.metadata.info.$id`                                      |
| DEPLOY_DATE        | 0x0a | uint32       | unix epoch, shown as YYYY-MM-DD                      | x        | `$.metadata.info.lastUpdate`                               |
| SIGNATURE          | 0xff | uint8[]      | signature of all the other struct fields             |          | computed by CAL                                            |

> [!CAUTION]
>
> - `$.metadata.owner` is optional, made `CREATOR_NAME` optional
> - `$.metadata.info.legalName` is optional, made `CREATOR_LEGAL_NAME` optional
> - `$.display.formats.<selector>.intent` is optional, possible fallbacks: `$.display.formats.<selector>.$id`, `$.display.formats.<selector>`
> - `CONTRACT_NAME` is not really materialized in the spec, closest is `$.metadata.info.$id`, but `$id` is supposed to be internal
> - `$.metadata.info.lastUpdate` is optional, made `DEPLOY_DATE` optional

## ENUM_VALUE

| Name          | Tag  | Payload type   | Description                                                              | Optional | Source / value                                             |
|---------------|------|----------------|--------------------------------------------------------------------------|----------|------------------------------------------------------------|
| VERSION       | 0x00 | uint8          | struct version                                                           |          | constant: `0x0`                                            |
| CHAIN_ID      | 0x01 | uint64         | EIP-155 chain ID                                                         |          | `$.context.contract.deployments.[<deployment id>].chainId` |
| CONTRACT_ADDR | 0x02 | uint8[20]      | EVM contract address                                                     |          | `$.context.contract.deployments.[<deployment id>].address` |
| SELECTOR      | 0x03 | uint8[4]       | function selector                                                        |          |                                                            |
| ID            | 0x04 | uint8          | identifier of the enum (to differentiate multiple enums in one contract) |          |                                                            |
| VALUE         | 0x05 | uint8          | enum entry integer value                                                 |          | `$.metadata.enums.<enum id>.<enum entry>.value`            |
| NAME          | 0x06 | char[]         | enum entry name (ASCII)                                                  |          | `$.metadata.enums.<enum id>.<enum entry>.name`             |
| SIGNATURE     | 0xff | uint8[]        | signature of all the other struct fields                                 |          | computed by CAL                                            |

> [!CAUTION]
>
> - `NAME` max length ?

## FIELD

It contains no signature since the signed TRANSACTION_INFO struct already has a hash of all the FIELD
structs, which attests of the authenticity, order and completeness of all FIELD structs.

| Name       | Tag  | Payload type | Description                | Optional | Source / value                                             |
|------------|------|--------------|----------------------------|----------|------------------------------------------------------------|
| VERSION    | 0x00 | uint8        | struct version             |          | constant: `0x0`                                            |
| NAME       | 0x01 | char[]       | field display name (ASCII) |          | `$.display.formats.<format id>.fields.[<field id>].label`  |
| PARAM_TYPE | 0x02 | uint8        | `ParamType`                |          | `$.display.formats.<format id>.fields.[<field id>].params` |
| PARAM      | 0x03 | PARAM_*      |                            |          | `$.display.formats.<format id>.fields.[<field id>].params` |

with `ParamType` enum defined as:

| Name         | Value |
|--------------|-------|
| RAW          | 0x00  |
| AMOUNT       | 0x01  |
| TOKEN_AMOUNT | 0x02  |
| NFT          | 0x03  |
| DATETIME     | 0x04  |
| DURATION     | 0x05  |
| UNIT         | 0x06  |
| ENUM         | 0x07  |
| TRUSTED_NAME | 0x08  |
| CALLDATA     | 0x09  |
| TOKEN        | 0x0a  |

> [!CAUTION]
>
> - `NAME` max length ?

### PARAM_RAW

| Name    | Tag  | Payload type | Description                   | Optional | Source / value                                           |
|---------|------|--------------|-------------------------------|----------|----------------------------------------------------------|
| VERSION | 0x00 | uint8        | struct version                |          | constant: `0x0`                                          |
| VALUE   | 0x01 | VALUE        | reference to value to display |          | `$.display.formats.<format id>.fields.[<field id>].path` |

> [!CAUTION]
>
> - not possible to provide static value ?

### PARAM_AMOUNT

| Name    | Tag  | Payload type | Description                   | Optional | Source / value                                           |
|---------|------|--------------|-------------------------------|----------|----------------------------------------------------------|
| VERSION | 0x00 | uint8        | struct version                |          | constant: `0x0`                                          |
| VALUE   | 0x01 | VALUE        | reference to value to display |          | `$.display.formats.<format id>.fields.[<field id>].path` |

### PARAM_TOKEN_AMOUNT

| Name                | Tag  | Payload type | Description                               | Optional | Source / value                                                                   |
|---------------------|------|--------------|-------------------------------------------|----------|----------------------------------------------------------------------------------|
| VERSION             | 0x00 | uint8        | struct version                            |          | constant: `0x0`                                                                  |
| VALUE               | 0x01 | VALUE        | reference to value to display             |          | `$.display.formats.<format id>.fields.[<field id>].path`                         |
| TOKEN               | 0x02 | VALUE        | reference to token address                | x        | `$.display.formats.<format id>.fields.[<field id>].params.tokenPath`             |
| NATIVE_CURRENCY     | 0x03 | uint8[20]    | address to interpret as native currency   | x        | `$.display.formats.<format id>.fields.[<field id>].params.nativeCurrencyAddress` |
| THRESHOLD           | 0x04 | uint256      | unlimited amount threshold                | x        | `$.display.formats.<format id>.fields.[<field id>].params.threshold`             |
| ABOVE_THRESHOLD_MSG | 0x05 | char[]       | unlimited amount label                    | x        | `$.display.formats.<format id>.fields.[<field id>].params.message`               |

This struct can contain `NATIVE_CURRENCY` multiple times for multiple addresses.

> [!CAUTION]
>
> - `NATIVE_CURRENCY` max count ?
> - `ABOVE_THRESHOLD_MSG` max length ?

### PARAM_NFT

| Name       | Tag  | Payload type | Description                         | Optional | Source / value                                                            |
|------------|------|--------------|-------------------------------------|----------|---------------------------------------------------------------------------|
| VERSION    | 0x00 | uint8        | struct version                      |          | constant: `0x0`                                                           |
| VALUE      | 0x01 | VALUE        | reference to value to display       |          | `$.display.formats.<format id>.fields.[<field id>].path`                  |
| COLLECTION | 0x02 | VALUE        | reference to the collection address |          | `$.display.formats.<format id>.fields.[<field id>].params.collectionPath` |

### PARAM_DATETIME

| Name    | Tag  | Payload type | Description                         | Optional | Source / value                                                      |
|---------|------|--------------|-------------------------------------|----------|---------------------------------------------------------------------|
| VERSION | 0x00 | uint8        | struct version                      |          | constant: `0x0`                                                     |
| VALUE   | 0x01 | VALUE        | reference to value to display       |          | `$.display.formats.<format id>.fields.[<field id>].path`            |
| TYPE    | 0x02 | uint8        | 0 for unix time, 1 for block height |          | `$.display.formats.<format id>.fields.[<field id>].params.encoding` |

### PARAM_DURATION

| Name    | Tag  | Payload type | Description                   | Optional | Source / value                                           |
|---------|------|--------------|-------------------------------|----------|----------------------------------------------------------|
| VERSION | 0x00 | uint8        | struct version                |          | constant: `0x0`                                          |
| VALUE   | 0x01 | VALUE        | reference to value to display |          | `$.display.formats.<format id>.fields.[<field id>].path` |

### PARAM_UNIT

| Name     | Tag  | Payload type | Description                   | Optional | Source / value                                                      |
|----------|------|--------------|-------------------------------|----------|---------------------------------------------------------------------|
| VERSION  | 0x00 | uint8        | struct version                |          | constant: `0x0`                                                     |
| VALUE    | 0x01 | VALUE        | reference to value to display |          | `$.display.formats.<format id>.fields.[<field id>].path`            |
| BASE     | 0x02 | char[]       |                               |          | `$.display.formats.<format id>.fields.[<field id>].params.base`     |
| DECIMALS | 0x03 | uint8        | defaults to 0                 | x        | `$.display.formats.<format id>.fields.[<field id>].params.decimals` |
| PREFIX   | 0x04 | bool         | defaults to false             | x        | `$.display.formats.<format id>.fields.[<field id>].params.prefix`   |

> [!CAUTION]
>
> - `BASE` max length ?

### PARAM_ENUM

| Name    | Tag  | Payload type | Description                   | Optional | Source / value                                           |
|---------|------|--------------|-------------------------------|----------|----------------------------------------------------------|
| VERSION | 0x00 | uint8        | struct version                |          | constant: `0x0`                                          |
| ID      | 0x01 | uint8        |                               |          |                                                          |
| VALUE   | 0x02 | VALUE        | reference to value to display |          | `$.display.formats.<format id>.fields.[<field id>].path` |

> [!CAUTION]
>
> - needs reference to enum ?

### PARAM_TRUSTED_NAME

| Name           | Tag  | Payload type        | Description                                | Optional | Source / value                                                           |
|----------------|------|---------------------|--------------------------------------------|----------|--------------------------------------------------------------------------|
| VERSION        | 0x00 | uint8               | struct version                             |          | constant: `0x0`                                                          |
| VALUE          | 0x01 | VALUE               | reference to value to display              |          | `$.display.formats.<format id>.fields.[<field id>].path`                 |
| TYPES          | 0x02 | TrustedNameType[]   | allowed types for types for trusted name   |          | `$.display.formats.<format id>.fields.[<field id>].params.types`         |
| SOURCES        | 0x03 | TrustedNameSource[] | allowed sources for types for trusted name |          | `$.display.formats.<format id>.fields.[<field id>].params.sources`       |
| SENDER_ADDRESS | 0x04 | uint8[20]           | address to interpret as the sender         | x        | `$.display.formats.<format id>.fields.[<field id>].params.senderAddress` |

This struct can contain `SENDER_ADDRESS` multiple times for multiple addresses.

with `TrustedNameType` enum defined as:

| Name            | Value | Description                                                                                        |
|-----------------|-------|----------------------------------------------------------------------------------------------------|
| EOA             | 0x01  | Address is an Externally Owned Account.                                                            |
| SMART_CONTRACT  | 0x02  | Address is smart contract.                                                                         |
| COLLECTION      | 0x03  | Address is a well known NFT collection.                                                            |
| TOKEN           | 0x04  | Address is a token contract.                                                                       |
| WALLET          | 0x05  | Address is owned by the wallet.                                                                    |
| CONTEXT_ADDRESS | 0x06  | Alias address bound to a specific execution context (e.g SPL address, contract specific address…). |

and `TrustedNameSource` enum defined as:

| Name               | Value | Description        |
|--------------------|-------|--------------------|
| LOCAL_ADDRESS_BOOK | 0x00  | Local address book |
| CRYPTO_ASSET_LIST  | 0x01  | CAL                |
| ENS                | 0x02  | ENS                |
| UNSTOPPABLE_DOMAIN | 0x03  | Unstoppable Domain |
| FREENAME           | 0x04  | Freename           |
| DNS                | 0x05  | DNS                |
| DYNAMIC_RESOLVER   | 0x06  | Dynamic Resolver   |

> [!CAUTION]
>
> - `SOURCES` array max length ?

### PARAM_TOKEN

| Name            | Tag  | Payload type | Description                             | Optional | Source / value                                                                   |
|-----------------|------|--------------|-----------------------------------------|----------|----------------------------------------------------------------------------------|
| VERSION         | 0x00 | uint8        | struct version                          |          | constant: `0x0`                                                                  |
| ADDRESS         | 0x01 | VALUE        | reference to value to display           |          | `$.display.formats.<format id>.fields.[<field id>].path`                         |
| NATIVE_CURRENCY | 0x02 | uint8[20]    | address to interpret as native currency | x        | `$.display.formats.<format id>.fields.[<field id>].params.nativeCurrencyAddress` |

This struct can contain `NATIVE_CURRENCY` multiple times for multiple addresses.

### VALUE

| Name           | Tag  | Payload type    | Description                             | Optional | Source / value                                            |
|----------------|------|-----------------|-----------------------------------------|----------|-----------------------------------------------------------|
| VERSION        | 0x00 | uint8           | struct version                          |          | constant: `0x0`                                           |
| TYPE_FAMILY    | 0x01 | `TypeFamily`    |                                         |          |                                                           |
| TYPE_SIZE      | 0x02 | uint8           | size of values (in bytes)               | x        |                                                           |
| DATA_PATH      | 0x03 | DATA_PATH       | path to value in serialized transaction | x        | `$.display.formats.<format id>.fields.[<field id>].path`  |
| CONTAINER_PATH | 0x04 | `ContainerPath` | container value enum                    | x        | `$.display.formats.<format id>.fields.[<field id>].path`  |
| CONSTANT       | 0x05 | uint8[]         | literal value                           | x        | `$.display.formats.<format id>.fields.[<field id>].value` |

with `TypeFamily` enum defined as:

| Name    | Value |
|---------|-------|
| UINT    | 0x01  |
| INT     | 0x02  |
| UFIXED  | 0x03  |
| FIXED   | 0x04  |
| ADDRESS | 0x05  |
| BOOL    | 0x06  |
| BYTES   | 0x07  |
| STRING  | 0x08  |

and `ContainerPath` enum defined as:

| Name  | Value |
|-------|-------|
| FROM  | 0x00  |
| TO    | 0x01  |
| VALUE | 0x02  |

The TLV payload must include exactly one of `DATA_PATH`, `CONTAINER_PATH` or `CONSTANT`.

with `TypeFamily` enum defined as:

| Name    | Value |
|---------|-------|
| UINT    | 0x01  |
| INT     | 0x02  |
| UFIXED  | 0x03  |
| FIXED   | 0x04  |
| ADDRESS | 0x05  |
| BOOL    | 0x06  |
| BYTES   | 0x07  |
| STRING  | 0x08  |

> [!CAUTION]
>
> - `PATH` max allowed length ?

### DATA_PATH

| Name    | Tag  | Payload type    | Description                                                                                                                               | Optional | Source / value  |
|---------|------|-----------------|-------------------------------------------------------------------------------------------------------------------------------------------|----------|-----------------|
| VERSION | 0x00 | uint8           | struct version                                                                                                                            |          | constant: `0x0` |
| TUPLE   | 0x01 | uint16          | move by {value} slots from current slot                                                                                                   | x        |                 |
| ARRAY   | 0x02 | ARRAY_ELEMENT   | current slot is array length, added to offset if negative. multiple by item_size and move by result slots. payload unset => iterate array | x        |                 |
| REF     | 0x03 |                 | read value of current slot. apply read value as offset from current slot                                                                  | x        |                 |
| LEAF    | 0x04 | `PathLeafType`  | current slot is a leaf type, specifying the type of path end                                                                              | x        |                 |
| SLICE   | 0x05 | SLICE_ELEMENT   | specify slicing to apply to final leaf value as (start, end)                                                                              | x        |                 |

with `PathLeafType` enum defined as:

| Name         | Value | Description                                                       |
|--------------|-------|-------------------------------------------------------------------|
| ARRAY_LEAF   | 0x01  | final offset is start of array encoding                           |
| TUPLE_LEAF   | 0x02  | final offset is start of tuple encoding                           |
| STATIC_LEAF  | 0x03  | final offset contains static encoded value (typ data on 32 bytes) |
| DYNAMIC_LEAF | 0x04  | final offset contains dynamic encoded value (typ length + data)   |

The payload must contain exactly one of `TUPLE`, `ARRAY`, `REF`, `LEAF` or `SLICE`.

In version 1 of the protocol:

- `ARRAY_LEAF` and `TUPLE_LEAF` are forbidden
- `ARRAY` with no payload means the same format should be applied to each array element. It can be used several
   times in a single path, in which case the application will recurse into sub-arrays (depth first)
- `LEAF` can only be used in last position of the path, expect if followed by a slice
- `SLICE` can only be used when all these conditions are met:
  - in last position of the path
  - previous element is `ARRAY_LEAF` or `DYNAMIC_LEAF` with `TYPE_FAMILY` = `BYTES` or `STRING`

> [!CAUTION]
>
> - What about [non-standard packed mode](https://docs.soliditylang.org/en/latest/abi-spec.html#non-standard-packed-mode) ?

### ARRAY_ELEMENT

| Name    | Tag  | Payload type    | Description                          | Optional | Source / value  |
|---------|------|-----------------|--------------------------------------|----------|-----------------|
| WEIGHT  | 0x01 | uint8           | size of each array element in chunks |          |                 |
| START   | 0x02 | int16           | start index (inclusive)              | x        |                 |
| END     | 0x03 | int16           | end index (exclusive)                | x        |                 |

### SLICE_ELEMENT

| Name    | Tag  | Payload type    | Description              | Optional | Source / value  |
|---------|------|-----------------|--------------------------|----------|-----------------|
| START   | 0x01 | int16           | start index (inclusive)  | x        |                 |
| END     | 0x02 | int16           | end index (exclusive)    | x        |                 |

## PROXY_INFO

| Name            | Tag  | Payload type     | Description                     | Optional |
|-----------------|------|------------------|---------------------------------|----------|
| STRUCT_TYPE     | 0x01 | uint8            | structure type                  |          |
| STRUCT_VERSION  | 0x02 | uint8            | structure version               |          |
| CHALLENGE       | 0x12 | uint32           | challenge to ensure freshness   |          |
| ADDRESS         | 0x22 | uint8[20]        | proxy contract address          |          |
| CHAIN_ID        | 0x23 | uint64           | EVM chain identifier            |          |
| SELECTOR        | 0x41 | uint[4]          | function selector               | x        |
| IMPL_ADDRESS    | 0x42 | uint8[20]        | implementation contract address |          |
| DELEGATION_TYPE | 0x43 | `DelegationType` | type of delegation              |          |
| SIGNATURE       | 0x15 | uint8[]          | signature of the structure      |          |

with `DelegationType` enum defined as :

| Name                 | Value |
|----------------------|-------|
| PROXY                | 0x01  |
| ISSUED_FROM_FACTORY  | 0x02  |
| DELEGATOR            | 0x03  |

## AUTH_7702

| Name           | Tag  | Payload type    | Description                     | Optional |
|----------------|------|-----------------|---------------------------------|----------|
| STRUCT_VERSION | 0x00 | uint8           | structure version (currently 1) |          |
| DELEGATE_ADDR  | 0x01 | uint8[20]       | delegate address                |          |
| CHAIN_ID       | 0x02 | uint64          | chain ID (0 for no restriction) |          |
| NONCE          | 0x03 | uint64          | nonce                           |          |

## NETWORK_INFO

| Name              | Tag  | Payload type | Description                   | Value                           |
|-------------------|------|--------------|-------------------------------|---------------------------------|
| STRUCTURE_TYPE    | 0x01 | uint8        | Structure type                | `0x08` (`TYPE_DYNAMIC_NETWORK`) |
| STRUCTURE_VERSION | 0x02 | uint8        | Structure version             | `0x01`                          |
| BLOCKCHAIN_FAMILY | 0x51 | uint8        | Family                        | `0x01` (`Ethereum`)             |
| CHAIN_ID          | 0x23 | uint64       | Network chain ID              |                                 |
| NETWORK_NAME      | 0x52 | char[31]     | Network name (without '\0')   |                                 |
| NETWORK_TICKER    | 0x24 | char[10]     | Network ticker (without '\0') |                                 |
| NETWORK_ICON_HASH | 0x53 | uint8[32]    | _sha256_ of the network icon  |                                 |
| SIGNATURE         | 0x15 | uint8[]      | Signature of the structure    |                                 |

The signature is computed on the full payload data, using `CX_CURVE_SECP256K1`.

## TX_SIMULATION

| Name                          | Tag  | Payload type | Description                              | Value                           |
|-------------------------------|------|--------------|------------------------------------------|---------------------------------|
| STRUCTURE_TYPE                | 0x01 | uint8        | Structure type                           | `0x09` (`TYPE_TX_SIMULATION`)   |
| STRUCTURE_VERSION             | 0x02 | uint8        | Structure version                        | `0x01`                          |
| ADDRESS                       | 0x22 | uint8[20]    | Ethereum `From` Address                  |                                 |
| CHAIN_ID                      | 0x23 | uint64       | Transaction chain ID                     |                                 |
| TX_HASH                       | 0x27 | uint8[32]    | Hash of the Tx that was simulated        |                                 |
| DOMAIN_HASH                   | 0x28 | uint8[32]    | _Domain Hash_ for EIP712                 |                                 |
| TX_CHECKS_NORMALIZED_RISK     | 0x80 | uint8        | Normalized risk score of the transaction |                                 |
| TX_CHECKS_NORMALIZED_CATEGORY | 0x81 | uint8        | Main category explaining the risk score  |                                 |
| TX_CHECKS_PROVIDER_MSG        | 0x82 | char[30]     | Provider specific message                |                                 |
| TX_CHECKS_TINY_URL            | 0x83 | char[30]     | URL to access the full report            |                                 |
| TX_CHECKS_SIMULATION_TYPE     | 0x84 | uint8        | Type of simulation                       |                                 |
| SIGNATURE                     | 0x15 | uint8[]      | Signature of the structure               |                                 |

The signature is computed on the full payload data, using `CX_CURVE_SECP256K1`.

The _Risk Score_ is normalized and interpreted like this:

- `0`: Benign
- `1`: Warning
- `2`: Malicious

The _Main Category_ is normalized and interpreted like this:

- `1`: Others
- `2`: Address
- `3`: dApp
- `4`: Losing Operation

The _Simulation Type_ is normalized and interpreted like this:

- `0`: Transaction
- `1`: Typed Data (EIP-712)
- `2`: Personal Message (EIP-191)
