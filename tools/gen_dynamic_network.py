#!/usr/bin/env python3

import os
import subprocess
import sys
import struct
import logging
import re
from hashlib import sha256
from enum import IntEnum
from typing import List, Optional
from pathlib import Path
import argparse

# Resolve the parent directory and append to sys.path
parent = Path(__file__).parent.parent.resolve()
client_module = Path(f"{parent}/client/src/ledger_app_clients/ethereum")
sys.path.append(f"{client_module}")
# Import the required module
from tlv import format_tlv  # type: ignore
from keychain import Key, sign_data   # type: ignore

# Retrieve the SDK path from the environment variable
sdk_path = os.getenv('BOLOS_SDK')
if sdk_path:
    # Import the library dynamically
    sys.path.append(f"{sdk_path}/lib_nbgl/tools")
    from icon2glyph import open_image, compute_app_icon_data  # type: ignore
else:
    print("Environment variable BOLOS_SDK is not set")
    sys.exit(1)


class NetworkInfoTag(IntEnum):
    STRUCTURE_TYPE = 0x01
    STRUCTURE_VERSION = 0x02
    BLOCKCHAIN_FAMILY = 0x51
    CHAIN_ID = 0x23
    NETWORK_NAME = 0x52
    TICKER = 0x24
    NETWORK_ICON_HASH = 0x53
    DER_SIGNATURE = 0x15


class P1Type(IntEnum):
    FIRST_CHUNK = 0x01
    NEXT_CHUNK = 0x00


class P2Type(IntEnum):
    NETWORK_CONFIG = 0x00
    NETWORK_ICON = 0x01
    GET_INFO = 0x02


PROVIDE_NETWORK_INFORMATION = 0x30
CLA = 0xE0

logger = logging.getLogger(__name__)


# ===============================================================================
#          Parameters
# ===============================================================================
def init_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description="Generate hex string for network icon, in NBGL format.")
    parser.add_argument("--icon", "-i", help="Input icon to process.")
    parser.add_argument("--name", "-n", required=True, help="Network name")
    parser.add_argument("--ticker", "-t", required=True, help="Network ticker")
    parser.add_argument("--chainid", "-c", type=int, required=True, help="Network chain_id")
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


# ===============================================================================
#          Check icon - Extracted from ledger-app-workflow/scripts/check_icon.sh
# ===============================================================================
def check_glyph(file: str) -> bool:
    extension = os.path.splitext(file)[1][1:]
    if extension not in ["gif", "bmp", "png"]:
        logger.error(f"Glyph extension should be '.gif', '.bmp', or '.png', not '.{extension}'")
        return False

    try:
        content = subprocess.check_output(["identify", "-verbose", file], text=True)
    except subprocess.CalledProcessError as e:
        logger.error(f"Failed to identify file: {e}")
        return False

    if "Alpha" in content:
        logger.error("Glyph should have no alpha channel")
        return False

    x = re.search(r"Colors: (.*)", content)
    if x is None:
            logger.error("Glyph should have the colors defined")
            return False
    nb_colors = int(x.group(1))
    if "Type: Bilevel" in content:
        logger.debug("Monochrome image type")

        if nb_colors != 2:
            logger.error("Glyph should have only 2 colors")
            return False

        if re.search(r"0.*0.*0.*black", content) is None:
            logger.error("Glyph should have the black color defined")
            return False

        if re.search(r"255.*255.*255.*white", content) is None:
            logger.error("Glyph should have the white color defined")
            return False

        if not any(depth in content for depth in ["Depth: 1-bit", "Depth: 8/1-bit"]):
            logger.error("Glyph should have 1 bit depth")
            return False

    elif "Type: Grayscale" in content:
        logger.debug("Grayscale image type")

        if nb_colors > 16:
            logger.error(f"4bpp glyphs can't have more than 16 colors, {nb_colors} found")
            return False

        if not any(depth in content for depth in ["Depth: 8-bit", "Depth: 8/8-bit"]):
            logger.error("Glyph should have 8 bits depth")
            return False

    else:
        logger.error("Glyph should be Monochrome or Grayscale")
        return False

    logger.info(f"Glyph '{file}' is compliant")
    return True


