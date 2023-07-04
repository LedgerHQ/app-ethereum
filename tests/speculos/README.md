# Speculos functional tests

These tests are implemented in Python with the `SpeculosClient` interface which allows easy execution on the [Speculos](https://github.com/LedgerHQ/speculos) emulator.

## Requirements

- [python >= 3.8](https://www.python.org/downloads/)
- [pip](https://pip.pypa.io/en/stable/installation/)

### Dependencies
Python dependencies are listed in [requirements.txt](requirements.txt)

```shell
python3 -m pip install -r requirements.txt
```

## Usage

### Compilation app

Go to the root of the repository:
```sh
make DEBUG=1 NFT_TESTING_KEY=1 BOLOS_SDK=$NANOX_SDK
mv bin/app.elf tests/speculos/<some name>.elf
```

Given the requirements are installed, just do (by default command):

```
cd tests/speculos/
pytest
```

### Custom options
- **--model:**  "nanos", "nanox", "nanosp" | default: "nanos"
- **--display:** "qt", "headless"          | default: "qt"
- **--path:** the path of the binary app   | default: path of makefile compilation

## Example

With `nanox` binary app:
```sh
# the --path is variable to where you put your binary

pytest --model nanox --path ./elfs/nanox.elf

# Execute specific test:
pytest --model nanox --path ./elfs/nanox.elf test_pubkey_cmd.py
```
