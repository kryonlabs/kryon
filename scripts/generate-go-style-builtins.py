#!/usr/bin/env python3
import argparse
import sys
from pathlib import Path

PACKS = [
    ("material", "material", "Material",
     "Clean Material-like controls with flat paint", "styles/kryon/material.kss"),
    ("classic", "classic", "Classic",
     "Dense toolkit controls for desktop utilities", "styles/kryon/classic.kss"),
    ("lightfield", "lightfield", "Lightfield",
     "Premium translucent controls with glow-capable treatment", "styles/kryon/lightfield.kss"),
]


def generated_source() -> str:
    lines = ["package kryon", ""]
    for name, _, _, _, path in PACKS:
        source = Path(path).read_text()
        if "`" in source:
            raise SystemExit(f"{path}: backticks cannot be embedded in a Go raw string")
        lines.append(f"const {name}StyleSource = `{source}`")
        lines.append("")

    lines.extend([
        "func RegisterBuiltInStylePacks() bool {",
        "\tfor _, pack := range []struct {",
        "\t\tid, label, description, source string",
        "\t}{",
    ])
    for name, pack_id, label, description, _ in PACKS:
        lines.append(
            f"\t\t{{{pack_id!r}, {label!r}, {description!r}, {name}StyleSource}},"
            .replace("'", '"'))
    lines.extend([
        "\t} {",
        "\t\tif !RegisterStylePackSource(pack.source, pack.label, pack.description) {",
        "\t\t\treturn false",
        "\t\t}",
        "\t\tif FindStylePack(pack.id) == nil {",
        "\t\t\treturn false",
        "\t\t}",
        "\t}",
        '\treturn SetActiveStylePack("material")',
        "}",
        "",
    ])
    return "\n".join(lines)


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--check", action="store_true",
                        help="fail if go/kryon/style_builtins.go is stale")
    args = parser.parse_args()

    path = Path("go/kryon/style_builtins.go")
    source = generated_source()
    if args.check:
        if path.read_text() != source:
            print(f"{path}: stale; run make go-style-builtins", file=sys.stderr)
            raise SystemExit(1)
        return
    path.write_text(source)


if __name__ == "__main__":
    main()
