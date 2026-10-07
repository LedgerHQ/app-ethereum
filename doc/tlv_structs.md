# TLV structures

## TRUSTED_NAME

| Name             | Tag  | Payload type      | Description                     | Optional |
|------------------|------|-------------------|---------------------------------|----------|
| STRUCT_TYPE      | 0x01 | uint8             | structure type (0x03)           |          |
| STRUCT_VERSION   | 0x02 | uint8             | structure version (currently 2) |          |
| NOT_VALID_AFTER  | 0x10 | uint8[3]          | app version (major,minor,patch) | x        |
| CHALLENGE        | 0x12 | uint32            |                                 | x        |
| SIG_KEY_ID       | 0x13 | uint8             |                                 | x        |
| SIG_ALGO         | 0x14 | uint8             |                                 | x        |
| SIGNATURE        | 0x15 | uint8[]           |                                 | x        |
| NAME             | 0x20 | char[]            | what to substitute with         |          |
| COIN_TYPE        | 0x21 | uint8             | as defined in SLIP-44           | x        |
| ADDRESS          | 0x22 | uint8[20]         | address to substitute           |          |
| CHAIN_ID         | 0x23 | uint64            |                                 | x        |
| TYPE             | 0x70 | uint8             |                                 | x        |
| SOURCE           | 0x71 | uint8             |                                 | x        |
| NFT_ID           | 0x72 | uint256           |                                 | x        |
| OWNER            | 0x74 | uint8[20]         |                                 | x        |
| OWNER_DERIV_PATH | 0x75 | (uint8, uint32[]) |                                 | x        |

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
> - `CONTRACT_NAME` is not really materialized in the spec, closest is `$.metadata.info.$id`,
     but `$id` is supposed to be internal
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

## FIELD

It contains no signature since the signed TRANSACTION_INFO struct already has a hash of all the FIELD
structs, which attests of the authenticity, order and completeness of all FIELD structs.

