#!/usr/bin/env python3
"""Generate the mixed native source graph and its CMake source manifest.

Resolution follows the project, not flat module paths: `ziran ir --project`
namespaces every package's modules, so an application module may share a
name with a Kryon module. Run through scripts/android-build.sh for
`kryon build --profile android`; concurrent ABIs serialize through one lock.
"""
import argparse
import fcntl
import json
import os
from pathlib import Path
import shutil
import shlex
import subprocess
import tomllib
from module_path_inputs import SOURCE_SUFFIXES, module_path_roots, module_path_sources


def android_native_recipes(root, modules, native_modules, ziran):
    """Resolve recipes for native-bearing modules that emitted code."""
    emitted = {path.stem for path in native_modules}
    package_roots = {}
    reached = {}
    inputs = {root / "ziran.toml", root / "ziran.lock"}
    local = root / "ziran.local.toml"
    if local.is_file():
        inputs.add(local)
    with (root / "build/.ziran/module-map.tsv").open() as module_map:
        for line in module_map:
            kind, identity, *fields = line.rstrip("\n").split("\t")
            if kind == "P":
                package_roots.setdefault(identity, Path(fields[0]))
            elif kind == "M":
                module, source = fields
                stem = (module if identity == "root" else f"std_{module}"
                        if identity == "std" else f"{identity}__{module}")
                # Folded/inlined modules still affect the emitted program even
                # when they have no remaining IR or C output of their own.
                path = Path(source)
                inputs.add(path)
                # Generated roots can be outside a dependency's module roots.
                if stem in emitted and path.is_relative_to(package_roots[identity]):
                    reached.setdefault(identity, set()).add(
                        path.relative_to(package_roots[identity]).as_posix())

    lock = json.loads((root / "ziran.lock").read_text())
    # Resolve exact lock identities through the package command. A direct
    # development override can replace its locked identity, so select that
    # edge by its declared alias. Transitive names may repeat: keep their IDs.
    locked = {item["id"]: item for item in lock["packages"]}
    overrides = {}
    if local.is_file():
        with local.open("rb") as source:
            overrides = tomllib.load(source).get("overrides", {})
    overridden = {name.casefold() for name in overrides}
    aliases = {identity: alias for alias, identity in lock["root"]["dependencies"].items()
               if alias.casefold() in overridden}
    selectors = [aliases.get(identity, identity) for identity in locked]
    resolved = json.loads(subprocess.check_output(
        [str(ziran), "pkg", "paths", *selectors], cwd=root, text=True)) if locked else []
    paths = {identity: Path(item["path"]).resolve()
             for identity, item in zip(locked, resolved, strict=True)}
    namespaces = {path.resolve(): name for name, path in package_roots.items()}
    packages = {namespaces[paths[identity]]: package
                for identity, package in locked.items() if paths[identity] in namespaces}
    packages["root"] = lock["root"]
    recipes = []
    for identity, used_modules in sorted(reached.items()):
        if identity not in packages:
            continue  # The toolchain's std modules have no package manifest.
        package = package_roots[identity]
        path = package / "ziran.toml"
        inputs.add(path)
        with path.open("rb") as manifest:
            native = tomllib.load(manifest).get("tool", {}).get("android", {}).get("native", {})
        requirements = native.get("modules", {})
        selected = used_modules & requirements.keys()
        if not selected:
            continue
        aliases = {alias for module in selected for alias in requirements[module]}
        sources = {}
        for alias in sorted(aliases):
            dependency = packages[identity]["dependencies"].get(alias)
            if dependency not in locked or not locked[dependency].get("source"):
                raise SystemExit(f"{path}: Android native source {alias!r} must be a locked source dependency")
            sources[alias] = paths[dependency]
        recipe = (package / native["cmake"]).resolve()
        if not recipe.is_relative_to(package) or not recipe.is_file():
            raise SystemExit(f"{path}: Android native recipe is missing or outside the package: {recipe}")
        recipes.append((identity, recipe, sources))
    return recipes, sorted(inputs)


def cmake_path(path):
    """A literal CMake argument, including paths with metacharacters."""
    value = str(path)
    equals = "="
    while f"]{equals}]" in value:
        equals += "="
    return f"[{equals}[{value}]{equals}]"

parser = argparse.ArgumentParser()
parser.add_argument("--ziran", type=Path, required=True)
parser.add_argument("--zi2c", type=Path, required=True)
parser.add_argument("--root", type=Path, required=True)
parser.add_argument("--kryon", type=Path, required=True)
parser.add_argument("--ir", type=Path, required=True)
parser.add_argument("--output", type=Path, required=True)
parser.add_argument("--manifest", type=Path, required=True)
parser.add_argument("--host", type=Path, help="Explicit NativeActivity host source for a platform test")
parser.add_argument("--retain", action="append", default=[],
                    help="Additional externally invoked module:procedure root")
parser.add_argument("--entry", type=Path, default=os.environ.get("KRYON_PROJECT_ENTRY", "src/app.tsx"),
                    help="Selected application source (or KRYON_PROJECT_ENTRY)")
