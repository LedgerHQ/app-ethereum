# Unit tests

## Prerequisite

Be sure to have installed:

- [CMake >= 3.10](https://cmake.org/download/)
- [lcov >= 1.14](http://ltp.sourceforge.net/coverage/lcov.php)

and for code coverage generation:

- lcov >= 1.14

## Overview

In `tests/unit` folder, compile with:

```shell
cmake -Bbuild -H. && make -C build
```

and run tests with:

```shell
CTEST_OUTPUT_ON_FAILURE=1 make -C build test
```

To get more verbose output, use:

```shell
CTEST_OUTPUT_ON_FAILURE=1 make -C build test ARGS="-V"
```

Or also directly with:

```shell
CTEST_OUTPUT_ON_FAILURE=1 build/test_param_network
```

## Generate code coverage

Just execute in `tests/unit` folder:

```shell
./gen_coverage.sh
```

it will output `coverage.total` and `coverage/` folder with HTML details (in `coverage/index.html`).
