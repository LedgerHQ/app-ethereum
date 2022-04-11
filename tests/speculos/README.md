# Speculos functional tests

These tests are implemented in Python with the `SpeculosClient` interface which allows easy execution on the [Speculos](https://github.com/LedgerHQ/speculos) emulator.

## Requirements

- [python >= 3.8](https://www.python.org/downloads/)
- [pip](https://pip.pypa.io/en/stable/installation/)

### Dependencies
Python dependencies are listed in [requirements.txt](requirements.txt)

```shell
python3 -m pip install --extra-index-url https://test.pypi.org/simple/ -r requirements.txt
```
> The extra index allows to fetch the latest version of Speculos.

## Usage

Given the requirements are installed, just do:

```
pytest tests/speculos/
```

## Tests by APDU

you will find the list of apdu [here](../../doc/apdu.md)

- Get
  - GET APP CONFIGURATION ( 1 test )
    - Get the configuration
  - GET ETH PUBLIC ADDRESS ( 3 tests )
    - Ether coin without display
    - Dai coin with display
    - Dai coin with display and reject
  - GET ETH2 PUBLIC KEY
    - [ ] Test get key
    - [ ] Test get key with display