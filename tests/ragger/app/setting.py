from enum import IntEnum, auto
from typing import List

class   SettingType(IntEnum):
    BLIND_SIGNING = 0,
    DEBUG_DATA = auto()
    NONCE = auto()
    VERBOSE_EIP712 = auto()

class   SettingImpl:
    devices: List[str]
    value: bool

    def __init__(self, devs: List[str]):
        self.devices = devs
