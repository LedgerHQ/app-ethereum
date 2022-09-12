class UnknownDeviceError(Exception):
    pass


class DenyError(Exception):
    pass


class WrongP1P2Error(Exception):
    pass


class WrongDataLengthError(Exception):
    pass


class InsNotSupportedError(Exception):
    pass


class ClaNotSupportedError(Exception):
    pass


class WrongResponseLengthError(Exception):
    pass


class DisplayBip32PathFailError(Exception):
    pass


class DisplayAddressFailError(Exception):
    pass


class DisplayAmountFailError(Exception):
    pass


class WrongTxLengthError(Exception):
    pass


class TxParsingFailError(Exception):
    pass


class TxHashFail(Exception):
    pass


class BadStateError(Exception):
    pass


class SignatureFailError(Exception):
    pass
