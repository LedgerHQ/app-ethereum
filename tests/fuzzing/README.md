# Fuzzing Tests

## Fuzzing

Fuzzing allows us to test how a program behaves when provided with invalid, unexpected, or random
data as input.

The fuzz target needs to implement `int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)`,
which provides an array of random bytes that can be used to simulate a serialized buffer.
If the application crashes, or a [sanitizer](https://github.com/google/sanitizers) detects
any kind of access violation, the fuzzing process is stopped, a report regarding the vulnerability is shown,
and the input that triggered the bug is written to disk under the name `crash-*`.

The vulnerable input file created can be passed as an argument to the fuzzer to triage the issue.

## Manual usage based on Ledger container

### Preparation

The fuzzer can be run using the Docker image `ledger-app-dev-tools`. You can download it from the
`ghcr.io` docker repository:

```bash
docker pull ghcr.io/ledgerhq/ledger-app-builder/ledger-app-dev-tools:latest
```

You can then enter this development environment by executing the following command from the
repository root directory:

```bash
docker run --rm -ti -v "$(realpath .):/app" ghcr.io/ledgerhq/ledger-app-builder/ledger-app-dev-tools:latest
```

### Writing your Harness

When writing your harness, keep the following points in mind:

- An SDK's interface for compilation is provided via the target `secure_sdk` in CMakeLists.txt
- If you are running it for the first time, consider using the script `local_run` from inside the
  Docker container using the flag build=1, if you need to manually
  add/remove macros you can then do it using the files macros/add_macros.txt or
  macros/exclude_macros.txt and rerunning it, or directly change the macros/generated/macros.txt.
- A typical harness looks like this:

  ```C
  int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    if (sigsetjmp(fuzz_exit_jump_ctx.jmp_buf, 1)) return 0;

    ### harness code ###

    return 0;
  }
  ```

  This allows a return point when the `os_sched_exit()` function is mocked.

- To provide an SDK interface, we automatically generate syscall mock functions located in
  `SECURE_SDK_PATH/fuzzing/mock/generated/generated_syscalls.c`, if you need a more specific mock,
  you can define it in `APP_PATH/fuzzing/mock` with the same name and without the WEAK attribute.

### Compile and run the fuzzer from the container

Once inside the container, navigate to the `tests/fuzzing` folder to generate the fuzzer:

#### Preparation

Install the needed modules and set the BOLOS_SDK variable:

```bash
export BOLOS_SDK=/opt/flex-secure-sdk/
cd tests/fuzzing
apt update && apt install -y libbsd-dev
```

#### Compile the fuzzers

```bash
${BOLOS_SDK}/fuzzing/local_run.sh --j=4 --build=1 --BOLOS_SDK=${BOLOS_SDK}
```

#### Run the fuzzer

```bash
${BOLOS_SDK}/fuzzing/local_run.sh --j=4 --run-fuzzer=1 --fuzzer=build/fuzz_dispatcher --compute-coverage=1
```

#### Compile and run the fuzzer

```bash
${BOLOS_SDK}/fuzzing/local_run.sh --j=4 --build=1 --BOLOS_SDK=${BOLOS_SDK} \
                                  --run-fuzzer=1 --fuzzer=build/fuzz_dispatcher --compute-coverage=1
```

### About local_run.sh

| Parameter              | Type                | Description                                                          |
| :--------------------- | :------------------ | :------------------------------------------------------------------- |
| `--BOLOS_SDK`          | `PATH TO BOLOS SDK` | **Required**. Path to the BOLOS SDK                                  |
| `--build`              | `bool`              | **Optional**. Whether to build the project (default: 0)              |
| `--fuzzer`             | `PATH`              | **Required**. Path to the fuzzer binary                              |
| `--compute-coverage`   | `bool`              | **Optional**. Whether to compute coverage after fuzzing (default: 0) |
| `--run-fuzzer`         | `bool`              | **Optional**. Whether to run or not the fuzzer (default: 0)          |
| `--run-crash`          | `FILENAME`          | **Optional**. Run the on a specific crash input file (default: 0) |
| `--sanitizer`          | `address or memory` | **Optional**. Compile with sanitizer (default: address)       |
| `--j`                  | `int`               | **Optional**. N-parallel jobs for build and fuzzing (default: 1) |
| `--help`               |                     | **Optional**. Display help message                                   |

### Visualizing code coverage

After running your fuzzer, if `--compute-coverage=1` the coverage will be available in your browser.

## Full usage based on `clusterfuzzlite` container

Exactly the same context as the CI, directly using the `clusterfuzzlite` environment.

More info can be found here: <https://google.github.io/clusterfuzzlite/>

### Preparation

The principle is to build the container, and run it to perform the fuzzing.

> **Note**: The container contains a copy of the sources (they are not cloned), which means the
> `docker build` command must be re-executed after each code modification.

```bash
# Prepare directory tree
mkdir tests/fuzzing/{corpus,out}
# Container generation
docker build -t fuzz-ethereum --file .clusterfuzzlite/Dockerfile .
```

### Compilation

```bash
docker run --rm --privileged -e FUZZING_LANGUAGE=c -v "$(realpath .)/tests/fuzzing/out:/out" -ti fuzz-ethereum
```

### Run

```bash
docker run --rm --privileged -e FUZZING_ENGINE=libfuzzer -e RUN_FUZZER_MODE=interactive -v "$(realpath .)/tests/fuzzing/corpus:/tmp/fuzz_corpus" -v "$(realpath .)/tests/fuzzing/out:/out" -ti gcr.io/oss-fuzz-base/base-runner run_fuzzer fuzzer
```
