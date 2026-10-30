#!/usr/bin/env python3
"""Generate bounded codecs for records that cross the portable bundle boundary.

Each application root and its nested structs get codec storage and
WidgetEncodeT, WidgetDecodeT and WidgetValidT operations. Fields match by name (see
src/backend/hosted_values.zi), so a bundle and its host may be built against
different layouts. Slices hold at most 1024 elements; the decoded elements
stay in the codec, and the caller attaches them after decoding. Slices in
HOST_DECODED_SLICES keep no room in the codec: their host decodes them into
its own storage, and an encoder sends them empty.

Run without arguments, it writes src/backend/widget_codec.zi for Kryon's own
hosted widgets; --check only verifies that file. An application generates
codecs for its own records with --uses src/backend/widget_codec.zi, so types
Kryon already supplies are reused. Partial modules carry their field schemas;
missing operations are generated in the consumer without duplicating storage.
--complete supplies every operation for the imported codec records, which also
lets native host tests exercise operations absent from the library itself.
"""
import argparse
import json
from pathlib import Path
import re
import sys

SLICE_LIMIT = 1024
# Slices whose codec is repeated in many records, with the constant that
# bounds them: every ImageProps codec (in buttons, title bars and each
# dropdown option) would otherwise hold room for a warped image's grid. The
# host decodes a grid's points for the Image effect alone.
HOST_DECODED_SLICES = {('ImageMesh', 'points'): 'IMAGE_MESH_POINT_LIMIT',
                       ('ImageProps', 'clip_polygon'): 'IMAGE_MESH_POINT_LIMIT'}


def slice_limit(kind, name):
    return HOST_DECODED_SLICES.get((kind, name), SLICE_LIMIT)


def host_decoded(kind, name):
    return (kind, name) in HOST_DECODED_SLICES

KRYON_ROOT = Path(__file__).resolve().parent.parent
KRYON_MODULES = ['session', 'input_protocol', 'geometry', 'drawing_props', 'text_props', 'image_props',
                 'button_props', 'control_props', 'text_input', 'text_input_props',
                 'scroll', 'scroll_props', 'semantic', 'text_align', 'checkbox_props',
                 'toggle_props', 'tab_bar_props', 'box_props', 'press_props', 'spinbox_props',
                 'time_picker_props', 'dropdown', 'dropdown_props', 'title_bar_props',
                 'modal', 'modal_props', 'context_menu_widget', 'svg_icon_props', 'tile_map',
                 'segmented_control_props', 'fieldset_props', 'shape_props', 'slider_props',
                 'progress_props', 'overlay_props']
KRYON_ROOTS = ['FrameInputSnapshot', 'PointerFrame', 'TextProps', 'ImageProps', 'ButtonProps', 'TextAreaProps', 'TextAreaResult',
               'ScrollProps', 'ScrollResult', 'Session', 'CheckboxProps', 'CheckboxValueResult',
               'ToggleProps', 'ToggleValueResult', 'TextFieldProps', 'TextFieldResult', 'Tab',
               'TabBarProps', 'TabBarResult', 'BoxProps', 'PressProps', 'PressResult',
               'SpinboxProps', 'SpinboxStepResult', 'TimePickerProps', 'TimePickerResult',
               'TitleBarProps', 'TitleBarResult', 'DropdownProps', 'DropdownResult', 'DropdownOption', 'ModalFrameProps',
               'ModalFrameResult', 'ActionModalProps', 'ActionModalAction', 'ActionModalResult',
               'ContextMenuProps', 'ContextMenuItem', 'ContextMenuResult', 'SvgIconProps',
               'TileMapProps', 'TileMapResult', 'SegmentedControlProps', 'SegmentedControlResult',
               'SegmentOption', 'FieldsetProps', 'Vector2', 'LineProps', 'CircleProps',
               'RingProps', 'TriangleProps', 'SliderProps', 'SliderStep', 'DiscreteSliderProps',
               'SliderDiscreteStep', 'ProgressProps', 'DismissibleOverlayProps',
               'DismissibleOverlayResult', 'PopupProps', 'PopupResult']


