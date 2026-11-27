"""Inputs of the extra root-package module folders passed to Ziran."""

import hashlib
import os
from pathlib import Path
import shlex


SOURCE_SUFFIXES = (".zi", ".ts", ".tsx")


def module_path_roots(arguments, base):
    roots = []
    words = iter(arguments)
    for word in words:
        if word == "--module-path":
            value = next(words, "")
        elif word.startswith("--module-path="):
            value = word.removeprefix("--module-path=")
        else:
            raise ValueError(f"unexpected module-path argument: {word!r}")
        if not value:
            raise ValueError("--module-path needs a directory")
        roots.append((base / value).resolve())
    return list(dict.fromkeys(roots))


def module_path_sources(roots):
    # Match the project driver's module scan: files in the root and one
    # subdirectory, including module.zi, module.ts and module.tsx packages.
    return sorted({path for root in roots for pattern in ("*", "*/*")
                   for path in root.glob(pattern)
                   if path.suffix in SOURCE_SUFFIXES and path.is_file()})


def module_path_fingerprint(roots):
    digest = hashlib.sha256()
    for root in roots:
        digest.update(os.fsencode(root) + b"\0" + bytes([root.is_dir()]))
    for path in module_path_sources(roots):
        digest.update(os.fsencode(path) + b"\0")
        with path.open("rb") as source:
            for chunk in iter(lambda: source.read(65536), b""):
                digest.update(chunk)
        digest.update(b"\0")
    return digest.hexdigest()


if __name__ == "__main__":
    roots = module_path_roots(shlex.split(os.environ.get("KRYON_MODULE_PATH_ARGS", "")),
                              Path.cwd())
    print(module_path_fingerprint(roots))
