try:
    from ledger_app_clients.ethereum.__version__ import __version__  # noqa
except ImportError:
    __version__ = "unknown version"  # noqa
