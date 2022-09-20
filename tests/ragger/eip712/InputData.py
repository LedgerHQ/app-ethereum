#!/usr/bin/env python3

import os
import json
import sys
import re
import hashlib
from ecdsa import SigningKey
from ecdsa.util import sigencode_der
from ethereum_client import EthereumClient, EIP712FieldType
import base64

# global variables
app_client: EthereumClient = None
filtering_paths = None
current_path = list()
sig_ctx = {}




# From a string typename, extract the type and all the array depth
# Input  = "uint8[2][][4]"          |   "bool"
# Output = ('uint8', [2, None, 4])  |   ('bool', [])
def get_array_levels(typename):
    array_lvls = list()
    regex = re.compile("(.*)\[([0-9]*)\]$")

    while True:
        result = regex.search(typename)
        if not result:
            break
        typename = result.group(1)

        level_size = result.group(2)
        if len(level_size) == 0:
            level_size = None
        else:
            level_size = int(level_size)
        array_lvls.insert(0, level_size)
    return (typename, array_lvls)


# From a string typename, extract the type and its size
# Input  = "uint64"         |   "string"
# Output = ('uint', 64)     |   ('string', None)
def get_typesize(typename):
    regex = re.compile("^(\w+?)(\d*)$")
    result = regex.search(typename)
    typename = result.group(1)
    typesize = result.group(2)
    if len(typesize) == 0:
        typesize = None
    else:
        typesize = int(typesize)
    return (typename, typesize)



def parse_int(typesize):
    return (EIP712FieldType.INT, int(typesize / 8))

def parse_uint(typesize):
    return (EIP712FieldType.UINT, int(typesize / 8))

def parse_address(typesize):
    return (EIP712FieldType.ADDRESS, None)

def parse_bool(typesize):
    return (EIP712FieldType.BOOL, None)

def parse_string(typesize):
    return (EIP712FieldType.STRING, None)

def parse_bytes(typesize):
    if typesize != None:
        return (EIP712FieldType.FIX_BYTES, typesize)
    return (EIP712FieldType.DYN_BYTES, None)

# set functions for each type
parsing_type_functions = {};
parsing_type_functions["int"] = parse_int
parsing_type_functions["uint"] = parse_uint
parsing_type_functions["address"] = parse_address
parsing_type_functions["bool"] = parse_bool
parsing_type_functions["string"] = parse_string
parsing_type_functions["bytes"] = parse_bytes



def send_struct_def_field(typename, keyname):
    type_enum = None

    (typename, array_lvls) = get_array_levels(typename)
    (typename, typesize) = get_typesize(typename)

    if typename in parsing_type_functions.keys():
        (type_enum, typesize) = parsing_type_functions[typename](typesize)
    else:
        type_enum = EIP712FieldType.CUSTOM
        typesize = None

    app_client.eip712_send_struct_def_struct_field(type_enum,
                                                   typename,
                                                   typesize,
                                                   array_lvls,
                                                   keyname)
    return (typename, type_enum, typesize, array_lvls)



def encode_integer(value, typesize):
    data = bytearray()

    # Some are already represented as integers in the JSON, but most as strings
    if isinstance(value, str):
        base = 10
        if value.startswith("0x"):
            base = 16
        value = int(value, base)

    if value == 0:
        data.append(0)
    else:
        if value < 0: # negative number, send it as unsigned
            mask = 0
            for i in range(typesize): # make a mask as big as the typesize
                mask = (mask << 8) | 0xff
            value &= mask
        while value > 0:
            data.append(value & 0xff)
            value >>= 8
        data.reverse()
    return data

def encode_int(value, typesize):
    return encode_integer(value, typesize)

def encode_uint(value, typesize):
    return encode_integer(value, typesize)

def encode_hex_string(value, size):
    data = bytearray()
    value = value[2:] # skip 0x
    byte_idx = 0
    while byte_idx < size:
        data.append(int(value[(byte_idx * 2):(byte_idx * 2 + 2)], 16))
        byte_idx += 1
    return data

def encode_address(value, typesize):
    return encode_hex_string(value, 20)

def encode_bool(value, typesize):
    return encode_integer(value, typesize)

def encode_string(value, typesize):
    data = bytearray()
    for char in value:
        data.append(ord(char))
    return data

def encode_bytes_fix(value, typesize):
    return encode_hex_string(value, typesize)

def encode_bytes_dyn(value, typesize):
    # length of the value string
    # - the length of 0x (2)
    # / by the length of one byte in a hex string (2)
    return encode_hex_string(value, int((len(value) - 2) / 2))

# set functions for each type
encoding_functions = {}
encoding_functions[EIP712FieldType.INT] = encode_int
encoding_functions[EIP712FieldType.UINT] = encode_uint
encoding_functions[EIP712FieldType.ADDRESS] = encode_address
encoding_functions[EIP712FieldType.BOOL] = encode_bool
encoding_functions[EIP712FieldType.STRING] = encode_string
encoding_functions[EIP712FieldType.FIX_BYTES] = encode_bytes_fix
encoding_functions[EIP712FieldType.DYN_BYTES] = encode_bytes_dyn



def send_struct_impl_field(value, field):
    # Something wrong happened if this triggers
    if isinstance(value, list) or (field["enum"] == EIP712FieldType.CUSTOM):
        breakpoint()

    data = encoding_functions[field["enum"]](value, field["typesize"])


    if filtering_paths:
        path = ".".join(current_path)
        if path in filtering_paths.keys():
            send_filtering_field_name(filtering_paths[path])

    app_client.eip712_send_struct_impl_struct_field(data)



