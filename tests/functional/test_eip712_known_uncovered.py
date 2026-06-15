# EIP-712 array-of-struct hashing -- KNOWN-UNCOVERED shapes (carve-out guard).
#
# Scope of the fix in src/features/sign_message_eip712/path.c
# ----------------------------------------------------------
# This PR fixes Bug A (keccak-rate boundary), Bug B (nested fixed arrays of
# structs), Class R (struct-first element regression guard) and Class C (empty
# array immediately before a struct field, scalar- and custom-struct-element).
#
# Two PRE-EXISTING defects remain UNCOVERED and are explicitly carved out here.
# They are NOT regressions: develop mis-hashes them identically. They only
# affect NON-canonical nested arrays of custom structs; no mainstream payload
# (Safe, Permit2, Seaport single-level and bulk OrderComponents[2][2],
# ERC-2612, ENS, CoW) is affected.
#
#   GAP-A -- a nested array (depth >= 2, fixed or dynamic) whose element struct's
#            FIRST field is itself a custom struct (e.g. Elem[2][2] with
#            Elem{Child c; ...}). The single-level Class-R case (Elem[]/Elem[2])
#            is fixed; the scalar-first nested case (Bug B / Seaport bulk) is
#            fixed; only their intersection at inner array levels is unhandled.
#   GAP-B -- an empty CUSTOM-struct array occupying an INNER dimension of a
#            multi-dimensional struct array (e.g. Leaf[][] with x=[[]]). The
#            scalar analogue uint256[][]=[[]] is correct; only the custom-struct
#            element kind nested as an inner empty dimension is unhandled.
#
# What this module guards
# -----------------------
# 1. The oracle for each carved-out shape is UNAMBIGUOUS: eth_account (== the
#    body of recover_message(), == eth-sig-util V4 == ethers v6) reproduces the
#    pinned messageHash. So the documented limitation is real, not an oracle
#    artifact.
# 2. The carved-out fixtures are intentionally kept OUT of the device-signing
#    auto-discovery in test_eip712.py (their filenames are not '*-data.json'),
#    so they cannot silently become a failing on-device test; this asserts that
#    invariant so a future rename is caught.
#
# If/when path.c is extended to cover these, move the corresponding fixture to a
# 'NN-...-data.json' name and add its reference to array_hashing_references.json.

import json
import os

import pytest
from eth_account.messages import encode_typed_data


EIP712_DIR = os.path.join(os.path.dirname(__file__), "eip712_input_files")
REFERENCES_FILE = os.path.join(EIP712_DIR, "known_uncovered_references.json")


def _load_references() -> dict:
    with open(REFERENCES_FILE, encoding="utf-8") as f:
        refs = json.load(f)
    refs.pop("_comment", None)
    return refs


REFERENCES = _load_references()


@pytest.mark.parametrize("name", sorted(REFERENCES.keys()))
def test_eip712_known_uncovered_oracle_is_unambiguous(name: str):
    """The carved-out shape's oracle hash is reproducible and unambiguous."""
    ref = REFERENCES[name]
    data_file = os.path.join(EIP712_DIR, f"{name}.json")
    assert os.path.isfile(data_file), f"missing fixture {data_file}"
    with open(data_file, encoding="utf-8") as f:
        data = json.load(f)

    got = "0x" + encode_typed_data(full_message=data).body.hex()
    assert got == ref["messageHash"], (
        f"{name} ({ref['shape']}, {ref['class']}): eth_account messageHash "
        f"{got} != pinned eth-sig-util/ethers reference {ref['messageHash']}"
    )


def test_known_uncovered_fixtures_excluded_from_device_signing():
    """Carved-out fixtures must not be device-signed by test_eip712_new.

    test_eip712.py auto-discovers and signs every '*-data.json' under
    eip712_input_files/. The known-uncovered fixtures deliberately do NOT use
    that suffix, so the device is never asked to sign a shape it mis-hashes.
    """
    for name in REFERENCES:
        assert not name.endswith("-data"), (
            f"{name}: known-uncovered fixture must not use the '*-data.json' "
            f"device-signing suffix"
        )


def test_known_uncovered_classes_documented():
    """Both carved-out classes are present so the carve-out stays explicit."""
    classes = {v["class"] for v in REFERENCES.values()}
    assert {"GAP-A", "GAP-B"} <= classes, f"missing carve-out classes: {classes}"
