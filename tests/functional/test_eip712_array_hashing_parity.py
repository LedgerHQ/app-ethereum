# EIP-712 array-of-struct hashing regression — MetaMask / ethers parity guard.
#
# Background
# ----------
# A defect in src/features/sign_message_eip712/path.c mis-hashed EIP-712
# *arrays of structs* in two independent ways:
#
#   Bug A — an array element struct whose ABI-encoding length is an exact
#           multiple of the 136-byte keccak-256 rate (e.g. a 16-field struct:
#           typeHash + 16*32 = 544 = 4*136) was judged "empty" and silently
#           dropped from the array fold, so the array hashed as if it had no
#           elements.
#   Bug B — nested fixed arrays of structs at depth >= 2 (e.g. T[2][2]) had
#           their accumulator sponge state corrupted, producing a wrong,
#           value-dependent hash. This covers real bulk-order trees such as
#           Seaport's BulkOrder = OrderComponents[2][2].
#
# Oracle
# ------
# The whole functional suite (test_eip712.py) verifies a device signature by
# recovering the signer with eth_account.recover_message(), which internally
# computes the EIP-712 digest via eth_account.messages.encode_typed_data().
# That digest is the same one @metamask/eth-sig-util (hashStruct V4) and
# ethers v6 (TypedDataEncoder) independently agree on. If the device hashes an
# array wrong, recovery yields a different address and the device test fails.
#
# This module is a fast, Speculos-free guard that pins the *expected* side of
# that equality: for every array-hashing fixture added under
# eip712_input_files/, it asserts that the repo's own oracle (eth_account,
# i.e. the body of recover_message) reproduces the published eth-sig-util /
# ethers reference messageHash. It also documents which fixtures are bug
# triggers vs. unaffected controls. The on-device half of the proof is
# test_eip712.py::test_eip712_new, which signs each fixture and asserts
# DEVICE_ADDR == recover_message(...).

import json
import os

import pytest
from eth_account.messages import encode_typed_data


EIP712_DIR = os.path.join(os.path.dirname(__file__), "eip712_input_files")
REFERENCES_FILE = os.path.join(EIP712_DIR, "array_hashing_references.json")


def _load_references() -> dict:
    with open(REFERENCES_FILE, encoding="utf-8") as f:
        refs = json.load(f)
    refs.pop("_comment", None)
    return refs


def _eip712_message_hash(data: dict) -> str:
    # Same code path recover_message() uses to validate every device signature:
    # encode_typed_data().body is keccak256(hashStruct(primaryType, message)),
    # i.e. the EIP-712 message hash. This equals @metamask/eth-sig-util's
    # hashStruct(...) and ethers' TypedDataEncoder.hashStruct(...).
    return "0x" + encode_typed_data(full_message=data).body.hex()


REFERENCES = _load_references()


@pytest.mark.parametrize("name", sorted(REFERENCES.keys()))
def test_eip712_array_hashing_parity(name: str):
    """The repo oracle (eth_account) must match the eth-sig-util/ethers hash.

    Runs without Speculos; pure parity check on each fixture's expected hash.
    """
    ref = REFERENCES[name]
    data_file = os.path.join(EIP712_DIR, f"{name}-data.json")
    assert os.path.isfile(data_file), f"missing fixture {data_file}"
    with open(data_file, encoding="utf-8") as f:
        data = json.load(f)

    got = _eip712_message_hash(data)
    assert got == ref["messageHash"], (
        f"{name} ({ref['shape']}, triggers={ref['triggers']}): "
        f"eth_account messageHash {got} != eth-sig-util/ethers reference "
        f"{ref['messageHash']}"
    )


def test_eip712_array_hashing_coverage():
    """The known bug shapes must all be represented among the fixtures."""
    shapes = {v["shape"] for v in REFERENCES.values()}
    required = {
        "SaleApproval[2]",        # Bug A: 16-field element at 136-byte boundary
        "SaleApproval[2][2]",     # Bug A + Bug B: nested fixed array
        "SaleApproval[2][2][2]",  # Bug B: triple-nested fixed array
        "SaleApprovalY[2][2]",    # Bug B only: 15-field element, nested
        "OrderComponents[2][2]",  # Bug B: Seaport BulkOrder bulk tree
        # --- structural vectors (NN>=25) ---
        "Elem16[2]",              # Bug A: 16-field element, fixed array
        "Elem33[2]",              # Bug A: 33-field element (1088 = 8*136)
        "Elem50[2]",              # Bug A: 50-field element (1632 = 12*136)
        "Elem16[]",               # Bug A: dynamic-array 16-field boundary (review gap)
        "Wrap{Elem16}",           # Bug A: directly-nested 16-field at depth>0
        "Root{Elem16}",           # Bug A: minimal single directly-nested 16-field
        "Small[2][2]",            # Bug B: nested fixed custom array
        "Small[2][2][2]",         # Bug B: triple-nested fixed custom array
        "S0{bytes4[]=[],S2}",     # Class C: empty SCALAR array then struct
        "Batch{Call[]=[],Meta}",  # Class C: empty CUSTOM-struct array then struct
        "Batch{Call[]=[],Meta{Inner,..}}",  # Class C: empty custom array then nested struct
        "Outer[]{Inner head,..}", # Class R reg-guard: struct-first element, dynamic
        "Outer[2]{Inner head,..}",# Class R reg-guard: struct-first element, fixed
        # --- GAP-A / GAP-B (previously carved out; now fixed pass-on-fixed) ---
        "Elem[2][2]{Child c,uint256 n}",   # GAP-A: struct-first element, fixed nested
        "Elem[][]{Child c,uint256 n}",     # GAP-A: struct-first element, dynamic nested
        "Elem[2][2][2]{Child c,uint256 n}",# GAP-A: struct-first element, triple fixed
        "Leaf[][] x=[[]]",                 # GAP-B: empty inner custom-struct array
        "Leaf[][][] x=[[[]]]",             # GAP-B: empty inner at depth 3
        "Leaf[][] x=[[],[leaf],[]]",       # GAP-B: mixed empty/non-empty inner
    }
    missing = required - shapes
    assert not missing, f"missing bug-trigger shapes: {sorted(missing)}"

    # GAP-A and GAP-B were previously carved out under known_uncovered; they are
    # now real pass-on-fixed fixtures and must be present as bug triggers.
    gap_triggers = {v["triggers"] for v in REFERENCES.values()}
    assert {"gapA", "gapB"} <= gap_triggers, (
        f"GAP-A/GAP-B fixtures missing from references: have {sorted(gap_triggers)}"
    )

    # The eth-sig-util/ethers per-type parity matrix is bundled here too.
    parity = {v["shape"] for v in REFERENCES.values() if v["triggers"] == "none-parity"}
    assert {"ParityMatrix(scalar*all)", "ParityMatrix(T[]*all)", "ParityMatrix(T[2]*all)"} <= parity

    controls = {
        v["shape"] for v in REFERENCES.values() if v["triggers"] == "none-control"
    }
    assert controls, "expected at least one unaffected control fixture"
