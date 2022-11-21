import os
import hashlib
from ecdsa.util import sigencode_der
from ecdsa import SigningKey

_key: SigningKey = None

def _init_key():
    global _key
    with open(os.path.dirname(__file__) + "/key.pem") as pem_file:
        _key = SigningKey.from_pem(pem_file.read(), hashlib.sha256)
    assert _key != None

def sign(data: bytes) -> bytes:
    global _key
    if not _key:
        _init_key()
    return _key.sign_deterministic(data, sigencode=sigencode_der)
