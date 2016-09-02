#!/usr/bin/env python
"""
*******************************************************************************
*   Ledger Blue
*   (c) 2016 Ledger
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
from ledgerblue.comm import getDongle
from ledgerblue.commException import CommException
import argparse
import struct
from decimal import Decimal
from ethBase import Transaction, UnsignedTransaction
from rlp import encode
from rlp.utils import decode_hex, encode_hex, str_to_bytes

def parse_bip32_path(path):
	if len(path) == 0:
		return ""
	result = ""
	elements = path.split('/')
	for pathElement in elements:
		element = pathElement.split('\'')
		if len(element) == 1:
			result = result + struct.pack(">I", int(element[0]))			
		else:
			result = result + struct.pack(">I", 0x80000000 | int(element[0]))
	return result

parser = argparse.ArgumentParser()
parser.add_argument('--nonce', help="Nonce associated to the account")
parser.add_argument('--gasprice', help="Network gas price")
parser.add_argument('--startgas', help="startgas", default='21000')
parser.add_argument('--amount', help="Amount to send in ether")
parser.add_argument('--to', help="Destination address")
parser.add_argument('--path', help="BIP 32 path to sign with")
parser.add_argument('--data', help="Data to add, hex encoded")
args = parser.parse_args()

if args.path == None:
	args.path = "44'/60'/0'/0/0"

if args.data == None:
	args.data = ""
else:
	args.data = decode_hex(args.data[2:])

amount = Decimal(args.amount) * 10**18

tx = Transaction(
    nonce=int(args.nonce),
    gasprice=int(args.gasprice),
    startgas=int(args.startgas),
    to=decode_hex(args.to[2:]),
    value=int(amount),
    data=args.data
)

encodedTx = encode(tx, UnsignedTransaction)

donglePath = parse_bip32_path(args.path)
apdu = "e0040000".decode('hex') + chr(len(donglePath) + 1 + len(encodedTx)) + chr(len(donglePath) / 4) + donglePath + encodedTx

dongle = getDongle(True)
result = dongle.exchange(bytes(apdu))

v = result[0]
r = int(str(result[1:1 + 32]).encode('hex'), 16)
s = int(str(result[1 + 32: 1 + 32 + 32]).encode('hex'), 16)

tx = Transaction(tx.nonce, tx.gasprice, tx.startgas, tx.to, tx.value, tx.data, v, r, s)

print "Signed transaction " + encode_hex(encode(tx))
