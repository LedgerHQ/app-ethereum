from eth_account import Account
from eth_account.messages import encode_defunct, encode_typed_data
import rlp


def get_selector_from_data(data: str) -> bytes:
    raw_data = bytes.fromhex(data[2:])
    return raw_data[:4]


def recover_message(msg, vrs: tuple) -> bytes:
    if isinstance(msg, dict):  # EIP-712
        smsg = encode_typed_data(full_message=msg)
    else:  # EIP-191
        smsg = encode_defunct(primitive=msg)
    addr = Account.recover_message(smsg, vrs)
    return bytes.fromhex(addr[2:])


# TODO: Figure out why it doesn't work for non-legacy transactions
def recover_transaction(tx_params, vrs: tuple) -> bytes:
    raw_tx = Account.create().sign_transaction(tx_params).rawTransaction
    prefix = bytes()
    if raw_tx[0] in [0x01, 0x02]:
        prefix = raw_tx[:1]
        raw_tx = raw_tx[len(prefix):]
    if prefix == bytes():
        # v is returned on one byte only so it might have overflowed
        # in that case, we will reconstruct it to its full value
        if "chainId" in tx_params:
            trunc_chain_id = tx_params["chainId"]
            while trunc_chain_id.bit_length() > 32:
                trunc_chain_id >>= 8
            target = tx_params["chainId"] * 2 + 35
            trunc_target = trunc_chain_id * 2 + 35
            diff = vrs[0][0] - (trunc_target & 0xff)
            vrs = (target + diff, vrs[1], vrs[2])
    decoded = rlp.decode(raw_tx)
    reencoded = rlp.encode(decoded[:-3] + list(vrs))
    addr = Account.recover_transaction(prefix + reencoded)
    return bytes.fromhex(addr[2:])