parser.add_argument("--define", action="append", default=[])
parser.add_argument("--codegen", choices=("c99", "zx"), default="c99")
parser.add_argument("--module-path-args-json",
                    help="JSON-encoded search arguments from Gradle/CMake; otherwise read KRYON_MODULE_PATH_ARGS")
args = parser.parse_args()
# Direct checks inherit the CLI's quoted environment value. Gradle carries the
# same text as one JSON-encoded CMake argument, so changed paths invalidate a
# cached configure and embedded newlines remain intact in CMakeCache.txt.
module_path_text = os.environ.get("KRYON_MODULE_PATH_ARGS", "")
if args.module_path_args_json:
    try:
        module_path_text = json.loads(args.module_path_args_json)
    except json.JSONDecodeError as error:
        parser.error(f"invalid module-path arguments: {error}")
    if not isinstance(module_path_text, str):
        parser.error("module-path arguments must encode a string")
try:
    module_path_args = shlex.split(module_path_text)
except ValueError as error:
    parser.error(f"invalid module-path arguments: {error}")
root = args.root.resolve()
try:
    module_roots = module_path_roots(module_path_args, root)
except ValueError as error:
    parser.error(str(error))
kryon = args.kryon.resolve()
entry = (root / args.entry).resolve()
source_suffixes = {".zi", ".ts", ".tsx"}
if not entry.is_file() or entry.suffix not in source_suffixes:
    parser.error(f"selected entry must be a native .tsx, .ts or .zi source: {entry}")
needs_program = args.host is None and args.codegen == "c99"
entry_module = entry.parent.name if entry.stem == "module" else entry.stem
sources = sorted(path for path in (root / "src").rglob("*")
                 if path.is_file() and path.suffix in source_suffixes)
if list((root / "src").rglob("*.c")) or list((root / "src").rglob("*.kry")):
    raise SystemExit("Android application sources must use native TSX, TypeScript or Ziran")
if not sources:
    raise SystemExit("No native .tsx, .ts or .zi sources under src/")
host_roots = [
    args.host.resolve() if args.host else kryon / "src/backend/android_run.zi",
    kryon / "src/backend/android_fopen.zi",
    kryon / "src/backend/android_notifications.zi",
    root / "src/main.zi",
    root / "src/android_glue.zi",
]
bindings = []
if needs_program:
    host_roots.append(entry)
    bindings = ["--bind",
                f"kryon/android_run:AndroidProgramMain={entry_module}:main"]
for path in host_roots:
    if not path.is_file():
        raise SystemExit(f"Missing host source {path}; run scripts/android_scaffold.py")

# The loader, NDK and C linker's --wrap option reach these symbols without
# a checked call from main. Retain their real declarations directly.
retained = [*args.retain, "android_glue:JNI_OnLoad",
            "kryon/android_fopen:__wrap_fopen",
            "kryon/android_fopen:__wrap_fopen64",
            "kryon/android_notifications:AndroidNotificationReceive",
            "kryon/android_notifications:AndroidNotificationRequestPermission"]
if args.codegen == "zx":
    host_roots.append(kryon / "src/backend/android_bundle_run.zi")
    retained.append("kryon/android_bundle_run:android_main")
elif args.host and args.host.resolve() == (kryon / "src/backend/android_runtime.zi").resolve():
    retained.append("kryon/android_runtime:android_main")

defines = ["--define", "KRYON_ANDROID_HOST"]
# The project tool supplies Ziran's validated settings. Direct Gradle runs
# read the same manifest and local overrides without starting the tool.
android_options = {}
tool_options = os.environ.get("ZIRAN_TOOL_OPTIONS")
if tool_options:
    with Path(tool_options).open() as options:
        for line in options:
            key, _, value = line.rstrip("\n").partition("=")
            if key.startswith("android."):
                android_options[key.removeprefix("android.")] = value
else:
    for manifest in (root / "ziran.toml", root / "ziran.local.toml"):
        if manifest.is_file():
            with manifest.open("rb") as source:
                android_options.update(tomllib.load(source).get("tool", {}).get("kryon", {}).get("android", {}))
bundled_fonts = android_options.get("bundled_fonts", False)
if not isinstance(bundled_fonts, bool) and bundled_fonts not in ("true", "false"):
    parser.error("[tool.kryon.android] bundled_fonts must be a boolean")
if bundled_fonts is True or bundled_fonts == "true":
    defines.extend(["--define", "KRYON_ANDROID_BUNDLED_FONTS"])
frame_mode = android_options.get("frame_mode", "adaptive")
if frame_mode not in ("adaptive", "events"):
    parser.error("[tool.kryon.android] frame_mode must be 'adaptive' or 'events'")
if frame_mode == "events" and needs_program:
    defines.extend(["--define", "KRYON_ANDROID_EVENT_FRAMES"])
