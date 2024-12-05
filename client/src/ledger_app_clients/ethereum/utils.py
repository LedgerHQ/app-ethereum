from eth_account import Account
from eth_account.messages import encode_defunct, encode_typed_data
import rlp


# eth_account requires it for some reason
def normalize_vrs(vrs: tuple) -> tuple:
    vrs_l = list()
    for elem in vrs:
        vrs_l.append(elem.lstrip(b'\x00'))
    return tuple(vrs_l)


def get_selector_from_data(data: str) -> bytes:
    raw_data = bytes.fromhex(data[2:])
    return raw_data[:4]


def recover_message(msg, vrs: tuple) -> bytes:
    if isinstance(msg, dict):  # EIP-712
        smsg = encode_typed_data(full_message=msg)
    else:  # EIP-191
        smsg = encode_defunct(primitive=msg)
    addr = Account.recover_message(smsg, normalize_vrs(vrs))
    return bytes.fromhex(addr[2:])


def recover_transaction(tx_params, vrs: tuple) -> bytes:
    raw_tx = Account.create().sign_transaction(tx_params).rawTransaction
    prefix = bytes()
    if raw_tx[0] in [0x01, 0x02]:
        prefix = raw_tx[:1]
        raw_tx = raw_tx[len(prefix):]
    else:
        if "chainId" in tx_params:
            # v is returned on one byte only so it might have overflowed
            # in that case, we will reconstruct it to its full value
            trunc_chain_id = tx_params["chainId"]
            while trunc_chain_id.bit_length() > 32:
                trunc_chain_id >>= 8

            trunc_target = trunc_chain_id * 2 + 35
            trunc_v = int.from_bytes(vrs[0], "big")

            if (trunc_target & 0xff) == trunc_v:
                parity = 0
            elif ((trunc_target + 1) & 0xff) == trunc_v:
                parity = 1
            else:
                # should have matched with a previous if
                assert False

            # https://github.com/ethereum/EIPs/blob/master/EIPS/eip-155.md
            full_v = parity + tx_params["chainId"] * 2 + 35
            # 9 bytes would be big enough even for the biggest chain ID
            vrs = (int(full_v).to_bytes(9, "big"), vrs[1], vrs[2])
        else:
            # Pre EIP-155 TX
            assert False
    decoded = rlp.decode(raw_tx)
    reencoded = rlp.encode(decoded[:-3] + list(normalize_vrs(vrs)))
    addr = Account.recover_transaction(prefix + reencoded)
    return bytes.fromhex(addr[2:])
