#!/bin/bash -eu
# ClusterFuzzLite build: delegate to the shared SDK script.
export BOLOS_SDK=/ledger-secure-sdk
export APP_DIR=/app
export APP_FUZZ_SUBDIR=tests/fuzzing
exec "${BOLOS_SDK}/fuzzing/scripts/cfl-build.sh"
