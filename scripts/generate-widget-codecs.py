#!/usr/bin/env python3
"""Generate bounded codecs for records that cross the portable bundle boundary.

Each root struct, and every struct it contains, gets a codec record plus
WidgetEncodeT, WidgetDecodeT and WidgetValidT. Fields match by name (see
src/backend/hosted_values.zi), so a bundle and its host may be built against
different layouts. Slices hold at most 1024 elements; the decoded elements
stay in the codec, and the caller attaches them after decoding.

Run without arguments, it writes src/backend/widget_codec.zi for Kryon's own
hosted widgets; --check only verifies that file. An application generates
codecs for its own records with --uses src/backend/widget_codec.zi, so types
Kryon already encodes are reused instead of generated again.
"""
import argparse
from pathlib import Path
import re
import sys

SLICE_LIMIT = 1024

KRYON_ROOT = Path(__file__).resolve().parent.parent
KRYON_MODULES = ['session', 'geometry', 'drawing_props', 'text_props', 'image_props',
                 'button_props', 'control_props', 'text_input', 'text_input_props',
                 'scroll', 'scroll_props', 'semantic', 'text_align', 'checkbox_props',
                 'toggle_props', 'tab_bar_props', 'box_props']
KRYON_ROOTS = ['TextProps', 'ImageProps', 'ButtonProps', 'TextAreaProps', 'TextAreaResult',
               'ScrollProps', 'ScrollResult', 'Session', 'CheckboxProps', 'CheckboxValueResult',
               'ToggleProps', 'ToggleValueResult', 'TextFieldProps', 'TextFieldResult', 'Tab',
               'TabBarProps', 'TabBarResult', 'BoxProps']


def read_schema(paths):
    schema = {}
    for path in paths:
        for name, body in re.findall(r'(\w+)\s*::\s*struct\s*\{([^{}]*)\}', Path(path).read_text()):
            fields = []
            for field, kind in re.findall(r'^\s*(\w+)\s*:\s*([^\n;]+)', body, re.M):
                kind = re.sub(r'\s+', '', kind.split('//')[0])
                if '=' in kind:
                    sys.exit(f'{path}: {name}.{field} has a default value, which a codec cannot carry')
                fields.append((field, kind))
            schema[name] = fields
    return schema


def provided_types(paths):
    """Types an imported codec module already encodes."""
    names = set()
    for path in paths:
        names.update(re.findall(r'^Widget(\w+)Codec :: struct', Path(path).read_text(), re.M))
    return names


def order(schema, roots, provided):
    needed = []

    def discover(kind):
        if kind.startswith('[]'):
            discover(kind[2:])
            return
        if kind in schema and kind not in needed and kind not in provided:
            for _, field_kind in schema[kind]:
                discover(field_kind)
            needed.append(kind)

    for root in roots:
        if root not in schema and root not in provided:
            sys.exit(f'Unknown root record: {root}')
        discover(root)
    return needed


def scalar_kind(kind):
    if kind == 'string':
        return 'HostString'
    if kind.startswith('float'):
        return 'HostReal'
    if kind.startswith('u'):
        return 'HostUnsigned'
    return 'HostInteger'


def generate(schema, needed, provided, header, imports):
    records = set(needed) | provided
    lines = [header, *(f'#import {line}' for line in imports), 'using HostValueKind;', '']
    for kind in needed:
        fields = schema[kind]
        lines += [f'Widget{kind}Codec :: struct {{', f'    fields: [{len(fields)}]HostField']
        for name, field_kind in fields:
            if field_kind in records:
                lines += [f'    {name}: Widget{field_kind}Codec']
            if field_kind.startswith('[]'):
                element = field_kind[2:]
                lines += [f'    {name}_values: [{SLICE_LIMIT}]HostValue',
                          f'    {name}_codecs: [{SLICE_LIMIT}]Widget{element}Codec',
                          f'    {name}_decoded: [{SLICE_LIMIT}]{element}']
        lines += ['}', '']
        lines += encoder(kind, fields, records)
        lines += decoder(kind, fields, records)
        lines += validator(kind, fields, records)
    return '\n'.join(lines)


def encoder(kind, fields, records):
    lines = [f'WidgetEncode{kind} :: (data: {kind}, codec: *Widget{kind}Codec) -> HostValue {{',
             '    value: HostValue', '    value.kind = cast(s32)HostRecord',
             f'    value.type_name = HostTextData("{kind}")',
             '    value.fields = *codec.fields[0]', f'    value.field_count = {len(fields)}']
    for index, (name, field_kind) in enumerate(fields):
        target = f'codec.fields[{index}].value'
        lines += [f'    codec.fields[{index}].name = HostTextData("{name}")']
        if field_kind in records:
            lines += [f'    {target} = WidgetEncode{field_kind}(data.{name}, *codec.{name})']
        elif field_kind.startswith('[]'):
            element = field_kind[2:]
            lines += [f'    {target}.kind = cast(s32)HostSlice',
                      f'    {target}.type_name = HostTextData("{field_kind}")',
                      f'    {target}.length = cast(usize)data.{name}.count',
                      f'    if {target}.length > cast(usize){SLICE_LIMIT} {{ {target}.length = cast(usize){SLICE_LIMIT} }}',
                      f'    {target}.elements = *codec.{name}_values[0]',
                      '    {', '        index: usize = 0',
                      f'        while index < {target}.length {{',
                      f'            codec.{name}_values[index] = WidgetEncode{element}(data.{name}[index], *codec.{name}_codecs[index])',
                      '            index += cast(usize)1', '        }', '    }']
        elif field_kind == 'string':
            lines += [f'    {target} = HostValue.{{.kind = cast(s32)HostString, .type_name = HostTextData("string"), '
                      f'.data = HostTextData(data.{name}), .length = cast(usize)data.{name}.count}}']
        elif field_kind.startswith('float'):
            lines += [f'    {target} = HostValue.{{.kind = cast(s32)HostReal, .type_name = HostTextData("{field_kind}"), '
                      f'.real = cast(float64)data.{name}}}']
        elif field_kind == 'bool':
            lines += [f'    {target} = HostScalar("bool", ifx data.{name} then 1 else 0)']
        else:
            lines += [f'    {target} = HostScalar("{field_kind}", cast(s64)data.{name})']
    return lines + ['    return value', '}', '']


