#!/usr/bin/env python3
"""Strip apostrophes from C comments in the given files (in place).

The native Plan 9 cpp (9legacy) mislexes apostrophes inside comments as
character-constant starts ("Unterminated string or char const"), so the
native build preprocesses with that cpp and needs comment text without
apostrophes. String and character literals are left untouched; only
// and /* */ comment text is rewritten (' dropped, e.g. "don't" -> "dont").
"""
import sys


def strip_file(path):
    with open(path, "r", encoding="utf-8") as f:
        src = f.read()
    out = []
    i = 0
    n = len(src)
    state = "code"
    changed = 0
    while i < n:
        c = src[i]
        if state == "code":
            if c == "/" and i + 1 < n and src[i + 1] == "/":
                state = "line"
                out.append(c)
            elif c == "/" and i + 1 < n and src[i + 1] == "*":
                state = "block"
                out.append(c)
            elif c == '"':
                state = "str"
                out.append(c)
            elif c == "'":
                state = "chr"
                out.append(c)
            else:
                out.append(c)
            i += 1
        elif state == "line":
            if c == "\n":
                state = "code"
                # A backslash left as the last character of a line comment
                # would splice the next line into the comment; drop it.
                if out and out[-1] == "\\":
                    out.pop()
                out.append(c)
            elif c == "'":
                changed += 1
            else:
                out.append(c)
            i += 1
        elif state == "block":
            if c == "*" and i + 1 < n and src[i + 1] == "/":
                state = "code"
                out.append("*/")
                i += 2
            elif c == "'":
                changed += 1
                i += 1
            else:
                out.append(c)
                i += 1
        elif state == "str":
            if c == "\\":
                out.append(src[i:i + 2])
                i += 2
            else:
                if c == '"':
                    state = "code"
                out.append(c)
                i += 1
        elif state == "chr":
            if c == "\\":
                out.append(src[i:i + 2])
                i += 2
            else:
                if c == "'":
                    state = "code"
                out.append(c)
                i += 1
    new = "".join(out)
    if new != src:
        with open(path, "w", encoding="utf-8") as f:
            f.write(new)
    return changed


def main():
    total = 0
    for path in sys.argv[1:]:
        total += strip_file(path)
    print(f"stripped {total} apostrophes from comments")


if __name__ == "__main__":
    main()