def evaluate_field(structs, data, field, lvls_left, new_level = True):
    array_lvls = field["array_lvls"]

    if new_level:
        current_path.append(field["name"])
    if len(array_lvls) > 0 and lvls_left > 0:
        app_client.eip712_send_struct_impl_array(len(data))
        idx = 0
        for subdata in data:
            current_path.append("[]")
            if not evaluate_field(structs, subdata, field, lvls_left - 1, False):
                return False
            current_path.pop()
            idx += 1
        if array_lvls[lvls_left - 1] != None:
            if array_lvls[lvls_left - 1] != idx:
                print("Mismatch in array size! Got %d, expected %d\n" %
                      (idx, array_lvls[lvls_left - 1]),
                      file=sys.stderr)
                return False
    else:
        if field["enum"] == EIP712FieldType.CUSTOM:
            if not send_struct_impl(structs, data, field["type"]):
                return False
        else:
            send_struct_impl_field(data, field)
    if new_level:
        current_path.pop()
    return True



def send_struct_impl(structs, data, structname):
    # Check if it is a struct we don't known
    if structname not in structs.keys():
        return False

    struct = structs[structname]
    for f in struct:
        if not evaluate_field(structs, data[f["name"]], f, len(f["array_lvls"])):
            return False
    return True

# ledgerjs doesn't actually sign anything, and instead uses already pre-computed signatures
def send_filtering_contract_name(display_name: str):
    global sig_ctx

    msg = bytearray()
    msg += sig_ctx["chainid"]
    msg += sig_ctx["caddr"]
    msg += sig_ctx["schema_hash"]
    for char in display_name:
        msg.append(ord(char))

    sig = sig_ctx["key"].sign_deterministic(msg, sigencode=sigencode_der)
    app_client.eip712_filtering_send_contract_name(display_name, sig)

# ledgerjs doesn't actually sign anything, and instead uses already pre-computed signatures
def send_filtering_field_name(display_name):
    global sig_ctx

    path_str = ".".join(current_path)

    msg = bytearray()
    msg += sig_ctx["chainid"]
    msg += sig_ctx["caddr"]
    msg += sig_ctx["schema_hash"]
    for char in path_str:
        msg.append(ord(char))
    for char in display_name:
        msg.append(ord(char))
    sig = sig_ctx["key"].sign_deterministic(msg, sigencode=sigencode_der)
    app_client.eip712_filtering_send_field_name(display_name, sig)

def read_filtering_file(domain, message, filtering_file_path):
    data_json = None
    with open(filtering_file_path) as data:
        data_json = json.load(data)
    return data_json

def prepare_filtering(filtr_data, message):
    global filtering_paths

    if "fields" in filtr_data:
        filtering_paths = filtr_data["fields"]
    else:
        filtering_paths = {}

def init_signature_context(types, domain):
    global sig_ctx

    env_key = os.getenv("CAL_SIGNATURE_TEST_KEY")
    if env_key:
        key = base64.b64decode(env_key).decode() # base 64 string -> decode bytes -> string
        print(key)
        sig_ctx["key"] = SigningKey.from_pem(key, hashlib.sha256)
        caddr = domain["verifyingContract"]
        if caddr.startswith("0x"):
            caddr = caddr[2:]
        sig_ctx["caddr"] = bytearray.fromhex(caddr)
        chainid = domain["chainId"]
        sig_ctx["chainid"] = bytearray()
        for i in range(8):
            sig_ctx["chainid"].append(chainid & (0xff << (i * 8)))
        sig_ctx["chainid"].reverse()
        schema_str = json.dumps(types).replace(" ","")
        schema_hash = hashlib.sha224(schema_str.encode())
        sig_ctx["schema_hash"] = bytearray.fromhex(schema_hash.hexdigest())

        return True
    return False

def process_file(aclient: EthereumClient, input_file_path: str, filtering_file_path = None) -> bool:
    global sig_ctx
    global app_client

    app_client = aclient
    with open(input_file_path, "r") as data:
        data_json = json.load(data)
        domain_typename = "EIP712Domain"
        message_typename = data_json["primaryType"]
        types = data_json["types"]
        domain = data_json["domain"]
        message = data_json["message"]

        if filtering_file_path:
            if not init_signature_context(types, domain):
                return False
            filtr = read_filtering_file(domain, message, filtering_file_path)

        # send types definition
        for key in types.keys():
            app_client.eip712_send_struct_def_struct_name(key)
            for f in types[key]:
                (f["type"], f["enum"], f["typesize"], f["array_lvls"]) = \
                send_struct_def_field(f["type"], f["name"])

        if filtering_file_path:
            app_client.eip712_filtering_activate()
            prepare_filtering(filtr, message)

        # send domain implementation
        app_client.eip712_send_struct_impl_root_struct(domain_typename)
        if not send_struct_impl(types, domain, domain_typename):
            return False

        if filtering_file_path:
            if filtr and "name" in filtr:
                send_filtering_contract_name(filtr["name"])
            else:
                send_filtering_contract_name(domain["name"])

        # send message implementation
        app_client.eip712_send_struct_impl_root_struct(message_typename)
        if not send_struct_impl(types, message, message_typename):
            return False

    return True