if args.codegen == "zx":
    defines.extend(["--define", "KRYON_ZX_APP", "--define", "BUNDLE_NATIVE_RENDERER"])
for name in args.define:
    defines.extend(["--define", name])

# Ziran target conditions are independent of the C compiler's preprocessor
# defines. Keep the target attached to both source-to-IR and IR-to-C so Android
# modules never retain a host-Linux branch merely because generation runs on a
# Linux workstation.
cpu_defines = {
    "__aarch64__": "arm64",
    "__x86_64__": "x64",
    "__arm__": "none",
    "__i386__": "none",
}
target_cpus = {cpu_defines[name] for name in args.define if name in cpu_defines}
if len(target_cpus) != 1:
    parser.error("select exactly one Android CPU define (__aarch64__, __arm__, __x86_64__ or __i386__)")
target = ["--target-os", "android", "--target-cpu", target_cpus.pop()]

if args.ir.exists():
    shutil.rmtree(args.ir)
if args.output.exists():
    shutil.rmtree(args.output)
args.ir.mkdir(parents=True)
args.output.mkdir(parents=True)

lock_path = root / "build" / "kryon-android-compiler.lock"
lock_path.parent.mkdir(parents=True, exist_ok=True)
with lock_path.open("w") as lock:
    fcntl.flock(lock, fcntl.LOCK_EX)
    subprocess.run(
        [str(args.ziran), "ir", "--project", *module_path_args, "--target=c",
         "--entry", "main:main",
         *(word for name in retained for word in ("--retain", name)),
         *target, "--root", str(entry.parent),
         *bindings, *defines, "-o", str(args.ir),
         *map(str, dict.fromkeys(host_roots))],
        cwd=root, check=True,
    )
    modules = sorted(args.ir.glob("*.zir"))
    subprocess.run(
        [str(args.zi2c), "--no-main", *target, *defines, "--root", str(args.ir),
         "-o", str(args.output), *map(str, modules)],
        cwd=root, check=True,
    )
    outputs = sorted(args.output.rglob("*.c"))
    native_recipes, configure_inputs = android_native_recipes(root, modules, outputs, args.ziran)

if not outputs:
    raise SystemExit("Native compiler emitted no Android C sources")
if not any(path.name == "main.c" for path in outputs):
    raise SystemExit("The Android host needs the scaffold's src/main.zi entry")
headers = sorted(args.output.rglob("*.h"))
include_dirs = sorted({args.output, *(p.parent for p in headers)})
lines = ["# Generated from the complete checked native import graph.",
         "set(APP_ZIRAN_GENERATED_SOURCES"]
lines.extend(f'    "{path}"' for path in outputs)
lines.extend([")", "set(APP_ZIRAN_GENERATED_HEADERS"])
lines.extend(f'    "{path}"' for path in headers)
lines.extend([")", "set(APP_ZIRAN_INCLUDE_DIRS"])
lines.extend(f'    "{path}"' for path in include_dirs)
lines.extend([")", "set(APP_ZIRAN_CONFIGURE_INPUTS"])
configure_inputs = sorted({*configure_inputs, *module_path_sources(module_roots),
                          kryon / "scripts/module_path_inputs.py"})
# Keep semicolons escaped while these arguments travel through a CMake list.
for path in configure_inputs:
    escaped = str(path).replace(";", "\\;")
    lines.append(f"    {cmake_path(escaped)}")
lines.append(")")
if module_roots:
    # CONFIGURE_DEPENDS also notices newly added and removed source files.
    lines.append("file(GLOB APP_ZIRAN_MODULE_PATH_SOURCES CONFIGURE_DEPENDS")
    for directory in module_roots:
        prefix = "".join({"[": "[[]", "]": "[]]", "*": "[*]", "?": "[?]"}.get(c, c)
                         for c in str(directory))
        lines.extend(f"    {cmake_path(prefix + '/' + pattern + suffix)}"
                     for pattern in ("*", "*/*") for suffix in SOURCE_SUFFIXES)
    lines.append(")")
if any(module.stem == "std_bundle_host" for module in modules):
    lines.append("set(APP_ZIRAN_BUNDLE_RUNTIME ON)")
for identity, recipe, dependencies in native_recipes:
    lines.append(f"function(_app_ziran_native_{identity} ZIRAN_ANDROID_TARGET)")
    lines.extend(f"    set(ZIRAN_SOURCE_{name} {cmake_path(path)})"
                 for name, path in dependencies.items())
    lines.extend([f"    include({cmake_path(recipe)})", "endfunction()"])
lines.append("function(app_ziran_link_native target)")
lines.extend(f'    _app_ziran_native_{identity}("${{target}}")'
             for identity, _, _ in native_recipes)
lines.append("endfunction()\n")
content = "\n".join(lines)
args.manifest.parent.mkdir(parents=True, exist_ok=True)
if not args.manifest.exists() or args.manifest.read_text() != content:
    args.manifest.write_text(content)
print(f"Generated {len(outputs)} Android C outputs from maintained native sources")
