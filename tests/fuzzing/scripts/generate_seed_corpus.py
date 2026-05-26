#!/usr/bin/env python3
"""Custom seed corpus generator for the Ethereum fuzz campaign.

Scope: targeted seeds for Generic Transaction Parser (GTP), EIP-712, EIP-7702,
provide_* TLV flows, and tx_simulation. Each seed is a self-contained single
APDU: Absolution prefix + tail (CLA, ins_idx, P1, P2, 2-byte TLV size, TLV body).
Prefix size is resolved dynamically from scenario_layout.h; it shrinks when
invariant overrides constrain pointer globals to a single value. State-dependent
INS are paired with a prefix that sets appState to SIGNING_TX (1) or
SIGNING_EIP712 (3) via the scenario_layout offset.

State shape the harness expects (see `app-ethereum/fuzzing/harness/fuzz_dispatcher.c`):
  - Fuzz commands use raw-lane dispatch: tail[0]=CLA (ignored), tail[1]=cmd_idx,
    tail[2]=P1 (clamped to spec->p1_max), tail[3]=P2 (clamped), tail[4..]=payload.
  - TLV INS expect payload[0..1]=BE tlv_size, payload[2..]=TLV body.
  - Swap pseudo-commands use raw payload bytes at tail[4..] (no TLV length prefix).
  - GTP flows use a harness-side bootstrap in fuzz_apdu_adapter.c that creates
    tx_ctx + parked calldata + tx_info when appState=SIGNING_TX and the command is
    INS_GTP_FIELD or INS_GTP_TRANSACTION_INFO (selector=0xAABBCCDD, chain_id=1).
  - EIP-712 flows use a bootstrap that calls eip712_context_init() when
    appState=SIGNING_EIP712 and the command is STRUCT_IMPL/FILTERING/SIGN.
  - Safe account signer descriptor flows use a bootstrap that allocates SAFE_DESC
    when the command is INS_PROVIDE_SAFE_ACCOUNT with P2=SIGNER_DESCRIPTOR.
"""
from __future__ import annotations

import os
import struct
import sys

SCRIPT_DIR = os.path.dirname(os.path.realpath(__file__))
BOLOS_SDK = os.environ.get("BOLOS_SDK")
SDK_SCRIPTS_DIR = os.environ.get(
    "SDK_FUZZ_SCRIPTS",
    os.path.join(BOLOS_SDK, "fuzzing", "scripts")
    if BOLOS_SDK
    else os.path.realpath(
        os.path.join(SCRIPT_DIR, "..", "..", "..", "..", "ledger-secure-sdk", "fuzzing", "scripts")
    ),
)
sys.path.insert(0, SDK_SCRIPTS_DIR)

from fuzz_seed_utils import (  # noqa: E402
    parse_layout_header,
    resolve_prefix_size,
    resolve_seed_prefix,
    validate_prefix_size,
    make_prefix_with_ctrl,
    raw_ctrl_bytes,
    get_layout_header_path,
)
from fuzz_command_registry import command_index_map  # noqa: E402


LAYOUT_HEADER = get_layout_header_path()
_LAYOUT_DEFS = parse_layout_header(LAYOUT_HEADER)
CTRL_OFF = _LAYOUT_DEFS.get("SCEN_CTRL_OFF", 0)
CTRL_LEN = _LAYOUT_DEFS.get("SCEN_CTRL_LEN", 16)
# appState byte offset inside the Absolution prefix (tracked via
# fuzz-manifest [layout].extra_args = "--global appState:SCEN_APPSTATE_OFF").
# Fallback of -1 disables the app-state override and the prefix byte is left
# as whatever Absolution's fuzzer.seed contains at that position.
APPSTATE_OFFSET = _LAYOUT_DEFS.get("SCEN_APPSTATE_OFF", -1)

CLA = 0xE0

APP_STATE_IDLE = 0
APP_STATE_SIGNING_TX = 1
APP_STATE_SIGNING_EIP712 = 3


CMD_IDX = command_index_map()

P1_FIRST_CHUNK = 0x01


def tlv(tag: int, value: bytes) -> bytes:
    """Encode a single TLV entry with a single-byte length (fits 0..255)."""
    if len(value) > 255:
        raise ValueError(f"tlv length {len(value)} exceeds 255")
    return bytes([tag & 0xFF, len(value) & 0xFF]) + value


# DER-encoded ECDSA signature, exactly 70 bytes, matches
# CX_ECDSA_SHA256_SIG_MIN_ASN1_LENGTH=70 / MAX=72 constraint used by every
# provide_* TLV parser. Contents are irrelevant because the fuzz build uses
# a link-time mock of check_signature_with_pubkey (in mock/mocks.c) that
# always returns true; only the DER framing and byte length matter.
DER_SIG_70 = b"\x30\x44\x02\x20" + b"\x00" * 32 + b"\x02\x20" + b"\x00" * 32


def build_tail(cmd_idx: int, p1: int, p2: int, tlv_body: bytes) -> bytes:
    """Assemble the 4-byte APDU header + 2-byte BE TLV size + TLV body."""
    tlv_size = len(tlv_body)
    header = bytes([CLA & 0xFF, cmd_idx & 0xFF, p1 & 0xFF, p2 & 0xFF])
    return header + struct.pack(">H", tlv_size) + tlv_body


def build_raw_tail(cmd_idx: int, p1: int, p2: int, payload: bytes) -> bytes:
    """Assemble the 4-byte command header + raw payload bytes."""
    return bytes([CLA & 0xFF, cmd_idx & 0xFF, p1 & 0xFF, p2 & 0xFF]) + payload


def build_prefix(base_prefix: bytes, *, app_state: int = APP_STATE_IDLE) -> bytes:
    """Overlay raw-lane ctrl bytes and state bytes onto the base prefix."""
    prefix = make_prefix_with_ctrl(base_prefix, CTRL_OFF, raw_ctrl_bytes(CTRL_LEN))
    buf = bytearray(prefix)
    if 0 <= APPSTATE_OFFSET < len(buf):
        buf[APPSTATE_OFFSET] = app_state & 0xFF
    return bytes(buf)


# ─── GTP TX_INFO payload (version 1, all required tags) ────────────────────

