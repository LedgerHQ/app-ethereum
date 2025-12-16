from enum import IntEnum, auto


class EIP712TypeDescOffset(IntEnum):
    ARRAY = 7
    SIZE = 6
    TYPE = 0


class EIP712TypeDescMask(IntEnum):
    ARRAY = (0b1 << EIP712TypeDescOffset.ARRAY)
    SIZE = (0b1 << EIP712TypeDescOffset.SIZE)
    TYPE = (0b1111 << EIP712TypeDescOffset.TYPE)


class EIP712FieldType(IntEnum):
    CUSTOM = 0
    INT = auto()
    UINT = auto()
    ADDRESS = auto()
    BOOL = auto()
    STRING = auto()
    FIX_BYTES = auto()
    DYN_BYTES = auto()
