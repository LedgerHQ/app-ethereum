# test_gcs_formatters.py

## Overview

GCS formatter-focused tests covering parameter types, iteration behaviour, separators,
display constraints and other formatter-level features.

These tests exercise the GCS engine *transversal* features (how the value is rendered
on screen) rather than full smart-contract use-cases. They were extracted from
`test_gcs.py` to keep the formatter coverage in a dedicated file.

## Test Context

GCS supports several "formatter" tags applied to fields, which alter how the parsed
value is presented to the user:

- **MAP_REF / MAP_ENTRY**: Lookup a value in a previously provided table to display a
  human-readable label instead of the raw value
- **PARAM_GROUP**: Group several sub-fields and iterate over them (e.g. one row per
  array element, with several columns)
- **SEPARATOR**: Insert a visible separator between repeated/iterated entries
- **CONSTRAINTS**: Limit when/how a field is shown (e.g. hide when zero)
- **Interoperable address (EIP-7930)**: Display chain-aware addresses
- **Iteration broadcast (§3.1.8)**: Repeat a single secondary value across the iteration
  of a primary array

## Functions Tested

### test_gcs_map_entry

**Purpose**: Test the `MAP_ENTRY` feature.

Maps a calldata `uint256` (`eventId`) to a human-readable event name through a
previously provided map table. Uses the POAP `mintToken` calldata as carrier.

**Contract**: PoapBridge `0x0bb4D3e88243F4A057Db77341e6916B0e449b158`

### test_gcs_map_entry_chain_id_key

**Purpose**: Test `MAP_ENTRY` with `ContainerPath.CHAIN_ID` as key.

Maps the transaction's `chain_id` to a network name, instead of relying on a calldata
field.

### test_gcs_group_sequential

**Purpose**: Test `PARAM_GROUP` with `SEQUENTIAL` iteration over scalar sub-fields.

Verifies the group/iteration mechanism when the sub-fields are scalars (one row per
iteration, several scalar columns).

### test_gcs_group_sequential_arrays

**Purpose**: Test `PARAM_GROUP` with `SEQUENTIAL` iteration over two array sub-fields.

Verifies the group/iteration mechanism when the sub-fields are themselves arrays —
each iteration step pulls one element from each array in lockstep.

### test_gcs_separator

**Purpose**: Test the field-level `SEPARATOR` tag on an array field.

Verifies that visible separators are inserted between repeated entries when the
descriptor requests it.

### test_gcs_formatter

**Purpose**: Test the various GCS formatters.

Parametrized test (`test_config`) covering different display formats (datetime,
token amount, enum, …). Originally located in `test_gcs.py`.

### test_gcs_constraints

**Purpose**: Test field-level constraints.

Verifies that GCS can enforce constraints on field values (e.g. equality, range,
visibility conditions). Originally located in `test_gcs.py`.

### test_gcs_interoperable_address

**Purpose**: Test EIP-7930 *Interoperable Addresses* rendering.

Parametrized test (`test_config`) verifying the formatter for chain-aware addresses
(address + chain identifier rendered together).

**Reference**: [EIP-7930](https://eips.ethereum.org/EIPS/eip-7930)

### test_gcs_iteration_broadcast

**Purpose**: Test §3.1.8 iteration broadcast.

When the primary iterated array has size N and the secondary array has size 1, the
single secondary value must be *broadcast* (repeated) across the N iterations.

## Coverage Summary

| Feature                       | Coverage                                              |
|:------------------------------|:------------------------------------------------------|
| MAP_REF / MAP_ENTRY           | Calldata key, ContainerPath.CHAIN_ID key              |
| PARAM_GROUP / iteration       | Sequential (scalars), sequential (arrays), broadcast  |
| Separator                     | Field-level separator on arrays                       |
| Formatters                    | Datetime, token amount, enum, …                       |
| Constraints                   | Equality, range, visibility                           |
| EIP-7930 interoperable addr   | Address + chain rendering                             |

## Related Documentation

- [Test Overview](../test_overview.md)
- [test_gcs.md](test_gcs.md) — Use-case-oriented GCS tests
- [GCS Documentation](../../doc/gcs.md)
- [EIP-7930 — Interoperable Addresses](https://eips.ethereum.org/EIPS/eip-7930)
