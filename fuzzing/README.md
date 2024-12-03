# Fuzzing ethereum app

## Build

The fuzzer can be built using the following commands from the app directory:
```bash
docker run --rm -it --user "$(id -u):$(id -g)" -v "$(realpath .):/app" ghcr.io/ledgerhq/ledger-app-builder/ledger-app-builder-legacy:latest bash

apt install libbsd-dev

cd fuzzing
cmake -DBOLOS_SDK=/opt/ledger-secure-sdk -DCMAKE_C_COMPILER=/usr/bin/clang -Bbuild -H.
cmake --build build
```

## Run

The fuzzer can be run using the following commands from the fuzzing directory:
```bash
./build/fuzzer_tlv -max_len=8192
```
