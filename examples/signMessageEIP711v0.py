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
from ledgerblue.commException import CommException
from decimal import Decimal
from Crypto.Hash import keccak
from eth_keys import KeyAPI
import argparse
import struct
import binascii

# Define here Chain_ID
CHAIN_ID = 0

# Magic define
SIGN_MAGIC = b'\x19\x01'

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
parser.add_argument('--path', help="BIP 32 path to sign with")
parser.add_argument('--domainHash', help="Domain Hash (hex)", required=True)
parser.add_argument('--messageHash', help='Message Hash (hex)', required=True)
args = parser.parse_args()

if args.path == None:
    args.path = "44'/60'"
domainHash = binascii.unhexlify(args.domainHash)
messageHash = binascii.unhexlify(args.messageHash)

encodedTx = domainHash + messageHash

donglePath = parse_bip32_path(args.path)
apdu = bytearray.fromhex("e00c0000")
apdu.append(len(donglePath) + 1 + len(encodedTx))
apdu.append(len(donglePath) // 4)
apdu += donglePath + encodedTx

dongle = getDongle(True)
result = dongle.exchange(bytes(apdu))

v = int(result[0])

# Compute parity
if (CHAIN_ID*2 + 35) + 1 > 255:
	ecc_parity = v - ((CHAIN_ID*2 + 35) % 256)
else:
	ecc_parity = (v + 1) % 2

v = "%02X" % ecc_parity
r = binascii.hexlify(result[1:1 + 32]).decode()
s = binascii.hexlify(result[1 + 32: 1 + 32 + 32]).decode()
msg_to_sign = SIGN_MAGIC + domainHash + messageHash
hash = keccak.new(digest_bits=256, data=msg_to_sign).digest()

signature = KeyAPI.Signature(vrs=(int(v, 16), int(r, 16), int(s, 16)))
pubkey = KeyAPI.PublicKey.recover_from_msg_hash(hash, signature)

print("[INFO] Hash is: 0x", binascii.hexlify(hash).decode(), sep='');
print('{')
print('  "address": "', pubkey.to_address(), '",', sep='')
print('  "domain hash": "', binascii.hexlify(domainHash),'",', sep='')
print('  "message hash": "', binascii.hexlify(messageHash),'",', sep='')
print('  "sig": "', signature, '",', sep = '')
print('  "version": "3"')
print('  "signed": "ledger"')
print('}')