def build_tx_info_v1_tlv(
    *,
    chain_id: int = 1,
    contract_addr: bytes = b"\x00" * 20,
    selector: bytes = b"\xaa\xbb\xcc\xdd",
    fields_hash: bytes = b"\x00" * 32,
    operation: bytes = b"Swap",
) -> bytes:
    """Assemble a complete TX_INFO TLV body.

    Required tags per `verify_tx_info_struct` (version 1): VERSION, CHAIN_ID,
    CONTRACT_ADDR, SELECTOR, FIELDS_HASH, OPERATION_TYPE, SIGNATURE. Signature
    bytes are irrelevant (link-time mock in mock/mocks.c returns true).
    The harness-side bootstrap (fuzz_apdu_adapter.c) provides prerequisite
    tx_ctx state so these seeds can exercise the deep TLV parsing.
    """
    parts = [
        tlv(0x00, b"\x01"),
        tlv(0x01, struct.pack(">Q", chain_id)),
        tlv(0x02, contract_addr),
        tlv(0x03, selector),
        tlv(0x04, fields_hash),
        tlv(0x05, operation),
        tlv(0xFF, b"\x00" * 64),
    ]
    return b"".join(parts)


# ─── GTP FIELD payload factories ───────────────────────────────────────────

def _value_tlv(
    *,
    type_family: int = 1,  # TF_UINT
    type_size: int = 1,
    source_tlv: bytes = b"",
) -> bytes:
    """Assemble s_value TLV body. source_tlv must already encode the source tag."""
    parts = [
        tlv(0x00, b"\x01"),                    # VERSION
        tlv(0x01, bytes([type_family])),       # TYPE_FAMILY
        tlv(0x02, bytes([type_size])),         # TYPE_SIZE
    ]
    if source_tlv:
        parts.append(source_tlv)
    return b"".join(parts)


def _data_path_tlv(elements: list[bytes]) -> bytes:
    """TAG_DATA_PATH(0x03) body: VERSION + sequence of element TLVs."""
    body = tlv(0x00, b"\x01") + b"".join(elements)
    return tlv(0x03, body)


def _dp_tuple(offset: int = 0) -> bytes:
    """TAG_TUPLE(0x01): uint16 BE calldata offset."""
    return tlv(0x01, struct.pack(">H", offset & 0xFFFF))


def _dp_leaf(leaf_type: int) -> bytes:
    """TAG_LEAF(0x04): uint8 leaf type. STATIC=3, DYNAMIC=4."""
    return tlv(0x04, bytes([leaf_type & 0xFF]))


def _dp_ref() -> bytes:
    """TAG_REF(0x03): empty payload (0-length)."""
    return tlv(0x03, b"")


def _dp_array(*, weight: int = 1, start: int | None = None, end: int | None = None) -> bytes:
    """TAG_ARRAY(0x02): WEIGHT (uint8, mandatory) + optional START/END (uint16 BE).

    Exercises `handle_array` in gtp_data_path.c and `handle_weight/start/end`
    in gtp_path_array.c. WEIGHT is the byte stride between array elements;
    START/END select a sub-range when present.
    """
    parts = [tlv(0x01, bytes([weight & 0xFF]))]
    if start is not None:
        parts.append(tlv(0x02, struct.pack(">H", start & 0xFFFF)))
    if end is not None:
        parts.append(tlv(0x03, struct.pack(">H", end & 0xFFFF)))
    return tlv(0x02, b"".join(parts))


def _dp_slice(*, start: int | None = None, end: int | None = None) -> bytes:
    """TAG_SLICE(0x05): optional START + END (uint16 BE).

    Exercises `handle_slice` in gtp_data_path.c and `handle_start/end` in
    gtp_path_slice.c. SLICE must follow a LEAF element to operate on its
    collected value (see `path_slice` which trims `collection->size-1`).
    """
    parts = []
    if start is not None:
        parts.append(tlv(0x01, struct.pack(">H", start & 0xFFFF)))
    if end is not None:
        parts.append(tlv(0x02, struct.pack(">H", end & 0xFFFF)))
    return tlv(0x05, b"".join(parts))


def _constant_tlv(value: bytes) -> bytes:
    """TAG_CONSTANT(0x05)."""
    return tlv(0x05, value)


def _container_path_tlv(kind: int) -> bytes:
    """TAG_CONTAINER_PATH(0x04): single byte."""
    return tlv(0x04, bytes([kind & 0xFF]))


def build_field_raw_constant(value: bytes = b"\x42") -> bytes:
    """GTP_FIELD body with PARAM_TYPE_RAW and a constant source value."""
    name = b"RawConst"
    param_raw = b"".join([
        tlv(0x00, b"\x01"),                             # VERSION
        tlv(0x01, _value_tlv(                           # TAG_VALUE
            type_family=1, type_size=len(value),
            source_tlv=_constant_tlv(value),
        )),
    ])
    return b"".join([
        tlv(0x00, b"\x01"),
        tlv(0x01, name),
        tlv(0x02, bytes([0])),                          # PARAM_TYPE_RAW=0
        tlv(0x03, param_raw),
        tlv(0x04, bytes([0])),                          # VISIBLE
    ])


def build_field_raw_datapath(elements: list[bytes], *, type_family: int = 1, type_size: int = 32) -> bytes:
    """GTP_FIELD with PARAM_RAW + TAG_DATA_PATH source (exercises gtp_data_path)."""
    name = b"RawData"
    param_raw = b"".join([
        tlv(0x00, b"\x01"),
        tlv(0x01, _value_tlv(
            type_family=type_family, type_size=type_size,
            source_tlv=_data_path_tlv(elements),
        )),
    ])
    return b"".join([
        tlv(0x00, b"\x01"),
        tlv(0x01, name),
        tlv(0x02, bytes([0])),
        tlv(0x03, param_raw),
        tlv(0x04, bytes([0])),
    ])


def build_field_raw_container(kind: int = 1) -> bytes:
    """GTP_FIELD with PARAM_RAW sourced from a container path (from, to, value, chain_id)."""
    name = b"RawContainer"
    param_raw = b"".join([
        tlv(0x00, b"\x01"),
        tlv(0x01, _value_tlv(
            type_family=5,  # TF_ADDRESS
            type_size=20,
            source_tlv=_container_path_tlv(kind),
        )),
    ])
    return b"".join([
        tlv(0x00, b"\x01"),
        tlv(0x01, name),
        tlv(0x02, bytes([0])),
        tlv(0x03, param_raw),
        tlv(0x04, bytes([0])),
    ])


def _build_field(name: bytes, param_type: int, param_body: bytes, *, visible: int = 0) -> bytes:
    """Generic GTP FIELD wrapper. param_body is the PARAM_TYPE-specific TLV body
    that will be wrapped inside TAG_PARAM(0x03). VISIBLE defaults to ALWAYS(0).
    """
    return b"".join([
        tlv(0x00, b"\x01"),
        tlv(0x01, name),
        tlv(0x02, bytes([param_type & 0xFF])),
        tlv(0x03, param_body),
        tlv(0x04, bytes([visible & 0xFF])),
    ])


