from eth_account import Account
from eth_account.messages import encode_defunct, encode_typed_data
from eth_account.datastructures import SignedSetCodeAuthorization
from eth_account.typed_transactions.set_code_transaction import Authorization
from eth_keys.datatypes import Signature
import rlp


def get_selector_from_data(data: str) -> bytes:
    raw_data = bytes.fromhex(data[2:])
    return raw_data[:4]


def recover_message(msg, vrs: tuple[int, int, int]) -> bytes:
    if isinstance(msg, dict):  # EIP-712
        smsg = encode_typed_data(full_message=msg)
    else:  # EIP-191
        smsg = encode_defunct(primitive=msg)
    addr = Account.recover_message(smsg, vrs)
    return bytes.fromhex(addr[2:])


def recover_transaction(tx_params, vrs: tuple[int, int, int]) -> bytes:
    raw_tx = Account.create().sign_transaction(tx_params).raw_transaction
    prefix = bytes()
    if raw_tx[0] in [0x01, 0x02, 0x04]:
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
            trunc_v = vrs[0]

            if (trunc_target & 0xff) == trunc_v:
                parity = 0
            elif ((trunc_target + 1) & 0xff) == trunc_v:
                parity = 1
            else:
                # should have matched with a previous if
                assert False

            # https://github.com/ethereum/EIPs/blob/master/EIPS/eip-155.md
            full_v = parity + tx_params["chainId"] * 2 + 35
            vrs = (full_v, vrs[1], vrs[2])
        else:
            # Pre EIP-155 TX
            assert False
    decoded = rlp.decode(raw_tx)
    reencoded = rlp.encode(decoded[:-3] + list(vrs))
    addr = Account.recover_transaction(prefix + reencoded)
    return bytes.fromhex(addr[2:])


# Code inspired by :
# https://github.com/ethereum/eth-account/blob/a1ba20c9a112d3534ac3296f21f51e2f5127bf9b/eth_account/account.py#L1057
def get_authorization_obj(chain_id: int,
                          nonce: int,
                          address: bytes,
                          vrs: tuple[int, int, int]) -> SignedSetCodeAuthorization:
    unsigned_authorization = Authorization(chain_id, address, nonce)
    sig = Signature(vrs=vrs)
    return SignedSetCodeAuthorization(
        chain_id=chain_id,
        address=address,
        nonce=nonce,
        y_parity=sig.v,
        r=sig.r,
        s=sig.s,
        signature=sig,
        authorization_hash=unsigned_authorization.hash(),
    )


def recover_authorization(chain_id: int, nonce: int, address: bytes, vrs: tuple[int, int, int]) -> bytes:
    return get_authorization_obj(chain_id, nonce, address, vrs).authority