# Operations used only by generated consumers and test fixtures. Their schemas
# remain available so --uses can generate each missing operation when needed.
KRYON_BUILD_TIME_METHODS = {
    'WidgetDecodeFrameInputSnapshot',
    'WidgetValidFrameInputSnapshot',
    'WidgetDecodeActionModalResult',
    'WidgetDecodeCheckboxValueResult',
    'WidgetDecodeContextMenuResult',
    'WidgetDecodeDropdownResult',
    'WidgetDecodeModalFrameLayout',
    'WidgetDecodeModalFrameResult',
    'WidgetDecodePointerFrame',
    'WidgetDecodePressResult',
    'WidgetDecodeScrollBarPaint',
    'WidgetDecodeScrollFrame',
    'WidgetDecodeScrollResult',
    'WidgetDecodeSpinboxStepResult',
    'WidgetDecodeTabBarResult',
    'WidgetDecodeTextAreaEdit',
    'WidgetDecodeTextAreaResult',
    'WidgetDecodeTileMapResult',
    'WidgetDecodeTimePickerResult',
    'WidgetDecodeTitleBarResult',
    'WidgetDecodeToggleValueResult',
    'WidgetEncodeActionModalAction',
    'WidgetEncodeActionModalProps',
    'WidgetEncodeBoxProps',
    'WidgetEncodeButtonProps',
    'WidgetEncodeCheckboxProps',
    'WidgetEncodeContextMenuItem',
    'WidgetEncodeContextMenuProps',
    'WidgetEncodeDropdownMenuInput',
    'WidgetEncodeDropdownOption',
    'WidgetEncodeDropdownProps',
    'WidgetEncodeDropdownTriggerInput',
    'WidgetEncodeImageMesh',
    'WidgetEncodeImageProps',
    'WidgetEncodeModalFrameProps',
    'WidgetEncodePressProps',
    'WidgetEncodeScrollInput',
    'WidgetEncodeScrollMetrics',
    'WidgetEncodeScrollProps',
    'WidgetEncodeSpinboxProps',
    'WidgetEncodeSvgIconProps',
    'WidgetEncodeTabBarInput',
    'WidgetEncodeTabBarProps',
    'WidgetEncodeTextAreaBlock',
    'WidgetEncodeTextAreaColorSpan',
    'WidgetEncodeTextAreaLine',
    'WidgetEncodeTextAreaProps',
    'WidgetEncodeTextProps',
    'WidgetEncodeTextRange',
    'WidgetEncodeTexture2D',
    'WidgetEncodeTileMapProps',
    'WidgetEncodeTimePickerProps',
    'WidgetEncodeTitleBarProps',
    'WidgetEncodeToggleProps',
    'WidgetValidActionModalResult',
    'WidgetValidCheckboxValueResult',
    'WidgetValidContextMenuResult',
    'WidgetValidDropdownResult',
    'WidgetValidModalFrameLayout',
    'WidgetValidModalFrameResult',
    'WidgetValidPointerFrame',
    'WidgetValidPressResult',
    'WidgetValidScrollBarPaint',
    'WidgetValidScrollFrame',
    'WidgetValidScrollResult',
    'WidgetValidSpinboxStepResult',
    'WidgetValidTabBarResult',
    'WidgetValidTextAreaEdit',
    'WidgetValidTextAreaResult',
    'WidgetValidTextFieldEdit',
    'WidgetValidTextFieldResult',
    'WidgetValidTileMapResult',
    'WidgetValidTimePickerResult',
    'WidgetValidTitleBarResult',
    'WidgetValidToggleValueResult',
}


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


def provided_info(paths):
    """Storage, operations and field schemas supplied by generated modules."""
    types, methods, schema = set(), set(), {}
    for path in paths:
        text = Path(path).read_text()
        storage = dict(re.findall(r'^Widget(\w+)Codec :: struct\s*\{\s*fields:\s*\[(\d+)\]HostField', text, re.M))
        types.update(re.findall(r'^Widget(\w+)Codec :: struct', text, re.M))
        methods.update(re.findall(r'^(Widget(?:Encode|Decode|Valid)\w+)\s*::\s*\(', text, re.M))
        for line in text.splitlines():
            if line.startswith('// Widget codec schema: '):
                try:
                    kind, fields = json.loads(line.removeprefix('// Widget codec schema: '))
                    valid = isinstance(kind, str) and kind in storage and isinstance(fields, list)
                    valid = valid and len(fields) == int(storage[kind])
                    valid = valid and all(isinstance(pair, list) and len(pair) == 2 and
                                          all(isinstance(value, str) for value in pair) for pair in fields)
                except (ValueError, TypeError, KeyError):
                    valid = False
                if not valid:
                    sys.exit(f'{path}: codec schema metadata does not match its storage')
                schema[kind] = fields
    return types, methods, schema


