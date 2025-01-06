#!/bin/bash -eu

# build fuzzers

pushd tests/fuzzing
cmake -DBOLOS_SDK=$(pwd)/../../BOLOS_SDK -B build -S .
cmake --build build
mv ./build/fuzzer "${OUT}"
popd
