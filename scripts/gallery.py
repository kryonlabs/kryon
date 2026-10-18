"""Build-time module registration and editable examples for the Ziran gallery."""
import argparse
import json
import re
import subprocess
import textwrap
from pathlib import Path
import tomllib

ROOT = Path(__file__).resolve().parents[1]
PROJECT = ROOT / 'examples/gallery'
SITE = ROOT / 'docs/site'


def write(path, text):
    path.parent.mkdir(parents=True, exist_ok=True)
    if not path.exists() or path.read_text() != text:
        path.write_text(text)


def closing(text, start):
    depth, quote, escape, comment = 0, False, False, False
    for at in range(start, len(text)):
        ch = text[at]
        if comment:
            if ch == '\n': comment = False
            continue
        if quote:
            if escape: escape = False
            elif ch == '\\': escape = True
            elif ch == '"': quote = False
            continue
        if ch == '/' and text[at:at + 2] == '//': comment = True; continue
        if ch == '"': quote = True; continue
        if ch == '{': depth += 1
        elif ch == '}':
            depth -= 1
            if not depth: return at
    raise ValueError('Unclosed example body')


def branches(text, start):
    result = []
    at = start
    while True:
        opening = text.index('{', at)
        condition = text[at:opening].strip().removeprefix('else ').removeprefix('if ')
        end = closing(text, opening)
        result.append((None if condition == 'else' else condition, text[opening + 1:end], at))
        following = re.match(r'\s*else\b', text[end + 1:])
        if not following: return result, end
        at = end + 1 + following.end() - 4


def selected(condition, index):
    if condition is None: return True
    assert re.fullmatch(r'[a-z0-9_ <>=!&|()+-]+', condition), condition
    return eval(condition.replace('&&', ' and ').replace('||', ' or '), {'__builtins__': {}}, {'scene': index})


def enabled_modules(only=None):
    dependencies = tomllib.loads((PROJECT / 'ziran.toml').read_text())['dependencies']
    modules = tomllib.loads((PROJECT / 'modules.toml').read_text())['modules']
    return [m for m in modules if m['dependency'] in dependencies and (only is None or m['id'] in only)]


def compiler_paths(modules):
    paths = ['/std', '/kryon/ui', 'kryon=/kryon/ui']
    for module in modules:
        if module['dependency'] == 'kryon': continue
        root = '/packages/' + module['dependency']
        paths.extend([root, module['dependency'] + '=' + root])
    return paths


def compiler_embeds(ziran='ziran', core=ROOT):
    modules, _ = prepare()
    embeds = [str(PROJECT / 'src/palette.zi') + '@/palette.zi']
    for module in modules:
        embeds.append(str(PROJECT / module['source']) + '@/' + module['module'] + '.zi')
        command = [ziran, 'pkg', 'path', module['dependency']]
        if not (PROJECT / 'ziran.local.toml').exists(): command.append('--locked')
        package = Path(core) if module['dependency'] == 'kryon' else Path(subprocess.check_output(command, cwd=PROJECT, text=True).strip())
        manifest = tomllib.loads((package / 'ziran.toml').read_text())
        source = package / manifest['package']['module_roots'][0]
        virtual = '/kryon/ui' if module['dependency'] == 'kryon' else '/packages/' + module['dependency']
        if module['dependency'] != 'kryon': embeds.append(str(source) + '@' + virtual)
        for name, alias in export_aliases(module['dependency'], package):
            embeds.append(str(alias) + '@' + virtual + '/' + name + '.zi')
    return embeds


def export_aliases(dependency, package):
    """Write module-path aliases for exports whose names differ from their files."""
    manifest = tomllib.loads((package / 'ziran.toml').read_text())
    source = package / manifest['package']['module_roots'][0]
    aliases = []
    for name, target in manifest.get('exports', {}).items():
        if (source / (name + '.zi')).is_file() or (source / name / 'module.zi').is_file(): continue
        try: local = Path(target).relative_to(source.relative_to(package)).with_suffix('').as_posix()
        except ValueError: continue
        alias = PROJECT / 'build/catalog/exports' / dependency / (name + '.zi')
        write(alias, 'using Export :: #import "' + local + '";\n')
        aliases.append((name, alias))
    return aliases


