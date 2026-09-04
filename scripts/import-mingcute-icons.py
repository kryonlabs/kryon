#!/usr/bin/env python3
"""Import only Kryon's used MingCute Core Filled UI subset."""

import argparse
import copy
import json
from pathlib import Path
import subprocess
import tempfile
import xml.etree.ElementTree as ET


ROOT = Path(__file__).resolve().parents[1]
OUTPUT = ROOT / "icons"
CELL_SIZE = 64
CONTENT_SIZE = 48
COLUMNS = 8
SVG_NAMESPACE = "http://www.w3.org/2000/svg"

MAPPING = {
    "activity": "sparkles.svg",
    "amen": "pray.svg",
    "backward": "skip_previous.svg",
    "c": "code.svg",
    "calendar": "calendar_3.svg",
    "check": "check.svg",
    "edit": "edit_3.svg",
    "eye": "eye.svg",
    "eye_off": "eye_close.svg",
    "fingerprint": "fingerprint.svg",
    "forward": "skip_forward.svg",
    "gear": "settings_3.svg",
    "globe": "earth_3.svg",
    "home": "home_3.svg",
    "jupiter": "planet.svg",
    "left": "arrow_left.svg",
    "lightoff": "bulb.svg",
    "lighton": "bulb_2.svg",
    "link": "link_2.svg",
    "manual": "book_4.svg",
    "mars": "male.svg",
    "mercury": "earth_4.svg",
    "moon": "moon_stars.svg",
    "music": "music_3.svg",
    "mute": "volume_mute.svg",
    "pause": "pause.svg",
    "pencil": "pencil_2.svg",
    "pet": "paw.svg",
    "play": "play.svg",
    "plus": "add.svg",
    "profile": "user_3.svg",
    "return": "back_2.svg",
    "right": "arrow_right.svg",
    "rocket": "rocket_2.svg",
    "routine": "repeat.svg",
    "saturn": "space.svg",
    "save": "save_2.svg",
    "sound": "volume.svg",
    "sound0": "volume_mute.svg",
    "sound1": "volume_low.svg",
    "sound2": "volume_medium.svg",
    "sound3": "volume.svg",
    "stack": "layers.svg",
    "stat": "chart_bar.svg",
    "sun": "sun.svg",
    "text": "text.svg",
    "timeline": "timeline.svg",
    "todos": "task_2.svg",
    "trash": "delete_2.svg",
    "venus": "female.svg",
    "weekly": "calendar_week.svg",
    "workbook/clear_formatting": "eraser.svg",
    "workbook/fill_color": "paint.svg",
    "workbook/text_color": "text_color.svg",
    "wrench": "tool.svg",
    "x": "close.svg",
}

# MingCute ships matching mute and two-wave volume glyphs, but no discrete
# low/medium files. These two variants retain the exact filled speaker and
# inner-wave geometry from its Apache-2.0 volume.svg so all four slider states
# form one visually stable family.
DERIVED_VOLUME_PATHS = {
    "volume_low.svg": (
        "M13.2607 3.29867C13.9887 2.7787 14.9997 3.29872 15 4.1932"
        "V19.8045C15 20.6992 13.9888 21.22 13.2607 20.7L6.67969 15.9989"
        "H4C2.89543 15.9989 2 15.1034 2 13.9989V9.99886"
        "C2.00021 8.89447 2.89556 7.99886 4 7.99886H6.67969L13.2607 3.29867Z"
    ),
    "volume_medium.svg": (
        "M13.2607 3.29867C13.9887 2.7787 14.9997 3.29872 15 4.1932"
        "V19.8045C15 20.6992 13.9888 21.22 13.2607 20.7L6.67969 15.9989"
        "H4C2.89543 15.9989 2 15.1034 2 13.9989V9.99886"
        "C2.00021 8.89447 2.89556 7.99886 4 7.99886H6.67969L13.2607 3.29867Z"
        "M16.2549 9.09652C16.6232 8.68499 17.2555 8.65007 17.667 9.01839"
        "C18.4835 9.7493 18.9999 10.8144 19 11.9989"
        "C19 13.1836 18.4837 14.2493 17.667 14.9803"
        "C17.2555 15.3486 16.6232 15.3137 16.2549 14.9022"
        "C15.8866 14.4907 15.9215 13.8584 16.333 13.4901"
        "C16.7438 13.1224 17 12.5911 17 11.9989"
        "C16.9999 11.4068 16.7436 10.8762 16.333 10.5086"
        "C15.9215 10.1403 15.8867 9.50805 16.2549 9.09652Z"
    ),
}


def write_derived_volume_icons(directory: Path) -> dict[str, Path]:
    paths = {}
    for name, path_data in DERIVED_VOLUME_PATHS.items():
        path = directory / name
        root = ET.Element(f"{{{SVG_NAMESPACE}}}svg", {
            "width": "24",
            "height": "24",
            "viewBox": "0 0 24 24",
        })
        ET.SubElement(root, f"{{{SVG_NAMESPACE}}}path", {
            "d": path_data,
            "fill": "#10161F",
        })
        ET.ElementTree(root).write(path, encoding="utf-8", xml_declaration=True)
        paths[name] = path
    return paths


def run(*arguments: str) -> None:
    result = subprocess.run(arguments, capture_output=True, text=True)
    if result.returncode != 0:
        raise SystemExit(result.stderr or result.stdout)