def provided_types(paths):
    """Types whose storage is already declared in imported codec modules."""
    return provided_info(paths)[0]


def required_methods(schema, seeds, available, records):
    """Close over actual generated calls, including validator callbacks."""
    selected = set()
    emitters = {'Encode': encoder, 'Decode': decoder, 'Valid': validator}

    def require(name):
        if name in available or name in selected:
            return
        operation, kind = re.fullmatch(r'Widget(Encode|Decode|Valid)(\w+)', name).groups()
        if kind not in schema:
            sys.exit(f'{name} needs its record declaration in --source or codec schema metadata')
        selected.add(name)
        body = '\n'.join(emitters[operation](kind, schema[kind], records))
        body = re.sub(r'"(?:\\.|[^"\\])*"|//[^\n]*', '', body)
        for dependency in re.findall(r'\bWidget(?:Encode|Decode|Valid)\w+\b', body):
            require(dependency)

    for name in sorted(seeds):
        require(name)
    return selected


def order(schema, roots, provided):
    needed = []

    def discover(kind):
        if kind.startswith('['):
            discover(kind.split(']', 1)[1])
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


def scalar_encode(value, kind):
    """A HostValue expression for a plain value of the given kind."""
    if kind == 'string':
        return (f'HostValue.{{.kind = cast(s32)HostString, .type_name = HostTextData("string"), '
                f'.data = HostTextData({value}), .length = cast(usize){value}.count}}')
    if kind.startswith('float'):
        return (f'HostValue.{{.kind = cast(s32)HostReal, .type_name = HostTextData("{kind}"), '
                f'.real = cast(float64){value}}}')
    if kind == 'bool':
        return f'HostScalar("bool", ifx {value} then 1 else 0)'
    return f'HostScalar("{kind}", cast(s64){value})'


def scalar_decode(source, kind):
    """The plain value a HostValue expression holds."""
    if kind == 'string':
        return f'HostValueText({source})'
    if kind == 'bool':
        return f'{source}.integer != 0'
    member = 'real' if kind.startswith('float') else 'bits' if kind.startswith('u') else 'integer'
    return f'cast({kind}){source}.{member}'


def generate(schema, needed, provided, header, imports, methods=None):
    records = set(needed) | provided
    emitters = {'Encode': encoder, 'Decode': decoder, 'Valid': validator}
    extra = sorted({re.fullmatch(r'Widget(?:Encode|Decode|Valid)(\w+)', name)[1]
                    for name in methods or ()} & provided)
    # A file import is written `#import, file "name.zi";`.
    lines = [header, *('#import' + ('' if line.startswith(',') else ' ') + line for line in imports),
             'using HostValueKind;', '']
    for kind in [*needed, *extra]:
        fields = schema[kind]
        if kind not in provided:
            if methods is not None and any('Widget' + op + kind not in methods for op in emitters):
                lines += ['// Widget codec schema: ' + json.dumps([kind, fields], separators=(',', ':'))]
            lines += [f'Widget{kind}Codec :: struct {{', f'    fields: [{len(fields)}]HostField']
            for name, field_kind in fields:
                if field_kind in records:
                    lines += [f'    {name}: Widget{field_kind}Codec']
                if field_kind.startswith('[') and not host_decoded(kind, name):
                    element = field_kind.split(']', 1)[1]
                    limit = slice_limit(kind, name) if field_kind.startswith('[]') else int(field_kind[1:].split(']')[0])
                    lines += [f'    {name}_values: [{limit}]HostValue']
                    if element in records:
                        lines += [f'    {name}_codecs: [{limit}]Widget{element}Codec']
                    if field_kind.startswith('[]'):
                        lines += [f'    {name}_decoded: [{limit}]{element}']
            lines += ['}', '']
        for operation, emit in emitters.items():
            if methods is None or 'Widget' + operation + kind in methods:
                lines += emit(kind, fields, records)
    return '\n'.join(lines)


