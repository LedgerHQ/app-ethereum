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
- [Quick start guide](#quick-start-guide)
  - [With VSCode](#with-vscode)
  - [With a terminal](#with-a-terminal)
- [Compilation and load](#compilation-and-load)
  - [Compilation](#compilation)
  - [Loading on a physical device](#loading-on-a-physical-device)
- [Tests](#tests)
  - [Functional Tests (Ragger based)](#functional-tests-ragger-based)
  - [Unit Tests](#unit-tests)
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

## Quick start guide

### With VSCode

You can quickly setup a convenient environment to build and test your application by using
[Ledger's VSCode developer tools extension](https://marketplace.visualstudio.com/items?itemName=LedgerHQ.ledger-dev-tools)
which leverages the [ledger-app-dev-tools](https://github.com/LedgerHQ/ledger-app-builder/pkgs/container/ledger-app-builder%2Fledger-app-dev-tools)
docker image.

It will allow you, whether you are developing on macOS, Windows or Linux,
to quickly **build** your apps, **test** them on **Speculos** and **load** them on any supported device.

- Install and run [Docker](https://www.docker.com/products/docker-desktop/).
- Make sure you have an X11 server running:
  - On Ubuntu Linux, it should be running by default.
  - On macOS, install and launch [XQuartz](https://www.xquartz.org/)
    (make sure to go to XQuartz > Preferences > Security and check "Allow client connections").
  - On Windows, install and launch [VcXsrv](https://sourceforge.net/projects/vcxsrv/)
    (make sure to configure it to disable access control).
- Install [VScode](https://code.visualstudio.com/download) and add [Ledger's extension](https://marketplace.visualstudio.com/items?itemName=LedgerHQ.ledger-dev-tools).
- Open a terminal and clone `app-ethereum` with `git clone git@github.com:LedgerHQ/app-ethereum.git`.
- Open the `app-ethereum` folder with VSCode.
- Use Ledger extension's sidebar menu or open the tasks menu with `ctrl + shift + b`
  (`command + shift + b` on a Mac) to conveniently execute actions:
  - Build the app for the device model of your choice with `Build`.
  - Test your binary on [Speculos](https://github.com/LedgerHQ/speculos) with `Run with Speculos`.
  - You can also run functional tests, load the app on a physical device, and more.

> The terminal tab of VSCode will show you what commands the extension runs behind the scene.

### With a terminal

The [ledger-app-dev-tools](https://github.com/LedgerHQ/ledger-app-builder/pkgs/container/ledger-app-builder%2Fledger-app-dev-tools)
docker image contains all the required tools and libraries to **build**, **test** and **load** an application.

You can download it from the ghcr.io docker repository:

```shell
sudo docker pull ghcr.io/ledgerhq/ledger-app-builder/ledger-app-dev-tools:latest
```

You can then enter this development environment by executing the following command
from the root directory of the application `git` repository:

#### Linux (Ubuntu)

```shell
sudo docker run --rm -ti --user "$(id -u):$(id -g)" --privileged -v "/dev/bus/usb:/dev/bus/usb" -v "$(realpath .):/app" ghcr.io/ledgerhq/ledger-app-builder/ledger-app-dev-tools:latest
```

#### macOS

```shell
sudo docker run  --rm -ti --user "$(id -u):$(id -g)" --privileged -v "$(pwd -P):/app" ghcr.io/ledgerhq/ledger-app-builder/ledger-app-dev-tools:latest
```

#### Windows (with PowerShell)

```shell
docker run --rm -ti --privileged -v "$(Get-Location):/app" ghcr.io/ledgerhq/ledger-app-builder/ledger-app-dev-tools:latest
```

The application's code will be available from inside the docker container,
you can proceed to the following compilation steps to build your app.

## Compilation and load

To easily setup a development environment for compilation and loading on a physical device, you can use the [VSCode integration](#with-vscode)
whether you are on Linux, macOS or Windows.

If you prefer using a terminal to perform the steps manually, you can use the guide below.

### Compilation

Setup a compilation environment by following the [shell with docker approach](#with-a-terminal).

Be sure you checkout the submodule:

```shell
git submodule update --init
```

From inside the container, use the following command to build the app:

```shell
make DEBUG=1  # compile optionally with PRINTF
```

You can choose which device to compile and load for by setting the `BOLOS_SDK` environment variable to the following values:

- `BOLOS_SDK=$NANOS_SDK`
- `BOLOS_SDK=$NANOX_SDK`
- `BOLOS_SDK=$NANOSP_SDK`
- `BOLOS_SDK=$STAX_SDK`

### Loading on a physical device

This step will vary slightly depending on your platform.

> Your physical device must be connected, unlocked and the screen showing the dashboard (not inside an application).

#### Linux (Ubuntu)

First make sure you have the proper udev rules added on your host.
See [udev-rules](https://github.com/LedgerHQ/udev-rules)

Then once you have [opened a terminal](#with-a-terminal) in the `app-builder` image and [built the app](#compilation-and-load)
for the device you want, run the following command:

```shell
# Run this command from the app-builder container terminal.
make load    # load the app on a Nano S by default
```

[Setting the BOLOS_SDK environment variable](#compilation-and-load) will allow you to load
on whichever supported device you want.

#### macOS / Windows (with PowerShell)

> It is assumed you have [Python](https://www.python.org/downloads/) installed on your computer.

Run these commands on your host from the app's source folder once you have [built the app](#compilation-and-load)
for the device you want:

```shell
# Install Python virtualenv
python3 -m pip install virtualenv
# Create the 'ledger' virtualenv
python3 -m virtualenv ledger
```

Enter the Python virtual environment

- macOS: `source ledger/bin/activate`
- Windows: `.\ledger\Scripts\Activate.ps1`

```shell
# Install Ledgerblue (tool to load the app)
python3 -m pip install ledgerblue
# Load the app.
python3 -m ledgerblue.runScript --scp --fileName bin/app.apdu --elfFile bin/app.elf
```

## Tests

The Ethereum app comes with different tests:

- Functional Tests implemented with Ledger's [Ragger](https://github.com/LedgerHQ/ragger) test framework.
- Unit Tests, allowing to test basic simple functions

### Functional Tests (Ragger based)

#### Linux (Ubuntu)

On Linux, you can use [Ledger's VS Code extension](#with-vscode) to run the tests.
If you prefer not to, open a terminal and follow the steps below.

Install the tests requirements:

```shell
pip install -r tests/ragger/requirements.txt
```

Then you can:

Run the functional tests (here for nanos but available for any device once you have built the binaries):

```shell
pytest tests/ragger/ --tb=short -v --device nanos
```

Please see the corresponding ducomentation [USAGE](tests/ragger/usage.md)

Or run your app directly with Speculos

```shell
speculos --model nanos build/nanos/bin/app.elf
```

#### macOS / Windows

To test your app on macOS or Windows, it is recommended to use [Ledger's VS Code extension](#with-vscode)
to quickly setup a working test environment.

You can use the following sequence of tasks and commands (all accessible in the **extension sidebar menu**):

- `Select build target`
- `Build app`

Then you can choose to execute the functional tests:

- Use `Run tests`.

Or simply run the app on the Speculos emulator:

- `Run with Speculos`.

### Unit Tests

Those tests are available in the directory `tests/unit`. Please see the corresponding [README](tests/unit/README.md)
to compile and run them.

## Contributing

Contributions are what makes the open source community such an amazing place to learn, inspire, and create.
Any contributions you make are **greatly appreciated**.

If you have a suggestion that would make this better, please fork the repo and create a pull request.
You can also simply open an issue with the tag `enhancement`.

1. Fork the Project
2. Create your Feature Branch (`git checkout -b feature/my-feature`)
3. Commit your Changes (`git commit -m 'feat: my new feature`)
4. Push to the Branch (`git push origin feature/my-feature`)
5. Open a Pull Request

Please try to follow [Conventional Commits](https://www.conventionalcommits.org/en/v1.0.0/).
