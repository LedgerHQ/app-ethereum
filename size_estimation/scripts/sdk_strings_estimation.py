#!/usr/bin/env python3
"""
sdk_strings_estimation.py
==========================
Estimate the SDK-side i18n cost for app-ethereum by:

  1. Scanning every .c file under src/ to find which nbgl_useCase* functions
     are actually called.
  2. Mapping those call sites to the hardcoded strings that the SDK function
     will emit at run-time (taken from a static analysis of
     ledger-secure-sdk/lib_nbgl/src/nbgl_use_case.c).
  3. Computing byte-sizes of the unique SDK strings that would need
     translation, broken down by use case and compared against the
     app-level string table measured by multi_lang_comparison.py.

Usage:
    python size_estimation/scripts/sdk_strings_estimation.py
    python size_estimation/scripts/sdk_strings_estimation.py --src-dir src
"""
import argparse
import re
from pathlib import Path

# ---------------------------------------------------------------------------
# SDK String Map
# Each key is an nbgl_useCase* function name.
# Each value is a dict:
#   "always"   → strings always present when the function is called
#   "tx"       → strings added only for TYPE_TRANSACTION
#   "msg"      → strings added only for TYPE_MESSAGE
#   "op"       → strings added only for TYPE_OPERATION
#   "addr"     → strings added only for address-review flow
#   "adv"      → strings added only for nbgl_useCaseAdvancedReview
#   "skip"     → strings from the optional "isSkippable" path
# ---------------------------------------------------------------------------

# All unique strings taken from a full grep of
#   /opt/flex-secure-sdk/lib_nbgl/src/nbgl_use_case.c
# Only strings that the SDK **hardcodes itself** are listed here.
# Strings supplied as arguments by the app are NOT included (they are already
# counted in the app-level table).

SDK_STRINGS: dict[str, dict[str, list[str]]] = {

    # ------------------------------------------------------------------
    # nbgl_useCaseReview — used for TYPE_TRANSACTION and TYPE_OPERATION
    # ------------------------------------------------------------------
    "nbgl_useCaseReview": {
        "always": [
            "Swipe to review",   # first-page subText
            "Hold to sign",      # long-press button
            "Reject",            # nav-bar quit text (getRejectReviewText)
            "More",              # "details" button on long tag-value pages
            "Yes, reject",       # rejection confirmation modal
        ],
        "tx": [
            "Sign transaction",        # review title
            "Reject transaction?",     # rejection modal title
            "Go back to transaction",  # rejection modal cancel
        ],
        "msg": [],   # TYPE_MESSAGE not used here; see nbgl_useCaseAdvancedReview
        "op": [
            "Sign operation",          # review title
            "Reject operation?",       # rejection modal title
            "Go back to operation",    # rejection modal cancel
        ],
        "skip": [
            "Skip",              # skippable header
            "Skip review?",      # skip warning modal title
            "Go back to review", # skip warning modal cancel
            "Yes, skip",         # skip warning modal confirm
        ],
    },

    # ------------------------------------------------------------------
    # nbgl_useCaseAdvancedReview — used for TYPE_TRANSACTION, TYPE_MESSAGE,
    #   TYPE_OPERATION. Includes the full security-warning flow.
    # ------------------------------------------------------------------
    "nbgl_useCaseAdvancedReview": {
        "always": [
            "Swipe to review",
            "Hold to sign",
            "Reject",
            "More",
            "Yes, reject",
            # Security report header
            "Security report",
            "Scan to view full report",
            # Warning page buttons
            "Back to safety",
            "Continue anyway",
            "Accept risk and continue",
            # Tip-box labels (shown in the review header bar)
            "Blind signing required.",
            "Transaction Check unavailable",
            "Critical threat detected.",
            "Potential risk detected.",
            "No threat detected by Transaction Check.",
            "Transaction Check unavailable.\nBlind signing required.",
            "Critical threat detected.\nBlind signing required.",
            "Potential risk detected.\nBlind signing required.",
            "No threat detected by Transaction Check but blind signing required.",
            # Warning-page titles
            "Blind signing ahead",
            "Potential risk",
            "Critical threat",
            # Warning-page body / long text
            "This transaction cannot be Clear Signed",
            (
                "This transaction or message cannot be decoded fully. "
                "If you choose to sign, you "
            ),
            "Transaction Check unavailable",
            # Blind signing required banner
            "Blind signing required",
            (
                "This transaction's details are not fully verifiable. "
                "If you sign, you could "
            ),
        ],
        "tx": [
            "Sign transaction",
            "Reject transaction?",
            "Go back to transaction",
        ],
        "msg": [
            "Sign message",
            "Reject message?",
            "Go back to message",
        ],
        "op": [
            "Sign operation",
            "Reject operation?",
            "Go back to operation",
        ],
        "skip": [
            "Skip",
            "Skip review?",
            "Go back to review",
            "Yes, skip",
        ],
    },

    # ------------------------------------------------------------------
    # nbgl_useCaseAddressReview
    # ------------------------------------------------------------------
    "nbgl_useCaseAddressReview": {
        "always": [
            "Swipe to continue",  # first-page subText
            "Cancel",             # nav-bar quit text
            "Address",            # tag label
            "Confirm",            # confirmation button
            "Close",              # QR-code modal close button
            "Show as QR",         # details button (address too long for 1 page)
            # Info tooltips shown for ENS / address-book labels
            "ENS names are resolved by Ledger backend.",
            "This account label comes from your Address Book in Ledger Wallet.",
        ],
    },

    # ------------------------------------------------------------------
    # nbgl_useCaseReviewStatus — strings depend on status type
    # The app uses: TRANSACTION_SIGNED, TRANSACTION_REJECTED,
    #               MESSAGE_SIGNED,     MESSAGE_REJECTED,
    #               OPERATION_SIGNED,   OPERATION_REJECTED,
    #               ADDRESS_VERIFIED,   ADDRESS_REJECTED
    # ------------------------------------------------------------------
    "nbgl_useCaseReviewStatus": {
        "always": [
            "Transaction signed",
            "Transaction rejected",
            "Message signed",
            "Message rejected",
            "Operation signed",
            "Operation rejected",
            "Address verified",
            "Address verification\ncancelled",
        ],
    },

    # ------------------------------------------------------------------
    # nbgl_useCaseHomeAndSettings — app provides all strings as parameters.
    # No SDK-hardcoded strings.
    # ------------------------------------------------------------------
    "nbgl_useCaseHomeAndSettings": {
        "always": [],
    },

    # ------------------------------------------------------------------
    # nbgl_useCaseChoice — title / body / button labels all from app.
    # ------------------------------------------------------------------
    "nbgl_useCaseChoice": {
        "always": [],
    },

    # ------------------------------------------------------------------
    # nbgl_useCaseAction — all strings from app.
    # ------------------------------------------------------------------
    "nbgl_useCaseAction": {
        "always": [],
    },

    # ------------------------------------------------------------------
    # nbgl_useCaseStatus — all strings from app.
    # ------------------------------------------------------------------
    "nbgl_useCaseStatus": {
        "always": [],
    },

    # ------------------------------------------------------------------
    # nbgl_useCaseSpinner — spinner text from app.
    # ------------------------------------------------------------------
    "nbgl_useCaseSpinner": {
        "always": [],
    },
}

# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------

def find_used_use_cases(src_dir: Path) -> dict[str, list[str]]:
    """Return {use_case_name: [call_sites…]} for every nbgl_useCase* call."""
    call_re = re.compile(r"\b(nbgl_useCase\w+)\s*\(")
    used: dict[str, list[str]] = {}

    for fpath in sorted(src_dir.rglob("*.c")):
        # Skip SDK / third-party files that may be vendored
        if "ethereum-plugin-sdk" in fpath.parts:
            continue
        try:
            text = fpath.read_text(encoding="utf-8", errors="replace")
        except OSError:
            continue
        for m in call_re.finditer(text):
            fn = m.group(1)
            rel = str(fpath.relative_to(src_dir.parent))
            used.setdefault(fn, [])
            site = f"{rel}:{text[:m.start()].count(chr(10)) + 1}"
            if site not in used[fn]:
                used[fn].append(site)

    return used


def collect_sdk_strings(used_use_cases: set[str]) -> dict[str, list[str]]:
    """
    For each used use case return the de-duplicated list of SDK strings it
    contributes.
    """
    per_uc: dict[str, list[str]] = {}
    for uc in sorted(used_use_cases):
        if uc not in SDK_STRINGS:
            per_uc[uc] = []
            continue
        mapping = SDK_STRINGS[uc]
        strings: list[str] = []
        # Collect from all sub-categories
        for sub in ("always", "tx", "msg", "op", "addr", "adv", "skip"):
            strings.extend(mapping.get(sub, []))
        # Deduplicate (preserve order)
        seen: set[str] = set()
        deduped: list[str] = []
        for s in strings:
            if s not in seen:
                seen.add(s)
                deduped.append(s)
        per_uc[uc] = deduped
    return per_uc