def encoder(kind, fields, records):
    lines = [f'WidgetEncode{kind} :: (data: {kind}, codec: *Widget{kind}Codec) -> HostValue {{',
             f'    value := HostRecordValue("{kind}", codec.fields[:])']
    for index, (name, field_kind) in enumerate(fields):
        target = f'codec.fields[{index}].value'
        if field_kind not in records and not field_kind.startswith('['):
            lines += [f'    codec.fields[{index}] = HostField.{{.name = HostTextData("{name}"), '
                      f'.value = {scalar_encode(f"data.{name}", field_kind)}}}']
            continue
        lines += [f'    codec.fields[{index}].name = HostTextData("{name}")']
        if field_kind in records:
            lines += [f'    {target} = WidgetEncode{field_kind}(data.{name}, *codec.{name})']
        elif field_kind.startswith('[]') and host_decoded(kind, name):
            lines += [f'    {target}.kind = cast(s32)HostSlice',
                      f'    {target}.type_name = HostTextData("{field_kind}")',
                      f'    {target}.length = cast(usize)0']
        elif field_kind.startswith('['):
            fixed = not field_kind.startswith('[]')
            element = field_kind.split(']', 1)[1]
            limit = int(field_kind[1:].split(']')[0]) if fixed else slice_limit(kind, name)
            lines += [f'    {target}.kind = cast(s32)' + ('HostArray' if fixed else 'HostSlice'),
                      f'    {target}.type_name = HostTextData("{field_kind}")',
                      f'    {target}.length = cast(usize)data.{name}.count',
                      f'    if {target}.length > cast(usize){limit} {{ {target}.length = cast(usize){limit} }}',
                      f'    {target}.elements = *codec.{name}_values[0]',
                      '    {', '        index: usize = 0',
                      f'        while index < {target}.length {{',
                      f'            codec.{name}_values[index] = ' + (
                          f'WidgetEncode{element}(data.{name}[index], *codec.{name}_codecs[index])'
                          if element in records else scalar_encode(f'data.{name}[index]', element)),
                      '            index += cast(usize)1', '        }', '    }']
    return lines + ['    return value', '}', '']


def matched(index, name, body):
    """Runs body when the record carries the named field."""
    return ['    {', f'        field_at := HostFieldAt(value, {index}, "{name}")', '        if field_at >= 0 {',
            *('        ' + line for line in body), '        }', '    }']


def decoder(kind, fields, records):
    lines = [f'WidgetDecode{kind} :: (value: HostValue, codec: *Widget{kind}Codec) -> {kind} {{', f'    data: {kind}', '    zero: HostValue']
    for index, (name, field_kind) in enumerate(fields):
        source = f'HostFieldValue(*value, {index}, "{name}", *zero)'
        if field_kind in records:
            lines += [f'    data.{name} = WidgetDecode{field_kind}({source}.*, *codec.{name})']
        elif field_kind.startswith('[]') and host_decoded(kind, name):
            # These slices are decoded into the rendering effect's storage.
            continue
        elif field_kind.startswith('['):
            source = 'value.fields[field_at].value'
            fixed = not field_kind.startswith('[]')
            element = field_kind.split(']', 1)[1]
            limit = int(field_kind[1:].split(']')[0]) if fixed else slice_limit(kind, name)
            body = ['    {', '        index: usize = 0', f'        while index < {source}.length' + (f' && index < cast(usize){limit}' if fixed else '') + ' {',
                    f'            ' + (f'data.{name}[index]' if fixed else f'codec.{name}_decoded[index]') + ' = ' + (
                        f'WidgetDecode{element}({source}.elements[index], *codec.{name}_codecs[index])'
                        if element in records else scalar_decode(f'{source}.elements[index]', element)),
                    '            index += cast(usize)1', '        }', '    }',
                    * ([] if fixed else ['    // Slice storage is attached by the rendering effect after decoding.'])]
            lines += matched(index, name, body)
        else:
            lines += [f'    data.{name} = ' + scalar_decode(source + ('.*' if field_kind == 'string' else ''), field_kind)]
    if not any('codec.' in line for line in lines):
        lines.insert(2, '    unused codec')
    return lines + ['    return data', '}', '']


