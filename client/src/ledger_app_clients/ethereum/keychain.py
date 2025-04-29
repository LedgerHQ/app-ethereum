import os
import hashlib
from enum import Enum, auto
from ecdsa import SigningKey
from ecdsa.util import sigencode_der


# Private key PEM files have to be named the same (lowercase) as their corresponding enum entries
# Example: for an entry in the Enum named DEV, its PEM file must be at keychain/dev.pem
class Key(Enum):
    CAL = auto()
    TRUSTED_NAME = auto()
    SET_PLUGIN = auto()
    NFT = auto()
    CALLDATA = auto()
    NETWORK = auto()
    WEB3_CHECK = auto()


_keys: dict[Key, SigningKey] = {}


# Open the corresponding PEM file and load its key in the global dict
def _init_key(key: Key):
    with open(f"{os.path.dirname(__file__)}/keychain/{key.name.lower()}.pem", encoding="utf-8") as pem_file:
        _keys[key] = SigningKey.from_pem(pem_file.read(), hashlib.sha256)
    assert (key in _keys) and (_keys[key] is not None)


# Generate a SECP256K1 signature of the given data with the given key
def sign_data(key: Key, data: bytes) -> bytes:
    if key not in _keys:
        _init_key(key)
    return _keys[key].sign_deterministic(data, sigencode=sigencode_der)
