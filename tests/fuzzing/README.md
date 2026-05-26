# Fuzzing Tests

Fuzzing allows us to test how a program behaves when provided with invalid,
unexpected, or random data as input. If the application crashes, or a
[sanitizer](https://github.com/google/sanitizers) detects any kind of access
violation, the fuzzing process stops, prints a report, and writes the input
that triggered the bug to disk as `crash-*`. That file can be replayed by
passing it to the fuzz binary to triage the issue.

This directory builds **two flavours of fuzz binary**, both produced by a
single `cmake` configure of `tests/fuzzing/CMakeLists.txt`:

| Flavour                  | Binaries                                          | Entry shape                                         | Driven by                                  |
|--------------------------|---------------------------------------------------|-----------------------------------------------------|--------------------------------------------|
| Per-feature harnesses    | 18× `fuzz_calldata`, `fuzz_eip712`, …             | one `LLVMFuzzerTestOneInput` per `harness/fuzz_*.c` | plain libFuzzer                            |
| Global-coverage dispatcher | 1× `fuzz_app`                                     | `fuzz_entry()` calling `apdu_parser()` and the real dispatch path | SDK's Absolution-based dispatcher (grammar-aware mutator + invariant) |

The two flavours share the same `CMakeLists.txt`, the same `mock/`
directory, the same common app library (`eth_fuzz_common`) and the same
defines. They differ only in which entry source file each executable links
and in the SDK helper used to register the dispatcher target.

When to use which:

- **Per-feature harnesses**: targeted fuzzing of a single parser or feature
  when you want to write the input bytes directly. Fast to iterate, no
  framework state required.
- **Global-coverage dispatcher**: full-app coverage through the real APDU
  parser. Restores app-wide global state from the fuzz prefix before each
  iteration and runs one APDU through the same dispatch path production
  uses. The recommended target for ClusterFuzzLite and long campaigns.

Both flavours are kept because they cover complementary shapes. The
per-feature harnesses reach raw input formats (RLP, EIP-712 struct
definitions, etc.) that the dispatcher does not, and they are convenient
for local triage. The dispatcher exercises the cross-INS state transitions
the per-feature harnesses cannot construct.

## Prerequisites

- `BOLOS_SDK` set to a checkout of `ledger-secure-sdk` that contains the
  `fuzzing/` framework (`fuzzing/cmake/LedgerAppFuzz.cmake`,
  `fuzzing/scripts/app-campaign.sh`, `fuzzing/template/`).
- Clang ≥ 14 (19 preferred), `cmake` ≥ 3.14, `ninja`, `pkg-config`,
  `libbsd-dev`, `zip`.
- First configure fetches the pinned Absolution Linux release from GitHub.
  Set `LEDGER_FUZZ_ABSOLUTION_LOCAL_DIR=/path/to/absolution` to skip the
  download.

## Primary workflow: `app-campaign.sh`

The SDK ships an end-to-end campaign script that builds the dispatcher
(`fuzz_app`), syncs the Absolution invariant, regenerates
`mock/scenario_layout.h`, generates seeds, runs warmup + main fuzzing,
and produces a coverage report. From the workspace root:

```bash
BOLOS_SDK="$(pwd)/ledger-secure-sdk" \
    "$BOLOS_SDK"/fuzzing/scripts/app-campaign.sh \
    --app-dir "$(pwd)/app-ethereum" \
    --fuzz-subdir tests/fuzzing \
    ethereum-smoke
```

- `ethereum-smoke` is the run name; the script writes outputs under
  `.fuzz-artifacts/ethereum-smoke/`. Omit it to default to a UTC
  timestamp.
- `--fuzz-subdir tests/fuzzing` is required because this app keeps fuzzing
  under `tests/fuzzing/` (the SDK default is `fuzzing/`).
- `BASE_CORPUS_DIR` defaults to `tests/fuzzing/base-corpus/` when the
  directory exists and its `.compat-key` matches the current build;
  `BASE_CORPUS_DIR=` skips it.

Useful environment overrides:

| Variable                | Default          | Meaning                                                  |
|-------------------------|------------------|----------------------------------------------------------|
| `WARMUP_SEC`            | `30`             | Warmup seconds per worker                                |
| `MAIN_SEC`              | `60`             | Main phase seconds per worker                            |
| `WORKERS`               | `min(2, nproc)`  | Parallel libFuzzer workers                               |
| `EXTRA_CORPUS`          | unset            | Colon-separated extra corpus dirs merged into bootstrap  |
| `BUILD_JOBS`            | CPU-based        | Parallel compile jobs                                    |
| `OVERWRITE=1`           | unset            | Replace an existing `.fuzz-artifacts/<name>/`            |
| `BOOTSTRAP_INVARIANT`   | `auto`           | `1` to always reset `fuzz_globals.zon` to `.{}` first    |
| `--clean`               | off              | Wipe build dirs before configure                         |

### Promoting a corpus into `base-corpus/`

After a long campaign the merged minset is the most valuable artifact.
Promote it so that future runs (and CFL) start from the larger seed set
instead of regenerating from scratch:

```bash
# 1. Copy the merged minset from the campaign output
cp -a .fuzz-artifacts/<run-name>/coverage/minset/. tests/fuzzing/base-corpus/

# 2. Stamp the compat-key for the same build configuration that produced it
prefix_size="$(python3 "$BOLOS_SDK"/fuzzing/scripts/fuzz_manifest.py \
    --prefix-size tests/fuzzing/fuzz-manifest.toml)"
python3 "$BOLOS_SDK"/fuzzing/scripts/fuzz_manifest.py \
    --compat-key tests/fuzzing/fuzz-manifest.toml \
    --prefix-size "${prefix_size}" \
    --invariant tests/fuzzing/invariants/fuzz_globals.zon \
    > tests/fuzzing/base-corpus/.compat-key
```

The `.compat-key` ties the corpus to the build it was produced with;
`app-campaign.sh` and `.clusterfuzzlite/build.sh` both refuse to merge a
base corpus when the keys disagree (a sanitizer change is enough to flip
it). Regenerate after any change that affects the prefix size or the
invariant.

### Known findings surfaced by the current corpus

A ~20-minute UBSan campaign (120s warmup + 900s main, 4 workers,
213 bootstrap seeds → 1858-file merged minset, `cov: 6121 ft: 14285`,
`0 crash file(s)` / `0 replay failures`) currently surfaces **14
distinct sanitizer reports**, all in production code. The merge step
drops the offending inputs from the promoted corpus, so the campaign
exits 0 and the corpus stays promotable. Refresh this table after
fixing app code or expanding seeds.

| File:line                                                          | Sanitizer check                          |
|--------------------------------------------------------------------|------------------------------------------|
| `src/features/generic_tx_parser/gtp_data_path.c:212`               | implicit-signed-integer-truncation (slice variant) |
| `src/features/generic_tx_parser/gtp_data_path.c:218`               | implicit-signed-integer-truncation       |
| `src/features/generic_tx_parser/gtp_data_path.c:265`               | implicit-signed-integer-truncation       |
| `src/features/generic_tx_parser/gtp_data_path.c:271`               | implicit-signed-integer-truncation       |
| `src/features/generic_tx_parser/gtp_path_array.c:27`               | implicit-integer-sign-change             |
| `src/features/generic_tx_parser/gtp_path_array.c:36`               | implicit-integer-sign-change             |
| `src/features/generic_tx_parser/gtp_path_slice.c:16`               | implicit-integer-sign-change             |
| `src/features/generic_tx_parser/gtp_path_slice.c:26`               | implicit-integer-sign-change             |
| `src/features/generic_tx_parser/gtp_param_datetime.c:67`           | implicit-integer-sign-change             |
| `src/features/generic_tx_parser/gtp_param_duration.c:61`           | implicit-unsigned-integer-truncation     |
| `src/features/get_public_key/get_public_key.c:23`                  | implicit-unsigned-integer-truncation     |
| `src/features/get_public_key/get_public_key.c:40`                  | implicit-unsigned-integer-truncation     |
| `src/features/perform_privacy_operation/cmd_perform_privacy_operation.c:123` | implicit-unsigned-integer-truncation |
| `ethereum-plugin-sdk/src/common_utils.c:96`                        | invalid-null-argument                    |

The GTP cluster is the largest: `path_array()` / `path_slice()` assign
`(int32_t) size + signed_offset` into a `uint16_t` without bounds
checks, and the dispatcher reaches that code by mutating the
`start`/`end` fields of array/slice GTP-path TLVs into a negative
`int16_t`. The `gtp_param_datetime` / `gtp_param_duration` reports
follow the same pattern (unbounded TLV-decoded integer narrowed at
assignment). The two `get_public_key` and the
`cmd_perform_privacy_operation` reports look like the classic
"BIP-32 length byte stored in an unsigned smaller type" bug surfacing
on long-but-bounded path lengths. The plugin-sdk `invalid-null-argument`
fires when an empty TLV value is passed to a helper that immediately
dereferences `value.ptr`.

None of these are regressions introduced by this branch — they are
pre-existing app code paths newly reached by the seed coverage added
for `gtp_param_*` / `gtp_path_*` / `whitelist_7702`. The fixes belong
in the source files listed above (clamp the conversion, bounds-check
before truncation, or change the destination type to a wider signed
integer). Until they land, every UBSan run will report this set.

## ClusterFuzzLite

[`.clusterfuzzlite/build.sh`](../../.clusterfuzzlite/build.sh) ships
**only `fuzz_app`** (the Absolution-driven dispatcher) to CFL together
with its generated seed corpus. The 18 per-feature harnesses are
intentionally left out — they are a developer-side triage tool, not a
coverage source (see "Legacy per-feature triage" below). CFL also
forces a `.{}` bootstrap of `fuzz_globals.zon` on every invocation so
the Absolution discovery step matches the current sanitizer profile;
otherwise ASan/UBSan/MSan red-zone padding shifts global offsets and
Absolution rejects the mismatch with `WholeValuesBlobMismatch`.

The CFL Dockerfile pulls a pinned SDK ref that ships the framework; see
[`.clusterfuzzlite/Dockerfile`](../../.clusterfuzzlite/Dockerfile).

## Legacy per-feature triage (optional)

The 18 per-feature harnesses under `harness/fuzz_*.c` remain in the tree
as classic libFuzzer binaries: one `LLVMFuzzerTestOneInput` per source
file, no Absolution prefix, no shared invariant. They are **not** wired
into `app-campaign.sh`; use them for narrow triage when you want to
mutate the input bytes of a single parser directly.

A typical harness looks like:

```c
int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    if (sigsetjmp(fuzz_exit_jump_ctx.jmp_buf, 1)) return 0;

    /* harness code */

    return 0;
}
```

The `sigsetjmp` gives the harness a return point when the app reaches
`os_sched_exit()` (mocked in `mock/mocks.c` to `longjmp` back here).

The `cmake` configure used by `app-campaign.sh` already builds them
alongside `fuzz_app`, so triaging a single feature is a direct binary
invocation:

```bash
cmake --build tests/fuzzing/build --target fuzz_calldata
./tests/fuzzing/build/fuzz_calldata corpus_dir/
```

For full-app coverage tracking use the dispatcher campaign (`fuzz_app`
via `app-campaign.sh`); the per-feature binaries are stock libFuzzer
and follow the standard `llvm-profdata` / `llvm-cov show` workflow.

## Tree layout

```
tests/fuzzing/
  CMakeLists.txt                  ← single configure for both flavours
  README.md                       ← this file
  fuzz-manifest.toml              ← dispatcher config: CLA/INS, seeds, dict
  base-corpus/                    ← dispatcher base corpus + .compat-key

  harness/                        ← per-feature harness sources
    fuzz_calldata.c
    fuzz_eip712.c
    …                             ← 18 files total

  src/
    fuzz_utils.c / .h             ← per-feature: init helper + scratch state
    fuzz_dispatcher.c             ← dispatcher: SDK boundary entry
    fuzz_runtime.c / .h           ← dispatcher: per-iteration reset + state bootstraps
    fuzz_apdu_adapter.c / .h      ← dispatcher: re-encodes commands as APDUs
    fuzz_apdu_dispatch.c / .h     ← dispatcher: 3-line wrapper around
                                     src/apdu_dispatch.c (production switch)
    fuzz_command_registry.c/.h/.inc  ← dispatcher: canonical INS table
    fuzz_non_apdu.c / .h          ← dispatcher: non-APDU coverage shims
    fuzz_tlv_config.c / .h        ← dispatcher: grammar-aware TLV mutator wiring

  mock/                           ← shared mock layer (linked by every binary)
    mocks.h / mocks.c             ← engine globals, exception/exit machinery,
                                    PKI/UI/main.c stubs, cx_* mocks
    app_globals.c                 ← single definition of shared app globals
    coverage_sigusr1.c            ← optional SIGUSR1 coverage dump hook
    scenario_layout.h             ← prefix offsets (regenerated by the SDK script)
    net_icons.gen.h               ← network icons stub

  invariants/
    zero-symbols.txt              ← globals removed from the dispatcher prefix
    domain-overrides.txt          ← per-field domain constraints
    fuzz_globals.zon              ← machine-local, NOT committed (.gitignore)

  scripts/
    generate_seed_corpus.py       ← seed generator driven by command registry
    fuzz_command_registry.py      ← Python view of the canonical INS table

  macros/
    exclude_macros.txt            ← SDK macro exclusions
```

## Production-parity dispatch

`fuzz_dispatch_apdu()` (in `src/fuzz_apdu_dispatch.c`) is a thin
wrapper that calls `handleApdu()` from `src/apdu_dispatch.c`. The
on-device build's `app_main()` calls the same function. Adding a new
`INS_FOO` in production is therefore immediately reachable from the
fuzzer with no harness-side edit — there is no mirror switch to keep
in sync.

`handleApdu()` used to live inline in `src/main.c` as a `static`
function. It was lifted verbatim into `src/apdu_dispatch.c` because
the fuzz build excludes `main.c` (to avoid pulling in `app_main`,
`coin_main`, `library_main`, `clone_main`). The function name,
signature, and switch body are preserved; the production-side diff
is one new `#include` plus the deletion of the moved body.

## Why `fuzz_globals.zon` is not committed

The `.zon` file is the SDK's Absolution model of the binary's global
memory layout, with concrete `.whole_values` blobs whose byte lengths
depend on the sanitizer profile, SDK revision and toolchain version. A
snapshot from one developer machine cannot be reused on another build
configuration — Absolution rejects the mismatch with
`WholeValuesBlobMismatch`.

The framework regenerates it locally:

- `LedgerAppFuzz.cmake`'s `ledger_fuzz_validate_app_files()` bootstraps a
  missing file to `.{}` at configure time.
- `app-campaign.sh` resets it to `.{}` when called with `--clean` or
  `BOOTSTRAP_INVARIANT=1`, then runs `sync_invariant` to fill it in for
  the current build.
- `.clusterfuzzlite/build.sh` runs the same bootstrap + sync ritual on
  every CFL invocation.

`.gitignore` excludes it. Do not stage local copies.

`mock/scenario_layout.h` follows the opposite policy: tracked with the SDK
template's bootstrap values, regenerated locally by
`update-scenario-layout.py`; do not commit local updates.

## Architecture notes for the dispatcher

- `src/fuzz_dispatcher.c` is the SDK boundary. It provides
  `LLVMFuzzerCustomMutator`, `fuzz_app_reset`, `fuzz_app_dispatch`, and
  `fuzz_entry`, all of which delegate to thin wrappers over real app
  code.
- `src/fuzz_apdu_adapter.c` re-encodes the picked fuzz command into a
  real APDU byte buffer and runs `apdu_parser()` before local dispatch,
  so the fuzzer exercises the same parsing path production uses.
- `src/fuzz_apdu_dispatch.c` is the thin wrapper described in
  "Production-parity dispatch" above.
- `src/fuzz_non_apdu.c` reaches the non-APDU coverage surfaces (swap
  callbacks, plugin helpers) that do not flow through the APDU
  dispatcher.
- `src/fuzz_runtime.c` owns the per-iteration reset and the harness-side
  state bootstraps (see below). The shared app globals it operates on
  are defined in `mock/app_globals.c` so the per-feature harnesses see
  one definition.

### Why some symbols are mocked instead of compiled in

The fuzz build uses link-time mock overrides for a small, deliberately
chosen set of symbols:

- **`check_signature_with_pubkey`** (`src/ledger_pki.c` excluded): always
  returns `true`. ECDSA verification is a pure cryptographic gate
  fuzzing cannot solve (2^-128 probability). Without this, every
  signature-gated TLV handler would bail at verification and downstream
  parsers would stay at 0% coverage. The sacrificed code has no
  memory-sensitive operations and is covered by unit tests with known
  key-pairs.
- **`ui_display_safe_account`**: no-op. The production function triggers
  an NBGL review flow that blocks on user interaction.
- **`parseBip32`**: uses the **real production implementation**
  (extracted from the excluded `src/main.c`), not a simplified stub,
  so the fuzzer exercises the same BIP32 validation as on-device.

The `src/features/sign_tx/eth_ustream.c` no-progress guard is active
under `FUZZING_BUILD_MODE_UNSAFE_FOR_PRODUCTION` (set automatically by
libFuzzer). It prevents infinite loops from Absolution-injected
`txContext` states where the normal `init_tx()` invariants are
intentionally broken.

### Harness-side state bootstraps

Some deep code paths (GTP, EIP-712, safe accounts) require multi-APDU
prerequisite state that cannot be produced inside a single-command
iteration. The adapter conditionally bootstraps this state before
dispatching the target command:

- **GTP**: when `appState == SIGNING_TX` and the command is
  `INS_GTP_FIELD` or `INS_GTP_TRANSACTION_INFO`, the bootstrap creates a
  `tx_ctx` with parked calldata (selector `0xAABBCCDD`), finds the
  matching context, and attaches a synthetic `tx_info` with
  `fields_hash = 0xFF…`. Field-table init runs as part of `tx_ctx_init`.
- **EIP-712**: when `appState == SIGNING_EIP712` and the command is
  `INS_EIP712_STRUCT_IMPL`, `INS_EIP712_FILTERING`, or
  `INS_SIGN_EIP_712_MESSAGE`, the bootstrap allocates the context,
  registers a minimal type schema (`EIP712Domain` with
  `name`/`chainId`/`verifyingContract` and `Mail` with `value`/`to`),
  sets both roots via `path_set_root`, and enables
  `EIP712_FILTERING_FULL`.
- **Safe account**: when the command is `INS_PROVIDE_SAFE_ACCOUNT` with
  `P2 == SIGNER_DESCRIPTOR`, the bootstrap allocates a `SAFE_DESC` with
  `threshold = 1`, `signers_count = 1`, `role = SIGNER`.
- **Enum values**: the GTP bootstrap also seeds a 4×4 grid of synthetic
  enum entries keyed to the bootstrapped contract address and selector,
  registered via the production `handle_enum_value_tlv_payload` +
  `verify_enum_value_struct` path (with the signature check mocked).

Trade-off: these bootstraps skip the APDU-level parsing that normally
builds the prerequisite structures (`INS_SIGN`, `INS_EIP712_STRUCT_DEF`,
`INS_PROVIDE_SAFE_ACCOUNT P2=SAFE_DESCRIPTOR`, `INS_PROVIDE_ENUM_VALUE`).
That parsing is already exercised by seeds targeting those INS directly.
The bootstrap only provides the minimum prerequisite state so dependent
handlers can run their own deep parsing.

## ClusterFuzzLite container workflow (manual)

To reproduce CI exactly, drive the build through the
`clusterfuzzlite` container. See
<https://google.github.io/clusterfuzzlite/> for the official
documentation.

```bash
mkdir -p tests/fuzzing/{corpus,out}
docker build -t fuzz-ethereum --file .clusterfuzzlite/Dockerfile .
```

Compilation:

```bash
docker run --rm --privileged -e FUZZING_LANGUAGE=c \
    -v "$(realpath .)/tests/fuzzing/out:/out" -ti fuzz-ethereum
```

Run a specific fuzzer:

```bash
docker run --rm --privileged \
    -e FUZZING_ENGINE=libfuzzer -e RUN_FUZZER_MODE=interactive \
    -v "$(realpath .)/tests/fuzzing/corpus:/tmp/fuzz_corpus" \
    -v "$(realpath .)/tests/fuzzing/out:/out" \
    -ti gcr.io/oss-fuzz-base/base-runner run_fuzzer fuzzer
```

The Dockerfile contains a copy of the sources (not a clone), so re-run
`docker build` after each code change.
