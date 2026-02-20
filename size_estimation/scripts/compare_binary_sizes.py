#!/usr/bin/env python3
"""
compare_binary_sizes.py
=======================
Measure the ROM / RAM overhead of embedding a second (French) language in the
Ethereum Ledger app binary.

The script performs two successive Docker-based builds:
  1. Baseline  — English strings only (current state of the repo)
  2. With-FR   — same build + ADD_FRENCH_LANG=1  (adds src/lang_fr.c)

After each build it runs ``arm-none-eabi-size`` on the resulting ELF to obtain
accurate section sizes, then prints a comparison table.

Usage
-----
    python size_estimation/scripts/compare_binary_sizes.py [--target TARGET] [--chain CHAIN]

Options
-------
    --target  Ledger target to build for (default: flex)
    --chain   Chain variant           (default: ethereum)
    --docker  Docker image to use     (default: auto-detected)
    --keep    Keep the with-FR ELF after the run (default: deleted)

Requirements
------------
  * Docker installed and running
  * The ledger-app-dev-tools image present locally
  * Run from the repo root (app-ethereum/)
"""

import argparse
import shutil
import subprocess
import sys
import re
from pathlib import Path
from typing import NamedTuple

# ---------------------------------------------------------------------------
# Constants
# ---------------------------------------------------------------------------
DEFAULT_TARGET = "flex"
DEFAULT_CHAIN = "ethereum"
DEFAULT_DOCKER_IMAGE = (
    "ghcr.io/ledgerhq/ledger-app-builder/ledger-app-dev-tools:latest"
)

REPO_ROOT     = Path(__file__).resolve().parent.parent.parent
ARTIFACTS_DIR = REPO_ROOT / "size_estimation" / "artifacts"

# Where make puts the output ELF (relative to repo root)
ELF_PATTERN = "build/{target}/bin/app.elf"

# arm-none-eabi-size output format: text  data  bss  dec  hex  filename
SIZE_RE = re.compile(
    r"^\s*(?P<text>\d+)\s+(?P<data>\d+)\s+(?P<bss>\d+)\s+"
    r"(?P<dec>\d+)\s+(?P<hex>[0-9a-fA-F]+)\s+",
    re.MULTILINE,
)


# ---------------------------------------------------------------------------
# Data model
# ---------------------------------------------------------------------------
class BinarySize(NamedTuple):
    text: int   # code + read-only data (ROM)
    data: int   # initialised data (ROM + RAM)
    bss: int    # zero-initialised data (RAM only)

    @property
    def rom(self) -> int:
        """Bytes consumed in flash (ROM)."""
        return self.text + self.data

    @property
    def ram(self) -> int:
        """Bytes consumed in RAM at runtime."""
        return self.data + self.bss


# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------
def _docker_volume(host_path: Path) -> str:
    """Convert a Windows or POSIX path to a Docker-compatible volume spec.

    Docker Desktop on Windows accepts both ``C:/Users/...:/app`` and the
    forward-slash POSIX form.  We use the former because it is unambiguous
    and matches what the user would type in PowerShell.
    """
    # as_posix() on Windows gives e.g. "C:/Users/..." which Docker Desktop
    # accepts directly.  On Linux/macOS it is already a valid POSIX path.
    return f"{host_path.as_posix()}:/app"


def _run_docker_build(
    docker_image: str,
    target: str,
    chain: str,
    extra_make_args: list[str],
    label: str,
    clean: bool = True,
) -> None:
    """Run `make` inside the ledger-app-dev-tools Docker container.

    `clean=True` runs ``make clean`` first so that object files compiled with
    different DEFINES are not reused from a previous build (make does not track
    DEFINES changes in its dependency graph).
    """
    clean_cmd = f"make TARGET={target} CHAIN={chain} clean && " if clean else ""
    make_cmd_str = "make " + " ".join([f"TARGET={target}", f"CHAIN={chain}"] + extra_make_args)
    full_cmd = clean_cmd + make_cmd_str + " 2>&1"

    cmd = [
        "docker", "run", "--rm",
        "-v", _docker_volume(REPO_ROOT),
        docker_image,
        "bash", "-c",
        f"cd /app && {full_cmd}",
    ]

    print(f"\n{'='*60}")
    print(f"  Building: {label}")
    print(f"  Command : {'make clean && ' if clean else ''}{make_cmd_str}")
    print(f"{'='*60}")

    proc = subprocess.run(cmd, capture_output=False, text=True)
    if proc.returncode != 0:
        print(f"\n[ERROR] Build failed for '{label}' (exit code {proc.returncode})")
        sys.exit(1)


def _measure_elf_size(docker_image: str, elf_path: Path) -> BinarySize:
    """Use arm-none-eabi-size (inside Docker) to measure section sizes."""
    container_elf = "/app/" + str(elf_path.relative_to(REPO_ROOT)).replace("\\", "/")
    cmd = [
        "docker", "run", "--rm",
        "-v", _docker_volume(REPO_ROOT),
        docker_image,
        "arm-none-eabi-size", container_elf,
    ]
    result = subprocess.run(cmd, capture_output=True, text=True)
    if result.returncode != 0:
        print(f"[ERROR] arm-none-eabi-size failed:\n{result.stderr}")
        sys.exit(1)

    m = SIZE_RE.search(result.stdout)
    if not m:
        print(f"[ERROR] Could not parse size output:\n{result.stdout}")
        sys.exit(1)

    return BinarySize(
        text=int(m.group("text")),
        data=int(m.group("data")),
        bss=int(m.group("bss")),
    )