def namespace_ids(root: ET.Element, prefix: str) -> None:
    replacements = {}
    for element in root.iter():
        identifier = element.get("id")
        if identifier:
            replacements[identifier] = f"{prefix}-{identifier}"
            element.set("id", replacements[identifier])
    for element in root.iter():
        for attribute, value in tuple(element.attrib.items()):
            for old, new in replacements.items():
                value = value.replace(f"url(#{old})", f"url(#{new})")
                if value == f"#{old}":
                    value = f"#{new}"
            element.set(attribute, value)


def render_sheet(paths: list[Path], destination: Path, columns: int,
                 temporary: Path) -> int:
    rows = (len(paths) + columns - 1) // columns
    width = columns * CELL_SIZE
    height = rows * CELL_SIZE
    ET.register_namespace("", SVG_NAMESPACE)
    root = ET.Element(f"{{{SVG_NAMESPACE}}}svg", {
        "width": str(width),
        "height": str(height),
        "viewBox": f"0 0 {width} {height}",
    })
    inset = (CELL_SIZE - CONTENT_SIZE) // 2
    for index, path in enumerate(paths):
        source = ET.parse(path).getroot()
        namespace_ids(source, f"icon-{index}")
        nested = ET.SubElement(root, f"{{{SVG_NAMESPACE}}}svg", {
            "x": str(index % columns * CELL_SIZE + inset),
            "y": str(index // columns * CELL_SIZE + inset),
            "width": str(CONTENT_SIZE),
            "height": str(CONTENT_SIZE),
            "viewBox": source.get("viewBox", "0 0 24 24"),
        })
        for child in source:
            nested.append(copy.deepcopy(child))

    svg_path = temporary / f"{destination.stem}.svg"
    raw_path = temporary / f"{destination.stem}-raw.png"
    ET.ElementTree(root).write(svg_path, encoding="utf-8", xml_declaration=True)
    run(
        "inkscape", str(svg_path), "--export-type=png",
        "--export-background-opacity=0", f"--export-width={width}",
        f"--export-height={height}", f"--export-filename={raw_path}",
    )
    run(
        "convert", str(raw_path), "-channel", "RGB", "-fill", "white",
        "-colorize", "100", "+channel", str(destination),
    )
    return rows


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("checkout", type=Path, help="local MingCute repository checkout")
    args = parser.parse_args()
    checkout = args.checkout.resolve()
    source_root = checkout / "assets" / "svg" / "core" / "filled"
    license_path = checkout / "LICENSE"

    if not source_root.is_dir():
        raise SystemExit(f"MingCute Core Filled directory not found: {source_root}")
    if not license_path.is_file() or "Apache License" not in license_path.read_text():
        raise SystemExit(f"MingCute Apache license not found: {license_path}")

    manifest_path = OUTPUT / "ui.json"
    manifest = json.loads(manifest_path.read_text())
    names = [icon["name"] for icon in manifest["icons"]]
    if set(names) != set(MAPPING):
        missing = sorted(set(names) - set(MAPPING))
        stale = sorted(set(MAPPING) - set(names))
        raise SystemExit(f"MingCute mapping mismatch; missing={missing}, stale={stale}")

    all_sources = sorted(
        source_root.rglob("*.svg"),
        key=lambda path: path.relative_to(source_root).as_posix(),
    )
    sources = {path.name: path for path in all_sources}
    if len(sources) != len(all_sources):
        raise SystemExit("MingCute Core Filled contains duplicate SVG filenames")
    unresolved = sorted(set(MAPPING.values()) - set(sources) - set(DERIVED_VOLUME_PATHS))
    if unresolved:
        raise SystemExit(f"MingCute SVGs not found: {unresolved}")

    revision_result = subprocess.run(
        ("git", "-C", str(checkout), "rev-parse", "HEAD"),
        capture_output=True, text=True,
    )
    revision = revision_result.stdout.strip() if revision_result.returncode == 0 else "unversioned"

    with tempfile.TemporaryDirectory(prefix="kryon-mingcute-") as temporary:
        temp = Path(temporary)
        sources.update(write_derived_volume_icons(temp))
        alias_paths = [sources[MAPPING[name]] for name in names]
        alias_rows = render_sheet(alias_paths, OUTPUT / "ui.png", COLUMNS, temp)

        for stale in (*OUTPUT.glob("mingcute-*.png"), *OUTPUT.glob("mingcute-*.json")):
            stale.unlink()

    for index, icon in enumerate(manifest["icons"]):
        icon.update({
            "index": index,
            "x": index % COLUMNS * CELL_SIZE,
            "y": index // COLUMNS * CELL_SIZE,
            "width": CELL_SIZE,
            "height": CELL_SIZE,
            "upstream": (f"derived/{MAPPING[icon['name']]}"
                         if MAPPING[icon["name"]] in DERIVED_VOLUME_PATHS
                         else f"core/filled/{MAPPING[icon['name']]}")
        })
    manifest.update({
        "style": "mingcute-core-filled",
        "license": "Apache-2.0",
        "upstream": "https://github.com/mingcute-design/mingcute-icons",
        "upstream_revision": revision,
        "content_size": CONTENT_SIZE,
        "columns": COLUMNS,
        "rows": alias_rows,
        "width": COLUMNS * CELL_SIZE,
        "height": alias_rows * CELL_SIZE,
        "tintable": True,
    })
    manifest_path.write_text(json.dumps(manifest, indent=2) + "\n")
    print(f"imported {len(names)} used MingCute Core Filled icons")


if __name__ == "__main__":
    main()
