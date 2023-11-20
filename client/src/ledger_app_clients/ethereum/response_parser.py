def signature(data: bytes) -> tuple[bytes, bytes, bytes]:
    assert len(data) == (1 + 32 + 32)

    v = data[0:1]
    data = data[1:]
    r = data[0:32]
    data = data[32:]
    s = data[0:32]

    return v, r, s


def challenge(data: bytes) -> int:
    assert len(data) == 4
    return int.from_bytes(data, "big")


def pk_addr(data: bytes, has_chaincode: bool = False):
    idx = 0

    if len(data) < (idx + 1):
        return None
    pk_len = data[idx]
    idx += 1

    if len(data) < (idx + pk_len):
        return None
    pk = data[idx:idx + pk_len]
    idx += pk_len

    if len(data) < (idx + 1):
        return None
    addr_len = data[idx]
    idx += 1

    if len(data) < (idx + addr_len):
        return None
    addr = data[idx:idx + addr_len]
    idx += addr_len

    if has_chaincode:
        if len(data) < (idx + 32):
            return None
        chaincode = data[idx:idx + 32]
        idx += 32
    else:
        chaincode = None

    if idx != len(data):
        return None

    return pk, bytes.fromhex(addr.decode()), chaincode
