#!/usr/bin/env python3
"""Create the droid/ Gradle project and host glue for a Kryon application.

The generated files belong to the application: this script writes only files
that do not exist yet, so an app customizes its activity or build freely.
Placeholders in kryon/templates/droid carry the application id, activity and
entry names.
"""
import argparse
from pathlib import Path

parser = argparse.ArgumentParser()
parser.add_argument("--root", type=Path, required=True)
parser.add_argument("--name", required=True)
parser.add_argument("--package", required=True)
parser.add_argument("--activity", required=True)
parser.add_argument("--kryon", type=Path, required=True)
args = parser.parse_args()
root = args.root.resolve()
templates = args.kryon.resolve() / "templates" / "droid"

for segment in args.package.split("."):
    if not segment or not (segment[0].isalpha() or segment[0] == "_"):
        raise SystemExit(f"Invalid Java package: {args.package}")
    if not all(c.isalnum() or c == "_" for c in segment):
        raise SystemExit(f"Invalid Java package: {args.package}")
if not (args.activity[0].isalpha() or args.activity[0] == "_"):
    raise SystemExit(f"Invalid activity class name: {args.activity}")
if not all(c.isalnum() or c == "_" for c in args.activity):
    raise SystemExit(f"Invalid activity class name: {args.activity}")

values = {
    "__NAME__": args.name,
    "__PACKAGE__": args.package,
    "__PACKAGE_PATH__": args.package.replace(".", "/"),
    "__ACTIVITY__": args.activity,
    "__ACTIVITY_CLASS__": args.package + "." + args.activity,
    "__ACTIVITY_SLASH__": args.package.replace(".", "/") + "/" + args.activity,
}

written = []
skipped = []
for template in sorted(templates.rglob("*")):
    if template.is_dir() or template.name == "README.md":
        continue
    relative = template.relative_to(templates)
    parts = []
    for part in relative.parts:
        for key, value in values.items():
            part = part.replace(key, value)
        parts.append(part)
    target = root.joinpath(*parts)
    if target.exists():
        skipped.append(target.relative_to(root))
        continue
    target.parent.mkdir(parents=True, exist_ok=True)
    if template.suffix == ".jar":
        # The Gradle wrapper jar is binary; copy it without substitution.
        target.write_bytes(template.read_bytes())
    else:
        text = template.read_text()
        for key, value in values.items():
            text = text.replace(key, value)
        target.write_text(text)
    written.append(target.relative_to(root))

if skipped:
    print("Kept existing files:")
    for path in skipped:
        print(f"  {path}")
if written:
    print("Created Android project files:")
    for path in written:
        print(f"  {path}")
else:
    print("Android project already present")
