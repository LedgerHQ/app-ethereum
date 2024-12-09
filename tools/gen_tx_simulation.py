#!/usr/bin/env python3

import os
import sys
import logging
from hashlib import sha256
from enum import IntEnum
from pathlib import Path
import argparse

# Resolve the parent directory and append to sys.path
parent = Path(__file__).parent.parent.resolve()
client_module = Path(f"{parent}/client/src/ledger_app_clients/ethereum")
sys.path.append(f"{client_module}")
# Import the required module
from tlv import format_tlv  # type: ignore
from keychain import Key, sign_data   # type: ignore


class P1Type(IntEnum):
    NORMAL = 0x00
    DEMO = 0x01

class TxSimulationTag(IntEnum):
    STRUCTURE_TYPE = 0x01
    STRUCTURE_VERSION = 0x02
    CHAIN_ID = 0x23
    TX_HASH = 0x27
    W3C_NORMALIZED_RISK = 0x80
    W3C_NORMALIZED_CATEGORY = 0x81
    W3C_PROVIDER_MSG = 0x82
    W3C_TINY_URL = 0x83
    DER_SIGNATURE = 0x15


STRING_MAX_LENGTH = 30

PROVIDE_TX_SIMULATION = 0x32
CLA = 0xE0

logger = logging.getLogger(__name__)


# ===============================================================================
#          Parameters
# ===============================================================================
def init_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description="Generate hex string for TX simulation APDU.")
    parser.add_argument("--chainid", "-c", type=int, required=True, help="Network chain_id")
    parser.add_argument("--risk", "-R", type=lambda x: int(x,0), required=True, help="Risk")
    parser.add_argument("--category", "-C", type=int, required=True, help="Category")
    parser.add_argument("--message", "-M", required=True, help="Provider Message")
    parser.add_argument("--url", "-U", required=True, help="Report Tiny URL")
    parser.add_argument("--demo", "-d", action='store_true', help="Set demo mode to just display the related screens, without real Tx")
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
#          Prepare APDU - Extracted from python client
# ===============================================================================
def serialize(p1: P1Type, cdata: bytes) -> bytes:
    """
    Serializes the TX Simulation information into byte format.

    Args:
        p1 (P1Type): P1 parameter to be included in the header.
        cdata (bytes): The TX Simulation data to be serialized.

    Returns:
        bytes: The serialized byte sequence combining the header and the data.
    """

    # Initialize a bytearray to construct the header
    header = bytearray()
    header.append(CLA)
    header.append(PROVIDE_TX_SIMULATION)
    header.append(p1)
    header.append(0x00)
    header.append(len(cdata))
    # Return the concatenation of the header and cdata
    return header + cdata


def generate_tlv_payload(risk: int,
                         category: int,
                         message: str,
                         url: str,
                         chain_id: int) -> bytes:
    """
    Generates a TLV (Type-Length-Value) payload for the given TX Simulation.

    Args:
        risk (int): The risk value for the TX Simulation.
        category (int): The category value for the TX Simulation.
        message (str): The provider message for the TX Simulation.
        url (str): The tiny URL for the TX Simulation.
        chain_id (int): The unique identifier for the blockchain network.

    Returns:
        bytes: The generated TLV payload.
    """

    # Construct the TLV payload
    tx_hash = bytes.fromhex("00"*32) #if chain_id == 0 else bytes.fromhex("deadbeaf"*8)
    payload: bytes = format_tlv(TxSimulationTag.STRUCTURE_TYPE, 9)
    payload += format_tlv(TxSimulationTag.STRUCTURE_VERSION, 1)
    payload += format_tlv(TxSimulationTag.CHAIN_ID, chain_id.to_bytes(8, 'big'))
    payload += format_tlv(TxSimulationTag.TX_HASH, tx_hash)
    payload += format_tlv(TxSimulationTag.W3C_NORMALIZED_RISK, risk.to_bytes(2, 'big'))
    payload += format_tlv(TxSimulationTag.W3C_NORMALIZED_CATEGORY, category)
    payload += format_tlv(TxSimulationTag.W3C_PROVIDER_MSG, message.encode('utf-8'))
    payload += format_tlv(TxSimulationTag.W3C_TINY_URL, url.encode('utf-8'))
    # Append the data Signature
    payload += format_tlv(TxSimulationTag.DER_SIGNATURE, sign_data(Key.CAL, payload))

    # Return the constructed TLV payload as bytes
    return payload


# ===============================================================================
#          Main entry
# ===============================================================================
def main() -> None:
    parser = init_parser()
    args = parser.parse_args()

    # Check parameters
    if not 0 <= args.risk <= 65535:
        logger.error("Invalid risk value")
        sys.exit(1)
    if not 0 <= args.category <= 255:
        logger.error("Invalid category value")
        sys.exit(1)
    if args.chainid < 0:
        logger.error("Invalid chain_id")
        sys.exit(1)
    if len(args.message) >= STRING_MAX_LENGTH:
        logger.error("Invalid message")
        sys.exit(1)
    if len(args.url) >= STRING_MAX_LENGTH:
        logger.error("Invalid url")
        sys.exit(1)

    set_logging(args.verbose)

    payload = generate_tlv_payload(args.risk, args.category, args.message, args.url, args.chainid)
    logger.info(f"TX Simulation Payload[{len(payload)}]: {payload.hex()}")
    p1 = P1Type.DEMO if args.demo else P1Type.NORMAL
    logger.info(f"TX Simulation APDU: {serialize(p1, payload).hex()}")


if __name__ == "__main__":
    main()
