#!/usr/bin/env python3
"""Generate function selector cache from ABI files."""

import json
import logging
from pathlib import Path

from eth_utils import keccak


def calculate_function_selector(signature: str) -> str:
    """Calculate function selector from signature.

    Args:
        signature: Function signature like "transfer(address,uint256)"

    Returns:
        8-character hex string of the selector
    """
    signature = signature.strip()
    hash_bytes = keccak(text=signature)
    selector = hash_bytes[:4].hex()
    return selector


def parse_abi_type(abi_type: dict) -> str:
    """Parse ABI type definition to string representation.

    Args:
        abi_type: ABI type object with 'type', 'components', etc.

    Returns:
        String representation like "address", "uint256", "(address,uint256)[]"
    """
    base_type = abi_type["type"]

    # Handle tuple types (structs)
    if base_type.startswith("tuple"):
        if "components" not in abi_type:
            return base_type

        # Build tuple signature recursively
        component_types = [parse_abi_type(comp) for comp in abi_type["components"]]
        tuple_sig = f"({','.join(component_types)})"

        # Handle arrays of tuples
        if base_type.endswith("[]"):
            return f"{tuple_sig}[]"
        else:
            return tuple_sig

    return base_type


def extract_function_signature(func: dict) -> str:
    """Extract function signature from ABI function definition.

    Args:
        func: ABI function object

    Returns:
        Function signature like "transfer(address,uint256)"
    """
    name = func["name"]

    if "inputs" not in func or not func["inputs"]:
        return f"{name}()"

    # Parse input types
    input_types = [parse_abi_type(input_param) for input_param in func["inputs"]]

    return f"{name}({','.join(input_types)})"


def process_abi_file(abi_path: Path, logger: logging.Logger) -> dict[str, str]:
    """Process a single ABI file and extract all function signatures.

    Args:
        abi_path: Path to ABI JSON file

    Returns:
        Dictionary mapping selector to signature
    """

    try:
        with open(abi_path) as f:
            abi = json.load(f)
    except Exception as e:
        logger.error(f"Failed to load {abi_path}: {e}")
        return {}

    selectors = {}

    for item in abi:
        # Only process function definitions
        if item.get("type") != "function":
            continue

        # Extract signature
        signature = extract_function_signature(item)

        # Calculate selector
        selector = calculate_function_selector(signature)

        # Store in cache
        if selector in selectors and selectors[selector] != signature:
            logger.warning(f"Selector collision: {selector}")
            logger.warning(f"  Existing: {selectors[selector]}")
            logger.warning(f"  New:      {signature}")

        selectors[selector] = signature

    return selectors


def scan_abi_directory(directory: Path, logger: logging.Logger) -> dict[str, str]:
    """Scan directory for ABI files and extract all signatures.

    Args:
        directory: Directory containing ABI JSON files

    Returns:
        Dictionary mapping selector to signature
    """
    all_selectors = {}

    # Find all JSON files
    json_files = list(directory.glob("*.json")) + list(directory.glob("**/*.json"))

    if not json_files:
        logger.warning(f"No JSON files found in {directory}")
        return {}

    for abi_path in sorted(json_files):
        selectors = process_abi_file(abi_path, logger)

        # Merge with existing selectors
        for selector, signature in selectors.items():
            if selector in all_selectors and all_selectors[selector] != signature:
                logger.warning(f"Duplicate selector {selector} from {abi_path.name}")
                logger.warning(f"  Keeping: {all_selectors[selector]}")
                logger.warning(f"  Ignoring: {signature}")
            else:
                all_selectors[selector] = signature

    return all_selectors


def gen_selector_cache(
    input_path: Path = Path("tests/functional/abis"),
    logger: logging.Logger | None = None,
) -> dict[str, str]:
    """Generate the function selector cache from a directory of ABI files.

    Args:
        input_path: Directory containing the ABI JSON files to scan
        logger: Logger to use; defaults to the module logger when None

    Returns:
        Dictionary mapping selector to signature

    Raises:
        FileNotFoundError: If input_path does not exist
        NotADirectoryError: If input_path is not a directory
    """
    # Fall back to a default logger so downstream helpers always get a valid logger
    if logger is None:
        logger = logging.getLogger(__name__)

    # Validate input directory
    if not input_path.exists():
        raise FileNotFoundError(f"Input path does not exist: {input_path}")

    if not input_path.is_dir():
        raise NotADirectoryError(f"Input path is not a directory: {input_path}")

    # Scan ABI files
    return scan_abi_directory(input_path, logger)
