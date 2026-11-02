#!/usr/bin/env python3
"""Verify the consumer sync contract and the original sheets' exact RGBA pixels."""
import hashlib
import json
import mimetypes
from pathlib import Path
import subprocess
import sys

from PIL import Image

ROOT = Path(__file__).resolve().parents[1]
EXPECTED = {
    "ui": ((512, 512), "074ff85b16edfb9f914a9adba72328ca62260ce95c0f1e658b3569a4d8d7f79e"),
    "pfp": ((320, 384), "283d9689a18ec5a8c0944507603310d180770c82fa30e4ed22dfce2caffc2aec"),
}


def main():
    destination = Path(sys.argv[1])
    destination.mkdir(parents=True, exist_ok=True)
    subprocess.run(["sh", str(ROOT / "scripts/sync-icons.sh"), str(destination)], check=True)
    for manifest in (ROOT / "icons").glob("*.json"):
        staged = json.loads((destination / manifest.name).read_text())
        image = destination / staged["image"]
        assert image.read_bytes() == (ROOT / "icons" / staged["image"]).read_bytes()
    for name, (size, digest) in EXPECTED.items():
        path = destination / f"{name}.webp"
        assert mimetypes.guess_type(path)[0] == "image/webp"
        data = path.read_bytes()
        assert data[:4] == b"RIFF" and data[8:12] == b"WEBP"
        assert b"VP8L" in data[:32], "sheet must be lossless WebP"
        with Image.open(path) as image:
            assert image.format == "WEBP" and image.size == size
            rgba = image.convert("RGBA").tobytes()
        assert hashlib.sha256(rgba).hexdigest() == digest, f"{name}: RGBA changed"
        assert not (ROOT / "icons" / f"{name}.png").exists()
        (destination / f"{name}.rgba").write_bytes(rgba)
    print("Icon sync, manifest paths, WebP MIME, dimensions and original RGBA passed")


if __name__ == "__main__":
    main()
