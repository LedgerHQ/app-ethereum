#!/usr/bin/env python3
"""
extract_ui_strings.py
=====================
Extract ONLY the strings that are actually displayed to the user via NBGL
(text = ..., subText = ..., title = ..., item = ..., nbgl_useCaseXxx() args).

Excludes:
  - PRINTF / printf / snprintf format strings
  - #include paths
  - Error log strings

Usage:
    python3 tools/extract_ui_strings.py [--nbgl-dir src/nbgl]
"""
import re
import glob
import argparse
from pathlib import Path

# Fields that directly assign a literal string to a display field
FIELD_PATTERNS = [
    r'\.text\s*=\s*"([^"]+)"',
    r'\.subText\s*=\s*"([^"]+)"',
    r'\.title\s*=\s*"([^"]+)"',
    r'\.description\s*=\s*"([^"]+)"',
    r'\.subTitle\s*=\s*"([^"]+)"',
    r'\.item\s*=\s*"([^"]+)"',
    r'\.value\s*=\s*"([^"]+)"',
]
FIELD_RE = re.compile("|".join(FIELD_PATTERNS))

# nbgl_useCase* call blocks (multi-line)
USECASE_BLOCK_RE = re.compile(r'(nbgl_useCase\w+\s*\([^;]+;)', re.DOTALL)
STRING_IN_BLOCK_RE = re.compile(r'"([^"]{2,})"')

# Lines to skip entirely (printf / logging / includes)
SKIP_LINE_RE = re.compile(
    r'(PRINTF|printf|snprintf|strlcpy|strncat|LEDGER_ASSERT|#include|//|LOG|"Error)'
)

def extract(nbgl_dir: str) -> list[str]:
    results: set[str] = set()

    for fpath in sorted(glob.glob(f"{nbgl_dir}/*.c")):
        content = open(fpath, encoding="utf-8", errors="replace").read()

        # 1. Field assignments — line by line (skip log lines)
        for line in content.splitlines():
            if SKIP_LINE_RE.search(line):
                continue
            for m in FIELD_RE.finditer(line):
                s = next(g for g in m.groups() if g is not None)
                if len(s) >= 3:
                    results.add(s)

        # 2. Full nbgl_useCase*(...) call blocks
        for m in USECASE_BLOCK_RE.finditer(content):
            block = m.group(1)
            for s in STRING_IN_BLOCK_RE.findall(block):
                # Filter out anything that looks like an identifier/include
                if len(s) < 3:
                    continue
                if re.search(r'\.(h|c|py|json|sh|mk)$', s):
                    continue
                if s.startswith("Error") or "printf" in s.lower():
                    continue
                results.add(s)

    return sorted(results)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--nbgl-dir", default="src/nbgl", help="Path to src/nbgl/")
    args = parser.parse_args()

    strings = extract(args.nbgl_dir)
    print(f"Found {len(strings)} unique display strings:\n")
    for i, s in enumerate(strings):
        print(f'    /* {i:02d} */ "{s}",')


if __name__ == "__main__":
    main()
