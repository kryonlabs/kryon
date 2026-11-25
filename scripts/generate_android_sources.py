#!/usr/bin/env python3
"""Generate the complete checked Zi graph and its CMake source manifest.

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
import subprocess
import tomllib


def android_native_recipes(root, modules, native_modules):
    """Resolve recipes for native-bearing modules that emitted code."""
    retained = {path.stem for path in modules}
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
                stem = module if identity == "root" else f"{identity}_{module}"
                if stem in retained:
                    path = Path(source)
                    inputs.add(path)
                    # Generated roots can be outside a dependency's module roots.
                    if stem in emitted and path.is_relative_to(package_roots[identity]):
                        reached.setdefault(identity, set()).add(
                            path.relative_to(package_roots[identity]).as_posix())

    lock = json.loads((root / "ziran.lock").read_text())
    packages = {item["id"]: item for item in lock["packages"]}
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
            if dependency not in packages or not packages[dependency].get("source"):
                raise SystemExit(f"{path}: Android native source {alias!r} must be a locked source dependency")
            sources[alias] = package_roots[dependency]
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
parser.add_argument("--entry", type=Path, default=os.environ.get("KRYON_PROJECT_ENTRY", "src/app.zi"),
                    help="Selected application source (or KRYON_PROJECT_ENTRY)")
parser.add_argument("--package-id", default=os.environ.get("ZIRAN_PACKAGE_ID"),
                    help="Resolved Kryon identity (or ZIRAN_PACKAGE_ID from ziran tool kryon)")
parser.add_argument("--define", action="append", default=[])
parser.add_argument("--codegen", choices=("c99", "zx"), default="c99")
args = parser.parse_args()
root = args.root.resolve()
kryon = args.kryon.resolve()
entry = (root / args.entry).resolve()
if not entry.is_file() or entry.suffix != ".zi":
    parser.error(f"selected application entry is not a Zi source: {entry}")
needs_program = args.host is None and args.codegen == "c99"
if needs_program and not args.package_id:
    parser.error("--package-id is required unless ZIRAN_PACKAGE_ID is supplied by ziran tool kryon")
entry_module = entry.parent.name if entry.stem == "module" else entry.stem
sources = sorted((root / "src").rglob("*.zi"))
if list((root / "src").rglob("*.c")) or list((root / "src").rglob("*.kry")):
    raise SystemExit("Android application implementation must be current Zi")
if not sources:
    raise SystemExit("No Zi sources under src/")
host_roots = [
    args.host.resolve() if args.host else kryon / "src/backend/android_run.zi",
    kryon / "src/backend/android_fopen.zi",
    root / "src/main.zi",
    root / "src/android_glue.zi",
]
bindings = []
if needs_program:
    host_roots.append(entry)
    bindings = ["--bind",
                f"{args.package_id}_android_run:AndroidProgramMain={entry_module}:main"]
for path in host_roots:
    if not path.is_file():
        raise SystemExit(f"Missing host source {path}; run scripts/android_scaffold.py")

# `ziran ir --entry` keeps only what its one entry reaches; a native C graph
# has no other root, and `#program_export` alone does not retain a function.
# Android calls the glue's JNI_OnLoad and the notification receiver callbacks
# by name through cmake/libmain.map.txt. The Android app also links the fopen
# wrappers and the NDK calls android_main. This generated entry keeps those
# functions and forwards a valid call straight to the
# application's stable `main(argc, argv)` and names the others only on the
# branch a C argc (never negative) cannot take, so it has no glue side effect.
# Kryon's entries are host capabilities bound to its exports, which keeps a
# called provider. The root is left out of the C inputs, so the host still
# starts the generated C `main` and nothing else.
ROOTS_MODULE, ROOTS_ENTRY = "kryon_android_roots", "KryonAndroidRoots"
kryon_roots = [("__wrap_fopen", "(path: *u8, mode: *u8) -> *void", "(null, null)",
                "kryon/android_fopen"),
               ("__wrap_fopen64", "(path: *u8, mode: *u8) -> *void", "(null, null)",
                "kryon/android_fopen")]
if args.codegen == "zx":
    kryon_roots.append(("android_main", "(app: *void)", "(null)", "kryon/android_bundle_run"))
elif args.host and args.host.resolve() == (kryon / "src/backend/android_runtime.zi").resolve():
    # A custom application's main reaches the host queries; only the NDK
    # reaches android_main, which then calls that main. Keep it as a root.
    kryon_roots.append(("android_main", "(app: *void)", "(null)", "kryon/android_runtime"))
roots_source = args.ir.with_name(args.ir.name + "-roots") / (ROOTS_MODULE + ".zi")
roots_text = "\n".join([
    "// Generated by kryon scripts/generate_android_sources.py.",
    '#import "main"', '#import "android_glue"',
    'NotificationReceiver :: #import "kryon/android_notifications";',
    'host_api :: #system_library "host_api";',
    *(f"{name} :: {signature} #foreign host_api;" for name, signature, _, _ in kryon_roots),
    f"{ROOTS_ENTRY} :: (argc: s32, argv: **u8) -> s32 {{",
    "    if argc < 0 {",
    *(f"        unused {name}{call}" if "->" in signature else f"        {name}{call}"
      for name, signature, call, _ in kryon_roots),
    "        NotificationReceiver.AndroidNotificationReceive(null, null, null, null)",
    "        NotificationReceiver.AndroidNotificationRequestPermission(null, null, null)",
    "        return JNI_OnLoad(null, null)",
    "    }",
    "    return main(argc, argv)",
    "}\n"])
for name, _, _, provider in kryon_roots:
    bindings.extend(["--bind", f"{ROOTS_MODULE}:{name}={provider}:{name}"])

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
roots_source.parent.mkdir(parents=True, exist_ok=True)
roots_source.write_text(roots_text)

lock_path = root / "build" / "kryon-android-compiler.lock"
lock_path.parent.mkdir(parents=True, exist_ok=True)
with lock_path.open("w") as lock:
    fcntl.flock(lock, fcntl.LOCK_EX)
    subprocess.run(
        [str(args.ziran), "ir", "--project", "--entry", f"{ROOTS_MODULE}:{ROOTS_ENTRY}",
         *target, "--root", str(entry.parent), *bindings, *defines, "-o", str(args.ir),
         *map(str, host_roots), str(roots_source)],
        cwd=root, check=True,
    )
    # The root only selects what the IR keeps; it never enters native output.
    modules = sorted(path for path in args.ir.glob("*.zir") if path.stem != ROOTS_MODULE)
    subprocess.run(
        [str(args.zi2c), "--no-main", *target, *defines, "--root", str(args.ir),
         "-o", str(args.output), *map(str, modules)],
        cwd=root, check=True,
    )
    outputs = sorted(args.output.rglob("*.c"))
    native_recipes, configure_inputs = android_native_recipes(root, modules, outputs)

if not outputs:
    raise SystemExit("Zi compiler emitted no Android native sources")
if not any(path.name == "main.c" for path in outputs):
    raise SystemExit("The Android host needs the scaffold's src/main.zi entry")
headers = sorted(args.output.rglob("*.h"))
include_dirs = sorted({args.output, *(p.parent for p in headers)})
lines = ["# Generated from the complete Zi import graph.",
         "set(APP_ZIRAN_GENERATED_SOURCES"]
lines.extend(f'    "{path}"' for path in outputs)
lines.extend([")", "set(APP_ZIRAN_GENERATED_HEADERS"])
lines.extend(f'    "{path}"' for path in headers)
lines.extend([")", "set(APP_ZIRAN_INCLUDE_DIRS"])
lines.extend(f'    "{path}"' for path in include_dirs)
lines.extend([")", "set(APP_ZIRAN_CONFIGURE_INPUTS"])
lines.extend(f"    {cmake_path(path)}" for path in configure_inputs)
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
print(f"Generated {len(outputs)} Android C outputs from maintained Zi sources")
