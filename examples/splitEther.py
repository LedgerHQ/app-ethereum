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

import argparse
import json
import struct
from decimal import Decimal

import requests
from ethBase import Transaction, UnsignedTransaction, sha3
from ledgerblue.comm import getDongle
from rlp import encode
from rlp.utils import decode_hex

# https://etherscan.io/address/0x5dc8108fc79018113a58328f5283b376b83922ef#code
SPLIT_CONTRACT_FUNCTION = decode_hex("9c709343")
SPLIT_CONTRACT_ADDRESS = "5dc8108fc79018113a58328f5283b376b83922ef"


def rpc_call(http, url, methodDebug):
    req = http.get(url)
    if req.status_code == 200:
        result = json.loads(req.text)
        if "error" in result:
            raise Exception("Server error - " + methodDebug + " - " + result["error"]["message"])
        return result
    else:
        raise Exception("Server error - " + methodDebug + " got status " + str(req.status_code))


def parse_bip32_path(path):
    if len(path) == 0:
        return b""
    result = b""
    elements = path.split("/")
    for pathElement in elements:
        element = pathElement.split("'")
        if len(element) == 1:
            result = result + struct.pack(">I", int(element[0]))
        else:
            result = result + struct.pack(">I", 0x80000000 | int(element[0]))
    return result


parser = argparse.ArgumentParser()
parser.add_argument("--nonce", help="Nonce associated to the account (default : query account)")
parser.add_argument("--gasprice", help="Network gas price (default : query network)")
parser.add_argument("--startgas", help="startgas", default="80000")
parser.add_argument(
    "--startgas-delta",
    help="difference applied to startgas if gasprice is automatically fetched",
    default="1000",
)
parser.add_argument("--amount", help="Amount to send in ether (default : query amount, use maximum)")
parser.add_argument("--to", help="BIP 32 destination path (default : default ETC path)")
parser.add_argument(
    "--split-to-eth",
    help="Split to the ETH chain (default : spit to ETC chain)",
    action="store_true",
)
parser.add_argument("--path", help="BIP 32 path to sign with (default : default ETH path)")
parser.add_argument(
    "--broadcast",
    help="Broadcast generated transaction (default : false)",
    action="store_true",
)
args = parser.parse_args()

if args.path is None:
    if args.split_to_eth:  # sign from ETC
        # args.path = "44'/60'/160720'/0'/0"
        args.path = "44'/60'/0'/0"
    else:  # sign from ETH
        args.path = "44'/60'/0'/0"

if args.to is None:
    if args.split_to_eth:  # target ETH
        args.to = "44'/60'/0'/0"
    else:  # target ETC transitional
        args.to = "44'/60'/160720'/0'/0"

dongle = getDongle(True)

donglePath = parse_bip32_path(args.to)
apdu = bytearray.fromhex("e0060000")
apdu.append(len(donglePath) + 1)
apdu.append(len(donglePath) // 4)
apdu += donglePath
dongle.exchange(bytes(apdu))

apdu = bytearray.fromhex("e0020000")
apdu.append(len(donglePath) + 1)
apdu.append(len(donglePath) // 4)
apdu += donglePath
result = dongle.exchange(bytes(apdu))
publicKey = result[1 : 1 + result[0]]
encodedPublicKey = sha3(publicKey[1:])[12:]

if (args.nonce is None) or (args.amount is None):
    donglePathFrom = parse_bip32_path(args.path)
    apdu = bytearray.fromhex("e0020000")
    apdu.append(len(donglePathFrom) + 1)
    apdu.append(len(donglePathFrom) // 4)
    apdu += donglePathFrom
    result = dongle.exchange(bytes(apdu))
    publicKeyFrom = result[1 : 1 + result[0]]
    encodedPublicKeyFrom = sha3(publicKeyFrom[1:])[12:]


http = None
if (args.gasprice is None) or (args.nonce is None) or (args.amount is None) or (args.broadcast):
    http = requests.session()

if args.gasprice is None:
    print("Fetching gas price")
    result = rpc_call(
        http,
        "https://api.etherscan.io/api?module=proxy&action=eth_gasPrice",
        "gasPrice",
    )
    args.gasprice = int(result["result"], 16)
    print("Gas price:", str(args.gasprice))

if args.nonce is None:
    print("Fetching nonce")
    result = rpc_call(
        http,
        "https://api.etherscan.io/api?module=proxy&action=eth_getTransactionCount&address=0x" + encodedPublicKeyFrom.hex(),
        "getTransactionCount",
    )
    args.nonce = int(result["result"], 16)
    print("Nonce for 0x", encodedPublicKeyFrom.hex(), " ", args.nonce, sep="")

if args.amount is None:
    print("Fetching balance")
    result = rpc_call(
        http,
        "https://api.etherscan.io/api?module=account&action=balance&address=0x" + encodedPublicKeyFrom.hex(),
        "getBalance",
    )
    amount = int(result["result"])
    print("Balance for 0x", encodedPublicKeyFrom.hex(), " ", str(amount), sep="")
    amount -= (int(args.startgas) - int(args.startgas_delta)) * int(args.gasprice)
    if amount < 0:
        raise Exception("Remaining amount too small to pay for contract fees")
else:
    amount = Decimal(args.amount) * 10**18

print("Amount transferred", str(Decimal(amount) / 10**18), "to", encodedPublicKey.hex())

txData = SPLIT_CONTRACT_FUNCTION
txData += b"\x00" * 31
if args.split_to_eth:
    txData += b"\x01"
else:
    txData += b"\x00"
txData += b"\x00" * 12
txData += encodedPublicKey

tx = Transaction(
    nonce=int(args.nonce),
    gasprice=int(args.gasprice),
    startgas=int(args.startgas),
    to=decode_hex(SPLIT_CONTRACT_ADDRESS),
    value=int(amount),
    data=txData,
)

encodedTx = encode(tx, UnsignedTransaction)

donglePath = parse_bip32_path(args.path)
apdu = bytearray.fromhex("e0040000")
apdu.append(len(donglePath) + 1 + len(encodedTx))
apdu.append(len(donglePath) // 4)
apdu += donglePath + encodedTx

result = dongle.exchange(bytes(apdu))

v = result[0]
r = int.from_bytes(result[1 : 1 + 32], "big")
s = int.from_bytes(result[1 + 32 : 1 + 32 + 32], "big")

tx = Transaction(tx.nonce, tx.gasprice, tx.startgas, tx.to, tx.value, tx.data, v, r, s)
serializedTx = encode(tx)

print("Signed transaction", serializedTx.hex())

if args.broadcast:
    result = rpc_call(
        http,
        "https://api.etherscan.io/api?module=proxy&action=eth_sendRawTransaction&hex=0x" + serializedTx.hex(),
        "sendRawTransaction",
    )
    print(result)
