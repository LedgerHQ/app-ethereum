from pathlib import Path
from ragger.navigator.navigation_scenario import NavigateWithScenario

from test_blind_sign import test_blind_sign as blind_sign
from test_eip712 import test_eip712_new as sign_eip712
from test_eip712 import eip712_json_path, set_wallet_addr

from client.gating import Gating
from client.utils import TxType


def test_gating_blind_signing(scenario_navigator: NavigateWithScenario) -> None:
    """Test the Gating descriptor APDU with a blind signing transaction"""

    descriptor = Gating(
        TxType.TRANSACTION,
        bytes.fromhex("6b175474e89094c44da98b954eedeac495271d0f"),
        "To scan for threats and verify this transaction before signing, use Ledger Multisig.",
        "ledger.com/ledger-multisig",
        1,
    )
    blind_sign(scenario_navigator.navigator,
               scenario_navigator,
               scenario_navigator.test_name,
               False,
               0.0,
               gating_params=descriptor)

def test_gating_blind_signing_with_proxy(scenario_navigator: NavigateWithScenario) -> None:
    """Test the Gating descriptor APDU with a blind signing transaction"""

    descriptor = Gating(
        TxType.TRANSACTION,
        bytes.fromhex("Dad77910DbDFdE764fC21FCD4E74D71bBACA6D8D"),
        "To scan for threats and verify this transaction before signing, use Ledger Multisig.",
        "ledger.com/ledger-multisig",
        1,
    )

    blind_sign(scenario_navigator.navigator,
               scenario_navigator,
               scenario_navigator.test_name,
               False,
               0.0,
               gating_params=descriptor,
               with_proxy=True)

def test_gating_eip712(scenario_navigator: NavigateWithScenario) -> None:
    """Test the Gating descriptor APDU with a EIP712 transaction"""

    json_file = Path(eip712_json_path()) / "00-simple_mail-data.json"
    descriptor = Gating(
        TxType.TYPED_DATA,
        bytes(),  # Address will be initialized from the json_file
        "To scan for threats and verify this transaction before signing, use Ledger Multisig.",
        "ledger.com/ledger-multisig",
    )
    # Ensure the wallet address is initialized
    set_wallet_addr(scenario_navigator.backend)
    sign_eip712(scenario_navigator, json_file, False, False, descriptor)
