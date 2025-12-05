#!/usr/bin/env python3
"""Decode APDU replay file to extract transaction details."""

from pathlib import Path
import argparse
import json
import logging
from typing import Callable, Optional
import rlp
import re
import requests
from eth_utils import to_int
from eth_abi import decode

from client.command_builder import InsType, P1Type, P2Type
from client.tlv import FieldTag as TLVFieldTag
from client.gcs import TxInfoTag as TagTransactionInfo, FieldTag as TagTransactionField
from client.enum_value import Tag as TagEnumValue
from client.eip712.struct import EIP712FieldType as StructFieldType, EIP712TypeDescMask as StructTypeDescMask


APP_CLA: int = 0xE0

MAX_BYTES_LEN: int = 70  # Max bytes to be logged fully

# Local selector cache at module level
LOCAL_SELECTORS = {}
CACHE_FILE = Path(__file__).parent / "function_selectors.json"

logger = logging.getLogger(__name__)

transaction = {
    "BIP32 path": "",  # Formatted BIP32 path
    "RLP transaction": bytes(),  # Full RLP bytes data
    "TX params": {},  # Decoded Transaction fields
}

tlv_info = {
    "Struct": {},  # Temporary TLV payload data and remaining length
    "TLV": {},     # Decoded TLV fields
}


# ===============================================================================
#          Parameters
# ===============================================================================
def init_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description="Decode APDU replay file to extract transaction details.")
    parser.add_argument("--input", "-i", required=True, help="Input apdu replay file.")
    parser.add_argument("--verbose", "-v", action='store_true', help="Verbose mode")
    return parser


# ===============================================================================
#          Logging
# ===============================================================================
def set_logging(verbose: bool = False) -> None:
    if verbose:
        logger.setLevel(level=logging.DEBUG)
    else:
        logger.setLevel(level=logging.INFO)
    logger.handlers.clear()
    handler = logging.StreamHandler()
    handler.setFormatter(logging.Formatter("[%(levelname)s] %(message)s"))
    logger.addHandler(handler)


def print_title(title: str) -> None:
    logger.info("=" * 60)
    logger.info(f"DECODED {title.upper()}")
    logger.info("=" * 60)


# ===============================================================================
#          BIP32 Path Formatting
# ===============================================================================
def format_bip32_path(path_indices: list) -> str:
    """Convert BIP32 path indices to readable format (m/44'/60'/0'/0/0)."""
    BIP32_HARDENED = 0x80000000
    path_parts = ["m"]

    for index in path_indices:
        if index >= BIP32_HARDENED:
            path_parts.append(f"{index - BIP32_HARDENED}'")
        else:
            path_parts.append(str(index))

    return "/".join(path_parts)


# ===============================================================================
#          Selector function Decoding
# ===============================================================================
def load_selector_cache() -> None:
    """Load function selector cache from JSON file."""
    global LOCAL_SELECTORS

    try:
        with open(CACHE_FILE, encoding='utf-8') as f:
            LOCAL_SELECTORS = json.load(f)
        logger.debug(f"Loaded {len(LOCAL_SELECTORS)} selectors from cache")
        return
    except FileNotFoundError:
        logger.warning(f"Selector cache file not found: {CACHE_FILE}")
        logger.warning("Run generate_selector_cache.py to create the cache")
    except Exception as e:
        logger.error(f"Failed to load selector cache: {e}")


def decode_function_selector(selector: str) -> str:
    """Decode function selector using local cache first, then API.

    Priority:
    1. Local cache (function_selectors.json)
    2. Online API (4byte.directory)

    Args:
        selector: 8-character hex string (without 0x prefix)

    Returns:
        Function signature or "Unknown (0x...)"
    """
    if not selector or len(selector) != 8:
        logger.error("Invalid selector %s", selector)
        return "Invalid selector"

    selector = selector.lower()

    # 1. Check local cache first
    if selector in LOCAL_SELECTORS:
        logger.debug(f"Found selector {selector} in local cache")
        return LOCAL_SELECTORS[selector]

    # 2. Try online API as fallback
    logger.debug(f"Selector {selector} not in cache, querying API...")
    try:
        url = f"https://www.4byte.directory/api/v1/signatures/?hex_signature=0x{selector}"
        response = requests.get(url, timeout=5)
        if response.status_code == 200:
            data = response.json()
            if data.get('results'):
                signature = data['results'][0]['text_signature']
                logger.debug(f"Found selector {selector} online: {signature}")
                return signature
        logger.debug(f"Failed to retrieve selector {selector} online ({response.status_code})")
    except Exception as e:
        logger.error(f"Failed to query API for selector {selector}: {e}")

    return f"Unknown (0x{selector})"


