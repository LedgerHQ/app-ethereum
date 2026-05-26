#!/usr/bin/env python3
"""Shared parser for the Ethereum fuzz command registry."""

from __future__ import annotations

from dataclasses import dataclass
from pathlib import Path
import re


REGISTRY_PATH = Path(__file__).resolve().parents[1] / "src" / "fuzz_command_registry.inc"

ENTRY_RE = re.compile(
    r"""^FUZZ_COMMAND\(
        \s*([A-Z_]+)\s*,            # group
        \s*([A-Z0-9_]+)\s*,         # INS symbol
        \s*([^,]+)\s*,              # p1_max
        \s*([^,]+)\s*,              # p2_max
        \s*([^,]+)\s*,              # flags
        \s*([A-Z_]+)\s*,            # payload kind
        \s*([A-Z0-9_]+)\s*,         # tlv kind
        \s*(true|false)\s*          # allow_empty_data
    \)\s*$""",
    re.VERBOSE,
)


@dataclass(frozen=True)
class FuzzCommand:
    index: int
    group: str
    name: str
    p1_max: str
    p2_max: str
    flags: str
    payload_kind: str
    tlv_kind: str
    allow_empty_data: bool


def load_commands(registry_path: Path | None = None) -> list[FuzzCommand]:
    path = registry_path or REGISTRY_PATH
    commands: list[FuzzCommand] = []

    for raw_line in path.read_text(encoding="utf-8").splitlines():
        line = raw_line.strip()
        if not line or line.startswith("/*") or line.startswith("*") or line.startswith("//"):
            continue
        match = ENTRY_RE.match(line)
        if not match:
            continue
        commands.append(
            FuzzCommand(
                index=len(commands),
                group=match.group(1),
                name=match.group(2),
                p1_max=match.group(3).strip(),
                p2_max=match.group(4).strip(),
                flags=match.group(5).strip(),
                payload_kind=match.group(6),
                tlv_kind=match.group(7),
                allow_empty_data=(match.group(8) == "true"),
            )
        )
    return commands


def command_index_map(commands: list[FuzzCommand] | None = None) -> dict[str, int]:
    entries = commands or load_commands()
    return {command.name: command.index for command in entries}


def canonical_apdu_commands(commands: list[FuzzCommand] | None = None) -> list[FuzzCommand]:
    entries = commands or load_commands()
    return [command for command in entries if command.group == "APDU"]
