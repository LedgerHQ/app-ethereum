import sys
from pathlib import Path
from os import path
import warnings
import glob
import re

import pytest

from ragger.conftest import configuration


#######################
# CONFIGURATION START #
#######################

# You can configure optional parameters by overriding the value of
# ragger.configuration.OPTIONAL_CONFIGURATION
# Please refer to ragger/conftest/configuration.py for their descriptions and accepted values


def pytest_addoption(parser):
    parser.addoption("--with_lib_mode", action="store_true", help="Run the test with Library Mode")


pattern = f"{Path(__file__).parent}/test_*.py"
testFiles = [path.basename(x) for x in glob.glob(pattern)]
collect_ignore = []
if "--with_lib_mode" in sys.argv:

    # ==============================================================================
    # /!\ Tests are started in Library mode: unselect (ignore) unrelated modules /!\
    # ==============================================================================

    warnings.warn("Main app is started in library mode")

    configuration.OPTIONAL.MAIN_APP_DIR = "tests/ragger/.test_dependencies/"

    collect_ignore += [f for f in testFiles if "test_clone" not in f]

else:

    # ===========================================================================
    # /!\ Standards tests without Library mode: unselect (ignore) clone tests /!\
    # ===========================================================================

    collect_ignore += [f for f in testFiles if "test_clone" in f]


@pytest.fixture(name="app_version")
def app_version_fixture(request) -> tuple[int, int, int]:
    with open(Path(__file__).parent.parent.parent / "Makefile") as f:
        parsed = dict()
        for m in re.findall(r"^APPVERSION_(\w)\s*=\s*(\d*)$", f.read(), re.MULTILINE):
            parsed[m[0]] = int(m[1])
    return (parsed["M"], parsed["N"], parsed["P"])


#####################
# CONFIGURATION END #
#####################

# Pull all features from the base ragger conftest using the overridden configuration
pytest_plugins = ("ragger.conftest.base_conftest", )
