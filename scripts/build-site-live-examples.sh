#!/bin/sh
set -eu

site_dir=${1:-build/site}
src_dir="$site_dir/examples-src"
manifest="$site_dir/examples-manifest.json"

rm -rf "$src_dir"
mkdir -p "$src_dir/examples" "$src_dir/widgets"

python3 - "$src_dir" "$manifest" <<'PY'
import json
import pathlib
import shutil
import sys

out_dir = pathlib.Path(sys.argv[1])
manifest_path = pathlib.Path(sys.argv[2])
root = pathlib.Path.cwd()

groups = [
    ("Examples", root / "examples", out_dir / "examples", "examples"),
    ("Widgets", root / "tests" / "parity", out_dir / "widgets", "widgets"),
]

items = []
for group_name, source_dir, dest_dir, public_prefix in groups:
    if not source_dir.exists():
        continue
    for path in sorted(source_dir.glob("*.kry")):
        dest = dest_dir / path.name
        shutil.copyfile(path, dest)
        stem = path.stem
        title = stem
        if "_" in title:
            first, rest = title.split("_", 1)
            if first.isdigit():
                title = rest
        title = title.replace("_", " ").replace("-", " ").title()
        items.append({
            "group": group_name,
            "title": title,
            "name": path.name,
            "path": f"{public_prefix}/{path.name}",
            "url": f"examples-src/{public_prefix}/{path.name}",
        })

manifest_path.write_text(json.dumps({"items": items}, indent=2) + "\n")
PY
