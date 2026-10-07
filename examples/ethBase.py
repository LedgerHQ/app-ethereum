#!/usr/bin/env python3
"""
*******************************************************************************
*   Ledger Ethereum App
*   (c) 2016-2019 Ledger
*
*  Licensed under the Apache License, Version 2.0 (the "License");
*  you may not use this file except in compliance with the License.
*  You may obtain a copy of the License at
*
*      http://www.apache.org/licenses/LICENSE-2.0
*
*  Unless required by applicable law or agreed to in writing, software
*  distributed under the License is distributed on an "AS IS" BASIS,
*  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
*  See the License for the specific language governing permissions and
*  limitations under the License.
********************************************************************************
"""

from eth_utils import keccak
from rlp import Serializable
from rlp.sedes import Binary, big_endian_int, binary

address = Binary.fixed_length(20, allow_empty=True)


def sha3(seed):
    if isinstance(seed, str):
        seed = seed.encode()
    return keccak(seed)


class Transaction(Serializable):
    fields = [  # noqa: RUF012
        ("nonce", big_endian_int),
        ("gasprice", big_endian_int),
        ("startgas", big_endian_int),
        ("to", address),
        ("value", big_endian_int),
        ("data", binary),
        ("v", big_endian_int),
        ("r", big_endian_int),
        ("s", big_endian_int),
    ]

    def __init__(self, nonce, gasprice, startgas, to, value, data, v=0, r=0, s=0):
        super().__init__(nonce, gasprice, startgas, to, value, data, v, r, s)


class UnsignedTransaction(Serializable):
    fields = [  # noqa: RUF012
        ("nonce", big_endian_int),
        ("gasprice", big_endian_int),
        ("startgas", big_endian_int),
        ("to", address),
        ("value", big_endian_int),
        ("data", binary),
        ("chainid", big_endian_int),
        ("dummy1", big_endian_int),
        ("dummy2", big_endian_int),
    ]


def unsigned_tx_from_tx(tx):
    return UnsignedTransaction(
        nonce=tx.nonce,
        gasprice=tx.gasprice,
        startgas=tx.startgas,
        to=tx.to,
        value=tx.value,
        data=tx.data,
    )
