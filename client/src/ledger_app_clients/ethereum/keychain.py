import os
import hashlib
from ecdsa import SigningKey
from ecdsa.util import sigencode_der
from enum import Enum, auto


# Private key PEM files have to be named the same (lowercase) as their corresponding enum entries
# Example: for an entry in the Enum named DEV, its PEM file must be at keychain/dev.pem
class Key(Enum):
    CAL = auto()
    DOMAIN_NAME = auto()
    SET_PLUGIN = auto()
    NFT = auto()


_keys: dict[Key, SigningKey] = dict()


# Open the corresponding PEM file and load its key in the global dict
def _init_key(key: Key):
    global _keys
    with open("%s/keychain/%s.pem" % (os.path.dirname(__file__), key.name.lower())) as pem_file:
        _keys[key] = SigningKey.from_pem(pem_file.read(), hashlib.sha256)
    assert (key in _keys) and (_keys[key] is not None)


# Generate a SECP256K1 signature of the given data with the given key
def sign_data(key: Key, data: bytes) -> bytes:
    global _keys
    if key not in _keys:
        _init_key(key)
    return _keys[key].sign_deterministic(data, sigencode=sigencode_der)
