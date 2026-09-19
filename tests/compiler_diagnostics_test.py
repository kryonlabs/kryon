#!/usr/bin/env python3
"""Source diagnostics must be equivalent and machine-readable across emitters."""

import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile


BIN = Path(sys.argv[1]).resolve()
CASES = {
    "syntax": ('#style missing\n', "parse.syntax", "#style requires"),
    "type": ('Check :: () -> i32 {\n    return true\n}\n', "check.type", "return type mismatch"),
    "record": ('Item :: struct {\n    value: void\n}\n', "check.record", "record field cannot have void type"),
}

with tempfile.TemporaryDirectory(prefix="kryon-diagnostics.") as temporary:
    work = Path(temporary)
    for name, (source, code, message) in CASES.items():
        filename = work / (name + ' "quoted".kry')
        filename.write_text(source)
        reference = None
        for target in ("k2c", "k2cpp", "k2go"):
            arguments = [str(BIN / target), "--strict", "--no-main", "--root", str(work),
                         "-o", str(work / target), str(filename)]
            result = subprocess.run(arguments[:1] + ["--diagnostics=json"] + arguments[1:],
                                    capture_output=True, text=True)
            assert result.returncode != 0, (target, name)
            diagnostics = [json.loads(line) for line in result.stderr.splitlines()]
            assert diagnostics, (target, name)
            diagnostic = next(item for item in diagnostics if item["code"] == code)
            assert message in diagnostic["message"], diagnostic
            assert Path(diagnostic["path"]).name == filename.name, diagnostic
            assert diagnostic["line"] >= 1 and diagnostic["column"] >= 1, diagnostic
            assert diagnostic["end_line"] >= diagnostic["line"], diagnostic
            assert diagnostic["severity"] == "error", diagnostic
            if reference is None:
                reference = diagnostics
            assert diagnostics == reference, (name, target, diagnostics, reference)
            text = subprocess.run(arguments[:1] + ["--diagnostics=text"] + arguments[1:],
                                  env=dict(os.environ, KRYON_DIAGNOSTICS="json"),
                                  capture_output=True, text=True)
            assert text.returncode != 0 and message in text.stderr, text.stderr
            assert not text.stderr.startswith("{"), text.stderr
            inherited = subprocess.run(arguments, env=dict(os.environ, KRYON_DIAGNOSTICS="json"),
                                       capture_output=True, text=True)
            assert inherited.stderr == result.stderr, inherited.stderr
    missing = subprocess.run([str(BIN / "k2kir"), "--diagnostics=json", "--root", str(work),
                              "-o", str(work / "kir"), str(work / "missing.kry")],
                             capture_output=True, text=True)
    diagnostic = json.loads(missing.stderr)
    assert missing.returncode != 0 and diagnostic["path"].endswith("missing.kry")
    assert diagnostic["line"] == 0 and "open failed" in diagnostic["message"]
    valid = work / "valid.kry"
    valid.write_text("Value :: () -> i32 {\n    return 3\n}\n")
    for target in ("k2c", "k2cpp", "k2go", "k2kir"):
        result = subprocess.run([str(BIN / target), "--diagnostics=json", "--root", str(work),
                                 "-o", str(work / target), str(valid)], capture_output=True, text=True)
        assert result.returncode == 0 and result.stderr == "", (target, result.stderr)
    print("compiler diagnostics: source ranges, JSON escaping, target parity, format override passed")
