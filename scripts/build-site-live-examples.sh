#!/bin/sh
set -eu

site_dir=${1:-build/site}
src_dir="$site_dir/examples-src"
manifest="$site_dir/examples-manifest.json"
source_manifest="examples/manifest.json"

rm -rf "$src_dir"
mkdir -p "$src_dir/examples"

python3 - "$src_dir" "$manifest" "$source_manifest" <<'PY'
import json
import pathlib
import shutil
import sys

out_dir = pathlib.Path(sys.argv[1])
manifest_path = pathlib.Path(sys.argv[2])
source_manifest_path = pathlib.Path(sys.argv[3])
root = pathlib.Path.cwd()

items = []
source = json.loads(source_manifest_path.read_text(encoding="utf-8"))
for entry in source.get("examples", []):
    relative = pathlib.Path(entry["path"])
    path = root / relative
    if not path.is_file():
        raise SystemExit(f"missing example source: {relative}")
    dest = out_dir / relative
    dest.parent.mkdir(parents=True, exist_ok=True)
    shutil.copyfile(path, dest)
    items.append({
        "id": entry["id"],
        "title": entry["title"],
        "name": path.name,
        "path": relative.as_posix(),
        "url": f"examples-src/{relative.as_posix()}",
    })

manifest_path.write_text(json.dumps({"items": items}, indent=2) + "\n")
PY
