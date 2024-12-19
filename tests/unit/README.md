# Unit tests

It is important to unit test your functions.
This also allows you to document how your functions work.
We use the library [**cmocka**](https://cmocka.org/#features)

## Requirement

- [CMake >= 3.10](https://cmake.org/download/)
- [lcov >= 1.14](http://ltp.sourceforge.net/coverage/lcov.php)

Don't worry, you don't necessarily need to install the `cmocka library`
because the **cmakelist automatically fetches** the library

## Add new test

Create new file into `tests` folder and follow [this initiation](https://cmocka.org/talks/cmocka_unit_testing_and_mocking.pdf)

Now go to the `CMakeLists.txt` file and add your test with the specific file you want to test.

## Usage

### Build

The `default rules` of makefile will compile the tests and run them.

```sh
make
```

The `coverage rule` will launch the default rules and generate the coverage
and you will be **automatically redirected** to the generated .html

```sh
make coverage
```

The `clean rule` will delete the folders and files generated

```sh
make clean
```
