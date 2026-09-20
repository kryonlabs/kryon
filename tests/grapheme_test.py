#!/usr/bin/env python3
"""Exercise the Kry grapheme host against pinned Unicode 17 conformance data."""

import ctypes
import os
from pathlib import Path
import shlex
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
DATA = ROOT / "tests/fixtures/GraphemeBreakTest-17.0.0.txt"
BUILD = Path(os.environ.get("KRYON_BUILD_DIR", "build/linux-x86_64"))
if not BUILD.is_absolute():
    BUILD = ROOT / BUILD
GENERATED = BUILD / "generated/src"

with tempfile.TemporaryDirectory(prefix="kryon-grapheme.") as temporary:
    library = Path(temporary) / "grapheme.so"
    subprocess.run(shlex.split(os.environ.get("CC", "cc")) + [
        "-shared", "-fPIC", "-std=c99", "-Wall", "-Wextra", "-Werror", "-O2",
        "-I" + str(GENERATED), "-I" + str(BUILD / "generated/include"),
        "-I" + str(ROOT / "include"), "-I" + str(ROOT / "vendor/utf8proc"),
        "-DUTF8PROC_STATIC", str(GENERATED / "ui/grapheme.c"),
        str(ROOT / "src/backend/kry_unicode.c"),
        "-o", str(library)], check=True)
    native = ctypes.CDLL(str(library))
    next_boundary = native.ui_grapheme_next_boundary
    next_boundary.argtypes = [ctypes.c_char_p, ctypes.c_int, ctypes.c_int]
    floor = native.ui_grapheme_floor_offset
    previous = native.ui_grapheme_prev_offset
    following = native.ui_grapheme_next_offset
    for function in (floor, previous, following):
        function.argtypes = [ctypes.c_char_p, ctypes.c_int]
    count = 0
    for number, line in enumerate(DATA.read_text().splitlines(), 1):
        tokens = line.split("#", 1)[0].split()
        if not tokens:
            continue
        text = bytearray()
        expected = []
        for token in tokens:
            if token == "\u00f7":
                expected.append(len(text))
            elif token != "\u00d7":
                text.extend(chr(int(token, 16)).encode("utf-8"))
        text = bytes(text)
        actual = [0]
        while actual[-1] < len(text):
            offset = next_boundary(text, len(text), actual[-1])
            assert actual[-1] < offset <= len(text), (number, offset)
            actual.append(offset)
        assert actual == expected, (number, actual, expected)
        if b"\0" not in text:
            for offset in range(len(text) + 1):
                assert floor(text, offset) == max(i for i in expected if i <= offset), number
                assert previous(text, offset) == max([i for i in expected if i < offset] or [0]), number
                assert following(text, offset) == min([i for i in expected if i > offset] or [len(text)]), number
        count += 1
    print(f"Kry grapheme host: {count} Unicode 17 conformance cases passed")
