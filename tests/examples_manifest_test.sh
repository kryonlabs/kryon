#!/usr/bin/env sh
set -eu

root="${1:-.}"
cd "$root"

python3 - <<'PY'
import json
import sys
from pathlib import Path

root = Path(".")
manifest_path = root / "examples" / "manifest.json"
data = json.loads(manifest_path.read_text(encoding="utf-8"))
entries = data.get("examples", [])
errors = []

ids = [entry.get("id") for entry in entries]
paths = [entry.get("path") for entry in entries]

if len(ids) != len(set(ids)):
    errors.append("duplicate example ids in examples/manifest.json")
if len(paths) != len(set(paths)):
    errors.append("duplicate example paths in examples/manifest.json")

listed = set()
for entry in entries:
    path = entry.get("path")
    if not path:
        errors.append(f"missing path for example id {entry.get('id')!r}")
        continue
    listed.add(path)
    if not (root / path).is_file():
        errors.append(f"listed example does not exist: {path}")
    if not entry.get("title"):
        errors.append(f"missing title for {path}")

actual = {path.as_posix() for path in sorted((root / "examples").glob("*.kry"))}
missing = sorted(actual - listed)
extra = sorted(listed - actual)
if missing:
    errors.append("examples missing from manifest: " + ", ".join(missing))
if extra:
    errors.append("manifest paths are not examples/*.kry: " + ", ".join(extra))

exact = [entry for entry in entries if entry.get("krb_exact")]
if not exact:
    errors.append("manifest must mark at least one krb_exact example")

if errors:
    for error in errors:
        print(error, file=sys.stderr)
    sys.exit(1)

print(f"examples manifest ok: {len(entries)} examples, {len(exact)} krb exact")
PY

work=$(mktemp -d "${TMPDIR:-/tmp}/kryon-site-examples.XXXXXX")
trap 'rm -rf "$work"' EXIT INT TERM
sh scripts/build-site-live-examples.sh "$work"

python3 - "$work" <<'PY'
import json
import sys
from pathlib import Path

root = Path(".")
site = Path(sys.argv[1])
source = json.loads((root / "examples" / "manifest.json").read_text(encoding="utf-8"))["examples"]
generated = json.loads((site / "examples-manifest.json").read_text(encoding="utf-8"))["items"]

expected = [(item["id"], item["title"], item["path"]) for item in source]
actual = [(item["id"], item["title"], item["path"]) for item in generated]
if actual != expected:
    raise SystemExit("website examples manifest does not match examples/manifest.json")

for item in generated:
    copied = site / item["url"]
    if not copied.is_file():
        raise SystemExit(f"website example source missing: {item['url']}")

print(f"website examples ok: {len(generated)} canonical examples")
PY