def _value_uint(*, type_size: int = 32, value: bytes | None = None) -> bytes:
    """Convenience: s_value TLV for TF_UINT sourced from a constant of the
    requested size. Defaults to a 32-byte 0x...0x42 constant.
    """
    if value is None:
        value = b"\x00" * (type_size - 1) + b"\x42"
    return _value_tlv(type_family=1, type_size=type_size, source_tlv=_constant_tlv(value))


def _value_address(addr: bytes = b"\x00" * 20) -> bytes:
    """Convenience: s_value TLV for TF_ADDRESS (family=5, size=20)."""
    return _value_tlv(type_family=5, type_size=20, source_tlv=_constant_tlv(addr))


def _value_bytes(value: bytes) -> bytes:
    """Convenience: s_value TLV for TF_BYTES (family=7) sourced from a constant."""
    return _value_tlv(type_family=7, type_size=len(value), source_tlv=_constant_tlv(value))


def build_param_amount() -> bytes:
    """PARAM_AMOUNT: VERSION + VALUE."""
    return b"".join([tlv(0x00, b"\x01"), tlv(0x01, _value_uint())])


def build_param_token_amount(*, threshold: bytes = b"") -> bytes:
    """PARAM_TOKEN_AMOUNT: VERSION + VALUE + TOKEN (+ optional THRESHOLD).

    TOKEN is itself an s_value of family TF_ADDRESS so format_param_token_amount
    can resolve the token info; THRESHOLD is a raw uint256 BE payload (<=32 B).
    """
    parts = [
        tlv(0x00, b"\x01"),
        tlv(0x01, _value_uint()),
        tlv(0x02, _value_address()),
    ]
    if threshold:
        parts.append(tlv(0x04, threshold))
    return b"".join(parts)


def build_param_nft() -> bytes:
    """PARAM_NFT: VERSION + ID + COLLECTION (both s_value)."""
    return b"".join([
        tlv(0x00, b"\x01"),
        tlv(0x01, _value_uint()),
        tlv(0x02, _value_address()),
    ])


def build_param_datetime(dt_type: int = 0) -> bytes:
    """PARAM_DATETIME: VERSION + VALUE + TYPE (0=DT_UNIX, 1=DT_BLOCKHEIGHT)."""
    return b"".join([
        tlv(0x00, b"\x01"),
        tlv(0x01, _value_uint(type_size=8, value=struct.pack(">Q", 0x12345678))),
        tlv(0x02, bytes([dt_type & 0xFF])),
    ])


def build_param_duration() -> bytes:
    """PARAM_DURATION: VERSION + VALUE (uint seconds)."""
    return b"".join([
        tlv(0x00, b"\x01"),
        tlv(0x01, _value_uint(type_size=8, value=struct.pack(">Q", 3600))),
    ])


def build_param_unit() -> bytes:
    """PARAM_UNIT: VERSION + VALUE + BASE (NUL-terminated ASCII) + DECIMALS + PREFIX (1 B)."""
    return b"".join([
        tlv(0x00, b"\x01"),
        tlv(0x01, _value_uint(type_size=8, value=struct.pack(">Q", 42))),
        tlv(0x02, b"ETH"),
        tlv(0x03, bytes([8])),
        tlv(0x04, bytes([0])),
    ])


def build_param_token() -> bytes:
    """PARAM_TOKEN: VERSION + ADDRESS (+ optional NATIVE_CURRENCY)."""
    return b"".join([
        tlv(0x00, b"\x01"),
        tlv(0x01, _value_address()),
    ])


def build_param_network(chain_id: int = 1) -> bytes:
    """PARAM_NETWORK: VERSION + VALUE (s_value TF_UINT size 8, BE chain_id).

    Only TF_UINT is accepted by `format_param_network`; chain_id must be in
    [1, MAX_VALID_CHAIN_ID] per EIP-2294.
    """
    return b"".join([
        tlv(0x00, b"\x01"),
        tlv(0x01, _value_uint(type_size=8, value=struct.pack(">Q", chain_id))),
    ])


def build_param_trusted_name(*, types: bytes = b"\x02\x04\x03",
                             sources: bytes = b"\x01") -> bytes:
    """PARAM_TRUSTED_NAME: VERSION + VALUE + TYPES + SOURCES.

    TYPES is a packed list of TN_TYPE_* values (e.g. CONTRACT=2, TOKEN=4,
    NFT=3); SOURCES is a packed list of TN_SOURCE_* (CAL=1).
    """
    return b"".join([
        tlv(0x00, b"\x01"),
        tlv(0x01, _value_address()),
        tlv(0x02, types),
        tlv(0x03, sources),
    ])


def build_param_calldata() -> bytes:
    """PARAM_CALLDATA: VERSION + VALUE (calldata bytes) + CALLEE (address).

    CHAIN_ID/SELECTOR/AMOUNT/SPENDER are intentionally omitted - libFuzzer
    can grow them organically from the dictionary tokens.
    """
    return b"".join([
        tlv(0x00, b"\x01"),
        tlv(0x01, _value_bytes(b"\xaa\xbb\xcc\xdd" + b"\x00" * 28)),
        tlv(0x02, _value_address()),
    ])


def build_field_enum(id_v: int = 0, val_v: int = 0) -> bytes:
    """GTP_FIELD PARAM_TYPE_ENUM: exercises enum lookup (id,val in 0..3)."""
    name = b"EnumVal"
    param_enum = b"".join([
        tlv(0x00, b"\x01"),                                # VERSION
        tlv(0x01, bytes([id_v & 0xFF])),                   # TAG_ID
        tlv(0x02, _value_tlv(                              # TAG_VALUE
            type_family=1, type_size=1,
            source_tlv=_constant_tlv(bytes([val_v & 0xFF])),
        )),
    ])
    return b"".join([
        tlv(0x00, b"\x01"),
        tlv(0x01, name),
        tlv(0x02, bytes([7])),                             # PARAM_TYPE_ENUM=7
        tlv(0x03, param_enum),
        tlv(0x04, bytes([0])),
    ])


# ─── PROVIDE_ENUM_VALUE payload (standalone, signature bypassed) ──────────

def build_enum_value_tlv(*, chain_id: int = 1, id_v: int = 0, val_v: int = 0) -> bytes:
    return b"".join([
        tlv(0x00, b"\x01"),
        tlv(0x01, struct.pack(">Q", chain_id)),
        tlv(0x02, b"\x00" * 20),
        tlv(0x03, b"\xaa\xbb\xcc\xdd"),
        tlv(0x04, bytes([id_v])),
        tlv(0x05, bytes([val_v])),
        tlv(0x06, b"fuzz_enum"),
        tlv(0xFF, b"\x00" * 64),
    ])


