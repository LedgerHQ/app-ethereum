from importlib.metadata import PackageNotFoundError, version

try:
    __version__ = version("ledger_app_clients.ethereum")
except PackageNotFoundError:
    # package is not installed
    pass