def prepare(only=None, *, catalog=None, site=SITE):
    modules = enabled_modules(only)
    assert modules and modules[0]['id'] == 'kryon', 'The core Kryon module is required'
    template = (PROJECT / 'templates/frame.zi').read_text()
    entries, imports, dispatch, styles = [], [], [], []
    output = Path(catalog) if catalog else PROJECT / 'build/catalog'
    for number, module in enumerate(modules):
        source = PROJECT / module['source']
        text = source.read_text()
        names = [(int(m[1]), m[2]) for m in re.finditer(r'if index == (\d+) \{ return "([^"]+)" \}', text)]
        assert [i for i, _ in names] == list(range(len(names))), source
        declaration = text.index('Compose ::')
        opening = text.index('{', declaration)
        end = closing(text, opening)
        body = text[opening + 1:end]
        candidates = [m.start() for m in re.finditer(r'(?m)^    if scene\b', body)]
        chain, _ = max((branches(body, at) for at in candidates), key=lambda pair: len(pair[0]))
        first = chain[0][2]
        preamble = body[:first]
        prefix = text[:declaration]
        alias = 'Examples' + str(number)
        imports.append(f'{alias} :: #import "{module["module"]}";')
        if module.get('styles'): styles.append(f'    {alias}.{module["styles"]}(dark)')
        offset = len(entries)
        dispatch.append(f'    if scene >= {offset} && scene < {offset + len(names)} {{ {alias}.Compose(session, scene - {offset}, disabled); return }}')
        for local, name in names:
            condition, fragment, position = next(part for part in chain if selected(part[0], local))
            category = next((key for key, indices in module.get('categories', {}).items() if local in indices), module['category'])
            index = len(entries)
            declarations = "\n".join(line for line in prefix.splitlines() if re.match(r'^(?:#import |.*:: #import )', line)) + '\nusing Example :: #import "' + module['module'] + '";\n'
            setup = f'    scene: s32 = {local}\n'
            if module['id'] == 'kryon': setup += '    scene_image = "catalog-image.png"\n'
            if module.get('styles'): setup += f'    {module["styles"]}(example_input.theme == 2)\n'
            code = template.replace('// @declarations', declarations.rstrip()).replace('// @title', name).replace('    // @example', setup + textwrap.indent(textwrap.dedent(preamble).strip(), '    ') + '\n' + textwrap.indent(textwrap.dedent(fragment).strip(), '    ') + '\n')
            code_path = 'gallery-sources/' + module['id'] + '/' + str(local) + '.zi'
            write(Path(site) / code_path, code)
            entries.append({'index': index, 'id': module['id'] + '/' + str(local), 'name': name,
                            'module': module['label'], 'moduleId': module['id'], 'category': category,
                            'source': f'https://github.com/kryonlabs/kryon/blob/master/examples/gallery/{module["source"]}#L{text[:opening + 1 + position].count(chr(10)) + 1}',
                            'code': code_path, 'available': local not in module.get('unavailable', []),
                            'note': module.get('notes', {}).get(str(local), 'Click a control to try it. Tab moves keyboard focus.')})
    # This directory contains generated examples only. Drop sources whose
    # module or example is no longer part of this build.
    expected = {Path(site) / entry['code'] for entry in entries}
    for path in (Path(site) / 'gallery-sources').rglob('*.zi'):
        if path not in expected: path.unlink()
    def string_function(name, value):
        return f'{name} :: (index: s32) -> string {{\n' + ''.join(f'    if index == {e["index"]} {{ return {json.dumps(value(e), ensure_ascii=False)} }}\n' for e in entries) + '    return ""\n}\n'
    registry = 'using UI :: #import "kryon/Widgets";\n#import "palette"\n' + '\n'.join(imports) + '\n'
    registry += f'GallerySceneCount :: {len(entries)};\n'
    registry += string_function('GallerySceneName', lambda e: e['name'])
    registry += string_function('GallerySceneLabel', lambda e: e['module'] + ' / ' + e['category'] + ' · ' + e['name'])
    registry += 'InstallGalleryPalette :: (dark: bool) {\n    InstallCatalogPalette(dark)\n' + '\n'.join(styles) + '\n}\n'
    registry += 'ComposeGallery :: (session: Session, scene: s32, disabled: bool) {\n' + '\n'.join(dispatch) + '\n}\n'
    write(output / 'catalog.zi', registry)
    write(output / 'modules.json', json.dumps(modules, indent=2) + '\n')
    write(Path(site) / 'gallery-data.js', 'window.galleryModules = ' + json.dumps([{'id': m['id'], 'label': m['label']} for m in modules]) + ';\nwindow.galleryCompilerPaths = ' + json.dumps(compiler_paths(modules)) + ';\nwindow.galleryScenes = ' + json.dumps(entries, ensure_ascii=False, indent=2) + ';\n')
    return modules, entries


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--modules', help='Comma-separated module IDs for an isolated build')
    args = parser.parse_args()
    modules, entries = prepare(None if args.modules is None else set(args.modules.split(',')))
    print('Prepared module integrations: ' + ', '.join(m['label'] for m in modules))