| Name       | Tag  | Payload type | Description                         | Optional | Source / value                                                |
|------------|------|--------------|-------------------------------------|----------|---------------------------------------------------------------|
| VERSION    | 0x00 | uint8        | struct version                      |          | constant: `0x0`                                               |
| NAME       | 0x01 | char[]       | field display name (ASCII)          |          | `$.display.formats.<format id>.fields.[<field id>].label`     |
| PARAM_TYPE | 0x02 | [ParamType](#paramtype-enum) |                                     |          | `$.display.formats.<format id>.fields.[<field id>].params`    |
| PARAM      | 0x03 | [PARAM_RAW](#param_raw) \| [PARAM_AMOUNT](#param_amount) \| [PARAM_TOKEN_AMOUNT](#param_token_amount) \| [PARAM_NFT](#param_nft) \| [PARAM_DATETIME](#param_datetime) \| [PARAM_DURATION](#param_duration) \| [PARAM_UNIT](#param_unit) \| [PARAM_ENUM](#param_enum) \| [PARAM_TRUSTED_NAME](#param_trusted_name) \| [PARAM_CALLDATA](#param_calldata) \| [PARAM_TOKEN](#param_token) \| [PARAM_NETWORK](#param_network) \| [PARAM_GROUP](#param_group) |                                     |          | `$.display.formats.<format id>.fields.[<field id>].params`    |
| VISIBLE    | 0x04 | [VisibleType](#visibletype-enum) | visibility condition  | x        | `$.display.formats.<format id>.fields.[<field id>].visible`   |
| CONSTRAINT | 0x05 | uint8[]      | constraint value (raw bytes)        | x        | `$.display.formats.<format id>.fields.[<field id>].visible`   |
| SEPARATOR  | 0x06 | char[]       | separator for array iteration       | x        | `$.display.formats.<format id>.fields.[<field id>].separator` |

> __Notes__:
>
> - `VISIBLE` defaults to `ALWAYS` (0x00) if not present
> - `VISIBLE` can be present only once and should be served before any `CONSTRAINT`
> - `CONSTRAINT` is only present when `VISIBLE` is `MUST_BE` or `IF_NOT_IN`
> - `CONSTRAINT` tag can appear multiple times for multiple allowed/excluded values (OR semantics). The limit is 5 constraints.
> - `SEPARATOR` is optional and only meaningful for array-typed fields;
>   `{index}` in the string is replaced with the 1-based element index at display time
>   (e.g. `"Token {index}"` → `"Token 1"`, `"Token 2"`, ...)

### ParamType enum

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
| NETWORK      | 0x0b  |
| GROUP        | 0x0c  |

### VisibleType enum

| Name      | Value | Description                                                                                |
|-----------|-------|--------------------------------------------------------------------------------------------|
| ALWAYS    | 0x00  | Field is always displayed (default)                                                        |
| MUST_BE   | 0x01  | Field not displayed but must match one of the constraint values, otherwise tx is rejected  |
| IF_NOT_IN | 0x02  | Field is displayed only if value is NOT in the constraint list                             |

### PARAM_RAW

| Name    | Tag  | Payload type | Description                   | Optional | Source / value                                           |
|---------|------|--------------|-------------------------------|----------|----------------------------------------------------------|
| VERSION | 0x00 | uint8        | struct version                |          | constant: `0x0`                                          |
| VALUE   | 0x01 | [VALUE](#value)        | reference to value to display |          | `$.display.formats.<format id>.fields.[<field id>].path` |

### PARAM_AMOUNT

| Name    | Tag  | Payload type | Description                   | Optional | Source / value                                           |
|---------|------|--------------|-------------------------------|----------|----------------------------------------------------------|
| VERSION | 0x00 | uint8        | struct version                |          | constant: `0x0`                                          |
| VALUE   | 0x01 | [VALUE](#value)        | reference to value to display |          | `$.display.formats.<format id>.fields.[<field id>].path` |

### PARAM_TOKEN_AMOUNT

| Name                | Tag  | Payload type | Description                               | Optional | Source / value                                                                   |
|---------------------|------|--------------|-------------------------------------------|----------|----------------------------------------------------------------------------------|
| VERSION             | 0x00 | uint8        | struct version                            |          | constant: `0x0`                                                                  |
| VALUE               | 0x01 | [VALUE](#value)        | reference to value to display             |          | `$.display.formats.<format id>.fields.[<field id>].path`                         |
| TOKEN               | 0x02 | [VALUE](#value)        | reference to token address                | x        | `$.display.formats.<format id>.fields.[<field id>].params.tokenPath`             |
| NATIVE_CURRENCY     | 0x03 | uint8[20]    | address to interpret as native currency   | x        | `$.display.formats.<format id>.fields.[<field id>].params.nativeCurrencyAddress` |
| THRESHOLD           | 0x04 | uint256      | unlimited amount threshold                | x        | `$.display.formats.<format id>.fields.[<field id>].params.threshold`             |
| ABOVE_THRESHOLD_MSG | 0x05 | char[]       | unlimited amount label                    | x        | `$.display.formats.<format id>.fields.[<field id>].params.message`               |

This struct can contain `NATIVE_CURRENCY` multiple times for multiple addresses.

> __Notes__:
>
> - When `VALUE` and `TOKEN` reference arrays of different lengths, iteration is still allowed if
>   `TOKEN` has exactly one element — that single token address is then broadcast (repeated) for
>   every element of `VALUE`. Arrays whose sizes differ and neither is 1 are rejected.

### PARAM_NFT

| Name       | Tag  | Payload type | Description                         | Optional | Source / value                                                            |
|------------|------|--------------|-------------------------------------|----------|---------------------------------------------------------------------------|
| VERSION    | 0x00 | uint8        | struct version                      |          | constant: `0x0`                                                           |
| VALUE      | 0x01 | [VALUE](#value)        | reference to value to display       |          | `$.display.formats.<format id>.fields.[<field id>].path`                  |
| COLLECTION | 0x02 | [VALUE](#value)        | reference to the collection address |          | `$.display.formats.<format id>.fields.[<field id>].params.collectionPath` |

### PARAM_DATETIME

| Name    | Tag  | Payload type | Description                         | Optional | Source / value                                                      |
|---------|------|--------------|-------------------------------------|----------|---------------------------------------------------------------------|
| VERSION | 0x00 | uint8        | struct version                      |          | constant: `0x0`                                                     |
| VALUE   | 0x01 | [VALUE](#value)        | reference to value to display       |          | `$.display.formats.<format id>.fields.[<field id>].path`            |
| TYPE    | 0x02 | uint8        | 0 for unix time, 1 for block height |          | `$.display.formats.<format id>.fields.[<field id>].params.encoding` |

### PARAM_DURATION

| Name    | Tag  | Payload type | Description                   | Optional | Source / value                                           |
|---------|------|--------------|-------------------------------|----------|----------------------------------------------------------|
| VERSION | 0x00 | uint8        | struct version                |          | constant: `0x0`                                          |
| VALUE   | 0x01 | [VALUE](#value)        | reference to value to display |          | `$.display.formats.<format id>.fields.[<field id>].path` |

### PARAM_UNIT

| Name     | Tag  | Payload type | Description                   | Optional | Source / value                                                      |
|----------|------|--------------|-------------------------------|----------|---------------------------------------------------------------------|
| VERSION  | 0x00 | uint8        | struct version                |          | constant: `0x0`                                                     |
| VALUE    | 0x01 | [VALUE](#value)        | reference to value to display |          | `$.display.formats.<format id>.fields.[<field id>].path`            |
| BASE     | 0x02 | char[]       |                               |          | `$.display.formats.<format id>.fields.[<field id>].params.base`     |
| DECIMALS | 0x03 | uint8        | defaults to 0                 | x        | `$.display.formats.<format id>.fields.[<field id>].params.decimals` |
| PREFIX   | 0x04 | bool         | defaults to false             | x        | `$.display.formats.<format id>.fields.[<field id>].params.prefix`   |

### PARAM_ENUM

| Name    | Tag  | Payload type | Description                   | Optional | Source / value                                           |
|---------|------|--------------|-------------------------------|----------|----------------------------------------------------------|
| VERSION | 0x00 | uint8        | struct version                |          | constant: `0x0`                                          |
| ID      | 0x01 | uint8        |                               |          |                                                          |
| VALUE   | 0x02 | [VALUE](#value)        | reference to value to display |          | `$.display.formats.<format id>.fields.[<field id>].path` |

### PARAM_TRUSTED_NAME

| Name           | Tag  | Payload type        | Description                                | Optional | Source / value                                                           |
|----------------|------|---------------------|--------------------------------------------|----------|--------------------------------------------------------------------------|
| VERSION        | 0x00 | uint8               | struct version                             |          | constant: `0x0`                                                          |
| VALUE          | 0x01 | [VALUE](#value)               | reference to value to display              |          | `$.display.formats.<format id>.fields.[<field id>].path`                 |
| TYPES          | 0x02 | [TrustedNameType](#trustednametype-enum)[]   | allowed types for types for trusted name   |          | `$.display.formats.<format id>.fields.[<field id>].params.types`         |
| SOURCES        | 0x03 | [TrustedNameSource](#trustednamesource-enum)[] | allowed sources for types for trusted name |          | `$.display.formats.<format id>.fields.[<field id>].params.sources`       |
| SENDER_ADDRESS | 0x04 | uint8[20]           | address to interpret as the sender         | x        | `$.display.formats.<format id>.fields.[<field id>].params.senderAddress` |
| VALUE_TYPE     | 0x05 | [TrustedNameValueType](#trustednamevaluetype-enum) | default: STANDARD | x        | `$.display.formats.<format id>.fields.[<field id>].params.valueType`     |

This struct can contain `SENDER_ADDRESS` multiple times for multiple addresses.

When `VALUE_TYPE` is absent it defaults to `STANDARD`.
When `VALUE_TYPE` is `INTEROPERABLE`, `VALUE` holds EIP-7930-encoded bytes
(`[chain_id (1–8 bytes, big-endian)][address (20 bytes)]`).
The device extracts the chain ID and EVM address, may display the chain as a network
name suffix, and applies trusted name resolution to the address part.

#### TrustedNameValueType enum

| Name          | Value | Description                                                      |
|---------------|-------|------------------------------------------------------------------|
| STANDARD      | 0x00  | VALUE is a 20-byte EVM address (default).                        |
| INTEROPERABLE | 0x01  | VALUE is an EIP-7930 interoperable address (chain_id + address). |

#### TrustedNameType enum

| Name            | Value | Description                                                                                        |
|-----------------|-------|----------------------------------------------------------------------------------------------------|
| EOA             | 0x01  | Address is an Externally Owned Account.                                                            |
| SMART_CONTRACT  | 0x02  | Address is smart contract.                                                                         |
| COLLECTION      | 0x03  | Address is a well known NFT collection.                                                            |
| TOKEN           | 0x04  | Address is a token contract.                                                                       |
| WALLET          | 0x05  | Address is owned by the wallet.                                                                    |
| CONTEXT_ADDRESS | 0x06  | Alias address bound to a specific execution context (e.g SPL address, contract specific address…). |

#### TrustedNameSource enum

| Name               | Value | Description        |
|--------------------|-------|--------------------|
| LOCAL_ADDRESS_BOOK | 0x00  | Local address book |
| CRYPTO_ASSET_LIST  | 0x01  | CAL                |
| ENS                | 0x02  | ENS                |
| UNSTOPPABLE_DOMAIN | 0x03  | Unstoppable Domain |
| FREENAME           | 0x04  | Freename           |
| DNS                | 0x05  | DNS                |
| DYNAMIC_RESOLVER   | 0x06  | Dynamic Resolver   |

### PARAM_CALLDATA

| Name            | Tag  | Payload type | Description                             | Optional | Source / value                                                                   |
|-----------------|------|--------------|-----------------------------------------|----------|----------------------------------------------------------------------------------|
| VERSION         | 0x00 | uint8        | struct version                          |          | constant: `0x0`                                                                  |
| VALUE           | 0x01 | [VALUE](#value)        |                                         |          |                                                                                  |
| CALLEE          | 0x02 | [VALUE](#value)        |                                         |          |                                                                                  |
| CHAIN_ID        | 0x03 | [VALUE](#value)        |                                         |    x     |                                                                                  |
| SELECTOR        | 0x04 | [VALUE](#value)        |                                         |    x     |                                                                                  |
| AMOUNT          | 0x05 | [VALUE](#value)        |                                         |    x     |                                                                                  |
| SPENDER         | 0x06 | [VALUE](#value)        |                                         |    x     |                                                                                  |

> __Notes__:
>
> - `CALLEE` and all optional VALUE fields (`CHAIN_ID`, `SELECTOR`, `AMOUNT`, `SPENDER`) may
>   reference an array with a single element; that value is then broadcast (repeated) for every
>   calldata iteration. Arrays whose sizes differ from the calldata count and are not 1 are rejected.

### PARAM_TOKEN

| Name            | Tag  | Payload type | Description                             | Optional | Source / value                                                                   |
|-----------------|------|--------------|-----------------------------------------|----------|----------------------------------------------------------------------------------|
| VERSION         | 0x00 | uint8        | struct version                          |          | constant: `0x0`                                                                  |
| ADDRESS         | 0x01 | [VALUE](#value)        | reference to value to display           |          | `$.display.formats.<format id>.fields.[<field id>].path`                         |
| NATIVE_CURRENCY | 0x02 | uint8[20]    | address to interpret as native currency | x        | `$.display.formats.<format id>.fields.[<field id>].params.nativeCurrencyAddress` |

This struct can contain `NATIVE_CURRENCY` multiple times for multiple addresses.

### PARAM_NETWORK

| Name    | Tag  | Payload type | Description                 | Optional | Source / value                                           |
|---------|------|--------------|-----------------------------|----------|----------------------------------------------------------|
| VERSION | 0x00 | uint8        | struct version              |          | constant: `0x0`                                          |
| VALUE   | 0x01 | [VALUE](#value)        | reference to chain ID value |          | `$.display.formats.<format id>.fields.[<field id>].path` |

The device looks up the network name from the chain ID using:

1. Dynamic networks
2. Built-in networks

If the network is not found, the device falls back to displaying the raw chain ID.

### PARAM_GROUP

| Name           | Tag  | Payload type | Description                               | Optional | Source / value                                                           |
|----------------|------|--------------|-------------------------------------------|----------|--------------------------------------------------------------------------|
| VERSION        | 0x00 | uint8        | struct version                            |          | constant: `0x01`                                                         |
| ITERATION_TYPE | 0x01 | [GroupIterationType](#groupiterationtype-enum) | iteration order    | x        | `$.display.formats.<format id>.fields.[<field id>].params.iterationType` |
| FIELD          | 0x02 | [FIELD](#field)        | a sub-field within the group (repeatable) |          | `$.display.formats.<format id>.fields.[<field id>].params.fields`        |

`FIELD` may appear multiple times (one per sub-field).

#### GroupIterationType enum

| Name       | Value | Description                                                                               |
|------------|-------|-------------------------------------------------------------------------------------------|
| BUNDLED    | 0x00  | Sub-fields are displayed interleaved by array index (e.g. ID[0]+Value[0], ID[1]+Value[1]) |
| SEQUENTIAL | 0x01  | All elements of each sub-field are displayed before moving to the next (default order)    |

> __Notes__:
>
> - `ITERATION_TYPE` defaults to `BUNDLED` (0x00) if not present.
> - Each `FIELD` payload is a fully encoded `FIELD` struct (same format as a top-level field descriptor).
> - `BUNDLED` iteration is not yet fully implemented; the device falls back to `SEQUENTIAL` order.
>   The root cause is architectural: `format_field` currently iterates over __all__ array elements of a
>   sub-field in one call (via `value_get` → full element collection), so the group formatter has no way
>   to request "element *i* only" from each sub-field.  Implementing true interleaving would require either
>   (a) a new per-element formatter API (e.g. `format_field_at_index`) exposed to the group layer, or
>   (b) post-processing the field table to reorder entries by array index after sequential formatting.
>   Both options require a non-trivial API or architecture revision; the spec flags this as
>   "ADR required (new TLV in descriptors)".

### VALUE

| Name           | Tag  | Payload type    | Description                             | Optional | Source / value                                            |
|----------------|------|-----------------|-----------------------------------------|----------|-----------------------------------------------------------|
| VERSION        | 0x00 | uint8           | struct version                          |          | constant: `0x0`                                           |
| TYPE_FAMILY    | 0x01 | [TypeFamily](#typefamily-enum)    |                                         |          |                                                           |
| TYPE_SIZE      | 0x02 | uint8           | size of values (in bytes)               | x        |                                                           |
| DATA_PATH      | 0x03 | [DATA_PATH](#data_path)       | path to value in serialized transaction | x        | `$.display.formats.<format id>.fields.[<field id>].path`  |
| CONTAINER_PATH | 0x04 | [ContainerPath](#containerpath-enum) | container value enum                    | x        | `$.display.formats.<format id>.fields.[<field id>].path`  |
| CONSTANT       | 0x05 | uint8[]         | literal value                           | x        | `$.display.formats.<format id>.fields.[<field id>].value` |
| MAP_REF        | 0x06 | [MAP_REF](#map_ref)         | reference to a map entry                | x        | `$.metadata.maps.<map id>`                                |

#### TypeFamily enum

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

#### ContainerPath enum

| Name     | Value |
|----------|-------|
| FROM     | 0x00  |
| TO       | 0x01  |
| VALUE    | 0x02  |
| CHAIN_ID | 0x03  |

> __Note__: The TLV payload must include exactly one of `DATA_PATH`, `CONTAINER_PATH`, `CONSTANT` or `MAP_REF`.

### DATA_PATH

| Name    | Tag  | Payload type    | Description                                                                                                                               | Optional | Source / value  |
|---------|------|-----------------|-------------------------------------------------------------------------------------------------------------------------------------------|----------|-----------------|
| VERSION | 0x00 | uint8           | struct version                                                                                                                            |          | constant: `0x0` |
| TUPLE   | 0x01 | uint16          | move by {value} slots from current slot                                                                                                   | x        |                 |
| ARRAY   | 0x02 | [ARRAY_ELEMENT](#array_element)   | current slot is array length, added to offset if negative. multiple by item_size and move by result slots. payload unset => iterate array | x        |                 |
| REF     | 0x03 |                 | read value of current slot. apply read value as offset from current slot                                                                  | x        |                 |
| LEAF    | 0x04 | [PathLeafType](#pathleaftype-enum)  | current slot is a leaf type, specifying the type of path end                                                                              | x        |                 |
| SLICE   | 0x05 | [SLICE_ELEMENT](#slice_element)   | specify slicing to apply to final leaf value as (start, end)                                                                              | x        |                 |

#### PathLeafType enum

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

## MAP_ENTRY

Provided via `PROVIDE_MAP_ENTRY` APDU (INS `0x3A`). Signed by CAL. Associates a key with a
value for context-dependent constants. The wallet resolves the key from the transaction context
and sends only the matching entry to the device.

| Name          | Tag  | Payload type | Description                                                    | Optional | Source / value                                             |
|---------------|------|--------------|----------------------------------------------------------------|----------|------------------------------------------------------------|
| VERSION       | 0x00 | uint8        | struct version                                                 |          | constant: `0x0`                                            |
| CHAIN_ID      | 0x01 | uint64       | EIP-155 chain ID                                               |          | `$.context.contract.deployments.[<deployment id>].chainId` |
| CONTRACT_ADDR | 0x02 | uint8[20]    | EVM contract address                                           |          | `$.context.contract.deployments.[<deployment id>].address` |
| SELECTOR      | 0x03 | uint8[4]     | function selector                                              |          |                                                            |
| ID            | 0x04 | uint8        | map identifier (to differentiate multiple maps in a contract)  |          |                                                            |
| KEY           | 0x05 | uint8[]      | map key (raw bytes)                                            |          | `$.metadata.maps.<map id>.<key>`                           |
| VALUE         | 0x06 | uint8[]      | map value (raw bytes)                                          |          | `$.metadata.maps.<map id>.<key>.value`                     |
| SIGNATURE     | 0xff | uint8[]      | signature of all the other struct fields                       |          | computed by CAL                                            |

> [!NOTE]
> The device verifies the KEY matches the value resolved from the transaction context before using the VALUE.
> If no matching MAP_ENTRY is found in the current context, the clear signing flow is aborted.

### MAP_REF

Embedded as VALUE tag `0x06`. References a stored MAP_ENTRY by ID and key.

| Name    | Tag  | Payload type | Description                            | Optional | Source / value  |
|---------|------|--------------|----------------------------------------|----------|-----------------|
| VERSION | 0x00 | uint8        | struct version                         |          | constant: `0x0` |
| ID      | 0x01 | uint8        | map identifier (references MAP_ENTRY)  |          |                 |
| KEY     | 0x02 | [VALUE](#value)        | key to look up in the stored MAP_ENTRY |          |                 |

> [!NOTE]
> The KEY is a VALUE that specifies where to get the key from the transaction context.
> Common key sources: `CONTAINER_PATH` with `CHAIN_ID` for chain-dependent values, or `DATA_PATH` for keys in calldata.
> Nested MAP_REF keys (MAP_REF within MAP_REF KEY) are not supported.

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
| DELEGATION_TYPE | 0x43 | [DelegationType](#delegationtype-enum) | type of delegation              |          |
| SIGNATURE       | 0x15 | uint8[]          | signature of the structure      |          |

### DelegationType enum

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
| NETWORK_ICON_HASH | 0x53 | uint8[32]    | *sha256* of the network icon  |                                 |
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
| DOMAIN_HASH                   | 0x28 | uint8[32]    | *Domain Hash* for EIP712                 |                                 |
| TX_CHECKS_NORMALIZED_RISK     | 0x80 | uint8        | Normalized risk score of the transaction |                                 |
| TX_CHECKS_NORMALIZED_CATEGORY | 0x81 | uint8        | Main category explaining the risk score  |                                 |
| TX_CHECKS_PROVIDER_MSG        | 0x82 | char[30]     | Provider specific message                |                                 |
| TX_CHECKS_TINY_URL            | 0x83 | char[30]     | URL to access the full report            |                                 |
| TX_TYPE                       | 0x84 | uint8        | Type of simulation                       |                                 |
| SIGNATURE                     | 0x15 | uint8[]      | Signature of the structure               |                                 |

The signature is computed on the full payload data, using `CX_CURVE_SECP256K1`.

The *Risk Score* is normalized and interpreted like this:

- `0`: Benign
- `1`: Warning
- `2`: Malicious

The *Main Category* is normalized and interpreted like this:

- `1`: Others
- `2`: Address
- `3`: dApp
- `4`: Losing Operation

The *Simulation Type* is normalized and interpreted like this:

- `0`: Transaction
- `1`: Typed Data (EIP-712)
- `2`: Personal Message (EIP-191)

## SAFE_ACCOUNT

### SAFE_DESCRIPTOR

| Name              | Tag  | Payload type | Description                    | Value                             |
|-------------------|------|--------------|--------------------------------|-----------------------------------|
| STRUCTURE_TYPE    | 0x01 | uint8        | Structure type                 | `0x27` (`TYPE_LESM_ACCOUNT_INFO`) |
| STRUCTURE_VERSION | 0x02 | uint8        | Structure version              | `0x01`                            |
| CHALLENGE         | 0x12 | uint32       | Challenge generated by the App |                                   |
| ADDRESS           | 0x22 | uint8[20]    | Safe Account Address           |                                   |
| THRESHOLD         | 0xA0 | uint8        | Number of required signatures  |                                   |
| SIGNERS_COUNT     | 0xA1 | uint8        | Total number of signers        |                                   |
| ROLE              | 0xA2 | uint8        | Role of the account            |                                   |
| SIGNATURE         | 0x15 | uint8[]      | Signature of the structure     |                                   |

The signature is computed on the full payload data, using `CX_CURVE_SECP256K1`.

The *Role* is normalized and interpreted like this:

- `0`: Signer
- `1`: Proposer

### SIGNER_DESCRIPTOR

| Name              | Tag  | Payload type | Description                    | Value                              |
|-------------------|------|--------------|--------------------------------|------------------------------------|
| STRUCTURE_TYPE    | 0x01 | uint8        | Structure type                 | `0x0A` (`TYPE_VERIFIABLE_ADDRESS`) |
| STRUCTURE_VERSION | 0x02 | uint8        | Structure version              | `0x01`                             |
| CHALLENGE         | 0x12 | uint32       | Challenge generated by the App |                                    |
| ADDRESS           | 0x22 | uint8[20]    | Signer Address                 |                                    |
| SIGNATURE         | 0x15 | uint8[]      | Signature of the structure     |                                    |

The signature is computed on the full payload data, using `CX_CURVE_SECP256K1`.

## GATING_SIGNING

### GATING_DESCRIPTOR

| Name              | Tag  | Payload type | Description                                             | Value                             |
|-------------------|------|--------------|---------------------------------------------------------|-----------------------------------|
| STRUCTURE_TYPE    | 0x01 | uint8        | Structure type                                          | `0x0D` (`TYPE_GATED_SIGNING`)     |
| STRUCTURE_VERSION | 0x02 | uint8        | Structure version                                       | `0x01`                            |
| ADDRESS           | 0x22 | uint8[20]    | `To` (SignTx) or `verifyingContract` (EIP712)           |                                   |
| CHAIN_ID          | 0x23 | uint64       | Network chain ID                                        |                                   |
| SELECTOR          | 0x40 | uint8[4-32]  | SC function selector (SignTx) or `schema_hash` (EIP712) |                                   |
| INTRO_MSG         | 0x82 | char[100]    | Provider specific message                               |                                   |
| TINY_URL          | 0x83 | char[30]     | URL to access the full report                           |                                   |
| TX_TYPE           | 0x84 | uint8        | Type of transaction                                     |                                   |
| SIGNATURE         | 0x15 | uint8[]      | Signature of the structure                              |                                   |

The signature is computed on the full payload data, using `CX_CURVE_SECP256K1`.

## DYNAMIC_TOKEN_DESCRIPTOR

Parsed by the SDK `tlv_use_case_dynamic_descriptor` helper.

| Name             | Tag  | Payload type | Description                                                         |
|------------------|------|--------------|---------------------------------------------------------------------|
| STRUCTURE_TYPE   | 0x01 | uint8        | Structure type, must be `0x90` (`TYPE_DYNAMIC_TOKEN`)               |
| VERSION          | 0x02 | uint8        | Serialization format version, currently `0x01`                      |
| COIN_TYPE        | 0x03 | uint32       | SLIP-44 coin type (e.g. `60` for Ethereum)                          |
| APPLICATION_NAME | 0x04 | char[]       | Ledger application name this descriptor targets (e.g. `"Ethereum"`) |
| TICKER           | 0x05 | char[]       | Token ticker symbol (without `'\0'`)                                |
| MAGNITUDE        | 0x06 | uint8        | Number of decimals                                                  |
| TUID             | 0x07 | [ETH_TUID](#eth_tuid)     | Token Unique ID — application-specific sub-TLV                      |
| SIGNATURE        | 0x08 | uint8[]      | Signature of the SHA-256 hash of all fields except SIGNATURE        |

The signature is verified via the Ledger PKI with key usage `CERTIFICATE_PUBLIC_KEY_USAGE_COIN_META`.

### ETH_TUID

The TUID field contains a nested TLV payload with the following tags:

| Name     | Tag  | Payload type | Description             |
|----------|------|--------------|-------------------------|
| ADDRESS  | 0x22 | uint8[20]    | ERC-20 contract address |
| CHAIN_ID | 0x23 | uint64       | EVM chain ID            |
