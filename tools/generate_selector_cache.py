#!/usr/bin/env python3
"""Generate function selector cache from ABI files."""

import json
import argparse
import logging
from pathlib import Path
from typing import Dict, List
from eth_utils import keccak

logging.basicConfig(
    level=logging.INFO,
    format='[%(levelname)s] %(message)s'
)
logger = logging.getLogger(__name__)


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
    base_type = abi_type['type']

    # Handle tuple types (structs)
    if base_type.startswith('tuple'):
        if 'components' not in abi_type:
            return base_type

        # Build tuple signature recursively
        component_types = [parse_abi_type(comp) for comp in abi_type['components']]
        tuple_sig = f"({','.join(component_types)})"

        # Handle arrays of tuples
        if base_type.endswith('[]'):
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
    name = func['name']

    if 'inputs' not in func or not func['inputs']:
        return f"{name}()"

    # Parse input types
    input_types = [parse_abi_type(input_param) for input_param in func['inputs']]

    return f"{name}({','.join(input_types)})"


def process_abi_file(abi_path: Path) -> Dict[str, str]:
    """Process a single ABI file and extract all function signatures.

    Args:
        abi_path: Path to ABI JSON file

    Returns:
        Dictionary mapping selector to signature
    """
    logger.info(f"Processing {abi_path.name}")

    try:
        with open(abi_path, 'r') as f:
            abi = json.load(f)
    except Exception as e:
        logger.error(f"Failed to load {abi_path}: {e}")
        return {}

    selectors = {}

    for item in abi:
        # Only process function definitions
        if item.get('type') != 'function':
            continue

        try:
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
            logger.debug(f"  {selector}: {signature}")

        except Exception as e:
            logger.error(f"Failed to process function {item.get('name', 'unknown')}: {e}")

    logger.info(f"  Found {len(selectors)} functions")
    return selectors


def scan_abi_directory(directory: Path) -> Dict[str, str]:
    """Scan directory for ABI files and extract all signatures.

    Args:
        directory: Directory containing ABI JSON files

    Returns:
        Dictionary mapping selector to signature
    """
    all_selectors = {}

    # Find all JSON files
    json_files = list(directory.glob('*.json')) + list(directory.glob('**/*.json'))

    if not json_files:
        logger.warning(f"No JSON files found in {directory}")
        return {}

    logger.info(f"Found {len(json_files)} JSON files")

    for abi_path in sorted(json_files):
        selectors = process_abi_file(abi_path)

        # Merge with existing selectors
        for selector, signature in selectors.items():
            if selector in all_selectors and all_selectors[selector] != signature:
                logger.warning(f"Duplicate selector {selector} from {abi_path.name}")
                logger.warning(f"  Keeping: {all_selectors[selector]}")
                logger.warning(f"  Ignoring: {signature}")
            else:
                all_selectors[selector] = signature

    return all_selectors


def save_cache(selectors: Dict[str, str], output_path: Path, format: str = 'json'):
    """Save selector cache to file.

    Args:
        selectors: Dictionary mapping selector to signature
        output_path: Output file path
        format: Output format ('json' or 'python')
    """
    if format == 'json':
        with open(output_path, 'w') as f:
            json.dump(selectors, f, indent=2, sort_keys=True)
        logger.info(f"Saved {len(selectors)} selectors to {output_path}")

    elif format == 'python':
        with open(output_path, 'w') as f:
            f.write("# Auto-generated function selector cache\n")
            f.write("# Generated from ABI files\n\n")
            f.write("FUNCTION_SELECTORS = {\n")
            for selector in sorted(selectors.keys()):
                signature = selectors[selector]
                f.write(f'    "{selector}": "{signature}",\n')
            f.write("}\n")
        logger.info(f"Saved {len(selectors)} selectors to {output_path}")


def main():
    parser = argparse.ArgumentParser(
        description='Generate function selector cache from ABI files'
    )
    parser.add_argument(
        '-i', '--input',
        type=Path,
        default=Path('tests/ragger/abis'),
        help='Directory containing ABI JSON files (default: tests/ragger/abis)'
    )
    parser.add_argument(
        '-o', '--output',
        type=Path,
        default=Path(__file__).parent / "function_selectors.json",
        help='Output cache file (default: function_selectors.json)'
    )
    parser.add_argument(
        '-f', '--format',
        choices=['json', 'python'],
        default='json',
        help='Output format (default: json)'
    )
    parser.add_argument(
        '-v', '--verbose',
        action='store_true',
        help='Enable verbose logging'
    )

    args = parser.parse_args()

    if args.verbose:
        logging.getLogger().setLevel(logging.DEBUG)

    # Validate input directory
    if not args.input.exists():
        logger.error(f"Input directory does not exist: {args.input}")
        return 1

    if not args.input.is_dir():
        logger.error(f"Input path is not a directory: {args.input}")
        return 1

    # Scan ABI files
    logger.info(f"Scanning {args.input} for ABI files...")
    selectors = scan_abi_directory(args.input)

    if not selectors:
        logger.warning("No function selectors found")
        return 1

    # Adjust output extension based on format
    if args.format == 'python' and args.output.suffix != '.py':
        args.output = args.output.with_suffix('.py')

    # Save cache
    save_cache(selectors, args.output, args.format)

    logger.info(f"Successfully generated cache with {len(selectors)} selectors")

    # Show some statistics
    logger.info("\nStatistics:")
    logger.info(f"  Total selectors: {len(selectors)}")

    return 0


if __name__ == '__main__':
    exit(main())
