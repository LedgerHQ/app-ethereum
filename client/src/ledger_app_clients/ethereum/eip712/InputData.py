import hashlib
import json
import re
import signal
import sys
import copy
from typing import Any, Callable, Optional, Union
import struct

from client import keychain
from client.client import EthAppClient, EIP712FieldType


# global variables
app_client: EthAppClient = None
filtering_paths: dict = {}
current_path: list[str] = list()
sig_ctx: dict[str, Any] = {}


def default_handler():
    raise RuntimeError("Uninitialized handler")


autonext_handler: Callable = default_handler
is_golden_run: bool


# From a string typename, extract the type and all the array depth
# Input  = "uint8[2][][4]"          |   "bool"
# Output = ('uint8', [2, None, 4])  |   ('bool', [])
def get_array_levels(typename):
    array_lvls = list()
    regex = re.compile(r"(.*)\[([0-9]*)\]$")

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
    regex = re.compile(r"^(\w+?)(\d*)$")
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
    if typesize is not None:
        return (EIP712FieldType.FIX_BYTES, typesize)
    return (EIP712FieldType.DYN_BYTES, None)


# set functions for each type
parsing_type_functions = {}
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

    with app_client.eip712_send_struct_def_struct_field(type_enum,
                                                        typename,
                                                        typesize,
                                                        array_lvls,
                                                        keyname):
        pass
    return (typename, type_enum, typesize, array_lvls)


def encode_integer(value: Union[str, int], typesize: int) -> bytes:
    # Some are already represented as integers in the JSON, but most as strings
    if isinstance(value, str):
        value = int(value, 0)

    if value == 0:
        data = b'\x00'
    else:
        # biggest uint type accepted by struct.pack
        uint64_mask = 0xffffffffffffffff
        data = struct.pack(">QQQQ",
                           (value >> 192) & uint64_mask,
                           (value >> 128) & uint64_mask,
                           (value >> 64) & uint64_mask,
                           value & uint64_mask)
        data = data[len(data) - typesize:]
        data = data.lstrip(b'\x00')
    return data


def encode_int(value: str, typesize: int) -> bytes:
    return encode_integer(value, typesize)


def encode_uint(value: str, typesize: int) -> bytes:
    return encode_integer(value, typesize)


def encode_hex_string(value: str, size: int) -> bytes:
    assert value.startswith("0x")
    value = value[2:]
    if len(value) < (size * 2):
        value = value.rjust(size * 2, "0")
    assert len(value) == (size * 2)
    return bytes.fromhex(value)


def encode_address(value: str, typesize: int) -> bytes:
    return encode_hex_string(value, 20)


def encode_bool(value: str, typesize: int) -> bytes:
    return encode_integer(value, 1)


def encode_string(value: str, typesize: int) -> bytes:
    return value.encode()


def encode_bytes_fix(value: str, typesize: int) -> bytes:
    return encode_hex_string(value, typesize)


def encode_bytes_dyn(value: str, typesize: int) -> bytes:
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
            if filtering_paths[path]["type"] == "amount_join_token":
                send_filtering_amount_join_token(filtering_paths[path]["token"])
            elif filtering_paths[path]["type"] == "amount_join_value":
                if "token" in filtering_paths[path].keys():
                    token = filtering_paths[path]["token"]
                else:
                    # Permit (ERC-2612)
                    token = 0xff
                send_filtering_amount_join_value(token,
                                                 filtering_paths[path]["name"])
            elif filtering_paths[path]["type"] == "datetime":
                send_filtering_datetime(filtering_paths[path]["name"])
            elif filtering_paths[path]["type"] == "raw":
                send_filtering_raw(filtering_paths[path]["name"])
            else:
                assert False

    with app_client.eip712_send_struct_impl_struct_field(data):
        enable_autonext()
    disable_autonext()


def evaluate_field(structs, data, field, lvls_left, new_level=True):
    array_lvls = field["array_lvls"]

    if new_level:
        current_path.append(field["name"])
    if len(array_lvls) > 0 and lvls_left > 0:
        with app_client.eip712_send_struct_impl_array(len(data)):
            pass
        idx = 0
        for subdata in data:
            current_path.append("[]")
            if not evaluate_field(structs, subdata, field, lvls_left - 1, False):
                return False
            current_path.pop()
            idx += 1
        if array_lvls[lvls_left - 1] is not None:
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


def start_signature_payload(ctx: dict, magic: int) -> bytearray:
    to_sign = bytearray()
    # magic number so that signature for one type of filter can't possibly be
    # valid for another, defined in APDU specs
    to_sign.append(magic)
    to_sign += ctx["chainid"]
    to_sign += ctx["caddr"]
    to_sign += ctx["schema_hash"]
    return to_sign


# ledgerjs doesn't actually sign anything, and instead uses already pre-computed signatures
def send_filtering_message_info(display_name: str, filters_count: int):
    global sig_ctx

    to_sign = start_signature_payload(sig_ctx, 183)
    to_sign.append(filters_count)
    to_sign += display_name.encode()

    sig = keychain.sign_data(keychain.Key.CAL, to_sign)
    with app_client.eip712_filtering_message_info(display_name, filters_count, sig):
        enable_autonext()
    disable_autonext()


