from Crypto.Hash import SHA256 as _sha256
from Crypto.Protocol.KDF import HKDF as _HKDF


def SHA256(x: bytes) -> bytes:
    return _sha256.new(x).digest()


def HKDF(*, salt: bytes, IKM: bytes, L: int, info: bytes = b"") -> bytes:
    res = _HKDF(master=IKM, key_len=L, salt=salt, hashmod=_sha256, context=info)
    return (
        res if isinstance(res, bytes) else res[0]
    )  # PyCryptodome can return Tuple[bytes]
