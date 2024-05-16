# How to use the Ragger test framework

This framework allows testing the application on the Speculos emulator or on a real device using `LedgerComm` or `LedgerWallet`

## Quickly get started with Ragger and Speculos

### Install ragger and dependencies

```shell
pip install --extra-index-url https://test.pypi.org/simple/ -r requirements.txt
sudo apt-get update && sudo apt-get install qemu-user-static
```

### Compile the application

The application to test must be compiled for all required devices.
You can use for this the container `ghcr.io/ledgerhq/ledger-app-builder/ledger-app-builder-lite`:

```shell
docker pull ghcr.io/ledgerhq/ledger-app-builder/ledger-app-builder-lite:latest
cd <your app repository>
docker run --user "$(id -u)":"$(id -g)" --rm -ti -v "$(realpath .):/app" --privileged -v "/dev/bus/usb:/dev/bus/usb" ledger-app-builder-lite:latest
make clean && make BOLOS_SDK=$<device>_SDK      # replace <device> with one of [NANOS, NANOX, NANOSP, STAX]
exit
```

### Run a simple test using the Speculos emulator

You can use the following command to get your first experience with Ragger and Speculos

```shell
pytest -v --tb=short --device nanox --display
```

Or you can refer to the section `Available pytest options` to configure the options you want to use

### Run a simple test using a real device

The application to test must be loaded and started on a Ledger device plugged in USB.
You can use for this the container `ghcr.io/ledgerhq/ledger-app-builder/ledger-app-builder-lite`:

```shell
docker pull ghcr.io/ledgerhq/ledger-app-builder/ledger-app-builder-lite:latest
cd app-<appname>/                                   # replace <appname> with the name of your app, (eg boilerplate)
docker run --user "$(id -u)":"$(id -g)" --rm -ti -v "$(realpath .):/app" --privileged -v "/dev/bus/usb:/dev/bus/usb" ledger-app-builder-lite:latest
make clean && make BOLOS_SDK=$<device>_SDK load     # replace <device> with one of [NANOS, NANOX, NANOSP, STAX]
exit
```

You can use the following command to get your first experience with Ragger and Ledgerwallet on a NANOX.
Make sure that the device is plugged, unlocked, and that the tested application is open.

```shell
pytest -v --tb=short --device nanox --backend ledgerwallet
```

Or you can refer to the section `Available pytest options` to configure the options you want to use

### Ethereum specific

With Ethereum App, it is also possible to load an app (like a _plugin_), and use the `app-ethereum` like a _library_.
Such case is tested with application clone, using `ThunderCore`.

This special configuration needs an additional command line parameter `--with_lib_mode`,
where only the dedicated tests are selected.

## Adding a test

When adding new Module for tests, just be carrefull to declare it correctly in order to be handled
following the parameter `--with_lib_mode`. please refer to `conftest.py`.

## Available pytest options

Standard useful pytest options

```shell
    -v              formats the test summary in a readable way
    -s              enable logs for successful tests, on Speculos it will enable app logs if compiled with DEBUG=1
    -k <testname>   only run the tests that contain <testname> in their names
    --tb=short      in case of errors, formats the test traceback in a readable way
```

Custom pytest options

```shell
    --with_lib_mode             run the test(s) dedicated to Library Mode
    --device <device>           run the test on the specified device [nanos,nanox,nanosp,stax,all]. This parameter is mandatory
    --backend <backend>         run the tests against the backend [speculos, ledgercomm, ledgerwallet]. Speculos is the default
    --display                   on Speculos, enables the display of the app screen using QT
    --golden_run                on Speculos, screen comparison functions will save the current screen instead of comparing
    --log_apdu_file <filepath>  log all apdu exchanges to the file in parameter. The previous file content is erased
    --seed=SEED                 set a custom seed
```
