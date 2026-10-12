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

## TRANSACTION_INFO / EIP712_MESSAGE_INFO

One structure describes a transaction (`TRANSACTION_INFO`) or an EIP-712 V2 message
(`EIP712_MESSAGE_INFO`, see [EIP712_MESSAGE_INFO](eip712.md#eip712_message_info)). The `Context` column
tells which tags each one has.

| Name               | Tag  | Payload type | Description                                          | Optional    | Context      | Source / value                                             |
|--------------------|------|--------------|------------------------------------------------------|-------------|--------------|------------------------------------------------------------|
| VERSION            | 0x00 | uint8        | struct version                                       |             | both         | constant: `0x1`                                            |
| CHAIN_ID           | 0x01 | uint64       | EIP-155 chain ID                                     |             | SignTx only  | `$.context.contract.deployments.[<deployment id>].chainId` |
| CONTRACT_ADDR      | 0x02 | uint8[20]    | EVM contract address                                 |             | SignTx only  | `$.context.contract.deployments.[<deployment id>].address` |
| SELECTOR           | 0x03 | uint8[4]     | selector (4 bytes form)                              |             | SignTx only  | `$.display.formats.<format id>`                            |
| FIELDS_HASH        | 0x04 | uint8[32]    | SHA3-256 hash of all the FIELD structs               |             | both         | computed by CAL                                            |
| OPERATION_TYPE     | 0x05 | char[]       | Will be appended to "Review ..." in the first screen |             | both         | `$.display.formats.<selector>.intent`                      |
| CREATOR_NAME       | 0x06 | char[]       |                                                      | x           | both         | `$.metadata.owner`                                         |
| CREATOR_LEGAL_NAME | 0x07 | char[]       |                                                      | x           | both         | `$.metadata.info.legalName`                                |
| CREATOR_URL        | 0x08 | char[]       | website of the dApp or company behind it             | x           | both         | `$.metadata.info.url`                                      |
| CONTRACT_NAME      | 0x09 | char[]       |                                                      | x           | both         | `$.metadata.info.$id`                                      |
| DEPLOY_DATE        | 0x0a | uint32       | unix epoch, shown as YYYY-MM-DD                      | x           | both         | `$.metadata.info.deploymentDate`                           |
| DOMAIN_SEPARATOR   | 0x0b | uint8[32]    | `hashStruct(domain)`                                 |             | EIP-712 only | `$.context.eip712.domain`, with the deployment             |
| PRIMARY_TYPE_HASH  | 0x0c | uint8[32]    | `keccak256(encodeType(primaryType))`                 |             | EIP-712 only | keccak256 of the `$.display.formats` key of the message    |
| SIGNATURE          | 0xff | uint8[]      | signature of all the other struct fields             |             | both         | computed by CAL                                            |

> [!CAUTION]
>
> - `$.metadata.owner` is optional, made `CREATOR_NAME` optional
> - `$.metadata.info.legalName` is optional, made `CREATOR_LEGAL_NAME` optional
> - `$.display.formats.<selector>.intent` is optional, possible fallbacks: `$.display.formats.<selector>.$id`, `$.display.formats.<selector>`
> - `CONTRACT_NAME` is not really materialized in the spec, closest is `$.metadata.info.$id`,
     but `$id` is supposed to be internal
> - `$.metadata.info.deploymentDate` is optional, made `DEPLOY_DATE` optional
> - Each context has its own tags: `CHAIN_ID`, `CONTRACT_ADDR` and `SELECTOR` for a transaction,
>   `DOMAIN_SEPARATOR` and `PRIMARY_TYPE_HASH` for an EIP-712 message. A structure with a tag of
>   the other context is refused, so the signed bytes tell which context they are for.
> - `OPERATION_TYPE` completes the title of the review: `Review transaction to <OPERATION_TYPE>` for
>   a transaction, `Review typed message to <OPERATION_TYPE>` for an EIP-712 message.
> - For an EIP-712 message, `DOMAIN_SEPARATOR` covers the `chainId` and the `verifyingContract` of
>   the domain, which are the chain and the contract of the message. See
>   [EIP712_MESSAGE_INFO](eip712.md#eip712_message_info).

## ENUM_VALUE

Provided via `PROVIDE_ENUM_VALUE` APDU (INS `0x24`). Signed by CAL. It is sent before the FIELD that
uses it, and may come before the [TRANSACTION_INFO](#transaction_info--eip712_message_info) or the
[EIP712_MESSAGE_INFO](eip712.md#eip712_message_info) of the operation it is for: its tags tell which
kind of operation it is for. It has `CHAIN_ID`, `CONTRACT_ADDR` and `SELECTOR` for a transaction,
`DOMAIN_SEPARATOR` and `PRIMARY_TYPE_HASH` for a typed message, and none of the other.

For a typed message, it is looked up with the domain separator and the primary type hash of the
message, whatever chain or contract its domain has.

| Name              | Tag  | Payload type | Description                                                              | Optional | Context      | Source / value                                             |
|-------------------|------|--------------|--------------------------------------------------------------------------|----------|--------------|------------------------------------------------------------|
| VERSION           | 0x00 | uint8        | struct version                                                           |          | both         | constant: `0x1`                                            |
| CHAIN_ID          | 0x01 | uint64       | EIP-155 chain ID                                                         |          | SignTx only  | `$.context.contract.deployments.[<deployment id>].chainId` |
| CONTRACT_ADDR     | 0x02 | uint8[20]    | EVM contract address                                                     |          | SignTx only  | `$.context.contract.deployments.[<deployment id>].address` |
| SELECTOR          | 0x03 | uint8[4]     | function selector                                                        |          | SignTx only  |                                                            |
| ID                | 0x04 | uint8        | identifier of the enum (to differentiate multiple enums in one contract) |          | both         | `$.metadata.enums.<enum id>`                               |
| VALUE             | 0x05 | uint8        | enum entry integer value                                                 |          | both         | `$.metadata.enums.<enum id>.<enum entry>.value`            |
| NAME              | 0x06 | char[]       | enum entry name (ASCII)                                                  |          | both         | `$.metadata.enums.<enum id>.<enum entry>.name`             |
| DOMAIN_SEPARATOR  | 0x07 | uint8[32]    | `hashStruct(domain)`                                                     |          | EIP-712 only | `$.context.eip712.domain`, with the deployment             |
| PRIMARY_TYPE_HASH | 0x08 | uint8[32]    | `keccak256(encodeType(primaryType))`                                     |          | EIP-712 only |                                                            |
| SIGNATURE         | 0xff | uint8[]      | signature of all the other struct fields                                 |          | both         | computed by CAL                                            |

> [!NOTE]
>
> - A selector is looked up for the call being described, and the contract is the callee of that
>   call, resolved through the proxy information if there is any. A primary type hash is looked up
>   for the message being described, with the domain separator of the message; no proxy is
>   involved.
> - The two kinds of key never match each other, whatever their bytes are.

## FIELD

It contains no signature since the signed TRANSACTION_INFO struct (or EIP712_MESSAGE_INFO struct)
already has a hash of all the FIELD structs, which attests of the authenticity, order and
completeness of all FIELD structs.

| Name       | Tag  | Payload type | Description                         | Optional | Source / value                                                |
|------------|------|--------------|-------------------------------------|----------|---------------------------------------------------------------|
| VERSION    | 0x00 | uint8        | struct version                      |          | constant: `0x1`                                               |
| NAME       | 0x01 | char[]       | field display name (ASCII)          |          | `$.display.formats.<format id>.fields.[<field id>].label`     |
| PARAM_TYPE | 0x02 | [ParamType](#paramtype-enum) |                                     |          | `$.display.formats.<format id>.fields.[<field id>].params`    |
| PARAM      | 0x03 | [PARAM_RAW](#param_raw) \| [PARAM_AMOUNT](#param_amount) \| [PARAM_TOKEN_AMOUNT](#param_token_amount) \| [PARAM_NFT](#param_nft) \| [PARAM_DATETIME](#param_datetime) \| [PARAM_DURATION](#param_duration) \| [PARAM_UNIT](#param_unit) \| [PARAM_ENUM](#param_enum) \| [PARAM_TRUSTED_NAME](#param_trusted_name) \| [PARAM_CALLDATA](#param_calldata) \| [PARAM_TOKEN](#param_token) \| [PARAM_NETWORK](#param_network) \| [PARAM_GROUP](#param_group) |                                     |          | `$.display.formats.<format id>.fields.[<field id>].params`    |
| VISIBLE    | 0x04 | [VisibleType](#visibletype-enum) | visibility condition  | x        | `$.display.formats.<format id>.fields.[<field id>].visible`   |
| CONSTRAINT | 0x05 | uint8[]      | constraint value (raw bytes)        | x        | `$.display.formats.<format id>.fields.[<field id>].visible`   |
| SEPARATOR  | 0x06 | char[]       | separator for array iteration       | x        | `$.display.formats.<format id>.fields.[<field id>].separator` |

> __Notes__:
>
> - `VERSION` is `0x1` in FIELD and in every structure it holds: the `PARAM_*` structures, `VALUE`,
>   `DATA_PATH` and `MAP_REF`. The device refuses any other version, and the descriptor with it.
> - `VISIBLE` defaults to `ALWAYS` (0x00) if not present
> - `VISIBLE` can be present only once and should be served before any `CONSTRAINT`
> - `CONSTRAINT` is only present when `VISIBLE` is `MUST_BE` or `IF_NOT_IN`
> - `CONSTRAINT` tag can appear multiple times for multiple allowed/excluded values (OR semantics).
>   There is no fixed limit; the number of constraints is bounded by the FIELD payload size.
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

| Name    | Tag  | Payload type    | Description                   | Optional | Source / value                                           |
|---------|------|-----------------|-------------------------------|----------|----------------------------------------------------------|
| VERSION | 0x00 | uint8           | struct version                |          | constant: `0x1`                                          |
| VALUE   | 0x01 | [VALUE](#value) | reference to value to display |          | `$.display.formats.<format id>.fields.[<field id>].path` |

### PARAM_AMOUNT

| Name    | Tag  | Payload type    | Description                   | Optional | Source / value                                           |
|---------|------|-----------------|-------------------------------|----------|----------------------------------------------------------|
| VERSION | 0x00 | uint8           | struct version                |          | constant: `0x1`                                          |
| VALUE   | 0x01 | [VALUE](#value) | reference to value to display |          | `$.display.formats.<format id>.fields.[<field id>].path` |

### PARAM_TOKEN_AMOUNT

| Name                | Tag  | Payload type    | Description                             | Optional | Source / value                                                                   |
|---------------------|------|-----------------|-----------------------------------------|----------|----------------------------------------------------------------------------------|
| VERSION             | 0x00 | uint8           | struct version                          |          | constant: `0x1`                                                                  |
| VALUE               | 0x01 | [VALUE](#value) | reference to value to display           |          | `$.display.formats.<format id>.fields.[<field id>].path`                         |
| TOKEN               | 0x02 | [VALUE](#value) | reference to token address              | x        | `$.display.formats.<format id>.fields.[<field id>].params.tokenPath`             |
| NATIVE_CURRENCY     | 0x03 | uint8[20]       | address to interpret as native currency | x        | `$.display.formats.<format id>.fields.[<field id>].params.nativeCurrencyAddress` |
| THRESHOLD           | 0x04 | uint256         | unlimited amount threshold              | x        | `$.display.formats.<format id>.fields.[<field id>].params.threshold`             |
| ABOVE_THRESHOLD_MSG | 0x05 | char[]          | unlimited amount label                  | x        | `$.display.formats.<format id>.fields.[<field id>].params.message`               |

This struct can contain `NATIVE_CURRENCY` multiple times for multiple addresses.

> __Notes__:
>
> - When `VALUE` and `TOKEN` reference arrays of different lengths, iteration is still allowed if
>   `TOKEN` has exactly one element — that single token address is then broadcast (repeated) for
>   every element of `VALUE`. Arrays whose sizes differ and neither is 1 are rejected.
> - When the app holds no information about the token (for that chain and that address, see
>   [PROVIDE ERC 20 TOKEN INFORMATION](apdu.md#provide-erc-20-token-information)), the amount is
>   shown as an integer, with `???` as its ticker.

### PARAM_NFT

| Name       | Tag  | Payload type    | Description                         | Optional | Source / value                                                            |
|------------|------|-----------------|-------------------------------------|----------|---------------------------------------------------------------------------|
| VERSION    | 0x00 | uint8           | struct version                      |          | constant: `0x1`                                                           |
| VALUE      | 0x01 | [VALUE](#value) | reference to value to display       |          | `$.display.formats.<format id>.fields.[<field id>].path`                  |
| COLLECTION | 0x02 | [VALUE](#value) | reference to the collection address |          | `$.display.formats.<format id>.fields.[<field id>].params.collectionPath` |

### PARAM_DATETIME

| Name    | Tag  | Payload type    | Description                         | Optional | Source / value                                                      |
|---------|------|-----------------|-------------------------------------|----------|---------------------------------------------------------------------|
| VERSION | 0x00 | uint8           | struct version                      |          | constant: `0x1`                                                     |
| VALUE   | 0x01 | [VALUE](#value) | reference to value to display       |          | `$.display.formats.<format id>.fields.[<field id>].path`            |
| TYPE    | 0x02 | uint8           | 0 for unix time, 1 for block height |          | `$.display.formats.<format id>.fields.[<field id>].params.encoding` |

### PARAM_DURATION

| Name    | Tag  | Payload type    | Description                   | Optional | Source / value                                           |
|---------|------|-----------------|-------------------------------|----------|----------------------------------------------------------|
| VERSION | 0x00 | uint8           | struct version                |          | constant: `0x1`                                          |
| VALUE   | 0x01 | [VALUE](#value) | reference to value to display |          | `$.display.formats.<format id>.fields.[<field id>].path` |

### PARAM_UNIT

| Name     | Tag  | Payload type    | Description                   | Optional | Source / value                                                      |
|----------|------|-----------------|-------------------------------|----------|---------------------------------------------------------------------|
| VERSION  | 0x00 | uint8           | struct version                |          | constant: `0x1`                                                     |
| VALUE    | 0x01 | [VALUE](#value) | reference to value to display |          | `$.display.formats.<format id>.fields.[<field id>].path`            |
| BASE     | 0x02 | char[]          |                               |          | `$.display.formats.<format id>.fields.[<field id>].params.base`     |
| DECIMALS | 0x03 | uint8           | defaults to 0                 | x        | `$.display.formats.<format id>.fields.[<field id>].params.decimals` |
| PREFIX   | 0x04 | bool            | defaults to false             | x        | `$.display.formats.<format id>.fields.[<field id>].params.prefix`   |

`PREFIX` has to be false: the device does not apply a prefix to the value, and it refuses a descriptor
that asks for one (`true`).

### PARAM_ENUM

| Name    | Tag  | Payload type    | Description                              | Optional | Source / value                                                  |
|---------|------|-----------------|------------------------------------------|----------|-----------------------------------------------------------------|
| VERSION | 0x00 | uint8           | struct version                           |          | constant: `0x1`                                                 |
| ID      | 0x01 | uint8           | identifier of the enum, as in ENUM_VALUE |          | `$.display.formats.<format id>.fields.[<field id>].params.$ref` |
| VALUE   | 0x02 | [VALUE](#value) | reference to value to display            |          | `$.display.formats.<format id>.fields.[<field id>].path`        |

### PARAM_TRUSTED_NAME

| Name           | Tag  | Payload type                                       | Description                                | Optional | Source / value                                                           |
|----------------|------|----------------------------------------------------|--------------------------------------------|----------|--------------------------------------------------------------------------|
| VERSION        | 0x00 | uint8                                              | struct version                             |          | constant: `0x1`                                                          |
| VALUE          | 0x01 | [VALUE](#value)                                    | reference to value to display              |          | `$.display.formats.<format id>.fields.[<field id>].path`                 |
| TYPES          | 0x02 | [TrustedNameType](#trustednametype-enum)[]         | allowed types for types for trusted name   |          | `$.display.formats.<format id>.fields.[<field id>].params.types`         |
| SOURCES        | 0x03 | [TrustedNameSource](#trustednamesource-enum)[]     | allowed sources for types for trusted name |          | `$.display.formats.<format id>.fields.[<field id>].params.sources`       |
| SENDER_ADDRESS | 0x04 | uint8[20]                                          | address to interpret as the sender         | x        | `$.display.formats.<format id>.fields.[<field id>].params.senderAddress` |
| VALUE_TYPE     | 0x05 | [TrustedNameValueType](#trustednamevaluetype-enum) | default: STANDARD                          | x        | `$.display.formats.<format id>.fields.[<field id>].params.valueType`     |

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

| Name     | Tag  | Payload type    | Description                                                         | Optional | Source / value                                                          |
|----------|------|-----------------|---------------------------------------------------------------------|----------|-------------------------------------------------------------------------|
| VERSION  | 0x00 | uint8           | struct version                                                      |          | constant: `0x1`                                                         |
| VALUE    | 0x01 | [VALUE](#value) | the embedded calldata                                               |          | `$.display.formats.<format id>.fields.[<field id>].path`                |
| CALLEE   | 0x02 | [VALUE](#value) | address of the contract that is called                              |          | `$.display.formats.<format id>.fields.[<field id>].params.calleePath`   |
| CHAIN_ID | 0x03 | [VALUE](#value) | chain of the call, when it is not the one of the operation          | x        | `$.display.formats.<format id>.fields.[<field id>].params.chainIdPath`  |
| SELECTOR | 0x04 | [VALUE](#value) | selector of the call, the first 4 bytes of the calldata when absent | x        | `$.display.formats.<format id>.fields.[<field id>].params.selectorPath` |
| AMOUNT   | 0x05 | [VALUE](#value) | native amount of the call                                           | x        | `$.display.formats.<format id>.fields.[<field id>].params.amountPath`   |
| SPENDER  | 0x06 | [VALUE](#value) | sender of the call                                                  | x        | `$.display.formats.<format id>.fields.[<field id>].params.spenderPath`  |

> __Notes__:
>
> - `CALLEE` and all optional VALUE fields (`CHAIN_ID`, `SELECTOR`, `AMOUNT`, `SPENDER`) may
>   reference an array with a single element; that value is then broadcast (repeated) for every
>   calldata iteration. Arrays whose sizes differ from the calldata count and are not 1 are rejected.

### PARAM_TOKEN

| Name            | Tag  | Payload type    | Description                             | Optional | Source / value                                                                   |
|-----------------|------|-----------------|-----------------------------------------|----------|----------------------------------------------------------------------------------|
| VERSION         | 0x00 | uint8           | struct version                          |          | constant: `0x1`                                                                  |
| ADDRESS         | 0x01 | [VALUE](#value) | reference to value to display           |          | `$.display.formats.<format id>.fields.[<field id>].path`                         |
| NATIVE_CURRENCY | 0x02 | uint8[20]       | address to interpret as native currency | x        | `$.display.formats.<format id>.fields.[<field id>].params.nativeCurrencyAddress` |

This struct can contain `NATIVE_CURRENCY` multiple times for multiple addresses.

### PARAM_NETWORK

| Name    | Tag  | Payload type    | Description                 | Optional | Source / value                                           |
|---------|------|-----------------|-----------------------------|----------|----------------------------------------------------------|
| VERSION | 0x00 | uint8           | struct version              |          | constant: `0x1`                                          |
| VALUE   | 0x01 | [VALUE](#value) | reference to chain ID value |          | `$.display.formats.<format id>.fields.[<field id>].path` |

The device looks up the network name from the chain ID using:

1. Dynamic networks
2. Built-in networks

If the network is not found, the device falls back to displaying the raw chain ID.

### PARAM_GROUP

| Name           | Tag  | Payload type                                   | Description                               | Optional | Source / value                                                           |
|----------------|------|------------------------------------------------|-------------------------------------------|----------|--------------------------------------------------------------------------|
| VERSION        | 0x00 | uint8                                          | struct version                            |          | constant: `0x01`                                                         |
| ITERATION_TYPE | 0x01 | [GroupIterationType](#groupiterationtype-enum) | iteration order                           | x        | `$.display.formats.<format id>.fields.[<field id>].params.iterationType` |
| FIELD          | 0x02 | [FIELD](#field)                                | a sub-field within the group (repeatable) |          | `$.display.formats.<format id>.fields.[<field id>].params.fields`        |

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
> - Groups nest at most 4 levels deep: a 5th nested group is refused.
> - `BUNDLED` iteration is not implemented: the device refuses a group that asks for it. A group with
>   no `ITERATION_TYPE` is a `BUNDLED` one, so a group has to say `SEQUENTIAL`.
>   The root cause is architectural: `format_field` currently iterates over __all__ array elements of a
>   sub-field in one call (via `value_get` → full element collection), so the group formatter has no way
>   to request "element *i* only" from each sub-field.  Implementing true interleaving would require either
>   (a) a new per-element formatter API (e.g. `format_field_at_index`) exposed to the group layer, or
>   (b) post-processing the field table to reorder entries by array index after sequential formatting.
>   Both options require a non-trivial API or architecture revision; the spec flags this as
>   "ADR required (new TLV in descriptors)".

### VALUE

| Name           | Tag  | Payload type                         | Description                             | Optional    | Context         | Source / value                                            |
|----------------|------|--------------------------------------|-----------------------------------------|-------------|-----------------|-----------------------------------------------------------|
| VERSION        | 0x00 | uint8                                | struct version                          |             | both            | constant: `0x1`                                           |
| TYPE_FAMILY    | 0x01 | [TypeFamily](#typefamily-enum)       |                                         |             | both            |                                                           |
| TYPE_SIZE      | 0x02 | uint8                                | size of values (in bytes), 1 to 32      | x           | both            |                                                           |
| DATA_PATH      | 0x03 | [DATA_PATH](#data_path)              | path to value in serialized transaction | x (GCS)     | SignTx/GCS only | `$.display.formats.<format id>.fields.[<field id>].path`  |
| EIP712_PATH    | 0x03 | [EIP712_PATH](#eip712_path)          | path to value in the EIP-712 value tree | x (EIP-712) | EIP-712 only    | `$.display.formats.<format id>.fields.[<field id>].path`  |
| CONTAINER_PATH | 0x04 | [ContainerPath](#containerpath-enum) | container value enum                    | x           | both            | `$.display.formats.<format id>.fields.[<field id>].path`  |
| CONSTANT       | 0x05 | uint8[]                              | literal value, up to 32 bytes           | x           | both            | `$.display.formats.<format id>.fields.[<field id>].value` |
| MAP_REF        | 0x06 | [MAP_REF](#map_ref)                  | reference to a map entry                | x           | both            | `$.metadata.maps.<map id>`                                |

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

> __Note__: The TLV payload must include exactly one of `DATA_PATH`/`EIP712_PATH` (tag `0x03`,
> see the `Context` column), `CONTAINER_PATH`, `CONSTANT` or `MAP_REF`, and both `VERSION` and
> `TYPE_FAMILY`. A VALUE that lacks one of them, or that has none or several of those sources, is
> refused, and so is the descriptor that holds it.
>
> The device reads tag `0x03` according to the operation the FIELD describes: as a `DATA_PATH` for
> a transaction, and for the calls a typed message names, and as an
> [EIP712_PATH](#eip712_path) for a typed message (see
> [EIP712_FIELD_DESCRIPTOR](#eip712_field_descriptor)). The bytes do not tell which one it is: the
> operation does, so a path is only meaningful for the operation it was written for.

What a `CONTAINER_PATH` stands for depends on the operation being described too:

| Value    | Transaction                      | Typed message (EIP-712)                                                                               |
|----------|----------------------------------|-------------------------------------------------------------------------------------------------------|
| FROM     | sender of the transaction        | account that signs, the one of the derivation path of `EIP712_VALUES`                                 |
| TO       | recipient of the transaction     | `verifyingContract` of the domain, when it is an address. Refused when the domain has none            |
| VALUE    | native amount of the transaction | refused                                                                                               |
| CHAIN_ID | chain of the transaction         | `chainId` of the domain. Refused when it has none                                                     |

The calls a typed message names are transactions: their `CONTAINER_PATH` values are their own. A
call that does not say who sends it is sent by the account that signs the message, and a call that
does not say its chain is on the chain of the message. When the message has no chain, such a call
is on none: the device refuses to describe or show it.

### DATA_PATH

| Name    | Tag  | Payload type                       | Description                                                                                                                               | Optional | Source / value  |
|---------|------|------------------------------------|-------------------------------------------------------------------------------------------------------------------------------------------|----------|-----------------|
| VERSION | 0x00 | uint8                              | struct version                                                                                                                            |          | constant: `0x1` |
| TUPLE   | 0x01 | uint16                             | move by {value} slots from current slot                                                                                                   | x        |                 |
| ARRAY   | 0x02 | [ARRAY_ELEMENT](#array_element)    | current slot is array length, added to offset if negative. multiple by item_size and move by result slots. payload unset => iterate array | x        |                 |
| REF     | 0x03 |                                    | read value of current slot. apply read value as offset from current slot                                                                  | x        |                 |
| LEAF    | 0x04 | [PathLeafType](#pathleaftype-enum) | current slot is a leaf type, specifying the type of path end                                                                              | x        |                 |
| SLICE   | 0x05 | [SLICE_ELEMENT](#slice_element)    | specify slicing to apply to final leaf value as (start, end)                                                                              | x        |                 |

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

`WEIGHT` is not zero: the device refuses a weight of zero, or one that does not fit a `uint8`.

### SLICE_ELEMENT

| Name    | Tag  | Payload type    | Description              | Optional | Source / value  |
|---------|------|-----------------|--------------------------|----------|-----------------|
| START   | 0x01 | int16           | start index (inclusive)  | x        |                 |
| END     | 0x02 | int16           | end index (exclusive)    | x        |                 |

### Bounds of a path

`START` and `END` of an `ARRAY_ELEMENT` and of a `SLICE_ELEMENT` are big-endian integers of one or
two bytes. The length gives the sign:

| Length  | Values            | Examples                                    |
|---------|-------------------|---------------------------------------------|
| 1 byte  | 0 to 255          | `0x05` is 5, `0xFF` is 255                  |
| 2 bytes | -32768 to 32767   | `0x00FF` is 255, `0xFFFF` is -1             |

A negative bound counts back from the end. The device refuses a bound of any other length, so a
bound is never read as another number than the one it holds.

## MAP_ENTRY

Provided via `PROVIDE_MAP_ENTRY` APDU (INS `0x3A`). Signed by CAL. Associates a key with a
value for context-dependent constants. The wallet resolves the key from the transaction context
and sends only the matching entry to the device.

Like an [ENUM_VALUE](#enum_value), it is sent before the FIELD that uses it, and may come before
the [TRANSACTION_INFO](#transaction_info--eip712_message_info) or the
[EIP712_MESSAGE_INFO](eip712.md#eip712_message_info) of the operation it is for: its tags tell which
kind of operation it is for, as for an ENUM_VALUE. For a typed message, it is looked up with the
domain separator and the primary type hash of the message.

| Name              | Tag  | Payload type | Description                                                   | Optional | Context      | Source / value                                             |
|-------------------|------|--------------|---------------------------------------------------------------|----------|--------------|------------------------------------------------------------|
| VERSION           | 0x00 | uint8        | struct version                                                |          | both         | constant: `0x1`                                            |
| CHAIN_ID          | 0x01 | uint64       | EIP-155 chain ID                                              |          | SignTx only  | `$.context.contract.deployments.[<deployment id>].chainId` |
| CONTRACT_ADDR     | 0x02 | uint8[20]    | EVM contract address                                          |          | SignTx only  | `$.context.contract.deployments.[<deployment id>].address` |
| SELECTOR          | 0x03 | uint8[4]     | function selector                                             |          | SignTx only  |                                                            |
| ID                | 0x04 | uint8        | map identifier (to differentiate multiple maps in a contract) |          | both         | `$.metadata.maps.<map id>`                                 |
| KEY               | 0x05 | uint8[]      | map key (raw bytes)                                           |          | both         | `$.metadata.maps.<map id>.<key>`                           |
| VALUE             | 0x06 | uint8[]      | map value (raw bytes)                                         |          | both         | `$.metadata.maps.<map id>.<key>.value`                     |
| DOMAIN_SEPARATOR  | 0x07 | uint8[32]    | `hashStruct(domain)`                                          |          | EIP-712 only | `$.context.eip712.domain`, with the deployment             |
| PRIMARY_TYPE_HASH | 0x08 | uint8[32]    | `keccak256(encodeType(primaryType))`                          |          | EIP-712 only |                                                            |
| SIGNATURE         | 0xff | uint8[]      | signature of all the other struct fields                      |          | both         | computed by CAL                                            |

> [!NOTE]
>
> - The device verifies the KEY matches the value resolved from the transaction context before using the VALUE.
>   If no matching MAP_ENTRY is found in the current context, the clear signing flow is aborted.
> - The KEY is compared byte for byte with the value it is looked up for, whose length counts. In a
>   transaction that value is a 32-byte word of the calldata. In a typed message it is the leaf
>   as the host sent it, which for an integer is its [minimal big-endian form](eip712.md#leaf-encoding) (`7` is `0x07`).
> - A selector is looked up for the call being described, and the contract is the callee of that
>   call, resolved through the proxy information if there is any. A primary type hash is looked up
>   for the message being described, with the domain separator of the message; no proxy is
>   involved.
> - The two kinds of key never match each other, whatever their bytes are.

### MAP_REF

Embedded as VALUE tag `0x06`. References a stored MAP_ENTRY by ID and key.

| Name    | Tag  | Payload type    | Description                            | Optional | Source / value                             |
|---------|------|-----------------|----------------------------------------|----------|--------------------------------------------|
| VERSION | 0x00 | uint8           | struct version                         |          | constant: `0x1`                            |
| ID      | 0x01 | uint8           | map identifier (references MAP_ENTRY)  |          | `map`, the path `$.metadata.maps.<map id>` |
| KEY     | 0x02 | [VALUE](#value) | key to look up in the stored MAP_ENTRY |          | `keyPath`                                  |

> [!NOTE]
> The KEY is a VALUE that specifies where to get the key from the operation being described.
> Common key sources: `CONTAINER_PATH` with `CHAIN_ID` for chain-dependent values, `DATA_PATH` for keys in
> calldata, or `EIP712_PATH` for keys in a typed message.
> Nested MAP_REF keys (MAP_REF within MAP_REF KEY) are not supported.
> The KEY, as the encoded `VALUE` struct, is at most 48 bytes: a long `EIP712_PATH` does not fit.

## PROXY_INFO

| Name            | Tag  | Payload type                           | Description                     | Optional |
|-----------------|------|----------------------------------------|---------------------------------|----------|
| STRUCT_TYPE     | 0x01 | uint8                                  | structure type                  |          |
| STRUCT_VERSION  | 0x02 | uint8                                  | structure version               |          |
| CHALLENGE       | 0x12 | uint32                                 | challenge to ensure freshness   |          |
| ADDRESS         | 0x22 | uint8[20]                              | proxy contract address          |          |
| CHAIN_ID        | 0x23 | uint64                                 | EVM chain identifier            |          |
| SELECTOR        | 0x41 | uint[4]                                | function selector               | x        |
| IMPL_ADDRESS    | 0x42 | uint8[20]                              | implementation contract address |          |
| DELEGATION_TYPE | 0x43 | [DelegationType](#delegationtype-enum) | type of delegation              |          |
| SIGNATURE       | 0x15 | uint8[]                                | signature of the structure      |          |

### DelegationType enum

| Name                 | Value |
|----------------------|-------|
| PROXY                | 0x00  |
| ISSUED_FROM_FACTORY  | 0x01  |
| DELEGATOR            | 0x02  |

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

| Name              | Tag  | Payload type | Description                                                | Value                         |
|-------------------|------|--------------|------------------------------------------------------------|-------------------------------|
| STRUCTURE_TYPE    | 0x01 | uint8        | Structure type                                             | `0x0D` (`TYPE_GATED_SIGNING`) |
| STRUCTURE_VERSION | 0x02 | uint8        | Structure version                                          | `0x01`                        |
| ADDRESS           | 0x22 | uint8[20]    | `To` (SignTx) or `verifyingContract` (EIP712 V1)           |                               |
| CHAIN_ID          | 0x23 | uint64       | Network chain ID                                           |                               |
| DOMAIN_SEPARATOR  | 0x28 | uint8[32]    | Domain separator (EIP712 V2)                               |                               |
| PRIMARY_TYPE_HASH | 0x2a | uint8[32]    | Hash of the primary type (EIP712 V2)                       |                               |
| SELECTOR          | 0x40 | uint8[4-28]  | SC function selector (SignTx) or `schema_hash` (EIP712 V1) |                               |
| INTRO_MSG         | 0x82 | char[100]    | Provider specific message                                  |                               |
| TINY_URL          | 0x83 | char[30]     | URL to access the full report                              |                               |
| TX_TYPE           | 0x84 | uint8        | Type of transaction                                        |                               |
| SIGNATURE         | 0x15 | uint8[]      | Signature of the structure                                 |                               |

The signature is computed on the full payload data, using `CX_CURVE_SECP256K1`.

A descriptor names one of these, with its tags and none of the others:

- A transaction: `ADDRESS` and `CHAIN_ID`, and `SELECTOR` if it is for one function only.
- A V1 message: `ADDRESS`, the `verifyingContract` of its domain, and `SELECTOR`, the `schema_hash`
  of its schema (SHA-224, 28 bytes, see [EIP712 FILTERING](apdu.md#eip712-filtering)). `CHAIN_ID` is
  optional: when it is absent, any chain matches.
- A V2 message: `DOMAIN_SEPARATOR`, `hashStruct(domain)`, and `PRIMARY_TYPE_HASH`,
  `keccak256(encodeType(primaryType))`, as in its [EIP712_MESSAGE_INFO](eip712.md#eip712_message_info).
  The device computes both from the message, whether it has an info or not, and compares all their
  bytes. The domain separator covers the chain and the contract of the domain, when it has them.

Gating applies to blind signing: a transaction with data, a V1 message in basic filtering mode and a
V2 message without field descriptors. The review of a described V2 message is not gated. When gating
applies, the device refuses to sign an operation that the descriptor it received does not name.

## DYNAMIC_TOKEN_DESCRIPTOR

Parsed by the SDK `tlv_use_case_dynamic_descriptor` helper.

| Name             | Tag  | Payload type          | Description                                                         |
|------------------|------|-----------------------|---------------------------------------------------------------------|
| STRUCTURE_TYPE   | 0x01 | uint8                 | Structure type, must be `0x90` (`TYPE_DYNAMIC_TOKEN`)               |
| VERSION          | 0x02 | uint8                 | Serialization format version, currently `0x01`                      |
| COIN_TYPE        | 0x03 | uint32                | SLIP-44 coin type (e.g. `60` for Ethereum)                          |
| APPLICATION_NAME | 0x04 | char[]                | Ledger application name this descriptor targets (e.g. `"Ethereum"`) |
| TICKER           | 0x05 | char[]                | Token ticker symbol (without `'\0'`)                                |
| MAGNITUDE        | 0x06 | uint8                 | Number of decimals                                                  |
| TUID             | 0x07 | [ETH_TUID](#eth_tuid) | Token Unique ID — application-specific sub-TLV                      |
| SIGNATURE        | 0x08 | uint8[]               | Signature of the SHA-256 hash of all fields except SIGNATURE        |

The signature is verified via the Ledger PKI with key usage `CERTIFICATE_PUBLIC_KEY_USAGE_COIN_META`.

### ETH_TUID

The TUID field contains a nested TLV payload with the following tags:

| Name     | Tag  | Payload type | Description             |
|----------|------|--------------|-------------------------|
| ADDRESS  | 0x22 | uint8[20]    | ERC-20 contract address |
| CHAIN_ID | 0x23 | uint64       | EVM chain ID            |

## EIP712_SCHEMA

Defines every struct type of the message, in one TLV payload. The schema holds the `EIP712Domain`
struct, and the device refuses two structs of the same name. A struct can refer to any struct of the
schema, itself included through an array.

The device does not resolve the references when it receives the schema: a missing `EIP712Domain`, or
a `STRUCT_NAME` that no struct of the schema carries, is refused with the values, see
[EIP712_VALUES](#eip712_values).

| Name          | Tag  | Payload type                               | Description                             | Optional |
|---------------|------|--------------------------------------------|-----------------------------------------|----------|
| VERSION       | 0x00 | uint8                                      | struct version, `0x01`                  |          |
| EIP712_STRUCT | 0x01 | [EIP712_STRUCT](#eip712_struct) (repeated) | one entry per struct type, at least one |          |

### EIP712_STRUCT

| Name         | Tag  | Payload type                             | Description                                                                           | Optional |
|--------------|------|------------------------------------------|---------------------------------------------------------------------------------------|----------|
| VERSION      | 0x00 | uint8                                    | struct version, `0x01`                                                                |          |
| NAME         | 0x01 | char[]                                   | struct name, see [Names](eip712.md#names). `EIP712Domain` is the struct of the domain |          |
| EIP712_FIELD | 0x02 | [EIP712_FIELD](#eip712_field) (repeated) | one entry per field, in declaration order, none for a struct without member           | x        |

### EIP712_FIELD

| Name        | Tag  | Payload type                     | Description                                                                                                                   | Optional |
|-------------|------|----------------------------------|-------------------------------------------------------------------------------------------------------------------------------|----------|
| VERSION     | 0x00 | uint8                            | struct version, `0x01`                                                                                                        |          |
| NAME        | 0x01 | char[]                           | field name, see [Names](eip712.md#names)                                                                                      |          |
| TYPE        | 0x02 | uint8 ([SolType](#soltype-enum)) | Solidity base type                                                                                                            |          |
| TYPE_SIZE   | 0x03 | uint8                            | size in bytes: required for `INT`, `UINT` and `BYTES_FIX` (1 to 32), refused for any other type                               | x        |
| ARRAY_DIM   | 0x04 | uint32                           | one entry per array dimension, innermost first: an empty payload is a dynamic dimension, a payload is the size of a fixed one | x        |
| STRUCT_NAME | 0x05 | char[]                           | name of the referenced struct: required for `STRUCT`, refused for any other type                                              | x        |

Array examples:

```text
address[]            ARRAY_DIM ()
OrderComponents[2]   ARRAY_DIM (2)
address[3][]         ARRAY_DIM (3), then ARRAY_DIM ()
```

A fixed dimension of size `0` (`T[0]`) carries an explicit zero byte: an entry with a payload is a
fixed dimension, an entry without one is a dynamic dimension.

```text
0400      T[]     dynamic, empty payload
040100    T[0]    fixed, size 0
040102    T[2]    fixed, size 2
```

Example, the `Person` struct of the Ether Mail message, `Person { string name; address[] wallets; }`.
In the examples of the EIP712_* structures a tag is one byte and a length is DER encoded: one byte below 128,
`81` and one byte from 128 to 255.

```text
01 2c                                    EIP712_STRUCT (44 bytes)
   00 01 01                              VERSION 1
   01 06 506572736f6e                    NAME "Person"
   02 0c                                 EIP712_FIELD (12 bytes)
      00 01 01                           VERSION 1
      01 04 6e616d65                     NAME "name"
      02 01 05                           TYPE STRING
   02 11                                 EIP712_FIELD (17 bytes)
      00 01 01                           VERSION 1
      01 07 77616c6c657473               NAME "wallets"
      02 01 03                           TYPE ADDRESS
      04 00                              ARRAY_DIM, dynamic
```

#### SolType enum

| Name       | Value |
|------------|-------|
| STRUCT     | 0x00  |
| INT        | 0x01  |
| UINT       | 0x02  |
| ADDRESS    | 0x03  |
| BOOL       | 0x04  |
| STRING     | 0x05  |
| BYTES_FIX  | 0x06  |
| BYTES_DYN  | 0x07  |

---

## EIP712_VALUES

Carries the values of the domain and of the message. The position of a value in the nested TLV
sequences is its address: the sequences follow the declaration order of the schema.

| Name            | Tag  | Payload type                          | Description                                                     | Optional |
|-----------------|------|---------------------------------------|-----------------------------------------------------------------|----------|
| VERSION         | 0x00 | uint8                                 | struct version, `0x01`                                          |          |
| PRIMARY_TYPE    | 0x01 | char[]                                | name of the struct type of the message                          |          |
| DERIVATION_PATH | 0x02 | uint32[]                              | BIP-32 path of the signing account: 1 to 10 entries, big endian |          |
| EIP712_DOMAIN   | 0x03 | [EIP712_VALUE_SEQ](#eip712_value_seq) | values of the domain                                            |          |
| EIP712_MESSAGE  | 0x04 | [EIP712_VALUE_SEQ](#eip712_value_seq) | values of the message                                           |          |

`PRIMARY_TYPE` comes before `EIP712_MESSAGE`, whose root sequence is an instance of that type, and
it has to be the whole name of a struct of the schema. `EIP712_DOMAIN` is an instance of
`EIP712Domain` and has no ordering constraint.

The device refuses the values when the schema has no `EIP712Domain`, and when a sequence reaches a
struct that the schema does not define.

### EIP712_VALUE_SEQ

An ordered sequence of values. It stands for a struct instance, with one entry per field in the
order of the schema, or for an array instance, with one entry per element. The position of an entry
is its address: no field index, element index or count is sent.

| Name | Tag  | Payload type                          | Description                                                                                         | Optional |
|------|------|---------------------------------------|-----------------------------------------------------------------------------------------------------|----------|
| LEAF | 0x00 | uint8[]                               | value bytes, when the target's type is scalar, as given in [Leaf encoding](eip712.md#leaf-encoding) | —        |
| SEQ  | 0x01 | [EIP712_VALUE_SEQ](#eip712_value_seq) | nested sequence: an array dimension or a struct instance                                            | —        |

The schema tells whether a `SEQ` is an array dimension or a struct instance, and the wire does not
say. It is an array dimension while dimensions of the field remain to be opened, and a struct
instance once they are all open and the type of the field is `STRUCT`.

A `LEAF` carries the bytes of a value and a `SEQ` carries a nested TLV sequence, so the wire tells
them apart. The device refuses a `SEQ` where the schema has a scalar, and a `LEAF` where it has an
array or a struct. The device builds the hash and the review from the same tree, so the message
shown is the message signed.

The element count of an array is the number of entries of its sequence. A fixed dimension needs
the size the schema gives it. A dynamic dimension takes any count, including zero. Every array
instance has its own entries, so the inner arrays of a `T[][]` can differ in length. A struct
sequence needs one entry per field: the device refuses missing entries and extra ones.

Example, the `from` of the Ether Mail message, a `Person` (`string name; address[] wallets;`) with
two wallets:

```text
01 33                                                  SEQ, the Person instance (51 bytes)
   00 03 436f77                                        LEAF "Cow", name
   01 2c                                               SEQ, wallets: one dimension (44 bytes)
      00 14 cd2a3d9f938e13cd947ec05abc7fe734df8dd826   LEAF address
      00 14 deadbeefdeadbeefdeadbeefdeadbeefdeadbeef   LEAF address
```

Example, `uint8[2][2]` holding `[[1, 2], [3, 4]]`:

```text
SEQ {                           // outer dimension
    SEQ { LEAF 1  LEAF 2 }      // inner dimension
    SEQ { LEAF 3  LEAF 4 }
}
```

## EIP712_FIELD_DESCRIPTOR

A descriptor is a [FIELD](#field) structure, with all its `PARAM_*` variants. Its `VERSION` is
`0x01`. A `FIELD` has no signature of its own: the signature of the `EIP712_MESSAGE_INFO` covers it
through `FIELDS_HASH`, which attests the order and the completeness of the descriptors. Several
descriptors can select the same value, for example once raw and once formatted.

The `VALUE` of a descriptor locates its value with an [EIP712_PATH](#eip712_path), which is a
location in the value tree of the message. It takes tag `0x03`, the tag of the `DATA_PATH` of a
transaction. `CONTAINER_PATH`, `CONSTANT` and `MAP_REF` are available with the meaning given in
[VALUE](#value).

### EIP712_PATH

Locates values in the value tree of the message.

| Name                | Tag  | Payload type                                         | Description                                                   | Optional |
|---------------------|------|------------------------------------------------------|---------------------------------------------------------------|----------|
| VERSION             | 0x00 | uint8                                                | struct version, `0x01`                                        |          |
| EIP712_STRUCT_FIELD | 0x01 | uint8 (repeated)                                     | index of the field in its struct, in declaration order from 0 | —        |
| EIP712_ARRAY_SLICE  | 0x02 | [EIP712_ARRAY_SLICE](#eip712_array_slice) (repeated) | array segment to match at this depth                          | —        |
| EIP712_LEAF_SLICE   | 0x03 | [EIP712_LEAF_SLICE](#eip712_leaf_slice)              | bytes of the leaf to keep                                     | x        |

A path walks the value tree of the message from its root, which is a struct, so it begins with an
`EIP712_STRUCT_FIELD`. A path cannot reach the domain: a descriptor reads it through
`CONTAINER_PATH`, see [VALUE](#value).

`EIP712_STRUCT_FIELD` and `EIP712_ARRAY_SLICE` entries are interleaved, in the order the path goes
down the tree. An `EIP712_LEAF_SLICE` comes last, at most once.

> [!IMPORTANT]
> The three tags are the steps of one ordered sequence. The tag gives the kind of step: go into a
> struct field, select an array segment, or keep bytes of the leaf. The order of reception is the
> order of descent, so a parser keeps the entries in one sequence. Collecting them per tag loses
> the interleaving, and the paths `field/slice/field` and `field/field/slice` become the same.

Example, `orders[2].items[0..3).price`, where `orders` is field 1 of the root struct, `items` is
field 2 of `Order`, and `price` is field 0 of `Item`:

```text
EIP712_PATH {
    EIP712_STRUCT_FIELD  1                       // orders
    EIP712_ARRAY_SLICE   { START 2, END 3 }      // [2]
    EIP712_STRUCT_FIELD  2                       // items
    EIP712_ARRAY_SLICE   { START 0, END 3 }      // [0..3)
    EIP712_STRUCT_FIELD  0                       // price
}
```

A path has to lead to values. The device refuses a path that:

- has a version other than `0x01`, or does not begin with an `EIP712_STRUCT_FIELD`;
- does not follow the tree: an `EIP712_STRUCT_FIELD` applies to a struct and names a field it has,
  an `EIP712_ARRAY_SLICE` applies to an array;
- does not end on a leaf. A path that ends on a struct, or on an array it did not go through in all
  its dimensions, leads to no value. Each dimension of an array takes one `EIP712_ARRAY_SLICE`,
  `{}` (wildcard) included.

A wildcard or a range in an `EIP712_ARRAY_SLICE` makes one path match several leaves, and one
descriptor formats all of them, as a `DATA_PATH` with an `ARRAY_ELEMENT` does. A path selects at
most 16 values, in the order of the message, and the device refuses a path that selects more. A
path that selects none, because an array it goes through is empty or a slice selects nothing, is
valid and shows nothing. The steps that follow an empty selection are not checked, as there is no
element to check them against.

### EIP712_ARRAY_SLICE

| Name  | Tag  | Payload type | Description             | Optional                               |
|-------|------|--------------|-------------------------|----------------------------------------|
| START | 0x01 | int16        | start index (inclusive) | x, defaults to `0`                     |
| END   | 0x02 | int16        | end index (exclusive)   | x, defaults to the length of the array |

Absent `START` and `END` (`{}`) is a wildcard: it matches every element.

`START` and `END` are two-byte big-endian signed integers. The device refuses a bound of another
length.

The device resolves a slice with `array_size`, the length of the array in the value tree.
`EIP712_VALUES` is complete before any descriptor is received, so the length is known.

1. A negative `START` gives `start = array_size + START`, so `-1` is the last element. A
   non-negative `START` gives `start = START`. An absent `START` gives `start = 0`.
2. `END` follows the same rule, with `end = array_size` when it is absent.
3. The device refuses the slice when `start < 0` or `end < 0`: it reaches before the array.
4. The device refuses the slice when `end > array_size`: it reaches past the array.
5. Otherwise the slice matches the elements `[start, end)`, and none when `end <= start`.

Whether an array has elements depends on the message, so an empty selection is valid: an empty or
inverted range, and `start >= array_size` as long as `end` is not past the array.

The device refuses a descriptor that holds an invalid slice.

### EIP712_LEAF_SLICE

Keeps some of the bytes of the leaf at the end of the path, as `SLICE_ELEMENT` does for a
`DATA_PATH`.

| Name  | Tag  | Payload type | Description                       | Optional                              |
|-------|------|--------------|-----------------------------------|---------------------------------------|
| START | 0x01 | int16        | first byte kept (inclusive)       | x, defaults to `0`                    |
| END   | 0x02 | int16        | end of the bytes kept (exclusive) | x, defaults to the length of the leaf |

The bounds are encoded as in an [EIP712_ARRAY_SLICE](#eip712_array_slice), and a negative bound
counts back from the end of the leaf. The leaf is of type `bytesN`, `bytes` or `string`. The slice
is inside the leaf and keeps at least one byte: the device refuses an empty selection.
