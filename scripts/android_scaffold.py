#!/usr/bin/env python3
"""Create the droid/ Gradle project and host glue for a Kryon application.

The generated files belong to the application. Only missing files and exact
copies of superseded templates are written, so an app can customize freely.
Library-owned notification receiver code is refreshed and its required manifest
entries are merged without replacing application routes or configuration.
Placeholders in kryon/templates/droid carry the application id, activity and
entry names.
"""
import argparse
from hashlib import sha256
from pathlib import Path
from android_notifications_scaffold import ensure_notifications

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
    # CMake reads words after a project name as languages.
    "__CMAKE_NAME__": "".join(c for c in args.name if c.isalnum() or c == "_"),
    "__NAME__": args.name,
    "__PACKAGE__": args.package,
    "__PACKAGE_PATH__": args.package.replace(".", "/"),
    "__ACTIVITY__": args.activity,
    "__ACTIVITY_CLASS__": args.package + "." + args.activity,
    "__ACTIVITY_SLASH__": args.package.replace(".", "/") + "/" + args.activity,
}

written = []
skipped = []
upgraded = []
# Normalize only the fields that were substituted by the old templates.
stock_upgrades = {
    "droid/app/build.gradle": (
        ("17dd534c81ba962446610198ce872cf3e62cdfb9b5b1715a1773536bd792a49e",
         "0f59566c9ed530ec6ebf84a6bef521651744426d49296eaa4c16c46023feb6c7"),
        ["    namespace '__PACKAGE__'", "        applicationId '__PACKAGE__'"]),
    "droid/app/src/main/cpp/CMakeLists.txt": (
        ("ae3d4b1624cb05f4143dfb9add7ce3a8a68d5e55136ae83de7b6056468824222",),
        ["project(__CMAKE_NAME__ C)"]),
    "droid/app/src/main/java/__PACKAGE_PATH__/__ACTIVITY__.java": (
        ("381a7e855806af3cd7af31f3f1e421a60cf5e2129c8a05bb802ef3dc3c8c6981",),
        ["package __PACKAGE__;", "public class __ACTIVITY__ extends NativeActivity {",
         '    private static final String TAG = "__ACTIVITY__";']),
}


def render(text):
    for key, value in values.items():
        text = text.replace(key, value)
    return text


for template in sorted(templates.rglob("*")):
    if template.is_dir() or template.name == "README.md":
        continue
    relative = template.relative_to(templates)
    # Play Billing stays in Kryon; only the Google Play build applies it
    # (templates/android-gplay.gradle), so apps hold no Google code.
    if relative.parts[:4] == ("droid", "app", "src", "gplay"):
        continue
    target = root.joinpath(*(render(part) for part in relative.parts))
    if target.exists():
        stock = stock_upgrades.get(relative.as_posix())
        if stock and target.is_file() and not target.is_symlink():
            digests, lines = stock
            text = target.read_text()
            for line in lines:
                text = text.replace(render(line), line, 1)
            if sha256(text.encode()).hexdigest() in digests:
                target.write_text(render(template.read_text()))
                upgraded.append(target.relative_to(root))
                continue
        skipped.append(target.relative_to(root))
        continue
    target.parent.mkdir(parents=True, exist_ok=True)
    if template.suffix == ".jar":
        # The Gradle wrapper jar is binary; copy it without substitution.
        target.write_bytes(template.read_bytes())
    else:
        target.write_text(render(template.read_text()))
    written.append(target.relative_to(root))

if skipped:
    print("Kept existing files:")
    for path in skipped:
        print(f"  {path}")
if upgraded:
    print("Upgraded matching Android stock files:")
    for path in upgraded:
        print(f"  {path}")
if written:
    print("Created Android project files:")
    for path in written:
        print(f"  {path}")
elif not upgraded:
    print("Android project already present")

ensure_notifications(root, args.kryon.resolve())
