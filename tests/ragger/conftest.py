from pathlib import Path
import warnings
import re

import pytest

from ragger.conftest import configuration


#######################
# CONFIGURATION START #
#######################

# You can configure optional parameters by overriding the value of
# ragger.configuration.OPTIONAL_CONFIGURATION
# Please refer to ragger/conftest/configuration.py for their descriptions and accepted values

configuration.OPTIONAL.ALLOWED_SETUPS = ["default", "lib_mode"]


def pytest_configure(config):
    current_setup = config.getoption("--setup")
    if current_setup == "lib_mode":
        warnings.warn("Main app is started in library mode")
        configuration.OPTIONAL.MAIN_APP_DIR = "tests/ragger/.test_dependencies/"


@pytest.fixture(name="app_version")
def app_version_fixture(request) -> tuple[int, int, int]:
    with open(Path(__file__).parent.parent.parent / "Makefile", encoding="utf-8") as f:
        parsed = {}
        for m in re.findall(r"^APPVERSION_(\w)\s*=\s*(\d*)$", f.read(), re.MULTILINE):
            parsed[m[0]] = int(m[1])
    return (parsed["M"], parsed["N"], parsed["P"])


#####################
# CONFIGURATION END #
#####################

# Pull all features from the base ragger conftest using the overridden configuration
pytest_plugins = ("ragger.conftest.base_conftest", )