# ===============================================================================
#          DER fields Decoding
# ===============================================================================
def der_decode(data: bytes, offset: int = 0) -> tuple[int, int]:
    """Decode a DER-encoded integer from bytes.

    Args:
        data: The bytes to decode from
        offset: Starting offset in the data

    Returns:
        tuple[int, int]: (decoded_value, new_offset)
        - decoded_value: The decoded integer value
        - new_offset: The offset after reading the value
    """
    if offset >= len(data):
        raise ValueError("Offset out of bounds")

    first_byte = data[offset]

    # Check if this is a multi-byte length encoding
    if first_byte & 0x80:
        # The lower 7 bits indicate how many bytes encode the length
        num_length_bytes = first_byte & 0x7F
        if offset + 1 + num_length_bytes > len(data):
            raise ValueError("Invalid DER encoding: not enough bytes for length")

        # Read the actual value from the following bytes
        value = int.from_bytes(data[offset + 1:offset + 1 + num_length_bytes], 'big')
        new_offset = offset + 1 + num_length_bytes
    else:
        # Single byte value
        value = first_byte
        new_offset = offset + 1

    return value, new_offset


# ===============================================================================
#          ABI Parameter Parsing
# ===============================================================================
def parse_function_signature(signature: str) -> tuple:
    """Parse function signature to extract parameter types.

    Args:
        signature: Function signature string.

    Example:
        - execTransaction(address,uint256,bytes,uint8,...)
        - batchExecute((address,uint256,bytes)[])

    Returns: ('function_name', ['param_type1', 'param_type2', ...])
    """
    match = re.match(r'(\w+)\((.*)\)', signature)
    if not match:
        return None, []

    function_name = match.group(1)
    params_str = match.group(2)

    if not params_str:
        return function_name, []

    # Parse parameters (handle nested types like bytes, arrays, tuples)
    params = []
    depth = 0
    current_param = ""

    for char in params_str:
        if char == '(' or char == '[':
            depth += 1
            current_param += char
        elif char == ')' or char == ']':
            depth -= 1
            current_param += char
        elif char == ',' and depth == 0:
            params.append(current_param.strip())
            current_param = ""
        else:
            current_param += char

    if current_param:
        params.append(current_param.strip())

    return function_name, params


def format_decoded_value(param_type: str, value, indent: int = 0) -> str:
    """Format a decoded value based on its type.

    Args:
        param_type: ABI type (e.g., 'address', 'uint256', '(address,uint256,bytes)[]')
        value: Decoded value
        indent: Indentation level for nested structures

    Returns:
        Formatted string representation
    """
    prefix = "  " * indent

    # Handle arrays (including tuple arrays)
    if param_type.endswith('[]'):
        if not value:
            return "[]"

        inner_type = param_type[:-2]  # Remove '[]'
        result = f"{prefix}\n"

        for i, item in enumerate(value):
            result += f"{prefix}  [{i}]:\n"
            formatted_item = format_decoded_value(inner_type, item, indent + 2)
            result += formatted_item + "\n"

        result += f"{prefix}"
        return result

    # Handle tuples
    if param_type.startswith('(') and param_type.endswith(')'):
        # Extract tuple component types
        inner = param_type[1:-1]  # Remove parentheses
        component_types = []
        depth = 0
        current = ""

        for char in inner:
            if char in '([':
                depth += 1
                current += char
            elif char in ')]':
                depth -= 1
                current += char
            elif char == ',' and depth == 0:
                component_types.append(current.strip())
                current = ""
            else:
                current += char

        if current:
            component_types.append(current.strip())

        # Format tuple components
        if not isinstance(value, (list, tuple)):
            return f"{prefix}{value}"

        result = f"{prefix}\n"
        for i, (comp_type, comp_value) in enumerate(zip(component_types, value)):
            type_name = comp_type.split('[')[0]  # Get base type name
            if comp_type.startswith('bytes') and isinstance(comp_value, bytes) and len(comp_value.hex()) > MAX_BYTES_LEN:
                result += f"{prefix}  {type_name}[{len(comp_value)}]: "
            else:
                result += f"{prefix}  {type_name}: "
            # Format the component value
            if comp_type.startswith('(') or comp_type.endswith('[]'):
                result += "\n" + format_decoded_value(comp_type, comp_value, indent + 2)
            else:
                formatted = format_decoded_value(comp_type, comp_value, 0)
                result += formatted.strip()
            result += "\n"

        result += f"{prefix}"
        return result

    # Handle basic types
    if param_type == 'address':
        if isinstance(value, bytes):
            return f"{prefix}0x{value.hex()}"
        return f"{prefix}{value}"
    if param_type.startswith('bytes'):
        if isinstance(value, bytes):
            return f"{prefix}0x{value.hex()}"
        return f"{prefix}{value}"
    if param_type.startswith('uint') or param_type.startswith('int'):
        return f"{prefix}{int(value)}"
    if isinstance(value, bytes):
        return f"{prefix}0x{value.hex()}"
    return f"{prefix}{value}"


