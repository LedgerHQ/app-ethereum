<br />
<div align="center">
  <a href="https://github.com/LedgerHQ/app-ethereum">
    <img src="https://img.icons8.com/nolan/64/ethereum.png"/>
  </a>

  <h1 align="center">app-ethereum</h1>

  <p align="center">
    Ethereum wallet application for Ledger Blue, Nano S, Nano S Plus and Nano X
    <br />
    <a href="https://github.com/LedgerHQ/app-ethereum/tree/master/doc"><strong>« Explore the docs »</strong></a>
    <br />
    <br />
    <a href="https://github.com/LedgerHQ/app-ethereum/issues">Report Bug</a>
    · <a href="https://github.com/LedgerHQ/app-ethereum/issues">Request Feature</a>
    · <a href="https://github.com/LedgerHQ/app-ethereum/issues">Request New Network</a>
  </p>
</div>
<br/>

<details>
  <summary>Table of Contents</summary>

- [About the project](#about-the-project)
- [Documentation](#documentation)
  - [Plugins](#plugins)
- [Testing](#testing)
  - [Requirements](#requirements)
    - [Build the applications required by the test suite](#build-the-applications-required-by-the-test-suite)
  - [Running all tests](#running-all-tests)
    - [With Makefile](#with-makefile)
    - [With yarn](#with-yarn)
  - [Running a specific tests](#running-a-specific-tests)
  - [Adding tests](#adding-tests)
    - [Zemu](#zemu)
    - [Update binaries](#update-binaries)
- [Contributing](#contributing)


</details>

## About the project

Ethereum wallet application framework for Nano S, Nano S Plus and Nano X.  
Ledger Blue is not maintained anymore, but the app can still be compiled for this target using the branch [`blue-final-release`](https://github.com/LedgerHQ/app-ethereum/tree/blue-final-release).

## Documentation

This app follows the specification available in the `doc/` folder.

To compile it and load it on a device, please check out our [developer portal](https://developers.ledger.com/docs/device-app/introduction).

### Plugins

We have the concept of plugins in the ETH app.  
Find the documentations here:  
- [Blog Ethereum plugins](https://blog.ledger.com/ethereum-plugins/)
- [Ethereum application Plugins : Technical Specifications](https://github.com/LedgerHQ/app-ethereum/blob/master/doc/ethapp_plugins.asc)
- [Plugin guide](https://hackmd.io/300Ukv5gSbCbVcp3cZuwRQ)
- [Boilerplate plugin](https://github.com/LedgerHQ/app-plugin-boilerplate)

## Testing

Testing is done via the open-source framework [zemu](https://github.com/Zondax/zemu).

### Requirements

- [nodeJS == 16](https://github.com/nvm-sh/nvm)
- [yarn](https://classic.yarnpkg.com/lang/en/docs/install/#debian-stable)
- [build environnement](https://github.com/LedgerHQ/ledger-app-builder/blob/master/Dockerfile)

#### Build the applications required by the test suite  

1. Add your BOLOS SDKs path to:
    - `NANOS_SDK` and `NANOX_SDK`

2. Go to the `tests` folder and run `./build_local_test_elfs.sh`
    - ```sh
        cd tests
        # This helper script will build the applications required by the test suite and move them at the right place.
        yarn install
        ./build_local_test_elfs.sh
      ```

### Running all tests
#### With Makefile

1. Then you can install and run tests by simply running on the `root` of the repo:
    - ```sh
        make test
      ```
    - This will run `make install_tests` and `make run_tests`

#### With yarn

1. Go to the `tests` folder and run:
    - ```sh
        yarn test
      ```

### Running a specific tests

1.  Go to the `tests` folder and run:
    - ```sh
        yarn jest --runInBand --detectOpenHandles {YourTestFile}
      ```
2.  For example with the `send test`:
    - ```sh
        yarn jest --runInBand --detectOpenHandles src/send.test.js
      ```


### Adding tests

#### Zemu

To add tests, copy one of the already existing test files in `tests/src/`.
You then need to adapt the `buffer` and `tx` variables to adapt to the APDU you wish to send.

- Adapt the expected screen flow. Please create a folder under `tests/snapshots` with the name of the test you're performing.
- Then adapt the `ORIGINAL_SNAPSHOT_PATH_PREFIX` with the name of the folder you just created.
- To create the snapshots, modify the `SNAPSHOT_PATH_PREFIX` and set it to be equal to `ORIGINAL_SNAPSHOT_PATH_PREFIX`.
- Run the tests once, this will create all the snapshots in the folder you created.
- Put back your `SNAPSHOT_PATH_PREFIX` to `snapshots/tmp/`.

Finally make sure you adapt the expected signature!

#### Update binaries

Don't forget to update the binaries in the test folder. To do so, compile with those environement variables:

```sh
make DEBUG=1 ALLOW_DATA=1
```

Then copy the binary to the `tests/elfs` folder (in this case, compiled with SDK for nanoS):

```sh
cp bin/app.elf tests/elfs/ethereum_nanos.elf
```

Repeat the operation for a binary compiled with nanoX SDK and change for `ethereum_nanox.elf`.


## Contributing

Contributions are what make the open source community such an amazing place to learn, inspire, and create. Any contributions you make are **greatly appreciated**.

If you have a suggestion that would make this better, please fork the repo and create a pull request. You can also simply open an issue with the tag `enhancement`.

1. Fork the Project
2. Create your Feature Branch (`git checkout -b feature/my-feature`)
3. Commit your Changes (`git commit -m 'feat: my new feature`)
4. Push to the Branch (`git push origin feature/my-feature`)
5. Open a Pull Request

Please try to follow [Conventional Commits](https://www.conventionalcommits.org/en/v1.0.0/).
