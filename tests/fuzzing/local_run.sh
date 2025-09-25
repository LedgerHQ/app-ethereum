#!/bin/bash

# Clean
rm -rf build

# Build the fuzzer
cmake -B build -S . -DCMAKE_C_COMPILER=/usr/bin/clang -DSANITIZER=address
cmake --build build

if ! [ -f ./build/fuzzer ]; then
    echo "Build failed, please check the output above."
    exit 1
fi

# Create the corpus directory if it doesn't exist
if ! [ -d ./corpus ]; then
    mkdir corpus
fi

# Run the fuzzer on half CPU cores
ncpus=$(nproc)
jobs=$(($ncpus/2))
echo "The fuzzer will start very soon, press Ctrl-C when you want to stop it and compute the coverage"
./build/fuzzer -max_len=8192 -jobs="$jobs" ./corpus


read -p "Would you like to compute coverage (y/n)? " -n 1 -r
echo
if [[ $REPLY =~ ^[Nn]$ ]]
then
    exit 0
fi

# Remove previous artifacts
rm default.profdata default.profraw

# Run profiling on the corpus
./build/fuzzer -max_len=8192 -runs=0 ./corpus

# Compute coverage
llvm-profdata merge -sparse *.profraw -o default.profdata
llvm-cov show build/fuzzer -instr-profile=default.profdata -format=html -ignore-filename-regex='ethereum-plugin-sdk\/|secure-sdk\/' > report.html
llvm-cov report build/fuzzer -instr-profile=default.profdata -ignore-filename-regex='ethereum-plugin-sdk\/|secure-sdk\/'
