#!/usr/bin/env python3
"""Build the pinned Ziran host runtime (libziran.a) for an Android target."""

import argparse
from pathlib import Path
import subprocess

p = argparse.ArgumentParser()
p.add_argument("--source", type=Path, required=True)
p.add_argument("--output", type=Path, required=True)
p.add_argument("--cc", required=True)
p.add_argument("--ar", default="ar")
p.add_argument("--objcopy", default="objcopy")
p.add_argument("--flags", default="-O2 -fPIC")
a = p.parse_args()
source = a.source.resolve()
output = a.output.resolve()
command = [
    "make",
    "-j4",
    "-C",
    str(source),
    "BOOTSTRAP=1",
    "BUILD_DIR=" + str(output),
    "CC=" + a.cc,
    "AR=" + a.ar,
    "OBJCOPY=" + a.objcopy,
    "CFLAGS=" + a.flags,
    "FRAMEFLAGS=-Wframe-larger-than=16384",
    str(output / "libziran.a"),
]
subprocess.run(command, check=True)
