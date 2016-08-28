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
from ethereum.transactions import Transaction, UnsignedTransaction
from rlp import encode
from rlp.utils import decode_hex, encode_hex, str_to_bytes

from ethereum import utils

SPLIT_CONTRACT_FUNCTION = decode_hex("9c709343")
SPLIT_CONTRACT_ADDRESS = "5dc8108fc79018113a58328f5283b376b83922ef"

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
parser.add_argument('--startgas', help="startgas", default='80000')
parser.add_argument('--amount', help="Amount to send in ether")
parser.add_argument('--to', help="BIP 32 destination path")
parser.add_argument('--split-to-eth', help="Split to the ETH chain (default : spit to ETC chain)", action='store_true')
parser.add_argument('--path', help="BIP 32 path to sign with")
args = parser.parse_args()

if args.path == None:
	if args.split_to_eth: #sign from ETC
		#args.path = "44'/60'/160720'/0'/0"
		args.path = "44'/60'/0'/0"
	else: #sign from ETH
		args.path = "44'/60'/0'/0"

if args.to == None:
	if args.split_to_eth: #target ETH
		args.to = "44'/60'/0'/0"
	else: #target ETC transitional
		args.to = "44'/60'/160720'/0'/0"

dongle = getDongle(True)

donglePath = parse_bip32_path(args.to)
apdu = "e0060000".decode('hex') + chr(len(donglePath) + 1) + chr(len(donglePath) / 4) + donglePath
dongle.exchange(bytes(apdu))

apdu = "e0020000".decode('hex') + chr(len(donglePath) + 1) + chr(len(donglePath) / 4) + donglePath
result = dongle.exchange(bytes(apdu))
publicKey = str(result[1 : 1 + result[0]])
encodedPublicKey = utils.sha3(publicKey[1:])[12:]

txData = SPLIT_CONTRACT_FUNCTION
txData += "\x00" * 31
if (args.split_to_eth):
	txData += "\x01"
else:
	txData += "\x00"
txData += "\x00" * 12
txData += encodedPublicKey

amount = Decimal(args.amount) * 10**18

tx = Transaction(
    nonce=int(args.nonce),
    gasprice=int(args.gasprice),
    startgas=int(args.startgas),
    to=decode_hex(SPLIT_CONTRACT_ADDRESS),
    value=int(amount),
    data=txData
)

encodedTx = encode(tx, UnsignedTransaction)

donglePath = parse_bip32_path(args.path)
apdu = "e0040000".decode('hex') + chr(len(donglePath) + 1 + len(encodedTx)) + chr(len(donglePath) / 4) + donglePath + encodedTx

result = dongle.exchange(bytes(apdu))

v = result[0]
r = int(str(result[1:1 + 32]).encode('hex'), 16)
s = int(str(result[1 + 32: 1 + 32 + 32]).encode('hex'), 16)

tx = Transaction(tx.nonce, tx.gasprice, tx.startgas, tx.to, tx.value, tx.data, v, r, s)

print "Signed transaction " + encode_hex(encode(tx))