# ===============================================================================
#          Prepare APDU - Extracted from python client
# ===============================================================================
def serialize(p1: int, p2: int, cdata: bytes) -> bytes:
    """
    Serializes the provided network information into byte format.

    Args:
        p1 (int): P1 parameter to be included in the header.
        p2 (int): P2 parameter to be included in the header.
        cdata (bytes): The network information data to be serialized.

    Returns:
        bytes: The serialized byte sequence combining the header and the data.
    """

    # Initialize a bytearray to construct the header
    header = bytearray()
    header.append(CLA)
    header.append(PROVIDE_NETWORK_INFORMATION)
    header.append(p1)
    header.append(p2)
    header.append(len(cdata))
    # Return the concatenation of the header and cdata
    return header + cdata


def generate_tlv_payload(name: str,
                         ticker: str,
                         chain_id: int,
                         icon: Optional[bytes] = None) -> bytes:
    """
    Generates a TLV (Type-Length-Value) payload for the given network information.

    Args:
        name (str): The name of the blockchain network.
        ticker (str): The ticker symbol of the blockchain network.
        chain_id (int): The unique identifier for the blockchain network.
        icon (Optional[bytes]): Optional icon data in bytes for the blockchain network.

    Returns:
        bytes: The generated TLV payload.
    """

    payload: bytes = format_tlv(NetworkInfoTag.STRUCTURE_TYPE, 8)
    payload += format_tlv(NetworkInfoTag.STRUCTURE_VERSION, 1)
    payload += format_tlv(NetworkInfoTag.BLOCKCHAIN_FAMILY, 1)
    payload += format_tlv(NetworkInfoTag.CHAIN_ID, chain_id.to_bytes(8, 'big'))
    payload += format_tlv(NetworkInfoTag.NETWORK_NAME, name.encode('utf-8'))
    payload += format_tlv(NetworkInfoTag.TICKER, ticker.encode('utf-8'))
    if icon:
        # Network Icon Hash
        payload += format_tlv(NetworkInfoTag.NETWORK_ICON_HASH, sha256(icon).digest())
    # Append the data Signature
    payload += format_tlv(NetworkInfoTag.DER_SIGNATURE,
                              sign_data(Key.CAL, payload))

    # Return the constructed TLV payload as bytes
    return payload


def prepare_network_information(name: str,
                                ticker: str,
                                chain_id: int,
                                icon: Optional[bytes] = None) -> List[bytes]:
    """
    Prepares network information for a given blockchain network.

    Args:
        name (str): The name of the blockchain network.
        ticker (str): The ticker symbol of the blockchain network.
        chain_id (int): The unique identifier for the blockchain network.
        icon (Optional[bytes]): Optional icon data in bytes for the blockchain network.

    Returns:
        List[bytes]: A list of byte chunks representing the network information.
    """

    # Initialize an empty list to store the byte chunks
    chunks: List[bytes] = []
    # Generate the TLV payload
    payload = generate_tlv_payload(name, ticker, chain_id, icon)
    # Check if the payload is larger than 0xff
    assert len(payload) < 0xff, "Payload too large"
    # Serialize the payload
    chunks.append(serialize(0x00, P2Type.NETWORK_CONFIG, payload))

    if icon:
        # Serialize the icon
        p1 = P1Type.FIRST_CHUNK
        while len(icon) > 0:
            chunks.append(serialize(p1, P2Type.NETWORK_ICON, icon[:0xff]))
            icon = icon[0xff:]
            p1 = P1Type.NEXT_CHUNK
    return chunks


# ===============================================================================
#          Main entry
# ===============================================================================
def main() -> None:
    parser = init_parser()
    args = parser.parse_args()

    set_logging(args.verbose)

    if args.icon:
        if not os.access(args.icon, os.R_OK):
            logger.error(f"Cannot read file {args.icon}")
            sys.exit(1)

        # Open image in luminance format
        im, bpp = open_image(args.icon)
        if im is None:
            logger.error(f"Unable to access icon file {args.icon}")
            sys.exit(1)

        # Check icon
        if not check_glyph(args.icon):
            logger.error(f"Invalid icon file {args.icon}")
            sys.exit(1)

        # Prepare and print app icon data
        _, image_data = compute_app_icon_data(False, im, bpp, False)
        logger.debug(f"image_data={image_data.hex()}")
    else:
        image_data = None

    chunks = prepare_network_information(args.name, args.ticker, args.chainid, image_data)
    # Print each chunk with its index
    for i, chunk in enumerate(chunks):
        logger.info(f"Chunk {i}[{len(chunk)}]: {chunk.hex()}")


if __name__ == "__main__":
    main()
