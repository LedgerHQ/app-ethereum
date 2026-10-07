from enum import IntEnum

from ragger.tlv import LedgerCommonFieldTag, LesMultisigFieldTag, TlvSerializable

from .signing_partners import SAFE_PARTNER


class AccountType(IntEnum):
    SAFE = 0x00
    SIGNER = 0x01


class LesMultiSigRole(IntEnum):
    SIGNER = 0x00
    PROPOSER = 0x01


class SafeAccount(TlvSerializable):
    account_type: AccountType
    challenge: int = 0
    address: list[bytes]
    lesm_role: LesMultiSigRole | None = None
    threshold: int = 0
    signer_counts: int = 0
    signature: bytes | None

    def __init__(
        self,
        account_type: AccountType,
        challenge: int,
        address: list[bytes],
        lesm_role: LesMultiSigRole | None = None,
        threshold: int = 0,
        signer_counts: int = 0,
        signature: bytes | None = None,
    ) -> None:
        assert account_type in (AccountType.SAFE, AccountType.SIGNER), (
            f"Invalid account_type: {account_type}. Must be SAFE(0) or SIGNER(1)"
        )
        if account_type == AccountType.SAFE:
            assert len(address) == 1, "Only a single Address for SAFE accounts is allowed"
            assert lesm_role in (LesMultiSigRole.SIGNER, LesMultiSigRole.PROPOSER), (
                f"Invalid lesm_role: {lesm_role}. Must be SIGNER(0) or PROPOSER(1)"
            )
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
        struct_type = 0x27 if self.account_type == AccountType.SAFE else 0x0A
        payload: bytes = self.serialize_field(LedgerCommonFieldTag.STRUCTURE_TYPE, struct_type)
        payload += self.serialize_field(LedgerCommonFieldTag.VERSION, 1)
        payload += self.serialize_field(LedgerCommonFieldTag.CHALLENGE, self.challenge)
        for addr in self.address:
            payload += self.serialize_field(LedgerCommonFieldTag.ADDRESS, addr)
        if self.account_type == AccountType.SAFE:
            if self.lesm_role is not None:
                payload += self.serialize_field(LesMultisigFieldTag.ROLE, self.lesm_role)
            payload += self.serialize_field(LesMultisigFieldTag.THRESHOLD, self.threshold)
            payload += self.serialize_field(LesMultisigFieldTag.SIGNERS_COUNT, self.signer_counts)

        # Append the data Signature
        sig = self.signature
        if sig is None:
            sig = SAFE_PARTNER.sign(payload)
        payload += self.serialize_field(LedgerCommonFieldTag.DER_SIGNATURE, sig)
        return payload