# ─── PROVIDE_TRUSTED_NAME (struct version 2: account/address) ─────────────

def build_trusted_name_v2_tlv(*, name_type: int = 2, name_source: int = 1) -> bytes:
    """TRUSTED_NAME struct version 2 (account/address form).

    Defaults match `verify_trusted_name_struct` success rules for contract
    entries: `name_type = TN_TYPE_CONTRACT(2)` with `name_source = TN_SOURCE_CAL(1)`.
    """
    return b"".join([
        tlv(0x01, b"\x03"),                     # STRUCTURE_TYPE (TRUSTED_NAME)
        tlv(0x02, b"\x02"),                     # STRUCTURE_VERSION
        tlv(0x13, b"\x01"),                     # SIGNER_KEY_ID
        tlv(0x14, b"\x01"),                     # SIGNER_ALGO
        tlv(0x20, b"fuzzer.eth"),               # TRUSTED_NAME
        tlv(0x22, b"\x00" * 20),                # ADDRESS
        tlv(0x23, b"\x00\x00\x00\x00\x00\x00\x00\x01"),  # CHAIN_ID
        tlv(0x70, bytes([name_type])),          # NAME_TYPE (CONTRACT=2 by default)
        tlv(0x71, bytes([name_source])),        # NAME_SOURCE (CAL=1 by default)
        tlv(0x15, DER_SIG_70),                  # DER_SIGNATURE (bypassed)
    ])


def build_trusted_name_v1_tlv() -> bytes:
    return b"".join([
        tlv(0x01, b"\x03"),
        tlv(0x02, b"\x01"),
        tlv(0x12, b"\x00\x00\x00\x00"),         # CHALLENGE
        tlv(0x13, b"\x01"),
        tlv(0x14, b"\x01"),
        tlv(0x20, b"fuzzer.eth"),
        tlv(0x21, b"\x00\x00\x00\x3c"),         # COIN_TYPE (Ethereum)
        tlv(0x22, b"\x00" * 20),
        tlv(0x15, DER_SIG_70),
    ])


def build_trusted_name_v2_mab_tlv() -> bytes:
    """TRUSTED_NAME v2 MAB source: account + owner + owner_deriv_path.

    Exercises `verify_fields` TN_SOURCE_MAB branch (requires tags 0x74, 0x75) and
    the BIP32 owner-derivation path in `matching_trusted_name` / ENS lookup.
    """
    bip32_path = b"\x05" + struct.pack(">I", 0x8000002C) + struct.pack(">I", 0x8000003C) + \
                 struct.pack(">I", 0x80000000) + b"\x00\x00\x00\x00" + b"\x00\x00\x00\x00"
    return b"".join([
        tlv(0x01, b"\x03"),
        tlv(0x02, b"\x02"),
        tlv(0x13, b"\x01"),
        tlv(0x14, b"\x01"),
        tlv(0x20, b"owner.eth"),
        tlv(0x22, b"\x00" * 20),
        tlv(0x23, b"\x00\x00\x00\x00\x00\x00\x00\x01"),
        tlv(0x70, b"\x01"),                     # NAME_TYPE = ACCOUNT
        tlv(0x71, b"\x07"),                     # NAME_SOURCE = MAB (7)
        tlv(0x74, b"\x00" * 20),                # OWNER (20 bytes address)
        tlv(0x75, bip32_path),                  # OWNER_DERIV_PATH (m/44'/60'/0'/0/0)
        tlv(0x15, DER_SIG_70),
    ])


# ─── PROVIDE_PROXY_INFO, NETWORK, SAFE, GATING, TX_SIMULATION ─────────────

def build_proxy_info_tlv() -> bytes:
    """PROXY_INFO struct: TYPE_PROXY_INFO = 0x26 (not 0x04)."""
    return b"".join([
        tlv(0x01, b"\x26"),
        tlv(0x02, b"\x01"),
        tlv(0x12, b"\x00\x00\x00\x00"),
        tlv(0x22, b"\x00" * 20),
        tlv(0x23, b"\x00\x00\x00\x00\x00\x00\x00\x01"),
        tlv(0x41, b"\xaa\xbb\xcc\xdd"),         # TAG_SELECTOR (matches gcs_init)
        tlv(0x42, b"\x11" * 20),                # TAG_IMPLEM_ADDRESS
        tlv(0x43, b"\x00"),                     # TAG_DELEGATION_TYPE
        tlv(0x15, DER_SIG_70),
    ])


def build_network_info_tlv() -> bytes:
    """DYNAMIC_NETWORK struct: TYPE_DYNAMIC_NETWORK = 0x08, family = ETHEREUM (1).

    Icon hash (0x53) is dropped by default: all-zero hash is rejected by
    `handle_icon_hash` and a matching 32-byte hash is unknown without a prior
    INS_PROVIDE_NETWORK_CONFIGURATION icon chunk flow.
    """
    return b"".join([
        tlv(0x01, b"\x08"),                     # STRUCTURE_TYPE = DYNAMIC_NETWORK
        tlv(0x02, b"\x01"),
        tlv(0x51, b"\x01"),                     # BLOCKCHAIN_FAMILY = ETHEREUM (1)
        tlv(0x23, b"\x00\x00\x00\x00\x00\x00\x00\x01"),
        tlv(0x52, b"ETH"),                      # TICKER
        tlv(0x24, b"Mainnet"),                  # NAME
        tlv(0x15, DER_SIG_70),
    ])


def build_safe_desc_tlv() -> bytes:
    """LESM SAFE account struct: TYPE_LESM_ACCOUNT_INFO = 0x27 (not 0x10)."""
    return b"".join([
        tlv(0x01, b"\x27"),
        tlv(0x02, b"\x01"),
        tlv(0x12, b"\x00\x00\x00\x00"),
        tlv(0x22, b"\x00" * 20),
        tlv(0xa0, b"\x00\x01"),                 # threshold
        tlv(0xa1, b"\x00\x01"),                 # owners count
        tlv(0xa2, b"\x01"),                     # role
        tlv(0x15, DER_SIG_70),
    ])


def build_signer_desc_tlv() -> bytes:
    """Signer descriptor matching the fuzz-only SAFE_DESC auto-bootstrap path."""
    return b"".join([
        tlv(0x01, b"\x0A"),
        tlv(0x02, b"\x01"),
        tlv(0x12, b"\x00\x00\x00\x00"),
        tlv(0x22, b"\x11" * 20),
        tlv(0x22, b"\x22" * 20),
        tlv(0x22, b"\x33" * 20),
        tlv(0x15, DER_SIG_70),
    ])


