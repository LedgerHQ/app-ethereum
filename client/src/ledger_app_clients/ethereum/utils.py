import sha3

def get_selector_from_function(fn: str) -> bytes:
    return sha3.keccak_256(fn.encode()).digest()[0:4]
