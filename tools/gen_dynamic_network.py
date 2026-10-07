#!/usr/bin/env python3

import argparse
import logging
import os
import re
import subprocess
import sys

# Retrieve the SDK path from the environment variable
sdk_path = os.getenv("BOLOS_SDK")
if sdk_path:
    # Import the library dynamically
    sys.path.append(f"{sdk_path}/lib_nbgl/tools")
    from icon2glyph import compute_app_icon_data, open_image  # type: ignore
else:
    print("Environment variable BOLOS_SDK is not set")
    sys.exit(1)


logger = logging.getLogger(__name__)


# ===============================================================================
#          Parameters
# ===============================================================================
def init_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description="Generate hex string for network icon, in NBGL format.")
    parser.add_argument("--icon", "-i", required=True, help="Input icon to process.")
    parser.add_argument("--verbose", "-v", action="store_true", help="Verbose mode")
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
#          Main entry
# ===============================================================================
def main() -> None:
    parser = init_parser()
    args = parser.parse_args()

    set_logging(args.verbose)

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
    logger.info(f"image_data={image_data.hex()}")


if __name__ == "__main__":
    main()