def decode_function_data(selector: str, data: bytes) -> dict:
    """Decode function call data using the function signature.

    Returns:
        Dictionary mapping parameter names to decoded values
    """

    if not selector or selector.startswith("Unknown") or \
        selector == "Invalid selector":
        logger.debug("Cannot decode function data: invalid or unknown selector")
        return {}

    try:
        # Extract function signature and parameter types
        selector_decoded = selector.split(' - ')[1]
        function_name, param_types = parse_function_signature(selector_decoded)

        if not param_types:
            logger.debug(f"Function {function_name} has no parameters to decode")  # Améliorer le message
            return {}

        # Decode the data
        decoded_params = decode(param_types, data)

        # Create a dictionary with parameter names
        result = {}
        for i, (param_type, value) in enumerate(zip(param_types, decoded_params)):
            # Create descriptive parameter name
            if param_type.startswith('(') and param_type.endswith('[]'):
                param_name = f"param_{i} (array of structs)"
            elif param_type.startswith('('):
                param_name = f"param_{i} (struct)"
            elif param_type == "bytes":
                # Try to extract the included function selector if present
                if isinstance(value, bytes) and len(value) >= 4:
                    inner_selector = value[:4].hex()
                    inner_signature = decode_function_selector(inner_selector)
                    if inner_signature.startswith("Unknown") or inner_signature == "Invalid selector":
                        param_name = f"param_{i} (bytes)"
                    else:
                        param_name = f"param_{i} (bytes, 0x{inner_selector} - {inner_signature})"
                        value = value[4:]  # Remove inner selector for decoding
                        # Decode inner function data
                        inner_selector = f"0x{inner_selector} - {inner_signature}"
                        decoded_params = decode_function_data(inner_selector, value)
                        format_decoded_params(decoded_params, f"'param_{i}' Function Parameters (bytes)")
                else:
                    param_name = f"param_{i} ({param_type})"
            else:
                param_name = f"param_{i} ({param_type})"

            # Format the value
            result[param_name] = format_decoded_value(param_type, value)

        return result

    except Exception as e:
        logger.error(f"Failed to decode function data: {e}")
        return {}


