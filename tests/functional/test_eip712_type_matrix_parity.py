# EIP-712 per-type PARITY MATRIX — @metamask/eth-sig-util V4 / ethers v6 guard.
#
# Background
# ----------
# The device's EIP-712 message hash is verified end-to-end in test_eip712.py by
# recover_message(), which derives the digest with eth_account
# (encode_typed_data().body == keccak256(hashStruct(primaryType, message))).
# That digest must equal what @metamask/eth-sig-util (hashStruct V4) and ethers
# v6 (TypedDataEncoder) independently produce. The array-of-struct hashing fix in
# src/features/sign_message_eip712/path.c must not perturb the hashing of any
# *elementary* EIP-712 type either.
#
# This module pins a baseline (type_matrix_parity_baseline.json) that was
# produced off-line by computing each hash with BOTH eth-sig-util V4 and ethers
# v6 and asserting the two agree. Here we re-derive every entry with eth_account
# (the exact body recover_message() relies on) and assert it equals the pinned
# value. So this is a fast, Speculos-free port of eth-sig-util's per-type matrix:
# it fixes the EXPECTED side of the device equality for every elementary type in
# three shapes (scalar, dynamic T[], fixed T[2]).
#
# The on-device half — DEVICE_ADDR == recover_message(sign(fixture)) — is covered
# for the bundled-matrix fixtures (25-parity_matrix_*) by test_eip712.py.

import json
import os

import pytest
from eth_account.messages import encode_typed_data

EIP712_DIR = os.path.join(os.path.dirname(__file__), "eip712_input_files")
BASELINE_FILE = os.path.join(EIP712_DIR, "type_matrix_parity_baseline.json")

DOMAIN = {
    "name": "Ledger EIP712 type matrix",
    "version": "1",
    "chainId": 1,
    "verifyingContract": "0xCcCCccccCCCCcCCCCCCcCcCccCcCCCcCcccccccC",
}
ETD = [
    {"name": "name", "type": "string"},
    {"name": "version", "type": "string"},
    {"name": "chainId", "type": "uint256"},
    {"name": "verifyingContract", "type": "address"},
]
A2 = "0x2222222222222222222222222222222222222222"
A3 = "0x3333333333333333333333333333333333333333"


def _val(t: str, alt: bool):
    # Mirror gen-parity-matrix.mjs exactly so the re-derived hash matches.
    if t == "bool":
        return False if alt else True
    if t == "address":
        return A3 if alt else A2
    if t == "string":
        return "second" if alt else "first-é✓"
    if t == "bytes":
        return "0x00" if alt else "0xdeadbeefcafe"
    if t.startswith("uint"):
        b = int(t[4:])
        return "1" if alt else ("200" if b <= 8 else str(2 ** (b - 1) + 12345))
    if t.startswith("int"):
        b = int(t[3:])
        return "-1" if alt else ("-100" if b <= 8 else str(-(2 ** (b - 2)) + 7))
    if t.startswith("bytes"):
        n = int(t[5:])
        return ("0x" + "11" * n) if alt else ("0x" + "ab" * n)
    raise ValueError(t)


def _build(key: str) -> dict:
    t, shape = key.split("|")
    if shape == "scalar":
        field_type, value = t, _val(t, False)
    elif shape == "dyn":
        field_type, value = f"{t}[]", [_val(t, False), _val(t, True)]
    elif shape == "fixed2":
        field_type, value = f"{t}[2]", [_val(t, False), _val(t, True)]
    else:
        raise ValueError(shape)
    return {
        "types": {"EIP712Domain": ETD, "M": [{"name": "v", "type": field_type}]},
        "primaryType": "M",
        "domain": DOMAIN,
        "message": {"v": value},
    }


def _eth_account_hash(data: dict) -> str:
    return "0x" + encode_typed_data(full_message=data).body.hex()


with open(BASELINE_FILE, encoding="utf-8") as _f:
    _BASELINE = json.load(_f)["entries"]


@pytest.mark.parametrize("key", sorted(_BASELINE.keys()))
def test_eip712_type_matrix_parity(key: str):
    """eth_account (recover_message oracle) must match the pinned eth-sig-util/ethers hash."""
    expected = _BASELINE[key]
    got = _eth_account_hash(_build(key))
    assert got == expected, (
        f"{key}: eth_account messageHash {got} != eth-sig-util/ethers baseline {expected}"
    )


def test_eip712_type_matrix_coverage():
    """Every elementary type must appear in all three shapes."""
    elementary = (
        [f"uint{b}" for b in range(8, 257, 8)]
        + [f"int{b}" for b in range(8, 257, 8)]
        + [f"bytes{n}" for n in range(1, 33)]
        + ["bool", "address", "string", "bytes"]
    )
    assert len(elementary) == 100
    for t in elementary:
        for shape in ("scalar", "dyn", "fixed2"):
            assert f"{t}|{shape}" in _BASELINE, f"missing {t}|{shape}"
    assert len(_BASELINE) == 300