def matched(index, name, body):
    """Runs body when the record carries the named field."""
    return ['    {', f'        field_at := HostFieldAt(value, {index}, "{name}")', '        if field_at >= 0 {',
            *('        ' + line for line in body), '        }', '    }']


def decoder(kind, fields, records):
    lines = [f'WidgetDecode{kind} :: (value: HostValue, codec: *Widget{kind}Codec) -> {kind} {{', f'    data: {kind}']
    for index, (name, field_kind) in enumerate(fields):
        source = 'value.fields[field_at].value'
        if field_kind in records:
            body = [f'    data.{name} = WidgetDecode{field_kind}({source}, *codec.{name})']
        elif field_kind.startswith('[]'):
            element = field_kind[2:]
            body = ['    {', '        index: usize = 0', f'        while index < {source}.length {{',
                    f'            codec.{name}_decoded[index] = WidgetDecode{element}({source}.elements[index], *codec.{name}_codecs[index])',
                    '            index += cast(usize)1', '        }', '    }',
                    '    // Slice storage is attached by the rendering effect after decoding.']
        elif field_kind == 'string':
            body = [f'    data.{name} = TextFromBytes(BytesFromPointer({source}.data, cast(u64){source}.length))']
        elif field_kind == 'bool':
            body = [f'    data.{name} = {source}.integer != 0']
        else:
            member = 'real' if field_kind.startswith('float') else 'bits' if field_kind.startswith('u') else 'integer'
            body = [f'    data.{name} = cast({field_kind}){source}.{member}']
        lines += matched(index, name, body)
    return lines + ['    return data', '}', '']


def validator(kind, fields, records):
    lines = [f'WidgetValid{kind} :: (value: HostValue) -> bool {{',
             f'    if value.kind != cast(s32)HostRecord || TextFromCString(value.type_name) != "{kind}" ||',
             '    value.field_count > cast(usize)HOST_FIELD_LIMIT ||',
             '    (value.field_count > cast(usize)0 && value.fields == null) { return false }']
    for index, (name, field_kind) in enumerate(fields):
        source = 'value.fields[field_at].value'
        if field_kind in records:
            body = [f'    if !WidgetValid{field_kind}({source}) {{ return false }}']
        elif field_kind.startswith('[]'):
            element = field_kind[2:]
            body = [f'    if {source}.kind != cast(s32)HostSlice || TextFromCString({source}.type_name) != "{field_kind}" ||',
                    f'        {source}.length > cast(usize){SLICE_LIMIT} || ({source}.length > cast(usize)0 && {source}.elements == null) {{ return false }}',
                    '    {', '        index: usize = 0', f'        while index < {source}.length {{',
                    f'            if !WidgetValid{element}({source}.elements[index]) {{ return false }}',
                    '            index += cast(usize)1', '        }', '    }']
        else:
            body = [f'    if !HostScalarValid({source}, "{field_kind}", cast(s32){scalar_kind(field_kind)}) {{ return false }}']
        lines += matched(index, name, body)
    return lines + ['    return true', '}', '']


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument('--source', action='append', default=[], help='file with the records')
    parser.add_argument('--root', action='append', default=[], help='record that crosses the boundary')
    parser.add_argument('--uses', action='append', default=[], help='generated codec whose types are reused')
    parser.add_argument('--import', dest='imports', action='append', default=[],
                        help='import line for the output, such as \'"std/vec"\' or \', file "types.zi";\'')
    parser.add_argument('--output', type=Path, help='file to write')
    parser.add_argument('--check', action='store_true', help='fail unless the output is current')
    parser.add_argument('--generated-by', default='scripts/generate-widget-codecs.py',
                        help='command named in the generated header')
    args = parser.parse_args()

    sources, roots, imports = args.source, args.root, args.imports
    output = args.output
    if not sources:
        # Kryon's own hosted widgets.
        sources = [KRYON_ROOT / 'src/ui' / (module + '.zi') for module in KRYON_MODULES]
        roots = KRYON_ROOTS
        imports = ['"std/bundle_host"', '"std/byte_text_linux"', '"hosted_values"',
                   *(f'"{module}"' for module in KRYON_MODULES)]
        output = output or KRYON_ROOT / 'src/backend/widget_codec.zi'
    if output is None or not roots:
        parser.error('an application codec needs --source, --root and --output')
    schema = read_schema(sources)
    provided = provided_types(args.uses)
    needed = order(schema, roots, provided)
    header = f'// Generated by {args.generated_by}; bounded public widget values.'
    text = generate(schema, needed, provided, header, imports)
    if args.check:
        if not output.exists() or output.read_text() != text:
            sys.exit(f'{output} is out of date; run {args.generated_by}')
        return
    output.write_text(text)


if __name__ == '__main__':
    main()
