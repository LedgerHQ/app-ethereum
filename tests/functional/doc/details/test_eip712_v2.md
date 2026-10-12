# `test_eip712_v2*.py` - Test Details

## Overview

These three files test the **EIP-712 V2 protocol**. The host sends a typed message to the device as
TLV structures: a schema, then the values, and optionally a signed message info and field
descriptors. The device computes the hashes itself and reviews the message before it signs.

The structures are described in [tlv_structs.md](../../../../doc/tlv_structs.md#eip712_schema) and the
commands in [apdu.md](../../../../doc/apdu.md).

| File                             | Tests | Covers                                                          |
|----------------------------------|-------|-----------------------------------------------------------------|
| `test_eip712_v2.py`              | 183   | Schema, values, signing, the raw review                         |
| `test_eip712_v2_message_info.py` | 50    | `EIP712_MESSAGE_INFO`                                           |
| `test_eip712_v2_fields.py`       | 129   | Field descriptors, the described review, calls, enums and maps  |

The V2 tests of `test_gating.py` are listed at the end.

## How the tests check

- **Signature:** the address that signed is recovered from a hash that `eth_account` computes
  independently from the same JSON. It only matches if the schema parsing, the value tree and the
  hashing of the device agree with the EIP-712 standard.
- **Screens:** each kind of review is compared with snapshots once (14 reviews). The other tests
  do not read the screens, so that they run on a device too: they go through the review and check
  the signature.
- **Refusals:** every refusal resets the signing flow, so each one is followed by a valid message
  that goes through. A test that leaves a message loaded resets the app at its end, so the memory
  profiling finds nothing allocated.
- **Payloads:** they are built by the `eip712` package of the Python client (`json_to_tlv`, schema,
  values) and by `gcs.py` (message info, descriptors).

## test_eip712_v2.py

### Signing

- Every `*-data.json` message of the V1 corpus is signed and its signer recovered.
- The BIP 32 path of the values selects the signing account.
- A raw message is blind signing: it is refused without the "Blind signing" or the "Raw messages"
  setting, and the "Raw messages" setting alone is enough.
- The raw review, compared with snapshots, shows the signer, then every value of the domain and of
  the message, more than the device formats at once. With the "Transaction hash" setting, it ends
  with the hash of the domain and the hash of the message. The two rows are rows of the review: a
  message a row short of the limit is refused when they are shown.
- The review has at most 255 rows, 252 on a Nano, the signer's included: a message of that many rows
  is signed, and one of a row more is refused before the review.
- A message whose review text would not fit in memory is refused before the review: 8 array
  elements that share a field name of 7,936 characters, each with a string of 255.
- A string with a NUL byte, in the domain or in the message, is refused at `EIP712 SIGN`.
- Rejecting the review returns no signature.

### The schema

- 58 malformed schemas are refused: versions, missing, duplicate or unknown tags, duplicate struct
  and field names, the rules of `TYPE_SIZE`, `STRUCT_NAME` and the array dimensions, and the names
  the device refuses.
- A schema without `EIP712Domain`, a reference to a struct that does not exist (with a value of it,
  or in an empty array), a truncated schema, a second schema, a chunk after a complete schema and a
  following chunk without a first one are refused. The first two are refused when the values arrive.
- A struct that holds an array of its own type is signed and recovered.
- A struct without member, an empty `EIP712Domain` or the type of a field, is signed and recovered.

### The values

- 24 malformed value payloads are refused: versions, an unknown tag, a primary type that is
  unknown, empty, a prefix
  or an extension of a struct name, or holds a NUL, an `EIP712_MESSAGE` before `PRIMARY_TYPE`,
  missing tags, a derivation path that is empty, unaligned, too long or whose count wraps a byte,
  missing and extra values, and a sequence where the schema has a scalar.
- Values without a schema, values twice, truncated and oversized payloads are refused.
- Fixed array cardinality, the value tree depth and the derivation path length are checked at their
  limits.

### Leaf encoding

- 20 leaves that are not the one encoding of a value of their type are refused: leading zeros, too
  long, too short, empty.
- Values at the edge of their types are signed and recovered.
- Hex values written shorter in the JSON, a `bytesN` value shorter than its type or an odd number of
  digits, are encoded by the client as the standard does, and signed and recovered.

### Names

- Struct names that are no identifier (`HyperliquidTransaction:UsdSend`) and field names with
  spaces, brackets, dots, quotes and backslashes are accepted and signed.
- Struct names of 200 and 2000 bytes are signed. A name larger than the heap is refused, and the
  next message goes through.
- A name the review cannot show is refused at `EIP712 SIGN` when the value is displayed. The same
  name is accepted when it is not displayed.

### The sign command

- `EIP712 SIGN` is refused without values, a second time, with data, and with an unknown `P1`.

### V1 and V2 sessions

- A V1 command (a struct definition, a struct implementation, the filtering activation or the
  V1 sign) is refused once a V2 schema is received, with and without the values, and the next
  message goes through.
- The reverse, a V2 schema, message info, values, descriptor or sign during a V1 session, is covered
  in `test_eip712_v2_message_info.py` and `test_eip712_v2_fields.py`.

## test_eip712_v2_message_info.py

- **Accepted:** a message info for a domain with a chain and a contract, a chain only, a contract
  only or neither, a domain `chainId` of `uint8` and `uint64`, or of text, which is no chain, a
  domain that names its contract with the string `cosmos`, and the optional metadata tags. A
  message info is optional, can span several chunks, and announces descriptors that are kept.
- **Refused (26 cases):** a domain separator of another domain name, of a domain with another
  field, another chain or contract, or none, or of another size, a primary type hash of another
  type or of another size, a bad signature, other versions, a `TRANSACTION_INFO`, a chain, a
  contract or a selector, missing required tags, an unknown tag and a tag twice.
- **The command:** other `P1` and `P2`, a message info without a message, a second one, one that
  does not outlive its flow, and the schema, the message info, the values and `EIP712 SIGN` during
  a V1 session.

## test_eip712_v2_fields.py

### Reviews

- A message described by descriptors is reviewed without the blind signing warning, whatever the
  "Blind signing" setting.
- The review shows the contract information, the signer, the descriptors in the order they were
  sent and the network when it is not the one of the app. It can be rejected.
- The row about the contract names the creator, or reads "a smart contract" when the info has no
  creator. Messages with and without a contract, a creator or other information about the
  contract are signed (5 cases).
- 40 described messages are signed in a row: each review gives back the memory it took.
- With the "Transaction hash" setting, the review ends with the hash of the domain and the hash of
  the message, and shows none without it.
- A permit, the message of the V1 advanced filtering test, is described by a token amount in the
  token of the contract and a datetime, with an intent and a creator in the message info. The
  amount shows in the token the app knows (`4.2 ENS`), or as an integer followed by `???` when it
  knows none.

### What the descriptors show

- `PARAM_RAW` of several types, a `CONSTANT`, `PARAM_AMOUNT`, `PARAM_DATETIME`, `PARAM_DURATION`,
  `PARAM_UNIT`, `PARAM_NETWORK` and a native token amount, in one review, then a group holding a
  group.
- Elements of an array, a field of a struct and slices of a packed `bytes` value, in one review
  where a hidden value and a value with no descriptor are not shown. An empty array and the
  `IF_NOT_IN` and `MUST_BE` visibility (3 cases) are signed.
- `ENUM_VALUE` and `MAP_ENTRY` of the message, with the key as the host sent it. A card with a
  chain, a contract or a selector, or without its domain separator or its type hash, is refused
  (10 cases).
- `CONTAINER_PATH` values: the signer, the contract and the chain of the domain, and the refusals
  when there is none.
- A domain without a chain: `PARAM_AMOUNT` and a token amount are refused, an enum and a map entry
  are not. A domain without a contract: none of them is refused.

### Which values are selected

- Array slices (7 cases), wildcards, 16 matches kept and 17 refused. The steps after an empty
  selection are not checked, while those after an element are.
- 16 malformed paths are refused: a version other than `0x01`, a path with no step, one that starts
  with an array step or a leaf slice, a step after the leaf slice, a field index out of range, a
  field step on a leaf or an array, an array step on a leaf, a path that ends on an array or on a
  struct, a leaf slice on a number, and slices that reach outside an array or a leaf.
- A `DATA_PATH` and a selector in a message are refused.
- 6 malformed `VALUE` structures are refused: no source, a path with a container path or a
  constant, a container path with a constant, and no `VERSION` or no `TYPE_FAMILY`.
- A `PARAM` or a `VALUE` whose `VERSION` is not `0x01` is refused (4 cases).

### Calls

- A `PARAM_CALLDATA` names a call, which is described as a transaction right after the descriptor or
  after all the descriptors of the message. The calls are numbered when there is more than one, and
  one with no calldata, which only sends an amount, is described by the app.
- The descriptors of a call can show its sender and its chain, the signer and the chain of the
  message.
- The `MAP_ENTRY` of a call is keyed by its selector, before or after its info.
- Refused: a call without its info, with the info of another call, half described, missing, and
  with other or more descriptors than announced.

### The command

- Descriptors are refused when none are announced, with no message or no message info, during a V1
  session, after the signature, while a call is described, and with an unknown `P1` or `P2`.
- Descriptors that are not the announced ones (7 cases) are refused at `EIP712 SIGN`, whatever the
  blind signing setting.

## test_gating.py

- A gating descriptor for a V2 message holds its domain separator and the hash of its primary type.
  The raw review shows the blind signing warning and the gating warning. A domain without a
  `verifyingContract` is gated too.
- 9 descriptors of another message are refused at `EIP712 SIGN`: another domain name, chain or
  contract, another type, flipped bits, the two hashes swapped, and the descriptor of a V1 message.
- A V2 descriptor that also has an address, a chain ID or a selector is refused.

## Related Documentation

- [Main Test Overview](../test_overview.md#test_eip712_v2py)
- [Quick Reference](../quick_reference.md)
- [test_eip712.py details](test_eip712.md) - the V1 protocol
- Spec: [tlv_structs.md](../../../../doc/tlv_structs.md#eip712_schema)
