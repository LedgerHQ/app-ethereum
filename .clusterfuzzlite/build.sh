#!/bin/bash -eu
# ClusterFuzzLite build for the unified Ethereum fuzz tree.
#
# Only the Absolution-driven `fuzz_app` dispatcher is shipped to CFL: it is
# the canonical full-app coverage target, the only one with a curated seed
# corpus, dictionary, invariant model, and TLV-aware mutator. The 18 classic
# per-feature harnesses under `harness/fuzz_*.c` remain a developer-side
# triage tool driven manually with libFuzzer (see README "Legacy per-feature
# triage"); they intentionally do not flow through CFL because they would
# fuzz from scratch with no seeds and contribute nearly no marginal coverage
# beyond `fuzz_app`.
#
# Absolution refuses to use an `invariants/<target>.zon` whose `.whole_values`
# blobs do not match the binary's current global layout and fails the build
# with `WholeValuesBlobMismatch`. CFL's sanitizer matrix (address / undefined
# / memory) shifts global offsets on each entry, so any `.zon` committed by a
# developer machine is guaranteed to mismatch at least two of the three CFL
# jobs. The defensive fix is to bootstrap the `.zon` to `.{}` here, let
# Absolution discover the layout against the current build, then
# `sync_invariant` to materialise a matching model before the final link.
#
# SANITIZER is forwarded into APP_SANITIZER so configure_fuzz_build picks the
# sanitizer profile that matches the current CFL matrix entry.

export BOLOS_SDK=/ledger-secure-sdk
export APP_DIR=/app
export APP_FUZZ_SUBDIR=tests/fuzzing
export APP_TARGET=flex
export APP_SANITIZER="${SANITIZER:-address}"

SCRIPT_DIR="${BOLOS_SDK}/fuzzing/scripts"

# shellcheck source=/dev/null
source "${SCRIPT_DIR}/app-common.sh"
# shellcheck source=/dev/null
source "${SCRIPT_DIR}/app-config.sh"

BUILD_FAST="${APP_DIR}/${APP_FUZZ_SUBDIR}/build"
INV="${APP_DIR}/${APP_FUZZ_SUBDIR}/invariants/fuzz_globals.zon"
LAYOUT="${APP_DIR}/${APP_FUZZ_SUBDIR}/mock/scenario_layout.h"

echo '.{}' > "${INV}"

rm -rf "${BUILD_FAST}"

configure_fuzz_build "${APP_DIR}" "${BUILD_FAST}" RelWithDebInfo 0

# fuzz_app is the only target shipped to CFL. Build it first so Absolution's
# generated fuzzer.c.zon exists before sync_invariant runs.
build_fuzzer_target "${BUILD_FAST}" fuzz_app

INVARIANT_CHANGED=0
sync_invariant "${BUILD_FAST}" fuzz_app "${INV}"
if [[ "${INVARIANT_CHANGED}" == "1" ]]; then
    build_fuzzer_target "${BUILD_FAST}" fuzz_app
fi

update_scenario_layout "${BUILD_FAST}" fuzz_app "${LAYOUT}"

prefix_size="$(prefix_size_from_generated_fuzzer "${BUILD_FAST}" fuzz_app)"
compat_key="$(python3 "${SCRIPT_DIR}/fuzz_manifest.py" --compat-key "${_APP_MANIFEST}" \
    --prefix-size "${prefix_size}" \
    --invariant "${INV}")"

SEED_CORPUS="${BUILD_FAST}/cfl-seed-corpus"
rm -rf "${SEED_CORPUS}"
mkdir -p "${SEED_CORPUS}"

export BUILD_DIR_FAST="${BUILD_FAST}"
generate_app_seed_corpus "${SEED_CORPUS}" fuzz_app

BASE_CORPUS="${APP_DIR}/${APP_FUZZ_SUBDIR}/base-corpus"
if [ -d "${BASE_CORPUS}" ]; then
    if [ -f "${BASE_CORPUS}/.compat-key" ]; then
        source_key="$(tr -d '[:space:]' < "${BASE_CORPUS}/.compat-key")"
        if [ -n "${source_key}" ] && [ "${source_key}" = "${compat_key}" ]; then
            echo "Merging compatible base-corpus into fuzz_app_seed_corpus.zip"
            cp -a "${BASE_CORPUS}/." "${SEED_CORPUS}/"
        else
            echo "Skipping incompatible base-corpus for fuzz_app_seed_corpus.zip"
            echo "  source compat_key: ${source_key:-<empty>}"
            echo "  current compat_key: ${compat_key}"
        fi
    else
        echo "Skipping base-corpus without .compat-key for fuzz_app_seed_corpus.zip"
    fi
fi

rm -f "${SEED_CORPUS}/.compat-key"
min_input_size=$((prefix_size + 4))
removed_count=0
for corpus_file in "${SEED_CORPUS}"/*; do
    [ -f "${corpus_file}" ] || continue
    fsize="$(wc -c < "${corpus_file}")"
    if (( fsize < min_input_size )); then
        rm -f "${corpus_file}"
        removed_count=$((removed_count + 1))
    fi
done
if (( removed_count > 0 )); then
    echo "Filtered ${removed_count} fuzz_app seed files smaller than ${min_input_size} bytes"
fi

cp "${BUILD_FAST}/fuzz_app" "${OUT}/"

echo "Zipping generated seed corpus into fuzz_app_seed_corpus.zip"
(cd "${SEED_CORPUS}" && zip -q -r "${OUT}/fuzz_app_seed_corpus.zip" .)
