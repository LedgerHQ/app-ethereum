from pathlib import Path
from typing import List
import re

from ragger.backend import BackendInterface
from ragger.utils.misc import get_current_app_name_and_version


def test_check_version(backend: BackendInterface):
    """Check version and name"""

    # Send the APDU
    app_name, version = get_current_app_name_and_version(backend)
    print(f" Name: {app_name}")
    print(f" Version: {version}")
    _verify_version(version.split("-")[0])


def _verify_version(version: str) -> None:
    """Verify the app version, based on defines in Makefile

    Args:
        Version (str): Version to be checked
    """

    vers_dict = {}
    vers_str = ""
    lines = _read_makefile()
    version_re = re.compile(r"^APPVERSION_(?P<part>\w)\s?=\s?(?P<val>\d*)", re.I)
    for line in lines:
        info = version_re.match(line)
        if info:
            dinfo = info.groupdict()
            vers_dict[dinfo["part"]] = dinfo["val"]
    try:
        vers_str = f"{vers_dict['M']}.{vers_dict['N']}.{vers_dict['P']}"
    except KeyError:
        pass
    assert version == vers_str


def _read_makefile() -> List[str]:
    """Read lines from the parent Makefile """

    parent = Path(__file__).parent.parent.parent.resolve()
    makefile = f"{parent}/Makefile"
    with open(makefile, "r", encoding="utf-8") as f_p:
        lines = f_p.readlines()
    return lines