def format_decoded_params(params: dict, title: str) -> None:
    """Format and log decoded parameters in a readable way."""
    if not params:
        return

    print_title(title)

    for param_name, value in params.items():
        # For nested structures (arrays, tuples), log directly without prefix
        if isinstance(value, str) and ('\n' in value or value.strip().startswith('[')):
            logger.info(f"{param_name}:")
            # Split and log each line separately
            for line in value.split('\n'):
                if line.strip():
                    logger.info(line)
        elif isinstance(value, str):
            length = len(value.replace("0x", ""))
            if value.startswith("0x"):
                length = (length // 2)  # Hex string length adjustment without '0x'
            if param_name != "selector" and length > MAX_BYTES_LEN:
                logger.info(f"{param_name}[{length}]: {value[:MAX_BYTES_LEN]}...")
                logger.debug(f"{param_name}[{length}]: {value}")
            elif value:
                logger.info(f"{param_name}: {value}")
        elif isinstance(value, bytes):
            value_hex = f"0x{value.hex()}"
            length = len(value)
            if length > MAX_BYTES_LEN:
                logger.info(f"{param_name}[{length}]: {value_hex[:MAX_BYTES_LEN]}...")
                logger.debug(f"{param_name}[{length}]: {value_hex}")
            elif value:
                logger.info(f"{param_name}: {value_hex}")
        else:
            logger.info(f"{param_name}: {value}")


# ===============================================================================
#          Value Formatting Helpers
# ===============================================================================
def format_gas_price(value_wei: int) -> str:
    """Format gas price in wei, gwei, and ether."""
    if value_wei == 0:
        return "0 wei"

    gwei = value_wei / 1e9
    ether = value_wei / 1e18

    if gwei < 1:
        return f"{value_wei} wei"
    elif gwei < 1000:
        return f"{gwei:.2f} gwei ({value_wei} wei)"
    else:
        return f"{gwei:.2f} gwei ({ether:.6f} ETH)"


def format_amount(value_wei: int) -> str:
    """Format amount in wei and ether."""
    if value_wei == 0:
        return "0 wei (0 ETH)"

    ether = value_wei / 1e18

    if ether < 0.000001:
        return f"{value_wei} wei"
    else:
        return f"{value_wei} wei ({ether:.6f} ETH)"


def format_gas_limit(value: int) -> str:
    """Format gas limit with common reference values."""
    references = {
        21000: "21,000 (simple transfer)",
        65000: "~65,000 (token transfer)",
        100000: "~100,000 (basic contract)",
    }

    # Find closest reference
    closest = min(references.keys(), key=lambda x: abs(x - value))
    if abs(closest - value) < 10000:
        return f"{value:,} ≈ {references[closest]}"

    return f"{value:,}"


# ===============================================================================
#          RLP Transaction Decoding
# ===============================================================================
def decode_rlp_transaction(rlp_bytes: bytes) -> None:
    """Decode RLP-encoded Ethereum transaction, based on EIP-1559
       https://github.com/ethereum/EIPs/blob/master/EIPS/eip-1559.md#abstract

    Args:
        rlp_bytes: RLP-encoded transaction bytes
    """

    decoded_tx = rlp.decode(rlp_bytes[1:])  # EIP-1559: Skip prefix byte
    nb_field = len(decoded_tx)

    # Extract selector if present
    selector = ""
    selector_decoded = ""
    if nb_field > 7 and decoded_tx[7] and len(decoded_tx[7]) >= 4:
        selector = decoded_tx[7][:4].hex()
        selector_decoded = decode_function_selector(selector)

    # Extract raw values
    chain_id = to_int(decoded_tx[0]) if nb_field > 0 and decoded_tx[0] else 0
    nonce = to_int(decoded_tx[1]) if nb_field > 1 and decoded_tx[1] else 0
    max_priority_fee = to_int(decoded_tx[2]) if nb_field > 2 and decoded_tx[2] else 0
    max_fee = to_int(decoded_tx[3]) if nb_field > 3 and decoded_tx[3] else 0
    gas_limit = to_int(decoded_tx[4]) if nb_field > 4 and decoded_tx[4] else 0
    to_address = to_int(decoded_tx[5]) if nb_field > 5 and decoded_tx[5] else 0
    amount = to_int(decoded_tx[6]) if nb_field > 6 and decoded_tx[6] else 0

    transaction["TX params"] = {
        "chainId": chain_id,
        "nonce": nonce,
        "maxPriorityFeePerGas": f"{max_priority_fee} -> {format_gas_price(max_priority_fee)}",
        "maxFeePerGas": f"{max_fee} -> {format_gas_price(max_fee)}",
        "gas": f"{gas_limit} -> {format_gas_limit(gas_limit)}",
        "to": f"0x{to_address:040x}" if to_address else "0x0",
        "amount": f"{amount} -> {format_amount(amount)}",
        "selector": f"0x{selector} - {selector_decoded}",
        "data": decoded_tx[7][4:] if nb_field > 7 and decoded_tx[7] else bytes(),
        "access_list": decoded_tx[8].hex() if nb_field > 8 and decoded_tx[8] else "",
        "signature_y_parity": decoded_tx[9].hex() if nb_field > 9 and decoded_tx[9] else "",
        "signature_r_parity": decoded_tx[10].hex() if nb_field > 10 and decoded_tx[10] else "",
        "signature_s_parity": decoded_tx[11].hex() if nb_field > 11 and decoded_tx[11] else "",
    }


# ===============================================================================
#          Generic TLV decoding
# ===============================================================================
def decode_generic_tlv(tag_enum_class: type,
                       string_tags: list = None,
                       number_tags: list = None,
                       selector_tags: list = None,
                       skip_tags: list = None) -> None:
    """Decode TLV-encoded structure with generic tag handling.

    Args:
        tag_enum_class: The IntEnum class to use for tags (e.g., TagTransactionInfo)
        string_tags: List of tags that should be decoded as UTF-8 strings
        number_tags: List of tags that should be decoded as integers
        selector_tags: List of tags that should be decoded as function selectors
        skip_tags: List of tags that should be skipped (not stored)
    """
    if string_tags is None:
        string_tags = []
    if skip_tags is None:
        skip_tags = []
    if number_tags is None:
        number_tags = []
    if selector_tags is None:
        selector_tags = []

    tlv_hex = tlv_info["Struct"]["Data"]
    tlv_len = len(tlv_hex)
    offset = 0

    while offset < tlv_len:
        try:
            # Decode tag
            tag_value, offset = der_decode(tlv_hex, offset)
            tag = tag_enum_class(tag_value)

            # Decode length
            length, offset = der_decode(tlv_hex, offset)

            # Extract value
            value = tlv_hex[offset:offset + length]
            offset += length

            # Skip tags that should not be displayed
            if tag in skip_tags:
                continue

            # Decode value based on tag type
            if tag in number_tags:
                val_int = to_int(value)
                decoded_value = f"0x{val_int:x} -> {val_int}"
            elif tag in string_tags:
                try:
                    decoded_value = value.decode('utf-8')
                except UnicodeDecodeError:
                    decoded_value = f"0x{value.hex()}"
            elif tag in selector_tags:
                decoded_value = f"0x{value.hex()} - {decode_function_selector(value.hex())}"
            else:
                decoded_value = f"0x{value.hex()}"

            # Store with tag name as key
            tlv_info["TLV"][tag.name] = decoded_value

        except (ValueError, IndexError) as e:
            logger.error(f"Failed to decode TLV at offset {offset}: {e}")
            break


# ===============================================================================
#          Decode APDU SIGN
# ===============================================================================
def decode_sign_apdu(apdu: bytes) -> None:
    """Decode APDU SIGN command for Ethereum transaction signing.
    Args:
        apdu: APDU command (without "0x" prefix)
    """

    p1 = to_int(apdu[2])
    p2 = to_int(apdu[3])
    lc = int(apdu[4])
    data = apdu[5:5 + lc]

    if p2 == P2Type.SIGN_STORE:
        if p1 == P1Type.SIGN_FIRST_CHUNK:
            # Extract derivation path
            path_length = to_int(data[0])
            data = data[1:]  # Skip path length byte
            derived_path = []
            for _ in range(path_length):
                index = to_int(data[:4])
                derived_path.append(index)
                data = data[4:]  # Skip 4 bytes (8 hex chars) for each index
            # Format and display the path
            transaction["BIP32 path"] = format_bip32_path(derived_path)

        # The remaining data is the RLP-encoded transaction
        transaction["RLP transaction"] += data
    elif p2 == P2Type.SIGN_START:
        print_title("SIGN APDU")

        decode_rlp_transaction(transaction["RLP transaction"])

        # Log transaction fields
        logger.info(f"BIP32 path: {transaction['BIP32 path']}")
        logger.info(f"RLP transaction[{len(transaction['RLP transaction'])}]: 0x{transaction['RLP transaction'].hex()[:MAX_BYTES_LEN]}...")
        format_decoded_params(transaction["TX params"], "Transaction Params")

        # Print 32-bytes length chunks of RLP transaction
        rlp_tx = transaction["TX params"]["data"]
        logger.debug("=" * 60)
        logger.debug("RLP TRANSACTION DATA (32-byte chunks)")
        logger.debug("=" * 60)
        for i in range(0, len(rlp_tx), 32):
            chunk_index = i // 32
            logger.debug(f"Chunk {chunk_index:3d} [{i:4d}-{i+31:4d}]: {rlp_tx[i:i+32].hex()}")

        # Decode function parameters if available
        decoded_params = decode_function_data(transaction["TX params"]["selector"], transaction["TX params"]["data"])
        format_decoded_params(decoded_params, "Function Parameters")


# ===============================================================================
#          Decode EIP712 APDU
# ===============================================================================
def decode_eip712_struct_def_apdu(apdu: bytes) -> None:
    """Decode APDU EIP712 STRUCT DEFINITION command for Ethereum transaction signing.
    Args:
        apdu: APDU command (without "0x" prefix)
    """

    p2 = to_int(apdu[3])
    lc = int(apdu[4])
    data = apdu[5:5 + lc]

    print_title("EIP712 STRUCT FIELD")
    if p2 == P2Type.STRUCT_NAME:
        logger.info(f"  STRUCT NAME: {data.decode('utf-8')}")

    elif p2 == P2Type.STRUCT_FIELD:
        typeDesc = data[0]
        data = data[1:]
        typeValue: StructFieldType = typeDesc & StructTypeDescMask.TYPE

        if typeValue == StructFieldType.CUSTOM:
            # Custom type name
            size = to_int(data[0])
            data = data[1:]
            name = data[:size].decode('utf-8')
            logger.info(f"  Field Name: {name}")
            data = data[size:]

        if typeDesc & StructTypeDescMask.SIZE:
            # Type size
            size = to_int(data[0])
            data = data[1:]
            logger.info(f"  Field Size [{StructFieldType(typeValue).name}]: {size}")

        if typeDesc & StructTypeDescMask.ARRAY:
            # Array levels
            levels_count = to_int(data[0])
            data = data[1:]
            logger.info(f"  Field ArrayLevels: {levels_count}")

        keyNameLen = to_int(data[0])
        data = data[1:]
        keyName = data[:keyNameLen]
        data = data[keyNameLen:]
        logger.info(f"  Field KeyName[{keyNameLen}]: {keyName.decode('utf-8')}")


def decode_eip712_struct_impl_apdu(apdu: bytes) -> None:
    """Decode APDU EIP712 STRUCT IMPLEMENTATION command for Ethereum transaction signing.
    Args:
        apdu: APDU command (without "0x" prefix)
    """

    p1 = to_int(apdu[2])
    p2 = to_int(apdu[3])
    lc = int(apdu[4])
    data = apdu[5:5 + lc]

    print_title("EIP712 STRUCT IMPLEMENTATION")
    if p2 == P2Type.STRUCT_NAME:
        logger.info(f"  STRUCT NAME: {data.decode('utf-8')}")

    elif p2 == P2Type.ARRAY:
        size = data[0]
        data = data[1:]
        logger.info(f"  ARRAY SIZE: {size}")

    elif p2 == P2Type.STRUCT_FIELD:
        size = to_int(apdu[0:2])
        data = data[2:]
        value = data[:size]
        logger.info(f"  STRUCT FIELD VALUE: {value.hex()}")


def __paramPresence(value: int) -> str:
    if value == 0:
        return "None"
    if value == 1:
        return "Present (filtered message field)"
    elif value == 2:
        return "Present (domain’s verifyingContract)"
    return "Unknown"


def decode_eip712_filtering_apdu(apdu: bytes) -> None:
    """Decode APDU EIP712 FILTERING command for Ethereum transaction signing.
    Args:
        apdu: APDU command (without "0x" prefix)
    """

    p1 = to_int(apdu[2])
    p2 = to_int(apdu[3])
    lc = int(apdu[4])
    data = apdu[5:5 + lc]

    print_title("EIP712 FILTERING")
    if p2 == P2Type.FILTERING_DISCARDED_PATH:
        size = data[0]
        data = data[1:]
        value = data[:size]
        logger.info(f"  PATH[{size}]: {value.hex()}")

    elif p2 == P2Type.FILTERING_MESSAGE_INFO:
        size = data[0]
        data = data[1:]
        value = data[:size]
        data = data[size:]
        logger.info(f"  MESSAGE INFO[{size}]: {value.decode('utf-8')}")
        filters = to_int(data[0])
        logger.info(f"  FILTERS COUNT: {filters}")

    elif p2 in (P2Type.FILTERING_CALLDATA_SPENDER,
                P2Type.FILTERING_CALLDATA_AMOUNT,
                P2Type.FILTERING_CALLDATA_SELECTOR,
                P2Type.FILTERING_CALLDATA_CHAIN_ID,
                P2Type.FILTERING_CALLDATA_CALLEE,
                P2Type.FILTERING_CALLDATA_VALUE,
                P2Type.FILTERING_AMOUNT_JOIN_TOKEN):
        logger.info(f"  CALLDATA {P2Type(p2).name.split('_')[-1]} index: {to_int(data[0])}")

    elif p2 == P2Type.FILTERING_CALLDATA_INFO:
        logger.info(f"  CALLDATA INFO index: {to_int(data[0])}")
        logger.info(f"  CALLDATA INFO value filter flag: {str(bool(data[1]))}")
        logger.info(f"  CALLDATA INFO callee filter flag: {__paramPresence(data[2])}")
        logger.info(f"  CALLDATA INFO ChaindId filter flag: {str(bool(data[3]))}")
        logger.info(f"  CALLDATA INFO Selector filter flag: {str(bool(data[4]))}")
        logger.info(f"  CALLDATA INFO Amount filter flag: {str(bool(data[5]))}")
        logger.info(f"  CALLDATA INFO Spender filter flag: {__paramPresence(data[6])}")

    elif p2 == P2Type.FILTERING_TRUSTED_NAME:
        size = data[0]
        data = data[1:]
        value = data[:size]
        data = data[size:]
        logger.info(f"  CALLDATA TRUSTED NAME[{size}]: {value.decode('utf-8')}")
        for elt in ("TYPES", "SOURCES"):
            size = data[0]
            data = data[1:]
            value = data[:size]
            data = data[size:]
            logger.info(f"  CALLDATA TRUSTED {elt}[{size}]: {value.hex()}")

    elif p2 in (P2Type.FILTERING_DATETIME, P2Type.FILTERING_RAW, P2Type.FILTERING_AMOUNT_JOIN_VALUE):
        size = data[0]
        data = data[1:]
        value = data[:size]
        logger.info(f"  CALLDATA {P2Type(p2).name.split('_')[-1]} Name: {value.decode('utf-8')}")
        if p2 == P2Type.FILTERING_AMOUNT_JOIN_VALUE:
            data = data[size:]
            logger.info(f"  CALLDATA VALUE TOKEN INDEX: {to_int(data[0])}")

    elif p2 == P2Type.FILTERING_AMOUNT_JOIN_VALUE:
        size = data[0]
        data = data[1:]
        logger.info(f"  AMOUNT FIELD SIZE: {size}")


def decode_sign_eip712_apdu(apdu: bytes) -> None:
    """Decode APDU SIGN command for Ethereum EIP712 transaction signing.
    Args:
        apdu: APDU command (without "0x" prefix)
    """

    p2 = to_int(apdu[3])
    lc = int(apdu[4])
    data = apdu[5:5 + lc]

    print_title("EIP712 SIGNING")
    # Extract derivation path
    path_length = to_int(data[0])
    data = data[1:]  # Skip path length byte
    derived_path = []
    for _ in range(path_length):
        index = to_int(data[:4])
        derived_path.append(index)
        data = data[4:]  # Skip 4 bytes (8 hex chars) for each index
    # Format and display the path
    logger.info(f"BIP32 path: {format_bip32_path(derived_path)}")

    if p2 == P2Type.V0_IMPLEM:
        domain_hash = data[:32]
        data = data[32:]
        message_hash = data[:32]
        logger.info(f"Domain Separator Hash: 0x{domain_hash.hex()}")
        logger.info(f"Message Hash: 0x{message_hash.hex()}")


# ===============================================================================
#          Transaction Info TLV decoding
# ===============================================================================
def decode_struct_information_tlv() -> None:
    """Decode TLV-encoded Transaction Info structure"""

    string_tags = [
        TagTransactionInfo.CREATOR_NAME,
        TagTransactionInfo.CREATOR_LEGAL_NAME,
        TagTransactionInfo.CREATOR_URL,
        TagTransactionInfo.CONTRACT_NAME,
        TagTransactionInfo.DEPLOY_DATE,
        TagTransactionInfo.OPERATION_TYPE,
    ]
    skip_tags = [
        TagTransactionInfo.SIGNATURE,
        TagTransactionInfo.FIELDS_HASH,
    ]
    number_tags = [
        TagTransactionInfo.CHAIN_ID,
    ]
    selector_tags = [
        TagTransactionInfo.SELECTOR,
    ]
    decode_generic_tlv(TagTransactionInfo, string_tags, number_tags, selector_tags, skip_tags)


# ===============================================================================
#          Transaction Field TLV decoding
# ===============================================================================
def decode_struct_field_tlv() -> None:
    """Decode TLV-encoded Transaction Field structure"""

    string_tags = [
        TagTransactionField.NAME,
    ]
    decode_generic_tlv(TagTransactionField, string_tags)


# ===============================================================================
#          Enum Value TLV decoding
# ===============================================================================
def decode_enum_value_tlv() -> None:
    """Decode TLV-encoded Enum Value structure"""

    string_tags = [
        TagEnumValue.NAME,
    ]
    skip_tags = [
        TagEnumValue.SIGNATURE,
    ]
    number_tags = [
        TagEnumValue.CHAIN_ID,
    ]
    selector_tags = [
        TagEnumValue.SELECTOR,
    ]
    decode_generic_tlv(TagEnumValue, string_tags, number_tags, selector_tags, skip_tags)


# ===============================================================================
#          Standard payload TLV decoding
# ===============================================================================
def decode_tlv() -> None:
    """Decode TLV-encoded payload"""

    string_tags = [
        TLVFieldTag.TICKER,
        TLVFieldTag.NETWORK_NAME,
        TLVFieldTag.TX_CHECKS_PROVIDER_MSG,
        TLVFieldTag.TX_CHECKS_TINY_URL,
    ]
    skip_tags = [
        TLVFieldTag.CHALLENGE,
        TLVFieldTag.DER_SIGNATURE,
        TLVFieldTag.TX_HASH,
        TLVFieldTag.DOMAIN_HASH,
        TLVFieldTag.NETWORK_ICON_HASH,
    ]
    number_tags = [
        TLVFieldTag.CHAIN_ID,
        TLVFieldTag.SIGNER_KEY_ID,
        TLVFieldTag.SIGNER_ALGO,
        TLVFieldTag.COIN_TYPE,
        TLVFieldTag.THRESHOLD,
        TLVFieldTag.SIGNERS_COUNT,
    ]
    selector_tags = [
        TLVFieldTag.SELECTOR,
    ]
    decode_generic_tlv(TLVFieldTag, string_tags, number_tags, selector_tags, skip_tags)


# ===============================================================================
#          Generic TLV payload parser
# ===============================================================================
def decode_tlv_apdu(apdu: bytes, tlv_parser: Callable, title: str) -> None:
    """Decode APDU for providing TLV-encoded structure information.

    Args:
        apdu: Hex string of the APDU command (without "0x" prefix)
        tlv_parser: Function to parse the TLV structure
        title: Title for logging
    """

    p1 = to_int(apdu[2])
    lc = int(apdu[4])
    data = apdu[5:5 + lc]

    tlv_info["TLV"].clear()

    if p1 == P1Type.FIRST_CHUNK:
        # Extract derivation path
        tlv_info["Struct"]["Length"] = to_int(data[0:2])
        tlv_info["Struct"]["Data"] = data[2:]  # Skip length
        tlv_info["Struct"]["Length"] -= (lc - 2)  # Adjust remaining length
    else:
        tlv_info["Struct"]["Data"] += data
        tlv_info["Struct"]["Length"] -= lc  # Adjust remaining length

    # Check if all chunks received
    if tlv_info["Struct"]["Length"] == 0:
        # All chunks received, decode struct information
        tlv_parser()
        format_decoded_params(tlv_info["TLV"], title)


# ===============================================================================
#          Decode APDU ERC20 Token Information
# ===============================================================================
def decode_erc20_token_apdu(apdu: bytes) -> None:
    """Decode APDU for providing ERC20 token information."""
    lc = int(apdu[4])
    data = apdu[5:5 + lc]

    # Simulte a TLV for logging purpose
    tlv_info["TLV"].clear()

    # Ticker length
    ticker_length = int(data[0])
    data = data[1:]
    tlv_info["TLV"]["Ticker"] = data[:ticker_length].decode('utf-8')
    data = data[ticker_length:]
    # Address
    tlv_info["TLV"]["Address"] = f"0x{data[:20].hex()}"
    data = data[20:]
    # Decimals
    tlv_info["TLV"]["Decimals"] = to_int(data[0:4])
    data = data[4:]
    # Chain ID
    tlv_info["TLV"]["Chain ID"] = to_int(data[0:4])

    format_decoded_params(tlv_info["TLV"], "ERC20 token")


# ===============================================================================
#          APDU File Parser
# ===============================================================================
def parse_apdu_line(line: str) -> Optional[bytes]:
    """Parse a line from APDU replay file.

    Handles two formats:
    1. Raw hex string: "e0040001ff..."
    2. Direction prefix: "=> e0040001ff..." or "<= 9000"

    Args:
        line: Line from the APDU file

    Returns:
        bytes: Parsed APDU bytes, or None if should be skipped
    """
    line = line.strip().lower()

    if not line:
        return None

    # Check for direction prefix
    if line.startswith("=>"):
        # Outgoing APDU (to device) - extract hex data
        hex_data = line[2:].strip()
        try:
            return bytes.fromhex(hex_data)
        except ValueError:
            logger.warning(f"Invalid hex data in outgoing APDU: {line}")
            return None
    if line.startswith("<="):
        # Incoming APDU (from device) - skip response
        logger.debug(f"Skipping response: {line}")
        return None

    # Raw hex string (legacy format) - parse directly
    try:
        return bytes.fromhex(line)
    except ValueError:
        logger.warning(f"Invalid hex data: {line}")
        return None


# ===============================================================================
#          Main entry
# ===============================================================================
def main() -> None:
    parser = init_parser()
    args = parser.parse_args()

    set_logging(args.verbose)

    # Load selector cache at startup
    load_selector_cache()
    logger.debug(f"Loaded {len(LOCAL_SELECTORS)} function selectors from cache")

    try:
        with open(args.input, encoding='utf-8') as f:
            for line in f.readlines():
                # Parse APDU line
                apdu_bytes = parse_apdu_line(line)

                # Skip if parsing failed or it's a response
                if apdu_bytes is None:
                    continue

                if to_int(apdu_bytes[0]) != APP_CLA:
                    logger.debug(f"Skipping non-Ethereum APDU (CLA={apdu_bytes[0]:02x})")
                    continue

                # Extract INS byte
                ins = apdu_bytes[1]

                # Dispatch to appropriate decoder
                if ins == InsType.SIGN:
                    decode_sign_apdu(apdu_bytes)
                elif ins == InsType.PROVIDE_ERC20_TOKEN_INFORMATION:
                    decode_erc20_token_apdu(apdu_bytes)
                elif ins == InsType.PROVIDE_TRANSACTION_INFO:
                    decode_tlv_apdu(apdu_bytes, decode_struct_information_tlv, "Transaction Info Struct")
                elif ins == InsType.PROVIDE_TRANSACTION_FIELD_DESC:
                    decode_tlv_apdu(apdu_bytes, decode_struct_field_tlv, "Transaction Field Struct")
                elif ins == InsType.PROVIDE_ENUM_VALUE:
                    decode_tlv_apdu(apdu_bytes, decode_enum_value_tlv, "Enum Value Struct")
                elif ins == InsType.PROVIDE_PROXY_INFO:
                    decode_tlv_apdu(apdu_bytes, decode_tlv, "Proxy Info Struct")
                elif ins == InsType.PROVIDE_NETWORK_INFORMATION:
                    decode_tlv_apdu(apdu_bytes, decode_tlv, "Network Info Struct")
                elif ins == InsType.EIP712_SEND_STRUCT_DEF:
                    decode_eip712_struct_def_apdu(apdu_bytes)
                elif ins == InsType.EIP712_SEND_STRUCT_IMPL:
                    decode_eip712_struct_impl_apdu(apdu_bytes)
                elif ins == InsType.EIP712_SEND_FILTERING:
                    decode_eip712_filtering_apdu(apdu_bytes)
                elif ins == InsType.EIP712_SIGN:
                    decode_sign_eip712_apdu(apdu_bytes)
                else:
                    logger.info(f"*** Skipping INS type: {ins:02x} -> {InsType(ins).name}")
    except FileNotFoundError:
        logger.error(f"Failed to open file: {args.input}")
        return 1
    except Exception as e:
        logger.error(f"Unexpected error: {e}")
        return 1

    return 0


if __name__ == "__main__":
    exit(main())
