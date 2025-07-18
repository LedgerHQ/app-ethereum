from enum import IntEnum
from typing import List, Optional
from .tlv import TlvSerializable, FieldTag
from .keychain import sign_data, Key


class AccountType(IntEnum):
    SAFE = 0x00
    SIGNER = 0x01


class LesMultiSigRole(IntEnum):
    SIGNER = 0x00
    PROPOSER = 0x01


class SafeAccount(TlvSerializable):
    account_type: AccountType
    challenge: int = 0
    address: List[bytes] = []
    lesm_role: Optional[LesMultiSigRole] = None
    threshold: int = 0
    signer_counts: int = 0
    signature: Optional[bytes]

    def __init__(self,
                 account_type: AccountType,
                 challenge: int,
                 address: List[bytes],
                 lesm_role: Optional[LesMultiSigRole] = None,
                 threshold: int = 0,
                 signer_counts: int = 0,
                 signature: Optional[bytes] = None) -> None:
        assert account_type in (AccountType.SAFE, AccountType.SIGNER), \
            f"Invalid account_type: {account_type}. Must be SAFE(0) or SIGNER(1)"
        if account_type == AccountType.SAFE:
            assert len(address) == 1, "Only a single Address for SAFE accounts is allowed"
            assert lesm_role in (LesMultiSigRole.SIGNER, LesMultiSigRole.PROPOSER), \
                f"Invalid lesm_role: {lesm_role}. Must be SIGNER(0) or PROPOSER(1)"
        self.account_type = account_type
        self.challenge = challenge
        self.address = address
        if account_type == AccountType.SAFE:
            self.lesm_role = lesm_role
            self.threshold = threshold
            self.signer_counts = signer_counts
        self.signature = signature

    def serialize(self) -> bytes:
        assert self.address is not None, "Address is required"
        assert self.challenge is not None, "Challenge is required"
        if self.account_type == AccountType.SAFE:
            assert self.threshold > 0, "Threshold must be greater than 0"
            assert self.signer_counts > 0, "Signer counts must be greater than 0"
            assert self.lesm_role is not None, "LESM role is required for SAFE accounts"
        # Construct the TLV payload
        payload: bytes = self.serialize_field(FieldTag.STRUCT_TYPE, 0x27)
        payload += self.serialize_field(FieldTag.STRUCT_VERSION, 1)
        payload += self.serialize_field(FieldTag.CHALLENGE, self.challenge)
        for addr in self.address:
            payload += self.serialize_field(FieldTag.ADDRESS, addr)
        if self.account_type == AccountType.SAFE:
            if self.lesm_role is not None:
                payload += self.serialize_field(FieldTag.LESM_ROLE, self.lesm_role)
            payload += self.serialize_field(FieldTag.THRESHOLD, self.threshold)
            payload += self.serialize_field(FieldTag.SIGNERS_COUNT, self.signer_counts)

        # Append the data Signature
        sig = self.signature
        if sig is None:
            sig = sign_data(Key.SAFE, payload)
        payload += self.serialize_field(FieldTag.DER_SIGNATURE, sig)
        return payload
