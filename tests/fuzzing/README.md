# Fuzzing Tests

## Fuzzing

Fuzzing allows us to test how a program behaves when provided with invalid, unexpected, or random data as input.

Our fuzz target needs to implement `int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)`,
which provides an array of random bytes that can be used to simulate a serialized buffer.
If the application crashes, or a [sanitizer](https://github.com/google/sanitizers) detects
any kind of access violation, the fuzzing process is stopped, a report regarding the vulnerability is shown,
and the input that triggered the bug is written to disk under the name `crash-*`.
The vulnerable input file created can be passed as an argument to the fuzzer to triage the issue.


## Manual usage based on Ledger container

### Preparation

The fuzzer can run from the docker `ledger-app-builder-legacy`. You can download it from the `ghcr.io` docker repository:

```console
sudo docker pull ghcr.io/ledgerhq/ledger-app-builder/ledger-app-builder-legacy:latest
```

You can then enter this development environment by executing the following command from the repository root directory:

```console
sudo docker run --rm -ti --user "$(id -u):$(id -g)" -v "$(realpath .):/app" ghcr.io/ledgerhq/ledger-app-builder/ledger-app-builder-legacy:latest
```

### Compilation

Once in the container, go into the `tests/fuzzing` folder to compile the fuzzer:

```console
cd tests/fuzzing

# cmake initialization
cmake -DBOLOS_SDK=/opt/ledger-secure-sdk -DCMAKE_C_COMPILER=/usr/bin/clang -DSANITIZER=[address|memory] -B build -S .

# Fuzzer compilation
cmake --build build
```

### Run

```console
./build/fuzzer -max_len=8192
```

If you want to do a fuzzing campain on more than one core and compute the coverage results, you can use the `local_run.sh` script within the container (it'll only run the address and UB sanitizers).

## Full usage based on `clusterfuzzlite` container

Exactly the same context as the CI, directly using the `clusterfuzzlite` environment.

More info can be found here:
<https://google.github.io/clusterfuzzlite/>

### Preparation

The principle is to build the container, and run it to perform the fuzzing.

> **Note**: The container contains a copy of the sources (they are not cloned),
> which means the `docker build` command must be re-executed after each code modification.

```console
# Prepare directory tree
mkdir tests/fuzzing/{corpus,out}
# Container generation
docker build -t app-ethereum --file .clusterfuzzlite/Dockerfile .
```

### Compilation

```console
docker run --rm --privileged -e FUZZING_LANGUAGE=c -v "$(realpath .)/tests/fuzzing/out:/out" -ti app-ethereum
```

### Run

```console
docker run --rm --privileged -e FUZZING_ENGINE=libfuzzer -e RUN_FUZZER_MODE=interactive -v "$(realpath .)/tests/fuzzing/corpus:/tmp/fuzz_corpus" -v "$(realpath .)/tests/fuzzing/out:/out" -ti gcr.io/oss-fuzz-base/base-runner run_fuzzer fuzzer
```