def _backup_elf(elf_path: Path, suffix: str) -> Path:
    """Copy the ELF to size_estimation/artifacts/app_<suffix>.elf and return the new path.

    The backup is kept outside build/ so that ``make clean`` issued by the
    next build step does not delete it.
    """
    ARTIFACTS_DIR.mkdir(parents=True, exist_ok=True)
    dest = ARTIFACTS_DIR / f"app_{suffix}.elf"
    shutil.copy2(elf_path, dest)
    return dest


def _format_delta(delta: int) -> str:
    sign = "+" if delta >= 0 else ""
    return f"{sign}{delta:,}"


def _print_report(
    baseline: BinarySize,
    with_fr: BinarySize,
    target: str,
    chain: str,
) -> None:
    delta_text = with_fr.text - baseline.text
    delta_data = with_fr.data - baseline.data
    delta_bss  = with_fr.bss  - baseline.bss
    delta_rom  = with_fr.rom  - baseline.rom
    delta_ram  = with_fr.ram  - baseline.ram

    w = 14  # column width

    header = f"  Binary size comparison — target={target}, chain={chain}"
    sep    = "-" * 66

    print(f"\n{'='*66}")
    print(header)
    print(f"{'='*66}")
    print(f"  {'Section':<10}  {'Baseline (EN)':>{w}}  {'+ French (FR)':>{w}}  {'Delta':>{w}}")
    print(sep)
    print(f"  {'text':<10}  {baseline.text:>{w},}  {with_fr.text:>{w},}  {_format_delta(delta_text):>{w}}")
    print(f"  {'data':<10}  {baseline.data:>{w},}  {with_fr.data:>{w},}  {_format_delta(delta_data):>{w}}")
    print(f"  {'bss':<10}  {baseline.bss:>{w},}  {with_fr.bss:>{w},}  {_format_delta(delta_bss):>{w}}")
    print(sep)
    print(f"  {'ROM  (text+data)':<10}  {baseline.rom:>{w},}  {with_fr.rom:>{w},}  {_format_delta(delta_rom):>{w}}")
    print(f"  {'RAM  (data+bss)':<10}  {baseline.ram:>{w},}  {with_fr.ram:>{w},}  {_format_delta(delta_ram):>{w}}")
    print(f"{'='*66}")

    pct_rom = delta_rom / baseline.rom * 100 if baseline.rom else 0
    pct_ram = delta_ram / baseline.ram * 100 if baseline.ram else 0
    print(f"\n  ROM overhead of French strings : {_format_delta(delta_rom)} bytes  ({pct_rom:+.2f}%)")
    print(f"  RAM overhead of French strings : {_format_delta(delta_ram)} bytes  ({pct_ram:+.2f}%)")
    print()


# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------
def main() -> None:
    parser = argparse.ArgumentParser(
        description="Compare Ethereum app binary sizes with and without French language strings."
    )
    parser.add_argument("--target", default=DEFAULT_TARGET, help=f"Ledger target (default: {DEFAULT_TARGET})")
    parser.add_argument("--chain",  default=DEFAULT_CHAIN,  help=f"Chain variant  (default: {DEFAULT_CHAIN})")
    parser.add_argument("--docker", default=DEFAULT_DOCKER_IMAGE, help="Docker image to use")
    parser.add_argument("--keep",   action="store_true",    help="Keep the with-FR ELF file after the run")
    parser.add_argument("--no-build-baseline", action="store_true",
                        help="Skip rebuilding baseline (re-use existing ELF)")
    args = parser.parse_args()

    elf_path = REPO_ROOT / ELF_PATTERN.format(target=args.target)
    if not elf_path.parent.exists():
        elf_path.parent.mkdir(parents=True, exist_ok=True)

    # ------------------------------------------------------------------
    # Step 1 – Baseline build (English only)
    # ------------------------------------------------------------------
    if not args.no_build_baseline:
        _run_docker_build(
            docker_image=args.docker,
            target=args.target,
            chain=args.chain,
            extra_make_args=[],
            label="Baseline (English only)",
        )
    else:
        print("\n[INFO] --no-build-baseline set: reusing existing ELF.")

    if not elf_path.exists():
        print(f"[ERROR] ELF not found after baseline build: {elf_path}")
        sys.exit(1)

    print(f"\n[INFO] Measuring baseline ELF: {elf_path}")
    baseline_size = _measure_elf_size(args.docker, elf_path)
    baseline_elf  = _backup_elf(elf_path, "baseline_en")
    print(f"       → ROM={baseline_size.rom:,} B  RAM={baseline_size.ram:,} B  (saved as {baseline_elf.name})")

    # ------------------------------------------------------------------
    # Step 2 – French build (ADD_FRENCH_LANG=1)
    # ------------------------------------------------------------------
    _run_docker_build(
        docker_image=args.docker,
        target=args.target,
        chain=args.chain,
        extra_make_args=["ADD_FRENCH_LANG=1"],
        label="With French strings (ADD_FRENCH_LANG=1)",
    )

    if not elf_path.exists():
        print(f"[ERROR] ELF not found after French build: {elf_path}")
        sys.exit(1)

    print(f"\n[INFO] Measuring French ELF: {elf_path}")
    fr_size = _measure_elf_size(args.docker, elf_path)
    fr_elf  = _backup_elf(elf_path, "with_fr")
    print(f"       → ROM={fr_size.rom:,} B  RAM={fr_size.ram:,} B  (saved as {fr_elf.name})")

    # ------------------------------------------------------------------
    # Step 3 – Report
    # ------------------------------------------------------------------
    _print_report(baseline_size, fr_size, args.target, args.chain)

    # Optionally clean up the French ELF
    if not args.keep:
        fr_elf.unlink(missing_ok=True)

    # Restore the baseline ELF as the active one
    shutil.copy2(baseline_elf, elf_path)
    print(f"[INFO] Restored baseline ELF as active build/{args.target}/bin/app.elf")


if __name__ == "__main__":
    main()
