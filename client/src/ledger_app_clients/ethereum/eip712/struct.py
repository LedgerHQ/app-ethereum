from enum import IntEnum, auto


class EIP712FieldType(IntEnum):
    CUSTOM = 0,
    INT = auto()
    UINT = auto()
    ADDRESS = auto()
    BOOL = auto()
    STRING = auto()
    FIX_BYTES = auto()
    DYN_BYTES = auto()
