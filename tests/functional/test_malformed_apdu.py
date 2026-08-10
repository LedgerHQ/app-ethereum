"""
Malformed APDU rejection tests.

All tests operate from the IDLE state and require no user interaction.
They verify the app's fail-closed behaviour on invalid inputs before
any signing flow is started.
"""

import pytest
from client.command_builder import InsType, P1Type, P2Type
from client.status_word import StatusWord
from ragger.backend import BackendInterface
from ragger.error import ExceptionRAPDU

CLA = 0xE0


def _send(backend: BackendInterface, cla: int, ins: int, p1: int, p2: int, data: bytes) -> int:
    apdu = bytes([cla, ins, p1, p2, len(data)]) + data
    with pytest.raises(ExceptionRAPDU) as exc:
        backend.exchange_raw(apdu)
    return exc.value.status


# =============================================================================
# Dispatcher-level guards
# =============================================================================


def test_wrong_cla_rejected(backend: BackendInterface) -> None:
    """Any APDU with CLA != 0xE0 must return INVALID_CLA."""
    status = _send(backend, 0xAA, InsType.GET_PUBLIC_ADDR, 0x00, 0x00, b"")
    assert status == StatusWord.SWO_INVALID_CLA


def test_unknown_ins_rejected(backend: BackendInterface) -> None:
    """An unknown INS (valid CLA) must return INVALID_INS."""
    status = _send(backend, CLA, 0xFF, 0x00, 0x00, b"")
    assert status == StatusWord.SWO_INVALID_INS


# =============================================================================
# Signing entry-point guards (IDLE state)
# =============================================================================


def test_sign_empty_payload_rejected(backend: BackendInterface) -> None:
    """SIGN P1_FIRST with an empty payload must be rejected before BIP32 parsing."""
    status = _send(backend, CLA, InsType.SIGN, P1Type.SIGN_FIRST_CHUNK, P2Type.SIGN_PROCESS_START, b"")
    assert status == StatusWord.SWO_INCORRECT_DATA


def test_sign_continuation_without_first_rejected(backend: BackendInterface) -> None:
    """A SIGN continuation chunk arriving without a prior P1_FIRST must be rejected."""
    status = _send(backend, CLA, InsType.SIGN, P1Type.SIGN_SUBSQT_CHUNK, P2Type.SIGN_PROCESS_START, b"\x00" * 4)
    assert status == StatusWord.SWO_COMMAND_NOT_ALLOWED


def test_personal_sign_empty_payload_rejected(backend: BackendInterface) -> None:
    """SIGN_PERSONAL_MESSAGE with no BIP32 path must be rejected."""
    status = _send(backend, CLA, InsType.PERSONAL_SIGN, P1Type.SIGN_FIRST_CHUNK, 0x00, b"")
    assert status == StatusWord.SWO_INCORRECT_DATA


def test_eip712_sign_empty_payload_rejected(backend: BackendInterface) -> None:
    """EIP-712 v0 sign with empty payload must be rejected."""
    status = _send(backend, CLA, InsType.EIP712_SIGN, P1Type.SIGN_FIRST_CHUNK, P2Type.EIP712_V0_IMPLEM, b"")
    assert status == StatusWord.SWO_INCORRECT_DATA


# =============================================================================
# GTP provisioning guards (IDLE state — no active signing flow)
# =============================================================================


def test_gtp_tx_info_without_signing_state_rejected(backend: BackendInterface) -> None:
    """GTP_TRANSACTION_INFO must be refused when no signing flow is active."""
    status = _send(backend, CLA, InsType.PROVIDE_TRANSACTION_INFO, P1Type.FIRST_CHUNK, 0x00, b"\x00" * 4)
    assert status == StatusWord.SWO_COMMAND_NOT_ALLOWED


def test_gtp_field_without_signing_state_rejected(backend: BackendInterface) -> None:
    """GTP_FIELD must be refused when no signing flow is active."""
    status = _send(backend, CLA, InsType.PROVIDE_TRANSACTION_FIELD_DESC, P1Type.FIRST_CHUNK, 0x00, b"\x00" * 4)
    assert status == StatusWord.SWO_COMMAND_NOT_ALLOWED


# =============================================================================
# Plugin-set guards (IDLE state sends correct data — guard only fires in non-IDLE)
# Sending malformed plugin data from IDLE verifies the parser rejects it.
# =============================================================================


def test_set_external_plugin_short_payload_rejected(backend: BackendInterface) -> None:
    """SET_EXTERNAL_PLUGIN with a truncated payload (name declared but body missing) must be rejected."""
    # name_len=1 + "A" (1 byte) = 2 bytes total, missing 20-byte address + 4-byte selector + sig
    status = _send(backend, CLA, InsType.EXTERNAL_PLUGIN_SETUP, 0x00, 0x00, b"\x01\x41")
    assert status == StatusWord.SWO_INCORRECT_DATA


def test_set_plugin_short_payload_rejected(backend: BackendInterface) -> None:
    """SET_PLUGIN with a payload at or below the 3-byte header must be rejected."""
    # type(1) + version(1) + name_len(1) = 3 bytes == HEADER_SIZE, strictly too small
    status = _send(backend, CLA, InsType.SET_PLUGIN, 0x00, 0x00, b"\x01\x01\x01")
    assert status == StatusWord.SWO_INCORRECT_DATA
