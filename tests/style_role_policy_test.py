#!/usr/bin/env python3
"""Reject hard-coded nonzero style role IDs in runtime call sites."""

from __future__ import annotations

import re
import sys
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]

CALLS = {
    "StyleControlRoleFacts": 3,
}

SOURCE_DIR = ROOT / "src/ui"


def source_files() -> list[Path]:
    return sorted(SOURCE_DIR.glob("*.zi"))


def split_args(text: str) -> list[str]:
    args: list[str] = []
    start = 0
    depth = 0
    for i, ch in enumerate(text):
        if ch in "([{":
            depth += 1
        elif ch in ")]}":
            depth -= 1
        elif ch == "," and depth == 0:
            args.append(text[start:i].strip())
            start = i + 1
    tail = text[start:].strip()
    if tail:
        args.append(tail)
    return args


def call_bodies(text: str, name: str) -> list[tuple[int, str]]:
    bodies: list[tuple[int, str]] = []
    pattern = re.compile(rf"\b{re.escape(name)}\s*\(")
    for match in pattern.finditer(text):
        i = match.end()
        depth = 1
        while i < len(text) and depth > 0:
            ch = text[i]
            if ch == "(":
                depth += 1
            elif ch == ")":
                depth -= 1
            i += 1
        if depth == 0:
            line = text.count("\n", 0, match.start()) + 1
            bodies.append((line, text[match.end():i - 1]))
    return bodies


def is_forbidden_literal(arg: str) -> bool:
    return re.fullmatch(r"[1-9][0-9]*", arg) is not None


def main() -> int:
    errors: list[str] = []
    for path in source_files():
        text = path.read_text(encoding="utf-8")
        rel = path.relative_to(ROOT)
        for name, role_index in CALLS.items():
            for line, body in call_bodies(text, name):
                args = split_args(body)
                if len(args) <= role_index:
                    continue
                role = args[role_index]
                if is_forbidden_literal(role):
                    errors.append(
                        f"{rel}:{line}: hard-coded style role {role} in {name}; "
                        "use a named Ziran role helper"
                    )
    if errors:
        print("style role policy drift:")
        print("\n".join(errors))
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
