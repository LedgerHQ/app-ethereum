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