def build_gating_tlv() -> bytes:
    """GATED_SIGNING struct: TYPE_GATED_SIGNING = 0x0D (not 0x20)."""
    return b"".join([
        tlv(0x01, b"\x0D"),
        tlv(0x02, b"\x01"),
        tlv(0x22, b"\x00" * 20),
        tlv(0x23, b"\x00\x00\x00\x00\x00\x00\x00\x01"),
        tlv(0x40, b"prelude"),
        tlv(0x82, b"details" + b"\x00" * 10),
        tlv(0x83, b"generic"),
        tlv(0x84, b"\x01"),
        tlv(0x15, DER_SIG_70),
    ])


def build_tx_simulation_tlv() -> bytes:
    """TX_SIMULATION struct: TYPE_TX_SIMULATION = 0x09 (not 0x40)."""
    return b"".join([
        tlv(0x01, b"\x09"),
        tlv(0x02, b"\x01"),
        tlv(0x22, b"\x00" * 20),
        tlv(0x23, b"\x00\x00\x00\x00\x00\x00\x00\x01"),
        tlv(0x27, b"\x00" * 32),
        tlv(0x28, b"\x00" * 32),
        tlv(0x80, b"\x01"),                     # risk
        tlv(0x81, b"\x00"),                     # category
        tlv(0x82, b"warning_msg"),
        tlv(0x83, b"tx_simulation_src"),
        tlv(0x84, b"\x01"),
        tlv(0x85, DER_SIG_70),
        tlv(0x15, DER_SIG_70),
    ])


# ─── PROVIDE_MAP_ENTRY ──────────────────────────────────────────────────────

def build_map_entry_tlv(*, entry_id: int = 1, key: bytes = b"\xAA", value: bytes = b"\xBB") -> bytes:
    return b"".join([
        tlv(0x00, b"\x01"),
        tlv(0x01, b"\x00\x00\x00\x00\x00\x00\x00\x01"),
        tlv(0x02, b"\x00" * 20),
        tlv(0x03, b"\xaa\xbb\xcc\xdd"),
        tlv(0x04, bytes([entry_id & 0xFF])),
        tlv(0x05, key),
        tlv(0x06, value),
        tlv(0xFF, DER_SIG_70),
    ])


# ─── EIP7702 AUTHORIZATION ────────────────────────────────────────────────

BIP32_ETH_PATH = (
    b"\x05"
    + struct.pack(">I", 0x8000002C)
    + struct.pack(">I", 0x8000003C)
    + struct.pack(">I", 0x80000000)
    + b"\x00\x00\x00\x00"
    + b"\x00\x00\x00\x00"
)

SIMPLE_7702_ACCOUNT = bytes([
    0x4C, 0xd2, 0x41, 0xE8, 0xd1, 0x51, 0x0e, 0x30, 0xb2, 0x07,
    0x63, 0x97, 0xaf, 0xc7, 0x50, 0x8A, 0xe5, 0x9C, 0x66, 0xc9,
])

EIP7702_TEST_ONE = b"\x01" * 20


def build_auth7702_payload(delegate: bytes = b"\x00" * 20, chain_id: int = 1) -> bytes:
    """BIP32 path prefix + 2-byte TLV size + TLV body for EIP-7702 auth.

    handle_sign_eip7702_authorization calls parseBip32 first (on P1_FIRST_CHUNK),
    consuming the BIP32 prefix, then passes the remainder to tlv_from_apdu which
    reads a 2-byte BE size then the TLV body.
    """
    tlv_body = b"".join([
        tlv(0x00, b"\x01"),                                   # VERSION
        tlv(0x01, delegate),                                   # DELEGATE_ADDR
        tlv(0x02, struct.pack(">Q", chain_id)),                # CHAIN_ID
        tlv(0x03, b"\x00\x00\x00\x00\x00\x00\x00\x00"),       # NONCE
    ])
    return BIP32_ETH_PATH + struct.pack(">H", len(tlv_body)) + tlv_body


# ─── EIP712 STRUCT_DEF / STRUCT_IMPL / FILTERING (non-TLV, legacy) ────────

def build_eip712_struct_def_name(name: bytes = b"Perm") -> bytes:
    """STRUCT_DEF P2=0 (name/type header)."""
    return name


def build_eip712_struct_def_field(type_desc: int = 0x42, type_size: int = 32,
                                  field_name: bytes = b"amount") -> bytes:
    """STRUCT_DEF P2=FIELD: [type_desc][type_size?][name_len][name]."""
    out = bytearray([type_desc, type_size, len(field_name)])
    out += field_name
    return bytes(out)


def build_eip712_filtering_raw(name: bytes = b"field") -> bytes:
    """EIP712 FILTERING P2=RAW_FIELD (0xFF): [name_len][name][sig_len][sig]."""
    return bytes([len(name)]) + name + bytes([0]) + b"\x00" * 64


def build_eip712_filtering_activate() -> bytes:
    """EIP712 FILTERING P2=ACTIVATE (0x00): empty body triggers schema hash."""
    return b""


def build_eip712_filtering_msg_info() -> bytes:
    """EIP712 FILTERING P2=MESSAGE_INFO (0x0F): [name_len][name][filters_count][sig]."""
    name = b"Permit"
    return bytes([len(name)]) + name + bytes([0x01]) + DER_SIG_70


def build_eip712_filtering_discard(path: bytes = b"EIP712Domain.chainId") -> bytes:
    """EIP712 FILTERING P2=DISCARDED_PATH (0x01): [path_len][path]."""
    return bytes([len(path)]) + path


# ─── Seed generation ──────────────────────────────────────────────────────

