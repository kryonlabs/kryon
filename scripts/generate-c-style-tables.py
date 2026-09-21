#!/usr/bin/env python3
"""Generate C StyleRule tables from authored KSS sources.

The script compiles a temporary native generator against Kryon's shared KSS
parser. The generated C file is a build artifact: it contains typed StyleRule
arrays and a registration function, but no parser or KSS source text.
"""

from __future__ import annotations

import argparse
import os
import shlex
import subprocess
import textwrap
from pathlib import Path

DEFAULT_PACKS = [
    ("material", "Material", "Clean Material-like controls with flat paint", "styles/kryon/material.kss", "", ""),
    ("classic", "Classic", "Dense toolkit controls for desktop utilities", "styles/kryon/classic.kss", "", ""),
    ("lightfield", "Lightfield", "Premium translucent controls with glow-capable treatment", "styles/kryon/lightfield.kss", "", ""),
]


def c_string(value: str) -> str:
    out = ['"']
    for ch in value:
        code = ord(ch)
        if ch == '\\':
            out.append('\\\\')
        elif ch == '"':
            out.append('\\"')
        elif ch == '\n':
            out.append('\\n')
        elif ch == '\r':
            out.append('\\r')
        elif ch == '\t':
            out.append('\\t')
        elif code < 32 or code >= 127:
            out.append(f'\\x{code:02x}')
        else:
            out.append(ch)
    out.append('"')
    return ''.join(out)


def parse_assignment(value: str, kind: str) -> tuple[str, str]:
    if '=' not in value:
        raise SystemExit(f"--{kind} expects name=path")
    name, path = value.split('=', 1)
    if not name or not path:
        raise SystemExit(f"--{kind} expects non-empty name=path")
    return name, path


def parse_pack(value: str) -> tuple[str, str, str, str, str, str]:
    parts = value.split('|')
    if len(parts) not in (4, 6):
        raise SystemExit(
            "--pack expects id|label|description|path or "
            "id|label|description|path|variant|theme"
        )
    if any(part == '' for part in parts[:4]):
        raise SystemExit("--pack id, label, description, and path must be non-empty")
    if len(parts) == 4:
        parts.extend(['', ''])
    return tuple(parts)  # type: ignore[return-value]


