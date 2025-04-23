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
from __future__ import print_function

from ledgerblue.comm import getDongle
import argparse
import struct
import binascii
import rlp


def der_encode(value):
    value_bytes = value.to_bytes(max(1, (value.bit_length() + 7) // 8), 'big')
    if value >= 0x80:
        value_bytes = (0x80 | len(value_bytes)).to_bytes(1, 'big') + value_bytes
    return value_bytes


def tlv_encode(tag, value):
    return der_encode(tag) + der_encode(len(value)) + value


def parse_bip32_path(path: str):
    if len(path) == 0:
        return b""
    result = b""
    elements = path.split('/')
    result += len(elements).to_bytes(1, "big")
    for pathElement in elements:
        element = pathElement.split('\'')
        if len(element) == 1:
            result = result + struct.pack(">I", int(element[0]))
        else:
            result = result + struct.pack(">I", 0x80000000 | int(element[0]))
    return result


parser = argparse.ArgumentParser()
parser.add_argument('--path', help="BIP 32 path to retrieve", default="44'/60'/0'/0/0")
parser.add_argument('--chainid', help="Chain ID", type=int, required=True)
parser.add_argument('--nonce', help="Account Nonce", type=int, required=True)
parser.add_argument('--delegate', help="Delegate address", type=str, required=True)
args = parser.parse_args()

tmp = tlv_encode(0x00, struct.pack(">B", 0x01))
data = binascii.unhexlify(args.delegate[2:])
tmp += tlv_encode(0x01, data)
tmp += tlv_encode(0x02, struct.pack(">Q", args.chainid))
tmp += tlv_encode(0x03, struct.pack(">Q", args.nonce))

tmp = parse_bip32_path(args.path) + struct.pack(">H", len(tmp)) + tmp

apdu = bytearray.fromhex("e0340100")
apdu += struct.pack(">B", len(tmp))
apdu += tmp

dongle = getDongle(True)
result = dongle.exchange(bytes(apdu))

v = result[0]
r = result[1:1 + 32]
s = result[1 + 32:]

print("v = " + str(v))
print("r = " + binascii.hexlify(r).decode('utf-8'))
print("s = " + binascii.hexlify(s).decode('utf-8'))

rlpData = [args.chainid, binascii.unhexlify(args.delegate[2:]), args.nonce, v, r, s]
print(binascii.hexlify(rlp.encode(rlpData)).decode('utf-8'))