def send_filtering_amount_join_token(token_idx: int):
    global sig_ctx

    path_str = ".".join(current_path)

    to_sign = start_signature_payload(sig_ctx, 11)
    to_sign += path_str.encode()
    to_sign.append(token_idx)
    sig = keychain.sign_data(keychain.Key.CAL, to_sign)
    with app_client.eip712_filtering_amount_join_token(token_idx, sig):
        pass


def send_filtering_amount_join_value(token_idx: int, display_name: str):
    global sig_ctx

    path_str = ".".join(current_path)

    to_sign = start_signature_payload(sig_ctx, 22)
    to_sign += path_str.encode()
    to_sign += display_name.encode()
    to_sign.append(token_idx)
    sig = keychain.sign_data(keychain.Key.CAL, to_sign)
    with app_client.eip712_filtering_amount_join_value(token_idx, display_name, sig):
        pass


def send_filtering_datetime(display_name: str):
    global sig_ctx

    path_str = ".".join(current_path)

    to_sign = start_signature_payload(sig_ctx, 33)
    to_sign += path_str.encode()
    to_sign += display_name.encode()
    sig = keychain.sign_data(keychain.Key.CAL, to_sign)
    with app_client.eip712_filtering_datetime(display_name, sig):
        pass


# ledgerjs doesn't actually sign anything, and instead uses already pre-computed signatures
def send_filtering_raw(display_name):
    global sig_ctx

    path_str = ".".join(current_path)

    to_sign = start_signature_payload(sig_ctx, 72)
    to_sign += path_str.encode()
    to_sign += display_name.encode()
    sig = keychain.sign_data(keychain.Key.CAL, to_sign)
    with app_client.eip712_filtering_raw(display_name, sig):
        pass


def prepare_filtering(filtr_data, message):
    global filtering_paths

    if "fields" in filtr_data:
        filtering_paths = filtr_data["fields"]
    else:
        filtering_paths = {}
    if "tokens" in filtr_data:
        for token in filtr_data["tokens"]:
            app_client.provide_token_metadata(token["ticker"],
                                              bytes.fromhex(token["addr"][2:]),
                                              token["decimals"],
                                              token["chain_id"])


def handle_optional_domain_values(domain):
    if "chainId" not in domain.keys():
        domain["chainId"] = 0
    if "verifyingContract" not in domain.keys():
        domain["verifyingContract"] = "0x0000000000000000000000000000000000000000"


def init_signature_context(types, domain):
    global sig_ctx

    handle_optional_domain_values(domain)
    caddr = domain["verifyingContract"]
    if caddr.startswith("0x"):
        caddr = caddr[2:]
    sig_ctx["caddr"] = bytearray.fromhex(caddr)
    chainid = domain["chainId"]
    sig_ctx["chainid"] = bytearray()
    for i in range(8):
        sig_ctx["chainid"].append(chainid & (0xff << (i * 8)))
    sig_ctx["chainid"].reverse()
    schema_str = json.dumps(types).replace(" ", "")
    schema_hash = hashlib.sha224(schema_str.encode())
    sig_ctx["schema_hash"] = bytearray.fromhex(schema_hash.hexdigest())


def next_timeout(_signum: int, _frame):
    autonext_handler()


def enable_autonext():
    if app_client._client.firmware.device in ("stax", "flex"):
        delay = 1/3
    else:
        delay = 1/4

    # golden run has to be slower to make sure we take good snapshots
    # and not processing/loading screens
    if is_golden_run:
        delay *= 3

    signal.setitimer(signal.ITIMER_REAL, delay, delay)


def disable_autonext():
    signal.setitimer(signal.ITIMER_REAL, 0, 0)


def process_data(aclient: EthAppClient,
                 data_json: dict,
                 filters: Optional[dict] = None,
                 autonext: Optional[Callable] = None,
                 golden_run: bool = False) -> bool:
    global sig_ctx
    global app_client
    global autonext_handler
    global is_golden_run

    # deepcopy because this function modifies the dict
    data_json = copy.deepcopy(data_json)
    app_client = aclient
    domain_typename = "EIP712Domain"
    message_typename = data_json["primaryType"]
    types = data_json["types"]
    domain = data_json["domain"]
    message = data_json["message"]

    if autonext:
        autonext_handler = autonext
        signal.signal(signal.SIGALRM, next_timeout)

    is_golden_run = golden_run

    if filters:
        init_signature_context(types, domain)

    # send types definition
    for key in types.keys():
        with app_client.eip712_send_struct_def_struct_name(key):
            pass
        for f in types[key]:
            (f["type"], f["enum"], f["typesize"], f["array_lvls"]) = \
             send_struct_def_field(f["type"], f["name"])

    if filters:
        with app_client.eip712_filtering_activate():
            pass
        prepare_filtering(filters, message)

    # send domain implementation
    with app_client.eip712_send_struct_impl_root_struct(domain_typename):
        enable_autonext()
    disable_autonext()
    if not send_struct_impl(types, domain, domain_typename):
        return False

    if filters:
        if filters and "name" in filters:
            send_filtering_message_info(filters["name"], len(filtering_paths))
        else:
            send_filtering_message_info(domain["name"], len(filtering_paths))

    # send message implementation
    with app_client.eip712_send_struct_impl_root_struct(message_typename):
        enable_autonext()
    disable_autonext()
    if not send_struct_impl(types, message, message_typename):
        return False

    return True
