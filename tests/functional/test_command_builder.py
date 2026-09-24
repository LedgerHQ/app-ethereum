"""Unit tests for the Ethereum client command builder.

Pure Python: these check the bytes the builder puts on the wire, with no device.
"""

from client.client import EIP712CalldataParamPresence
from client.command_builder import CommandBuilder


def test_eip712_calldata_info_flags_are_not_narrowed():
    """The callee and spender flags are three-valued, and the signature covers them as
    bytes (InputData.send_filtering_calldata_info appends int(flag)). Packing them as
    booleans in the APDU made PRESENT_VERIFYING_CONTRACT arrive as PRESENT_FILTERED,
    which no longer matched the signed payload."""
    verifying_contract = EIP712CalldataParamPresence.PRESENT_VERIFYING_CONTRACT
    apdu = CommandBuilder().eip712_filtering_calldata_info(
        index=0,
        value_filter_flag=True,
        callee_filter_flag=verifying_contract,
        chain_id_filter_flag=False,
        selector_filter_flag=False,
        amount_filter_flag=True,
        spender_filter_flag=verifying_contract,
        sig=b"",
    )
    # 5-byte APDU header, then index, value, callee, chain_id, selector, amount, spender
    cdata = apdu[5:]
    assert cdata[2] == verifying_contract, "callee flag narrowed"
    assert cdata[6] == verifying_contract, "spender flag narrowed"