def write_helper(path: Path, root: Path, modules: list[tuple[str, str]], packs: list[tuple[str, str, str, str, str, str]], function_name: str, active_pack: str) -> None:
    module_entries = []
    for module_id, module_path in modules:
        source_path = (root / module_path).resolve()
        module_entries.append(f"    {{{c_string(module_id)}, {c_string(str(source_path))}}},")
    pack_entries = []
    for pack_id, label, description, source, variant, theme in packs:
        source_path = (root / source).resolve()
        pack_entries.append(
            "    {"
            f"{c_string(pack_id)}, {c_string(label)}, {c_string(description)}, "
            f"{c_string(str(source_path))}, {c_string(variant)}, {c_string(theme)}"
            "},"
        )

    helper = rf"""
#include "ui_style_pack_props.generated.h"
#include "ui/kss_parser.h"

#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define RULE_CAPACITY 1024

typedef struct ModuleSpec {{
    const char *id;
    const char *path;
}} ModuleSpec;

typedef struct PackSpec {{
    const char *id;
    const char *label;
    const char *description;
    const char *path;
    const char *variant;
    const char *theme;
}} PackSpec;

static const ModuleSpec modules[] = {{
{textwrap.indent(chr(10).join(module_entries), '')}
    {{NULL, NULL}},
}};

static const PackSpec packs[] = {{
{textwrap.indent(chr(10).join(pack_entries), '')}
}};

static char *
read_file(const char *path)
{{
    FILE *file = fopen(path, "rb");
    long size;
    char *text;

    if(file == NULL) {{
        fprintf(stderr, "%s: %s\n", path, strerror(errno));
        exit(1);
    }}
    if(fseek(file, 0, SEEK_END) != 0) {{
        fprintf(stderr, "%s: seek failed\n", path);
        exit(1);
    }}
    size = ftell(file);
    if(size < 0) {{
        fprintf(stderr, "%s: size failed\n", path);
        exit(1);
    }}
    if(fseek(file, 0, SEEK_SET) != 0) {{
        fprintf(stderr, "%s: seek failed\n", path);
        exit(1);
    }}
    text = (char *)malloc((size_t)size + 1);
    if(text == NULL) {{
        fprintf(stderr, "%s: allocation failed\n", path);
        exit(1);
    }}
    if(fread(text, 1, (size_t)size, file) != (size_t)size) {{
        fprintf(stderr, "%s: read failed\n", path);
        exit(1);
    }}
    text[size] = '\0';
    fclose(file);
    return text;
}}

static void
emit_c_string(FILE *out, const char *text, size_t length)
{{
    fputc('"', out);
    for(size_t i = 0; i < length; i++) {{
        unsigned char ch = (unsigned char)text[i];
        if(ch == '\\')
            fputs("\\\\", out);
        else if(ch == '"')
            fputs("\\\"", out);
        else if(ch == '\n')
            fputs("\\n", out);
        else if(ch == '\r')
            fputs("\\r", out);
        else if(ch == '\t')
            fputs("\\t", out);
        else if(ch < 32 || ch >= 127)
            fprintf(out, "\\x%02x", ch);
        else
            fputc(ch, out);
    }}
    fputc('"', out);
}}

static void
emit_identifier(FILE *out, const char *prefix, const char *id, const char *suffix)
{{
    fputs(prefix, out);
    for(const char *p = id; *p != '\0'; p++) {{
        unsigned char ch = (unsigned char)*p;
        if((ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') ||
           (ch >= '0' && ch <= '9'))
            fputc(ch, out);
        else
            fputc('_', out);
    }}
    fputs(suffix, out);
}}

static void
emit_string_value(FILE *out, String value)
{{
    if(value.data == NULL || value.length == 0) {{
        fputs("{{ NULL, 0 }}", out);
        return;
    }}
    fputs("{{ ", out);
    emit_c_string(out, value.data, value.length);
    fprintf(out, ", %zu }}", value.length);
}}

static void
emit_selector(FILE *out, StyleSelector selector)
{{
    fprintf(out,
        "{{ .kind = %d, .name = %d, .class_name = %d, .role = %d, "
        ".tone = %d, .emphasis = %d, .size = %d, .state = %d, "
        ".validation = %d, .orientation = %d, .placement = %d }}",
        selector.kind, selector.name, selector.class_name, selector.role,
        selector.tone, selector.emphasis, selector.size, selector.state,
        selector.validation, selector.orientation, selector.placement);
}}

static void
emit_style(FILE *out, StyleData style)
{{
    fprintf(out,
        "{{ .fields = 0x%08xU, .background = 0x%08xU, "
        ".foreground = 0x%08xU, .border = 0x%08xU, .focus = 0x%08xU, "
        ".radius = %.9ef, .border_width = %.9ef, .opacity = %.9ef, "
        ".padding_x = %.9ef, .padding_y = %.9ef, .gap = %.9ef, "
        ".font_size = %.9ef, .icon_size = %.9ef, .offset_x = %.9ef, "
        ".offset_y = %.9ef, .background_end = 0x%08xU, "
        ".material = (MaterialKind)%d, .typeface = ",
        style.fields, style.background, style.foreground, style.border,
        style.focus, style.radius, style.border_width, style.opacity,
        style.padding_x, style.padding_y, style.gap, style.font_size,
        style.icon_size, style.offset_x, style.offset_y, style.background_end,
        (int)style.material);
    emit_string_value(out, style.typeface);
    fprintf(out, ", .letter_spacing = %.9ef }}", style.letter_spacing);
}}

static void
emit_pack(FILE *out, const PackSpec *pack, const StyleRule *rules, int rule_count,
          int index)
{{
    fprintf(out, "static const StyleRule ");
    emit_identifier(out, "kryon_compiled_style_", pack->id, "_rules");
    fputs("[] = {{\n", out);
    for(int i = 0; i < rule_count; i++) {{
        fputs("    {{ .selector = ", out);
        emit_selector(out, rules[i].selector);
        fprintf(out, ", .state = %d, .layer = %d, .order = %d, .style = ",
                rules[i].state, rules[i].layer, rules[i].order);
        emit_style(out, rules[i].style);
        fputs(" }},\n", out);
    }}
    fputs("}};\n\n", out);
    fprintf(out, "static const StyleSheet kryon_compiled_style_sheet_%d = {{ .rules = ", index);
    emit_identifier(out, "kryon_compiled_style_", pack->id, "_rules");
    fprintf(out, ", .rule_count = %d }};\n\n", rule_count);
}}

int
main(int argc, char **argv)
{{
    FILE *out;
    StyleRule rules[sizeof(packs) / sizeof(packs[0])][RULE_CAPACITY];
    int rule_counts[sizeof(packs) / sizeof(packs[0])];

    if(argc != 2) {{
        fprintf(stderr, "usage: %s output.c\n", argv[0]);
        return 2;
    }}

    for(size_t i = 0; modules[i].id != NULL; i++) {{
        char *source = read_file(modules[i].path);
        if(!RegisterStyleModule(modules[i].id, source)) {{
            fprintf(stderr, "%s: failed to register style module %s\n",
                    modules[i].path, modules[i].id);
            return 1;
        }}
        free(source);
    }}

    for(size_t i = 0; i < sizeof(packs) / sizeof(packs[0]); i++) {{
        KssParseResult result = {{0}};
        char diagnostic[256];
        char *source = read_file(packs[i].path);
        memset(rules[i], 0, sizeof(rules[i]));
        if(!kss_parse_with_environment(source, packs[i].variant, packs[i].theme,
                                       rules[i], RULE_CAPACITY, &result,
                                       diagnostic, sizeof(diagnostic))) {{
            fprintf(stderr, "%s: %s\n", packs[i].path, diagnostic);
            return 1;
        }}
        if(result.rule_count <= 0) {{
            fprintf(stderr, "%s: no style rules emitted\n", packs[i].path);
            return 1;
        }}
        if(strcmp(result.pack_id, packs[i].id) != 0 &&
           packs[i].variant[0] == '\0' && packs[i].theme[0] == '\0') {{
            fprintf(stderr, "%s: pack id %s did not match expected %s\n",
                    packs[i].path, result.pack_id, packs[i].id);
            return 1;
        }}
        rule_counts[i] = result.rule_count;
        free(source);
    }}

    out = fopen(argv[1], "wb");
    if(out == NULL) {{
        fprintf(stderr, "%s: %s\n", argv[1], strerror(errno));
        return 1;
    }}
    fputs("/* Generated by scripts/generate-c-style-tables.py. DO NOT EDIT. */\n", out);
    fputs("#include \"ui_style_pack_props.generated.h\"\n\n", out);
    for(size_t i = 0; i < sizeof(packs) / sizeof(packs[0]); i++)
        emit_pack(out, &packs[i], rules[i], rule_counts[i], (int)i);

    fprintf(out, "bool %s(void)\n{{\n", {c_string(function_name)});
    for(size_t i = 0; i < sizeof(packs) / sizeof(packs[0]); i++) {{
        fprintf(out, "    if(!RegisterStylePack((StylePack){{ .id = ");
        emit_c_string(out, packs[i].id, strlen(packs[i].id));
        fputs(", .label = ", out);
        emit_c_string(out, packs[i].label, strlen(packs[i].label));
        fputs(", .description = ", out);
        emit_c_string(out, packs[i].description, strlen(packs[i].description));
        fprintf(out, ", .sheet = &kryon_compiled_style_sheet_%zu }}))\n        return false;\n", i);
    }}
    fputs("    return SetActiveStylePack(", out);
    emit_c_string(out, {c_string(active_pack)}, strlen({c_string(active_pack)}));
    fputs(");\n}}\n", out);
    fclose(out);
    ClearStyleModules();
    return 0;
}}
"""
    path.write_text(helper)


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument('--root', default='.', help='repository root')
    parser.add_argument('--build-dir', default='build/linux-x86_64')
    parser.add_argument('--generated-src-dir', default=None)
    parser.add_argument('--cc', default=os.environ.get('CC', 'cc'))
    parser.add_argument('--output', required=True)
    parser.add_argument('--function-name', default='RegisterCompiledBuiltInStylePacks')
    parser.add_argument('--active-pack', default='material')
    parser.add_argument('--module', action='append', default=[], help='module-id=path')
    parser.add_argument('--pack', action='append', default=[], help='id|label|description|path[|variant|theme]')
    args = parser.parse_args()

    root = Path(args.root).resolve()
    build_dir = Path(args.build_dir)
    if not build_dir.is_absolute():
        build_dir = root / build_dir
    generated_src = Path(args.generated_src_dir) if args.generated_src_dir else build_dir / 'generated' / 'src'
    if not generated_src.is_absolute():
        generated_src = root / generated_src
    output = Path(args.output)
    if not output.is_absolute():
        output = root / output
    output.parent.mkdir(parents=True, exist_ok=True)
    build_dir.mkdir(parents=True, exist_ok=True)

    modules = [parse_assignment(value, 'module') for value in args.module]
    packs = [parse_pack(value) for value in args.pack] if args.pack else DEFAULT_PACKS

    helper_c = build_dir / 'style_table_generator.c'
    helper_bin = build_dir / 'style_table_generator'
    write_helper(helper_c, root, modules, packs, args.function_name, args.active_pack)

    cc_cmd = shlex.split(args.cc) + [
        '-std=c99', '-Wall', '-Werror',
        '-Iinclude', f'-I{generated_src}', '-Isrc',
        str(helper_c),
        str(generated_src / 'ui' / 'kss_parser.c'),
        str(generated_src / 'runtime' / 'kss_parser.c'),
        str(generated_src / 'runtime' / 'kss_formatter.c'),
        str(generated_src / 'ui' / 'style_sheet.c'),
        str(generated_src / 'runtime' / 'style_sheet.c'),
        str(generated_src / 'runtime' / 'style.c'),
        str(generated_src / 'runtime' / 'surface.c'),
        '-lm', '-o', str(helper_bin),
    ]
    subprocess.run(cc_cmd, cwd=root, check=True)
    subprocess.run([str(helper_bin), str(output)], cwd=root, check=True)


if __name__ == '__main__':
    main()
