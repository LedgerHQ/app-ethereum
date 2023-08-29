# Python client for the Ledger Ethereum application

This package allows to communicate with the Ledger Ethereum application, either on a
real device, or emulated on Speculos.

## Installation

This package is deployed:

- on `pypi.org` for the stable version. This version will work with the
  application available on the `master` branch.

  ```bash
  pip install ledger_app_clients.ethereum
  ```
- on `test.pypi.org` for the rolling release. This version will work with the
  application available on the `develop` branch.

  ```bash
  pip install --extra-index-url https://test.pypi.org/simple/ ledger_app_clients.ethereum
  ```

### Installation from sources

You can install the client from this repo:

```bash
pip install .
```
