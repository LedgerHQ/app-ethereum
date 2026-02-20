#!/usr/bin/env python3
"""
audit_fr_strings.py
===================
Compare lang_fr.c against the verified display-only strings extracted from
src/nbgl/*.c, and report:
  1. Strings in lang_fr.c that are NOT actual display strings (snprintf
     format strings or noise from the original broad regex search)
  2. Strings MISSING from lang_fr.c that ARE real display strings
"""
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent

# ---------------------------------------------------------------------------
# 1.  Parse lang_fr.c — collect the English originals from the comments
# ---------------------------------------------------------------------------
fr_src = (ROOT / "src" / "lang_fr.c").read_text(encoding="utf-8")

# Each entry looks like:  /* 00  "Original English string" */\n    "Translation"
# We want the English original from the comment (after the entry number)
en_from_fr: set[str] = set()
for m in re.finditer(r'/\*\s*\d+\s+\"([^\"]+)\"', fr_src):
    en_from_fr.add(m.group(1))

# Fallback: also grab the actual translation keys if comments are sparse
# (Not needed here since we have good comment format)

# ---------------------------------------------------------------------------
# 2.  Extract TRUE display strings from src/nbgl/*.c
# ---------------------------------------------------------------------------
FIELD_PATS = [
    r'\.text\s*=\s*"([^"]+)"',
    r'\.subText\s*=\s*"([^"]+)"',
    r'\.title\s*=\s*"([^"]+)"',
    r'\.description\s*=\s*"([^"]+)"',
    r'\.subTitle\s*=\s*"([^"]+)"',
    r'\.item\s*=\s*"([^"]+)"',
    r'\.value\s*=\s*"([^"]+)"',
]
FIELD_RE = re.compile("|".join(FIELD_PATS))
USECASE_RE = re.compile(r'(nbgl_useCase\w+\s*\([^;]+;)', re.DOTALL)
STR_RE = re.compile(r'"([^"]{2,})"')
SKIP_RE = re.compile(r'(PRINTF|printf|snprintf|strlcpy|#include|//\s|LEDGER_ASSERT)')

nbgl_dir = ROOT / "src" / "nbgl"
display_strings: set[str] = set()

for fpath in sorted(nbgl_dir.glob("*.c")):
    content = fpath.read_text(encoding="utf-8", errors="replace")
    # Field assignments (per line, skip log lines)
    for line in content.splitlines():
        if SKIP_RE.search(line):
            continue
        for m in FIELD_RE.finditer(line):
            s = next(g for g in m.groups() if g is not None)
            if len(s) >= 2:
                display_strings.add(s)
    # nbgl_useCase*() full call blocks
    for m in USECASE_RE.finditer(content):
        for s in STR_RE.findall(m.group(1)):
            if len(s) < 3:
                continue
            if re.search(r'\.(h|c|py|json)$', s):
                continue
            if s.startswith("Error"):
                continue
            display_strings.add(s)

# Remove obviously broken fragments
display_strings = {s for s in display_strings if not s.endswith(" is not ")}
display_strings = {s for s in display_strings if not s.endswith("which is not ")}
display_strings.discard("in the whitelist.")

# ---------------------------------------------------------------------------
# 3.  Report differences
# ---------------------------------------------------------------------------
has_pct = {s for s in en_from_fr if "%" in s}          # snprintf format strings
not_display = (en_from_fr - display_strings) - has_pct  # in FR but not a display string
missing = display_strings - en_from_fr                   # real display but absent from FR

print("=" * 65)
print(f"  Extracted display strings from src/nbgl : {len(display_strings)}")
print(f"  English originals in lang_fr.c          : {len(en_from_fr)}")
print("=" * 65)

print("\n=== In lang_fr.c but NOT an actual display string (to review/remove) ===")
for s in sorted(not_display):
    print(f"  {repr(s)}")

print("\n=== Format strings in lang_fr.c (contain %, used in snprintf) ===")
for s in sorted(has_pct):
    print(f"  {repr(s)}")

print("\n=== MISSING from lang_fr.c (real display strings to add) ===")
for s in sorted(missing):
    print(f"  {repr(s)}")

if not not_display and not has_pct and not missing:
    print("  ✓  lang_fr.c covers exactly the right set of strings.")
