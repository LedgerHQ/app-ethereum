#!/usr/bin/env python
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
from ledgerblue.commException import CommException
import argparse
import struct
import binascii
import math
import rlp

def intToBytes(i: int) -> bytes:
    if i == 0:
        return b"\x00"
    return i.to_bytes(math.ceil(i.bit_length() / 8), 'big')

def parse_bip32_path(path):
    if len(path) == 0:
        return b""
    result = b""
    elements = path.split('/')
    for pathElement in elements:
        element = pathElement.split('\'')
        if len(element) == 1:
            result = result + struct.pack(">I", int(element[0]))
        else:
            result = result + struct.pack(">I", 0x80000000 | int(element[0]))
    return result


parser = argparse.ArgumentParser()
parser.add_argument('--path', help="BIP 32 path to retrieve")
parser.add_argument('--chainid', help="Chain ID", type=int, required=True)
parser.add_argument('--nonce', help="Account Nonce", type=int, required=True)
parser.add_argument('--delegate', help="Delegate address", type=str, required=True)
args = parser.parse_args()

if args.path == None:
    args.path = "44'/60'/0'/0/0"

data = binascii.unhexlify(args.delegate[2:])
tmp = intToBytes(args.chainid)
data += struct.pack(">B", len(tmp)) + tmp
tmp = intToBytes(args.nonce)
data += struct.pack(">B", len(tmp)) + tmp

donglePath = parse_bip32_path(args.path)
apdu = bytearray.fromhex("e0320000")
apdu += struct.pack(">B", len(donglePath) + 1 + len(data))
apdu += struct.pack(">B", len(donglePath) // 4)
apdu += donglePath + data

dongle = getDongle(True)
result = dongle.exchange(bytes(apdu))

v = result[0]
r = result[1 : 1 + 32]
s = result[1 + 32 :]

print("v = " + str(v))
print("r = " + binascii.hexlify(r).decode('utf-8'))
print("s = " + binascii.hexlify(s).decode('utf-8'))

rlpData = [ args.chainid, binascii.unhexlify(args.delegate[2:]), args.nonce, v, r, s ]
print(binascii.hexlify(rlp.encode(rlpData)).decode('utf-8'))

