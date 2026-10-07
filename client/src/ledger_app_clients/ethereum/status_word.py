from enum import IntEnum
from typing import Any, TypeAlias, cast

from ragger.error import StatusWords as global_SW

# Custom error codes specific to Boilerplate app
# fmt: off
custom_errors = {
    "EXCEPTION_OVERFLOW": 0x6807,
    "SW_SWAP_FAIL":       0xC000,
}
# fmt: on

# Build the combined dictionary first
_errors_dict = {m.name: m.value for m in global_SW}
_errors_dict.update(custom_errors)

# Create the Errors enum.
# Cast to Any so that mypy does not flag accesses to dynamically-created
# members (e.g. StatusWord.SWO_SUCCESS, StatusWord.EXCEPTION_OVERFLOW).
StatusWord: TypeAlias = cast(Any, IntEnum("Errors", _errors_dict))  # type: ignore[misc]
