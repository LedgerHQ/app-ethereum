# Ethereum app fuzzing

Absolution-based, coverage-guided fuzzing for the Ethereum app, built on the
Ledger SDK fuzzing framework.

**Read the framework documentation first.** Concepts (what a corpus is, how the
`[ prefix | tail ]` input works, the manifest, invariants, mocks, harnesses,
the CMake API, CI, and how to maintain it) live in the **Fuzzing Framework**
page of the SDK documentation, published at
<https://ledgerhq.github.io/ledger-secure-sdk/>. This file only documents what
is specific to the Ethereum app.

## Quickstart

```bash
export BOLOS_SDK=/path/to/ledger-secure-sdk
"$BOLOS_SDK"/fuzzing/scripts/app-campaign.sh \
    --app-dir "$(pwd)" --fuzz-subdir tests/fuzzing my-campaign
```

`--fuzz-subdir tests/fuzzing` is required: this app keeps its fuzzing tree under
`tests/fuzzing/` (the SDK default is `fuzzing/`). Pass a run name (or omit it for
a timestamp); outputs land in `.fuzz-artifacts/<name>/`.

## Targets

| Target | What it drives |
|--------|----------------|
| `fuzz_app`    | A sequence of fuzzed APDUs through the real `apdu_parser()` → `handleApdu()` path |
| `fuzz_plugin` | One internal plugin through the production `eth_plugin_call()` sequence |
| `fuzz_parser` | The generic tx parser, EIP-712 and the calldata store, at their own entry points |

`fuzz_plugin` and `fuzz_parser` exist because the APDU path cannot reach that
code: the internal plugins are called through the plugin interface rather than
an INS, and the generic tx parser runs from a descriptor the dispatcher never
builds. The EIP-712 handlers `fuzz_parser` also drives are reachable from
`fuzz_app`; it reaches them without spending budget on APDU framing. Both still go
through the framework contract, so they get the prefix-aware mutator and the
lane split for free; control byte 1 picks the plugin or parser.

## Why one input is a sequence

`fuzz_app` and `fuzz_parser` replay a *sequence* of steps per input, not one.
Twelve commands accumulate their TLV descriptor across APDUs through
`tlv_from_apdu()`, which only runs the payload handler once the descriptor is
complete, and an APDU carries at most 253 bytes of TLV after the two-byte length
header — so a single dispatch can never finish a descriptor that carries a
signature. The same holds inside EIP-712: `handle_eip712_v1_filtering()` returns
before doing anything unless a prior `P2_FILT_ACTIVATE` call switched the mode,
and a type is built by several struct-def calls.

The fuzzer picks every field of every step, including how many steps there are.
The only structure the harness adds is the protocol's own framing.

`fuzz-manifest.toml` is the authoritative list of targets, seeds and dictionary.

## Where values come from

The harnesses own **structure** and never author **content**. `mock/plugin_model.c`
decides that a `txInt256_t` length must fit its array and that a ticker stays
NUL-terminated; every byte inside those fields, plus every ABI word, address,
selector and digest, comes from the fuzzer.

`fuzz_plugin` reads its shaping bytes from a fixed-size header
(`eth_plugin_header_t`, declared to the framework as `FUZZ_APP_HEADER_LEN`), so
the ABI words after it always start at `fuzz_tail_ptr[0]`. Drawing those bytes
sequentially instead would shift the calldata every time a length changed, and
the fuzzer would keep losing the input it had built. `fuzz_app` and `fuzz_parser`
read sequentially through the cursor in `mock/fuzz_input.h`.

`fuzz_app` adds one grammar-aware mutation on top of the framework's, applied to
the sequence's *last* step so a descriptor changing size cannot disturb the steps
built before it. It runs on half the mutations; the rest are generic, which is
what produces malformed framing.

Constants the app keeps private — the ERC-721 and ERC-1155 selector tables —
live in the manifest dictionary rather than as a second copy in C, so the
harness cannot drift from the plugin. Where the app already exports a table
(`ERC20_SELECTORS`, `ETH2_ADDRESSES`, …) the harness indexes it directly.

## App-owned files in this tree

Everything the SDK framework needs from the app lives here; the framework itself
is in the SDK and is not duplicated:

```text
tests/fuzzing/
  fuzz-manifest.toml   targets, seeds, dictionary, coverage key files
  base-corpus.zip      promoted fuzz_app corpus (+ base-corpus.compat-key sidecar)
  harness/             one fuzz_*.c per target
  mock/                app globals, engine stubs, input cursor, model builders
  invariants/          zero-symbols.txt, domain-overrides.txt
  macros/              add_macros.txt / exclude_macros.txt
```

A compat key names the fuzzer it was promoted from, so the single tracked
`base-corpus.zip` seeds `fuzz_app` only; `fuzz_parser` and `fuzz_plugin` report
it as incompatible and start from generated seeds. Promote with
`corpus.py promote .fuzz-artifacts/<run>/targets/fuzz_app/corpus
tests/fuzzing/base-corpus.zip` after any change to the prefix layout, the SDK
version or `harness_version`.

`.clusterfuzzlite/build.sh` is a thin wrapper that delegates to the SDK's shared
`fuzzing/scripts/cfl-build.sh`; the CFL Dockerfile copies the SDK from the
Ledger app development image.
