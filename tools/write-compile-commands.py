#!/usr/bin/env python3
"""Describe generated C compilation to editors using the real build flags."""

import json
import pathlib
import shlex
import shutil
import sys


project = pathlib.Path.cwd().resolve()
generated = (project / "build/generated/c").resolve()
ziran_include = pathlib.Path(sys.argv[1]).resolve()
compiler = shlex.split(sys.argv[2])
compiler[0] = shutil.which(compiler[0]) or compiler[0]
flags = [
    "-std=c99",
    "-pedantic-errors",
    "-O2",
    "-ffunction-sections",
    "-fdata-sections",
    f"-I{ziran_include}",
    f"-I{generated}",
]
commands = [
    {
        "directory": str(project),
        "file": str(source),
        "arguments": compiler + flags + ["-c", str(source)],
    }
    for source in sorted(generated.glob("*.c"))
]
destination = project / "build/generated/compile_commands.json"
temporary = destination.with_suffix(".json.tmp")
temporary.write_text(json.dumps(commands, indent=2) + "\n")
temporary.replace(destination)
