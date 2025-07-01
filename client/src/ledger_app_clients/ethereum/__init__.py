from importlib.metadata import version, PackageNotFoundError

try:
    __version__ = version("ledger_app_clients.ethereum")
except PackageNotFoundError:
    # package is not installed
    pass
