import os
import hashlib
from ecdsa.util import sigencode_der
from ecdsa import SigningKey
from enum import Enum, auto

class Key(Enum):
    CAL = auto()
    TRUSTED_NAME = auto()

_keys: dict[Key, SigningKey] = dict()

def _init_key(key: Key):
    global _keys
    with open("%s/keys/%s.pem" % (os.path.dirname(__file__), key.name.lower())) as pem_file:
        _keys[key] = SigningKey.from_pem(pem_file.read(), hashlib.sha256)
    assert (key in _keys) and (_keys[key] != None)

def key_sign(key: Key, data: bytes) -> bytes:
    global _keys
    if key not in _keys:
        _init_key(key)
    return _keys[key].sign_deterministic(data, sigencode=sigencode_der)
