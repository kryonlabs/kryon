"""Resolve the same configured Ziran package as the Makefile and shell tests."""

import os
import json
from pathlib import Path
import subprocess
import tempfile


JS_RUNTIME = os.environ.get("JS_RUNTIME", "node")

ROOT = Path(__file__).resolve().parents[1]
UI_MODULE_ARGS = [arg for directory in sorted((ROOT / "src/ui").rglob("*"))
                  if directory.is_dir() for arg in ("--module-path", directory)]
UI_MODULE_ARGS[:0] = ["--module-path", ROOT / "src/ui"]
source = os.environ.get("ZIRAN_ROOT")
if not source:
    source = subprocess.check_output(
        [os.environ.get("ZIRAN_BIN", "ziran"), "pkg", "path", "ziran"],
        cwd=ROOT, text=True,
    ).strip()
ZIRAN_ROOT = Path(source).resolve()
BIN = Path(os.environ.get("ZIRAN_BUILD_DIR", ZIRAN_ROOT / "build")) / "bin"
ZIRAN = Path(os.environ.get("ZIRAN_BIN", BIN / "ziran"))


def chromium_temp_directory():
    """A temporary directory short enough for Chromium's Unix socket paths,
    which must stay under 108 bytes whatever TMPDIR the caller has."""
    return tempfile.TemporaryDirectory(prefix="kr-", dir="/var/tmp" if os.path.isdir("/var/tmp") else "/tmp")


def fixture_identity():
    """The [toolchain] and [dependencies.kryon] tables of a fixture project:
    the exact Gitea Ziran the starter templates pin, which Kryon's checked
    packages lock against, and this checkout's revision of Kryon."""
    import tomllib
    toolchain = tomllib.loads((ROOT / "templates/app/ziran.toml").read_text())["toolchain"]
    revision = subprocess.check_output(["git", "rev-parse", "--verify", "HEAD"],
                                       cwd=ROOT, text=True).strip()
    return (f'[toolchain]\ngit = "{toolchain["git"]}"\nref = "{toolchain["ref"]}"\n'
            f'[dependencies.kryon]\ngit = "https://git.waozi.xyz/kryonlabs/kryon.git"\n'
            f'ref = "{revision}"\n')


def fixture_overrides(package_overrides=()):
    """A fixture's ziran.local.toml: this Kryon checkout and any packages under
    test. Ziran stays the pinned toolchain; a local toolchain override would
    not match the checked package metadata."""
    overrides = (("kryon", ROOT), *package_overrides)
    return "[overrides]\n" + "".join(
        name + " = " + json.dumps(str(path)) + "\n" for name, path in overrides)


def pinned(manifest):
    """MANIFEST with its [toolchain] and [dependencies.kryon] tables replaced
    by fixture_identity(), wherever the fixture declared them."""
    kept = []
    table = ""
    for line in manifest.splitlines():
        stripped = line.strip()
        if stripped.startswith("["):
            table = stripped
            if table in ("[toolchain]", "[dependencies.kryon]"):
                continue
        if table in ("[toolchain]", "[dependencies.kryon]") and stripped.split("=")[0].strip() in ("git", "ref"):
            continue
        kept.append(line)
    return "\n".join(kept).rstrip("\n") + "\n" + fixture_identity()


def kryon_only(overrides):
    """A fixture's ziran.local.toml text without its ziran line: checked
    packages lock only against the pinned toolchain, never a local one."""
    return "".join(line + "\n" for line in overrides.splitlines()
                   if line.replace(" ", "").split("=")[0] != "ziran")


def write_linked_value_app(project, title, library, symbol):
    """Build the same input-driven frame around each native linker fixture."""
    source = project / "src"
    (source / "linked_value.zi").write_text(
        f'native :: #system_library "{library}";\n'
        f'NativeValue :: () -> s32 #foreign native "{symbol}";\n'
        'ReadValue :: () -> s32 { return NativeValue() }\n'
    )
    (source / "app.tsx").write_text('''import * as App from "kryon/app";
import { Session } from "kryon/session";
import { TypedCodepointTake } from "kryon/tree_input";
import { Text } from "kryon/tsx/text";
import { WindowQuit } from "kryon/window";
import { ReadValue } from "linked_value";

let exitCode: s32 = s32(0);

function Frame(frame: App.Frame): void {
    const session: Session = {slot: frame.slot, epoch: frame.epoch};
    <Text session={session} props={{text: "Press q to read the linked value", bounds: frame.viewport}}/>;
    if (TypedCodepointTake(session) === s32(113)) { exitCode = ReadValue(); }
    if (exitCode !== s32(0)) { WindowQuit(); }
}

export function main(): s32 {
    App.Run({title: TITLE}, Frame);
    return exitCode;
}
'''.replace('TITLE', json.dumps(title)))


def kryon_test_project(directory, package_overrides=()):
    """Resolve an ordinary consumer fixture against the canonical local packages."""
    directory = Path(directory)
    (directory / "src").mkdir(parents=True, exist_ok=True)
    (directory / "ziran.toml").write_text(
        '[package]\nname = "kryon_test_fixture"\nmodule_roots = ["src"]\n' + fixture_identity())
    (directory / "ziran.local.toml").write_text(fixture_overrides(package_overrides))
    subprocess.run([str(ZIRAN), "lock", "--offline"], cwd=directory, check=True)
    packages = json.loads((directory / "ziran.lock").read_text())["packages"]
    return next(package["id"] for package in packages if package["name"] == "kryon")