def validator(kind, fields, records):
    lines = [f'WidgetValid{kind} :: (value: HostValue) -> bool {{',
             f'    if !HostRecordValid(*value, "{kind}") {{ return false }}']
    for index, (name, field_kind) in enumerate(fields):
        if field_kind in records:
            lines += [f'    if !HostFieldRecordValid(*value, {index}, "{name}", WidgetValid{field_kind}) {{ return false }}']
        elif field_kind.startswith('['):
            fixed = not field_kind.startswith('[]')
            element = field_kind.split(']', 1)[1]
            limit = int(field_kind[1:].split(']')[0]) if fixed else slice_limit(kind, name)
            rule = (f'.validate_record = WidgetValid{element}' if element in records else
                    f'.type_name = "{element}", .kind = cast(s32){scalar_kind(element)}')
            if fixed:
                body = [f'    field := value.fields[field_at].value',
                        f'    if !HostArrayValid(field, "{field_kind}", cast(usize){limit}) {{ return false }}',
                        '    index: usize = 0', f'    while index < cast(usize){limit} {{',
                        ('        if !WidgetValid' + element + '(field.elements[index]) { return false }' if element in records else
                         f'        if !HostScalarValid(field.elements[index], "{element}", cast(s32){scalar_kind(element)}) {{ return false }}'),
                        '        index += cast(usize)1', '    }']
                lines += matched(index, name, body)
            else:
                lines += [f'    if !HostFieldSliceValid(*value, {index}, "{name}", "{field_kind}", cast(usize){limit},',
                          f'        HostElementRule.{{{rule}}}) {{ return false }}']
        else:
            lines += [f'    if !HostFieldScalarValid(*value, {index}, "{name}", "{field_kind}", cast(s32){scalar_kind(field_kind)}) {{ return false }}']
    return lines + ['    return true', '}', '']


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument('--source', action='append', default=[], help='file with the records')
    parser.add_argument('--root', action='append', default=[], help='record that crosses the boundary')
    parser.add_argument('--uses', action='append', default=[], help='generated codec whose types are reused')
    parser.add_argument('--import', dest='imports', action='append', default=[],
                        help='import line for the output, such as \'"std/vec"\' or \', file "types.zi";\'')
    parser.add_argument('--output', type=Path, help='file to write')
    parser.add_argument('--defer-method', action='append', default=[],
                        help='generate this operation only when another operation requires it')
    parser.add_argument('--complete', action='store_true',
                        help='include all operations for Kryon and imported codec storage')
    parser.add_argument('--check', action='store_true', help='fail unless the output is current')
    parser.add_argument('--generated-by', default='scripts/generate-widget-codecs.py',
                        help='command named in the generated header')
    args = parser.parse_args()

    sources, roots, imports = args.source, args.root, args.imports
    output = args.output
    omitted = set(args.defer_method)
    if not sources:
        # Kryon's own hosted widgets.
        sources = [KRYON_ROOT / 'src/ui' / (module + '.zi') for module in KRYON_MODULES]
        roots = KRYON_ROOTS
        imports = ['"std/bundle_host"', '"std/byte_text_linux"', '"hosted_values"',
                   *(f'"{module}"' for module in KRYON_MODULES)]
        if args.complete:
            imports += args.imports
        else:
            omitted |= KRYON_BUILD_TIME_METHODS
        output = output or KRYON_ROOT / 'src/backend/widget_codec.zi'
    if output is None or not roots:
        parser.error('an application codec needs --source, --root and --output')
    if any(output.resolve() == Path(path).resolve() for path in args.uses):
        parser.error('output must differ from imported --uses modules')
    provided, available, schema = provided_info(args.uses)
    schema.update(read_schema(sources))
    needed = order(schema, roots, provided)
    records = set(needed) | provided
    all_methods = {'Widget' + op + kind for kind in records for op in ('Encode', 'Decode', 'Valid')}
    if omitted - all_methods:
        parser.error('unknown deferred methods: ' + ', '.join(sorted(omitted - all_methods)))
    seed_types = set(needed) | set(roots)
    if args.complete:
        seed_types |= provided
    seeds = {'Widget' + op + kind for kind in seed_types for op in ('Encode', 'Decode', 'Valid')} - omitted
    methods = required_methods(schema, seeds, available, records)
    header = f'// Generated by {args.generated_by}; bounded public widget values.'
    text = generate(schema, needed, provided, header, imports, methods)
    if args.check:
        if not output.exists() or output.read_text() != text:
            sys.exit(f'{output} is out of date; run {args.generated_by}')
        return
    output.write_text(text)


if __name__ == '__main__':
    main()
