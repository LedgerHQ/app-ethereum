#!/bin/bash -eu

export BOLOS_SDK=/ledger-secure-sdk

# build fuzzers using the docker images.
pushd tests/fuzzing
cmake -S . -B build -DCMAKE_C_COMPILER=clang -DCMAKE_BUILD_TYPE=Debug \
            -G Ninja -DCMAKE_EXPORT_COMPILE_COMMANDS=On \
            -DBOLOS_SDK="${BOLOS_SDK}" -DTARGET=flex \
            -DAPP_BUILD_PATH=/app

# Generates .zip for initial corpus in clusterFuzz
for dir in harness/*; do
    if [ -d "${dir}" ]; then
        fuzzer_name=$(basename "${dir}")
        zip_name="${fuzzer_name}_seed_corpus.zip"
        echo "Zipping corpus from ${dir} into ${zip_name}"

        (cd "${dir}" && zip -q -r "${zip_name}" .)

        mv "${dir}/${zip_name}" "${OUT}"
    fi
done
cmake --build build
mv ./build/fuzz_* "${OUT}"
popd