def byte_size(s: str) -> int:
    """ROM cost of one C string literal in bytes (UTF-8 encoded + NUL byte)."""
    return len(s.encode("utf-8")) + 1


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--src-dir",
        default="src",
        help="App source directory to scan (default: src/)",
    )
    parser.add_argument(
        "--app-strings",
        type=int,
        default=78,
        help="Number of app-level strings per language (default: 78)",
    )
    parser.add_argument(
        "--app-kb-per-lang",
        type=float,
        default=3.00,
        help="Measured ROM delta per language for app strings (default: 3.00 KB)",
    )
    args = parser.parse_args()

    repo_root = Path(__file__).resolve().parent.parent.parent
    src_dir = repo_root / args.src_dir

    # ------------------------------------------------------------------
    # 1. Discover which use cases are actually called
    # ------------------------------------------------------------------
    print("=" * 68)
    print("SDK STRING ESTIMATION — app-ethereum × nbgl_use_case.c")
    print("=" * 68)
    print(f"\nScanning: {src_dir}\n")

    used = find_used_use_cases(src_dir)

    print("Use cases called by the app:")
    print(f"  {'Function':<45} {'Call sites':>10}")
    print(f"  {'-'*45}   {'-'*10}")
    for fn, sites in sorted(used.items()):
        in_sdk = "(SDK strings?)" if fn in SDK_STRINGS and any(
            SDK_STRINGS[fn].get(k) for k in ("always","tx","msg","op","addr","adv","skip")
        ) else "(app-provided strings only)"
        print(f"  {fn:<45} {len(sites):>3} sites  {in_sdk}")

    # ------------------------------------------------------------------
    # 2. Collect SDK strings per use case
    # ------------------------------------------------------------------
    per_uc = collect_sdk_strings(set(used.keys()))

    # Global unique set
    global_sdk_strings: set[str] = set()
    for strings in per_uc.values():
        global_sdk_strings.update(strings)

    # ------------------------------------------------------------------
    # 3. Print per-use-case breakdown
    # ------------------------------------------------------------------
    print("\n" + "=" * 68)
    print("SDK HARDCODED STRINGS PER USE CASE")
    print("=" * 68)

    for uc, strings in sorted(per_uc.items()):
        if not strings:
            # No SDK strings — app provides them all
            print(f"\n  {uc}")
            print("    → all strings provided by the app (none hardcoded in SDK)")
            continue
        uc_bytes = sum(byte_size(s) for s in strings)
        print(f"\n  {uc}  [{len(strings)} strings, {uc_bytes} bytes]")
        for i, s in enumerate(strings):
            display = s.replace("\n", "\\n")
            if len(display) > 60:
                display = display[:57] + "..."
            print(f"    {i+1:2d}. \"{display}\"  ({byte_size(s)} B)")

    # ------------------------------------------------------------------
    # 4. Global summary
    # ------------------------------------------------------------------
    total_sdk_bytes = sum(byte_size(s) for s in sorted(global_sdk_strings))
    total_sdk_kb    = total_sdk_bytes / 1024

    # App-level reference (from actual build measurement)
    app_bytes_per_lang = args.app_kb_per_lang * 1024  # approx
    app_avg_str_bytes  = app_bytes_per_lang / args.app_strings

    print("\n" + "=" * 68)
    print("SUMMARY")
    print("=" * 68)
    print(f"\n  App-level strings (measured)")
    print(f"    Count  : {args.app_strings} strings per language")
    print(f"    ROM    : {args.app_kb_per_lang:.2f} KB / language")
    print(f"    Avg    : {app_avg_str_bytes:.1f} bytes / string")

    print(f"\n  SDK-level hardcoded strings (static analysis)")
    print(f"    Count  : {len(global_sdk_strings)} unique strings")
    print(f"    Total  : {total_sdk_bytes} bytes  ({total_sdk_kb:.2f} KB) per language")

    total_kb = args.app_kb_per_lang + total_sdk_kb
    print(f"\n  Full-stack i18n cost per language")
    print(f"    App strings : {args.app_kb_per_lang:.2f} KB")
    print(f"    SDK strings : {total_sdk_kb:.2f} KB")
    print(f"    TOTAL       : {total_kb:.2f} KB / language")

    # Extrapolation table
    print(f"\n  Extrapolation (full-stack, {total_kb:.2f} KB / lang):")
    print(f"    {'Langs':>5}  {'App only (KB)':>15}  {'Full stack (KB)':>16}")
    print(f"    {'-'*5}  {'-'*15}  {'-'*16}")
    for n_langs in (3, 5, 10, 20):
        app_only   = args.app_kb_per_lang * n_langs
        full_stack = total_kb * n_langs
        print(f"    {n_langs:>5}  {app_only:>15.2f}  {full_stack:>16.2f}")

    # ------------------------------------------------------------------
    # 5. All unique SDK strings alphabetically
    # ------------------------------------------------------------------
    print(f"\n  All {len(global_sdk_strings)} unique SDK strings (alphabetical):")
    for i, s in enumerate(sorted(global_sdk_strings), 1):
        display = s.replace("\n", "\\n")
        if len(display) > 70:
            display = display[:67] + "..."
        print(f"    {i:3d}. \"{display}\"  ({byte_size(s)} B)")

    print()


if __name__ == "__main__":
    main()