def build_seeds(base_prefix: bytes) -> list[tuple[str, bytes]]:
    prefix_tx = build_prefix(base_prefix, app_state=APP_STATE_SIGNING_TX)
    prefix_712 = build_prefix(base_prefix, app_state=APP_STATE_SIGNING_EIP712)
    prefix_idle = build_prefix(base_prefix, app_state=APP_STATE_IDLE)

    seeds: list[tuple[str, bytes]] = []

    # ── GTP TX_INFO (full v1, matches gcs_init_fuzz_ctx + tx_info_full_v1 token)
    tx_info = build_tx_info_v1_tlv()
    seeds.append((
        "gtp_tx_info_full_v1_signtx",
        prefix_tx + build_tail(CMD_IDX["INS_GTP_TRANSACTION_INFO"], P1_FIRST_CHUNK, 0, tx_info),
    ))
    seeds.append((
        "gtp_tx_info_full_v1_eip712",
        prefix_712 + build_tail(CMD_IDX["INS_GTP_TRANSACTION_INFO"], P1_FIRST_CHUNK, 0, tx_info),
    ))

    # Variant: "Send" operation type
    tx_info_send = build_tx_info_v1_tlv(operation=b"Send")
    seeds.append((
        "gtp_tx_info_full_v1_send",
        prefix_tx + build_tail(CMD_IDX["INS_GTP_TRANSACTION_INFO"], P1_FIRST_CHUNK, 0, tx_info_send),
    ))

    # ── GTP FIELD PARAM_RAW/CONSTANT (simple, no data-path traversal)
    for nbytes in (1, 2, 4, 8, 16, 20, 32):
        body = build_field_raw_constant(b"\x42" * nbytes)
        seeds.append((
            f"gtp_field_raw_const_{nbytes}B",
            prefix_tx + build_tail(CMD_IDX["INS_GTP_FIELD"], P1_FIRST_CHUNK, 0, body),
        ))

    # ── GTP FIELD PARAM_RAW + DATA_PATH (single leaf, tuple+leaf, tuple+ref+leaf)
    body = build_field_raw_datapath([_dp_leaf(3)], type_size=32)    # LEAF_STATIC
    seeds.append(("gtp_field_raw_path_leaf_static",
                  prefix_tx + build_tail(CMD_IDX["INS_GTP_FIELD"], P1_FIRST_CHUNK, 0, body)))
    body = build_field_raw_datapath([_dp_leaf(4)], type_size=32)    # LEAF_DYNAMIC
    seeds.append(("gtp_field_raw_path_leaf_dyn",
                  prefix_tx + build_tail(CMD_IDX["INS_GTP_FIELD"], P1_FIRST_CHUNK, 0, body)))
    body = build_field_raw_datapath([_dp_tuple(0), _dp_leaf(3)], type_size=32)
    seeds.append(("gtp_field_raw_path_tuple_leaf",
                  prefix_tx + build_tail(CMD_IDX["INS_GTP_FIELD"], P1_FIRST_CHUNK, 0, body)))
    body = build_field_raw_datapath([_dp_tuple(0x10), _dp_ref(), _dp_leaf(3)], type_size=32)
    seeds.append(("gtp_field_raw_path_tuple_ref_leaf",
                  prefix_tx + build_tail(CMD_IDX["INS_GTP_FIELD"], P1_FIRST_CHUNK, 0, body)))
    # Type-family variants exercising format dispatch
    for tf, ts, tag in ((1, 32, "uint256"), (2, 32, "int256"), (5, 20, "address"),
                        (6, 1, "bool"), (7, 32, "bytes32"), (8, 32, "string")):
        body = build_field_raw_datapath([_dp_leaf(3)], type_family=tf, type_size=ts)
        seeds.append((f"gtp_field_raw_path_tf_{tag}",
                      prefix_tx + build_tail(CMD_IDX["INS_GTP_FIELD"], P1_FIRST_CHUNK, 0, body)))

    # ── GTP FIELD PARAM_RAW + CONTAINER_PATH (from, to, value, chain_id)
    for kind, tag in ((0, "from"), (1, "to"), (2, "value"), (3, "chain_id")):
        body = build_field_raw_container(kind)
        seeds.append((f"gtp_field_raw_container_{tag}",
                      prefix_tx + build_tail(CMD_IDX["INS_GTP_FIELD"], P1_FIRST_CHUNK, 0, body)))

    # ── GTP FIELD PARAM_ENUM with synthetic id/val matching gcs_init_fuzz_ctx entries
    for id_v in range(4):
        for val_v in range(4):
            body = build_field_enum(id_v, val_v)
            seeds.append((f"gtp_field_enum_id{id_v}_val{val_v}",
                          prefix_tx + build_tail(CMD_IDX["INS_GTP_FIELD"], P1_FIRST_CHUNK, 0, body)))

    # ── GTP FIELD PARAM_AMOUNT / TOKEN_AMOUNT / TOKEN / NFT / DATETIME /
    # DURATION / UNIT / NETWORK / TRUSTED_NAME / CALLDATA (one seed per param
    # type to bootstrap coverage on gtp_param_*.c; libFuzzer mutates the
    # constants and source mix from there).
    _gtp_param_seeds: list[tuple[str, int, bytes]] = [
        ("gtp_field_param_amount",         1,  build_param_amount()),
        ("gtp_field_param_token_amount",   2,  build_param_token_amount()),
        ("gtp_field_param_token_amount_t", 2,  build_param_token_amount(threshold=b"\x01")),
        ("gtp_field_param_nft",            3,  build_param_nft()),
        ("gtp_field_param_datetime_unix",  4,  build_param_datetime(dt_type=0)),
        ("gtp_field_param_datetime_block", 4,  build_param_datetime(dt_type=1)),
        ("gtp_field_param_duration",       5,  build_param_duration()),
        ("gtp_field_param_unit",           6,  build_param_unit()),
        ("gtp_field_param_token",          10, build_param_token()),
        ("gtp_field_param_network",        11, build_param_network(chain_id=1)),
        ("gtp_field_param_trusted_name",   8,  build_param_trusted_name()),
        ("gtp_field_param_calldata",       9,  build_param_calldata()),
    ]
    for tag, pt, pbody in _gtp_param_seeds:
        body = _build_field(b"Param", pt, pbody)
        seeds.append((tag, prefix_tx + build_tail(CMD_IDX["INS_GTP_FIELD"], P1_FIRST_CHUNK, 0, body)))

    # ── GTP DATA_PATH ARRAY / SLICE elements (drive gtp_path_array.c and
    # gtp_path_slice.c TLV parsers via handle_array / handle_slice in
    # gtp_data_path.c). SLICE must follow a LEAF to operate on its collection.
    body = build_field_raw_datapath([_dp_array(weight=1)], type_size=32)
    seeds.append(("gtp_field_raw_path_array_w",
                  prefix_tx + build_tail(CMD_IDX["INS_GTP_FIELD"], P1_FIRST_CHUNK, 0, body)))
    body = build_field_raw_datapath([_dp_array(weight=2, start=0, end=2)], type_size=32)
    seeds.append(("gtp_field_raw_path_array_we",
                  prefix_tx + build_tail(CMD_IDX["INS_GTP_FIELD"], P1_FIRST_CHUNK, 0, body)))
    body = build_field_raw_datapath([_dp_leaf(3), _dp_slice(start=0, end=16)], type_size=32)
    seeds.append(("gtp_field_raw_path_slice",
                  prefix_tx + build_tail(CMD_IDX["INS_GTP_FIELD"], P1_FIRST_CHUNK, 0, body)))
    body = build_field_raw_datapath(
        [_dp_array(weight=1), _dp_leaf(3), _dp_slice(start=0, end=8)], type_size=32)
    seeds.append(("gtp_field_raw_path_array_leaf_slice",
                  prefix_tx + build_tail(CMD_IDX["INS_GTP_FIELD"], P1_FIRST_CHUNK, 0, body)))

    # ── EIP-7702 whitelist coverage: seed every entry compiled in when
    # HAVE_EIP7702_WHITELIST_TEST is defined. Each seed targets a different
    # chain_id × address pair that produces a successful `get_delegate_name`
    # lookup in whitelist_7702.c.
    whitelist_addr_one = b"\x01" * 20
    whitelist_addr_two = b"\x02" * 20
    whitelist_addr_max = b"\xff" * 20
    body = build_auth7702_payload(delegate=whitelist_addr_one, chain_id=1)
    seeds.append(("sign_eip7702_whitelist_one",
                  prefix_idle + build_raw_tail(CMD_IDX["INS_SIGN_EIP7702_AUTHORIZATION"], P1_FIRST_CHUNK, 0, body)))
    body = build_auth7702_payload(delegate=whitelist_addr_two, chain_id=2)
    seeds.append(("sign_eip7702_whitelist_two",
                  prefix_idle + build_raw_tail(CMD_IDX["INS_SIGN_EIP7702_AUTHORIZATION"], P1_FIRST_CHUNK, 0, body)))
    body = build_auth7702_payload(delegate=whitelist_addr_max, chain_id=0xFFFFFFFFFFFFFFFF)
    seeds.append(("sign_eip7702_whitelist_max",
                  prefix_idle + build_raw_tail(CMD_IDX["INS_SIGN_EIP7702_AUTHORIZATION"], P1_FIRST_CHUNK, 0, body)))

    # ── PROVIDE_ENUM_VALUE standalone
    for id_v in (0, 1, 2, 3):
        for val_v in (0, 1):
            body = build_enum_value_tlv(id_v=id_v, val_v=val_v)
            seeds.append((f"provide_enum_value_id{id_v}_val{val_v}",
                          prefix_idle + build_tail(CMD_IDX["INS_PROVIDE_ENUM_VALUE"], P1_FIRST_CHUNK, 0, body)))

    # ── PROVIDE_TRUSTED_NAME v1 and v2 (including MAB + contract/token/NFT variants)
    body = build_trusted_name_v1_tlv()
    seeds.append(("provide_trusted_name_v1",
                  prefix_idle + build_tail(CMD_IDX["INS_PROVIDE_TRUSTED_NAME"], P1_FIRST_CHUNK, 0, body)))
    # v2 variants: CONTRACT (2)/CAL (1), TOKEN (4)/CAL (1), NFT_COLLECTION (3)/CAL (1)
    for tn_type, label in ((2, "contract"), (4, "token"), (3, "nft")):
        body = build_trusted_name_v2_tlv(name_type=tn_type, name_source=1)
        seeds.append((f"provide_trusted_name_v2_{label}_cal",
                      prefix_idle + build_tail(CMD_IDX["INS_PROVIDE_TRUSTED_NAME"], P1_FIRST_CHUNK, 0, body)))
    # v2 MAB (account + owner + owner_deriv_path)
    body = build_trusted_name_v2_mab_tlv()
    seeds.append(("provide_trusted_name_v2_mab",
                  prefix_idle + build_tail(CMD_IDX["INS_PROVIDE_TRUSTED_NAME"], P1_FIRST_CHUNK, 0, body)))

    # ── PROVIDE_PROXY_INFO
    body = build_proxy_info_tlv()
    seeds.append(("provide_proxy_info",
                  prefix_idle + build_tail(CMD_IDX["INS_PROVIDE_PROXY_INFO"], P1_FIRST_CHUNK, 0, body)))

    # ── PROVIDE_NETWORK_CONFIGURATION
    body = build_network_info_tlv()
    seeds.append(("provide_network_info",
                  prefix_idle + build_tail(CMD_IDX["INS_PROVIDE_NETWORK_CONFIGURATION"], P1_FIRST_CHUNK, 0, body)))

    # ── PROVIDE_SAFE_ACCOUNT
    body = build_safe_desc_tlv()
    seeds.append(("provide_safe_account",
                  prefix_idle + build_tail(CMD_IDX["INS_PROVIDE_SAFE_ACCOUNT"], P1_FIRST_CHUNK, 0, body)))
    body = build_signer_desc_tlv()
    seeds.append(("provide_safe_account_signer",
                  prefix_idle + build_tail(CMD_IDX["INS_PROVIDE_SAFE_ACCOUNT"], P1_FIRST_CHUNK, 1, body)))

    # ── PROVIDE_GATING
    body = build_gating_tlv()
    seeds.append(("provide_gating",
                  prefix_idle + build_tail(CMD_IDX["INS_PROVIDE_GATING"], P1_FIRST_CHUNK, 0, body)))

    # ── PROVIDE_MAP_ENTRY
    body = build_map_entry_tlv(entry_id=1, key=b"\xAA", value=b"\x01\x02")
    seeds.append(("provide_map_entry_basic",
                  prefix_idle + build_tail(CMD_IDX["INS_PROVIDE_MAP_ENTRY"], P1_FIRST_CHUNK, 0, body)))

    # ── PROVIDE_TX_SIMULATION
    body = build_tx_simulation_tlv()
    seeds.append(("provide_tx_simulation",
                  prefix_idle + build_tail(CMD_IDX["INS_PROVIDE_TX_SIMULATION"], P1_FIRST_CHUNK, 0, body)))

    # ── SIGN_EIP7702_AUTHORIZATION
    # build_raw_tail because the payload includes BIP32 + TLV size + body (not wrapped by build_tail).
    body_revoke = build_auth7702_payload(delegate=b"\x00" * 20, chain_id=1)
    seeds.append(("sign_eip7702_revoke",
                  prefix_idle + build_raw_tail(CMD_IDX["INS_SIGN_EIP7702_AUTHORIZATION"], P1_FIRST_CHUNK, 0, body_revoke)))
    body_simple = build_auth7702_payload(delegate=SIMPLE_7702_ACCOUNT, chain_id=1)
    seeds.append(("sign_eip7702_simple_acct",
                  prefix_idle + build_raw_tail(CMD_IDX["INS_SIGN_EIP7702_AUTHORIZATION"], P1_FIRST_CHUNK, 0, body_simple)))
    body_test = build_auth7702_payload(delegate=EIP7702_TEST_ONE, chain_id=1)
    seeds.append(("sign_eip7702_test_one",
                  prefix_idle + build_raw_tail(CMD_IDX["INS_SIGN_EIP7702_AUTHORIZATION"], P1_FIRST_CHUNK, 0, body_test)))
    body_all = build_auth7702_payload(delegate=SIMPLE_7702_ACCOUNT, chain_id=0xFFFFFFFFFFFFFFFF)
    seeds.append(("sign_eip7702_chain_all",
                  prefix_idle + build_raw_tail(CMD_IDX["INS_SIGN_EIP7702_AUTHORIZATION"], P1_FIRST_CHUNK, 0, body_all)))

    # ── SWAP pseudo-commands (bounded harness-side adapters for 0% library handlers)
    seeds.append(("swap_check_address_match",
                  prefix_idle + build_raw_tail(CMD_IDX["INS_FUZZ_SWAP_CHECK_ADDRESS"],
                                               0,
                                               0,
                                               b"\x00\x00\x00\x01\x00\x00")))
    seeds.append(("swap_check_address_mismatch",
                  prefix_idle + build_raw_tail(CMD_IDX["INS_FUZZ_SWAP_CHECK_ADDRESS"],
                                               0,
                                               0,
                                               b"\x00\x00\x00\x01\x01\x00")))
    seeds.append(("swap_get_printable_amount",
                  prefix_idle + build_raw_tail(CMD_IDX["INS_FUZZ_SWAP_PRINTABLE_AMOUNT"],
                                               0,
                                               0,
                                               b"\x01\x00\x00\x00\x00\x00\x00\x08\x01")))
    seeds.append(("swap_copy_transaction_standard",
                  prefix_idle + build_raw_tail(CMD_IDX["INS_FUZZ_SWAP_COPY_TRANSACTION"],
                                               0,
                                               0,
                                               b"\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x08\x00\x04\x00")))
    seeds.append(("swap_copy_transaction_crosschain",
                  prefix_idle + build_raw_tail(CMD_IDX["INS_FUZZ_SWAP_COPY_TRANSACTION"],
                                               0,
                                               0,
                                               b"\x01\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x08\x00\x04\x00")))
    seeds.append(("plugin_utils",
                  prefix_idle + build_raw_tail(CMD_IDX["INS_FUZZ_PLUGIN_UTILS"],
                                               0,
                                               0,
                                               b"\x12\x34\x11\x22\x33\x44\x01")))

    # ── EIP712 STRUCT_DEF (non-TLV header + field variants)
    header = build_eip712_struct_def_name(b"Permit")
    tail = bytes([CLA, CMD_IDX["INS_EIP712_STRUCT_DEF"], 0, 0]) + struct.pack(">H", len(header)) + header
    seeds.append(("eip712_struct_def_name_permit", prefix_712 + tail))
    for tdesc, tsize, fname in ((0x42, 32, b"amount"), (0x03, 0, b"owner"),
                                 (0x05, 0, b"name"), (0x46, 32, b"digest"),
                                 (0xC2, 32, b"amounts[]")):
        field = build_eip712_struct_def_field(tdesc, tsize, fname)
        tail = bytes([CLA, CMD_IDX["INS_EIP712_STRUCT_DEF"], 0xFF, 0xFF]) + struct.pack(">H", len(field)) + field
        seeds.append((f"eip712_struct_def_field_{fname.decode('ascii', errors='ignore')}", prefix_712 + tail))

    # ── EIP712 STRUCT_IMPL (P2=NAME sets root, P2=FIELD sends field data, P2=ARRAY sets depth)
    impl_idx = CMD_IDX["INS_EIP712_STRUCT_IMPL"]
    # P2_IMPL_NAME (0x00): set root type by name — triggers path_set_root + type_hash
    for root_name in (b"EIP712Domain", b"Mail"):
        seeds.append((f"eip712_struct_impl_name_{root_name.decode()}",
                      prefix_712 + build_raw_tail(impl_idx, 0, 0x00, root_name)))
    # P2_IMPL_FIELD (0xFF): field data — triggers field_hash with 2-byte BE length + value
    # uint256 value (32 bytes): length prefix 0x0020 + 32 bytes of data
    field_data = struct.pack(">H", 32) + b"\x00" * 32
    seeds.append(("eip712_struct_impl_field_uint256",
                  prefix_712 + build_raw_tail(impl_idx, 0, 0xFF, field_data)))
    # address (20 bytes)
    field_data = struct.pack(">H", 20) + b"\x00" * 20
    seeds.append(("eip712_struct_impl_field_addr",
                  prefix_712 + build_raw_tail(impl_idx, 0, 0xFF, field_data)))
    # string (short, 4 bytes)
    field_data = struct.pack(">H", 4) + b"test"
    seeds.append(("eip712_struct_impl_field_string",
                  prefix_712 + build_raw_tail(impl_idx, 0, 0xFF, field_data)))
    # P2_IMPL_ARRAY (0x0F): array depth — 1 byte array size
    seeds.append(("eip712_struct_impl_array",
                  prefix_712 + build_raw_tail(impl_idx, 0, 0x0F, b"\x03")))

    # ── EIP712 FILTERING (valid P2 values)
    filt_idx = CMD_IDX["INS_EIP712_FILTERING"]
    body = build_eip712_filtering_activate()
    seeds.append(("eip712_filtering_activate",
                  prefix_712 + build_tail(filt_idx, P1_FIRST_CHUNK, 0x00, body)))
    body = build_eip712_filtering_msg_info()
    seeds.append(("eip712_filtering_msg_info",
                  prefix_712 + build_raw_tail(filt_idx, 0, 0x0F, body)))
    body = build_eip712_filtering_discard()
    seeds.append(("eip712_filtering_discard",
                  prefix_712 + build_raw_tail(filt_idx, 0, 0x01, body)))
    body = build_eip712_filtering_raw(b"Permit.amount")
    seeds.append(("eip712_filtering_raw_field",
                  prefix_712 + build_raw_tail(filt_idx, 0, 0xFF, body)))

    return seeds


def generate_seeds(output_dir: str) -> None:
    os.makedirs(output_dir, exist_ok=True)
    prefix_size = resolve_prefix_size()
    validate_prefix_size(prefix_size, _LAYOUT_DEFS)
    base_prefix = resolve_seed_prefix(prefix_size)
    seeds = build_seeds(base_prefix)
    written = 0
    for name, blob in seeds:
        path = os.path.join(output_dir, f"eth_{name}")
        with open(path, "wb") as f:
            f.write(blob)
        written += 1
    print(f"Generated {written} Ethereum-specific seed corpus files in {output_dir}")


if __name__ == "__main__":
    out = sys.argv[1] if len(sys.argv) > 1 else "base-corpus"
    generate_seeds(out)
