#!/usr/bin/env python3
"""Replace short compound literals like (Vector2){0} with file-local zero
constants.

The native Plan 9 compiler (8c) rejects compound literals whose initializer
list is shorter than the struct ("constructor list too short"), while
plain zero-initialized declarations are fine. This rewrites each
`(Type){0}` site to a `static const Type kryon_zero_type` object - a copy
of a zero object, which is semantically identical on every platform.
"""
import re
import sys

LIT = re.compile(r"\(([A-Za-z_][A-Za-z0-9_]*)\)\{0\}")


def rewrite(path):
    with open(path, "r", encoding="utf-8") as f:
        src = f.read()
    types = sorted(set(LIT.findall(src)))
    if not types:
        return False
    out = LIT.sub(lambda m: "kryon_zero_" + m.group(1).lower(), src)
    decls = "\n/* zero constants: the native Plan 9 compiler rejects short\n"
    decls += " * compound literals like (Type){0}, and a copy of a zero\n"
    decls += " * object is equivalent on every platform. */\n"
    for t in types:
        decls += f"static const {t} kryon_zero_{t.lower()};\n"
    # insert after the last #include line at the top of the file
    lines = out.split("\n")
    last_inc = 0
    for i, line in enumerate(lines[:400]):
        if line.startswith("#include"):
            last_inc = i
    lines.insert(last_inc + 1, decls)
    with open(path, "w", encoding="utf-8") as f:
        f.write("\n".join(lines))
    return True


def main():
    for path in sys.argv[1:]:
        if rewrite(path):
            print(f"rewrote {path}")


if __name__ == "__main__":
    main()
