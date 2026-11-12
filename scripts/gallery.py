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


def scene_apis(fragment):
    apis = []
    for name in re.findall(r'(?<![\w.])([A-Z][A-Za-z0-9_]*)\s*\(', fragment):
        if name not in apis and name not in ('Prepare', 'Label'):
            apis.append(name)
    return apis


def scene_options(fragment):
    options = []
    for fields in re.findall(r'[A-Z][A-Za-z0-9_]*Props\s*(?:=\s*)?\.\s*\{([^}]*)', fragment):
        for option in re.findall(r'\.\s*([a-z][A-Za-z0-9_]*)\s*=', fields):
            if option not in options: options.append(option)
    return options


def enabled_modules(only=None):
    dependencies = tomllib.loads((PROJECT / 'ziran.toml').read_text())['dependencies']
    modules = tomllib.loads((PROJECT / 'modules.toml').read_text())['modules']
    return [m for m in modules if m['dependency'] in dependencies and (only is None or m['id'] in only)]


def compiler_paths(modules):
    paths = []
    for module in modules:
        dependency = module['dependency']
        if export_aliases(dependency, compiler_package(dependency)):
            paths.append(dependency + '=/exports/' + dependency)
    roots = tomllib.loads((ROOT / 'ziran.toml').read_text())['package']['module_roots']
    for root in roots:
        try: relative = Path(root).relative_to('src').as_posix()
        except ValueError: continue
        virtual = '/kryon/' + relative
        paths.extend([virtual, 'kryon_source=' + virtual])
    for module in modules:
        if module['dependency'] == 'kryon': continue
        root = '/packages/' + module['dependency']
        paths.extend([root, module['dependency'] + '_source=' + root])
    paths.append('/std')
    return paths


def compiler_package(dependency, ziran='ziran', core=ROOT):
    if dependency == 'kryon': return Path(core)
    command = [ziran, 'pkg', 'path', dependency]
    if not (PROJECT / 'ziran.local.toml').exists(): command.append('--locked')
    return Path(subprocess.check_output(command, cwd=PROJECT, text=True).strip())


def compiler_embeds(ziran='ziran', core=ROOT):
    modules, _ = prepare()
    embeds = [str(PROJECT / 'src/palette.zi') + '@/palette.zi', str(ROOT / 'examples/shared/widget_context.zi') + '@/widget_context.zi']
    for module in modules:
        embeds.append(str(PROJECT / module['source']) + '@/' + module['module'] + '.zi')
        package = compiler_package(module['dependency'], ziran, core)
        manifest = tomllib.loads((package / 'ziran.toml').read_text())
        source = package / manifest['package']['module_roots'][0]
        virtual = '/kryon/ui' if module['dependency'] == 'kryon' else '/packages/' + module['dependency']
        if module['dependency'] != 'kryon': embeds.append(str(source) + '@' + virtual)
        aliases = export_aliases(module['dependency'], package)
        if aliases: embeds.append(str(aliases[0][1].parent) + '@/exports/' + module['dependency'])
    return embeds


def export_aliases(dependency, package):
    """Separate public export names from internal source module names."""
    manifest = tomllib.loads((package / 'ziran.toml').read_text())
    roots = [package / root for root in manifest['package']['module_roots']]
    aliases = []
    for name, target in manifest.get('exports', {}).items():
        locals = []
        for root in roots:
            try: locals.append(Path(target).relative_to(root.relative_to(package)).with_suffix('').as_posix())
            except ValueError: pass
        if not locals: continue
        local = min(locals, key=len).removesuffix('/module')
        alias = PROJECT / 'build/catalog/exports' / dependency / (name + '.zi')
        write(alias, 'using Export :: #import "' + dependency + '_source/' + local + '";\n')
        aliases.append((name, alias))
    expected = {path for _, path in aliases}
    for stale in (PROJECT / 'build/catalog/exports' / dependency).glob('*.zi'):
        if stale not in expected: stale.unlink()
    return aliases


def prepare(only=None, *, catalog=None, site=SITE, examples=None):
    examples_root = Path(examples) if examples else ROOT / "examples"
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
        prefix = text[:declaration] + text[end + 1:]
        alias = 'Examples' + str(number)
        if module.get('styles'):
            imports.append(f'{alias} :: #import "{module["module"]}";')
        if module.get('styles'): styles.append(f'    {alias}.{module["styles"]}(dark)')
        offset = len(entries)
        for local, name in names:
            condition, fragment, position = next(part for part in chain if selected(part[0], local))
            category = next((key for key, indices in module.get('categories', {}).items() if local in indices), module['category'])
            index = len(entries)
            declarations = "\n".join(line for line in prefix.splitlines() if re.match(r'^(?:#import |.*:: #import )', line)) + '\nusing Example :: #import "' + module['module'] + '";\n'
            setup = f'    scene: s32 = {local}\n'
            if module['id'] == 'kryon': setup += '    scene_image = "assets/pfp.webp"\n'
            if module.get('styles'): setup += f'    {module["styles"]}(example_input.theme == 2)\n'
            code = template.replace('// @declarations', declarations.rstrip()).replace('// @title', name).replace('    // @example', setup + textwrap.indent(textwrap.dedent(preamble).strip(), '    ') + '\n' + textwrap.indent(textwrap.dedent(fragment).strip(), '    ') + '\n')
            slug = re.sub(r'[^a-z0-9]+', '-', name.lower()).strip('-')
            slug = {'cui-style':'stylepicker', 'button-emphasis':'button', 'dropdown-open':'dropdown', 'circle-ring-triangle-line':'shape', 'page-section-heading-paragraphtext':'page', 'text-selection':'text', 'transition-fade':'transition', 'listboxmulti':'listbox', 'plot-bars':'plotbars', 'plot-lines':'plotlines', 'separator-and-bullet':'separator'}.get(slug, slug)
            example = examples_root / slug
            entry = example / (slug + '.zi')
            if not entry.exists():
                # Keep only the example state/helpers referenced by this scene.
                imports_code = "\n".join(line for line in prefix.splitlines() if re.match(r'^(?:#import |.*:: #import )', line))
                starts = list(re.finditer(r'(?m)^[A-Za-z_][A-Za-z0-9_]*\s*:(?::|[^:])', prefix))
                declarations = {}
                for at, match in enumerate(starts):
                    stop = starts[at + 1].start() if at + 1 < len(starts) else len(prefix)
                    name_match = re.match(r'([A-Za-z_][A-Za-z0-9_]*)', match.group())
                    value = prefix[match.start():stop].strip()
                    if '#import ' not in value:
                        declarations[name_match[1]] = value
                setup_code = setup
                preamble_code = preamble
                if module['id'] == 'kryon' and not re.search(r'\b(field_input|area_input)\b', fragment):
                    preamble_code = ''
                wanted = set()
                used = fragment + preamble_code + setup_code + ' SetGalleryViewport '
                while True:
                    following = {key for key in declarations if re.search(r'\b' + key + r'\b', used)} - wanted
                    if not following: break
                    wanted |= following
                    used += '\n' + '\n'.join(declarations[key] for key in following)
                native_code = imports_code + '\n#import "widget_context"\n#import "palette"\n'
                native_code += '\n\n'.join(value for key, value in declarations.items() if key in wanted) + '\n\n'
                frame = template[template.index('#program_export'):]
                frame = frame.replace('// @title', name).replace('PreviewInputRead()', 'WidgetInput(session)')
                frame = frame.replace('    // @example', setup_code + textwrap.indent(textwrap.dedent(preamble_code).strip(), '    ') + '\n' + textwrap.indent(textwrap.dedent(fragment).strip(), '    '))
                write(entry, native_code + frame)
            code = entry.read_text()
            manifest = (PROJECT / 'ziran.toml').read_text()
            manifest = manifest.replace('name = "gallery"', 'name = "' + slug + '"').replace('entry = "src/app.zi"', 'entry = "' + slug + '.zi"')
            manifest = manifest.replace('module_roots = ["src", "build/catalog"]', 'module_roots = ["."]')
            manifest = manifest.replace('host = "src/desktop.zi"\n', '').replace('host = "src/web.zi"\n', '').replace('bridge_modules = ["app"]\n', '')
            if 'assets/pfp.webp' not in code:
                manifest = manifest.replace('assets = "assets"\n', '')
            else:
                import shutil
                (example / 'assets').mkdir(exist_ok=True)
                if not (example / 'assets/pfp.webp').exists():
                    shutil.copyfile(PROJECT / 'assets/pfp.webp', example / 'assets/pfp.webp')
            if not (example / 'ziran.toml').exists(): write(example / 'ziran.toml', manifest)
            if not (example / 'ziran.lock').exists():
                write(example / 'ziran.lock', (PROJECT / 'ziran.lock').read_text())
            code_path = 'gallery-sources/' + module['id'] + '/' + str(local) + '.zi'
            write(Path(site) / code_path, code)
            apis = scene_apis(fragment)
            options = scene_options(fragment)
            entries.append({'index': index, 'id': module['id'] + '/' + str(local), 'name': name,
                            'module': module['label'], 'moduleId': module['id'], 'category': category,
                            'source': 'https://github.com/kryonlabs/kryon/blob/master/' + str(entry.relative_to(ROOT)),
                            'sourcePath': str(entry.relative_to(ROOT)),
                            'code': code_path, 'available': local not in module.get('unavailable', []),
                            'status': 'Available in the maintained gallery' if local not in module.get('unavailable', []) else 'Not available in the current build',
                            'api': apis, 'options': options,
                            'keyboard': module.get('keyboard', {}).get(str(local), 'Keyboard behavior is not documented by this scene.'),
                            'summary': module.get('summaries', {}).get(str(local),
                                        f'A maintained {name} example from the {module["label"]} module.'),
                            'alt': f'Ziran-rendered {name} example using the {{theme}} palette.',
                            'images': {'light': f'assets/components/{module["id"]}-{local}-light.webp',
                                       'dark': f'assets/components/{module["id"]}-{local}-dark.webp'},
                            'note': module.get('notes', {}).get(str(local), 'Static catalog capture rendered from the Ziran gallery source.')})
    # Standalone public components are maintained directly in examples/.
    additional = {
        'alert': 'Alert', 'avatar': 'Avatar', 'badge': 'Badge', 'banner': 'Banner',
        'buttongroup': 'Button group', 'confirmbutton': 'Confirm button',
        'copybutton': 'Copy button', 'countdown': 'Countdown', 'emptystate': 'Empty state',
        'keycap': 'Keycap', 'breadcrumb': 'Breadcrumb', 'svgicon': 'SVG icon',
        'gridmulti': 'Grid multi', 'holdbutton': 'Hold button', 'dpad': 'Direction pad',
        'joystick': 'Joystick', 'swipearea': 'Swipe area', 'loading': 'Loading indicator',
        'loadingscreen': 'Loading screen', 'checklist': 'Onboarding checklist',
        'progressring': 'Progress ring', 'timeline': 'Timeline', 'swiperow': 'Swipe row',
        'carousel': 'Carousel', 'tilemap': 'Tile map',
    }
    component_categories = {
        'Actions': ['buttongroup', 'confirmbutton', 'copybutton', 'holdbutton'],
        'Feedback': ['alert', 'badge', 'banner', 'countdown', 'emptystate', 'loading', 'loadingscreen', 'checklist', 'progressring', 'timeline'],
        'Navigation': ['breadcrumb'],
        'Inputs': ['dpad', 'joystick', 'swipearea', 'swiperow'],
        'Layout': ['gridmulti'],
        'Media & graphics': ['avatar', 'svgicon', 'carousel', 'tilemap'],
        'Content': ['keycap'],
    }
    core_count = sum(entry["moduleId"] == "kryon" for entry in entries)
    for extra, (slug, name) in enumerate(additional.items()):
        entry = examples_root / slug / (slug + '.zi')
        code = entry.read_text()
        index = len(entries)
        code_path = 'gallery-sources/kryon/' + slug + '.zi'
        write(Path(site) / code_path, code)
        entries.append({'index': index, 'id': 'kryon/' + str(core_count + extra), 'name': name,
                        'module': 'Kryon', 'moduleId': 'kryon', 'category': next(category for category, slugs in component_categories.items() if slug in slugs),
                        'source': 'https://github.com/kryonlabs/kryon/blob/master/' + str(entry.relative_to(ROOT)),
                        'sourcePath': str(entry.relative_to(ROOT)), 'code': code_path,
                        'available': True, 'status': 'Available in the maintained gallery',
                        'api': [name.title().replace(' ', '')], 'options': [], 'keyboard': '',
                        'summary': f'A maintained {name} usage example.',
                        'alt': f'Live {name} example',
                        'images': {theme: f'assets/components/kryon-{core_count + extra}-{theme}.webp' for theme in ('light', 'dark')}, 'note': ''})
    # Every renderer uses the maintained Frame shown by the source editor.
    # Variants sharing an authored file also share its module and state.
    render_modules = {}
    for entry in entries:
        path = entry['sourcePath']
        if path not in render_modules:
            alias = 'Usage' + str(len(render_modules))
            module_name = 'usage_' + Path(path).stem
            code = (ROOT / path).read_text()
            write(output / (module_name + '.zi'), code.replace('#program_export\n', ''))
            imports.append(f'{alias} :: #import "{module_name}";')
            render_modules[path] = alias
        alias = render_modules[path]
        dispatch.append(f"    if scene == {entry['index']} {{ {alias}.SetExampleOptions(disabled, catalog_reduced_motion); unused {alias}.Frame(session, catalog_viewport); return }}")
    # This directory contains generated examples only. Drop sources whose
    # module or example is no longer part of this build.
    expected = {Path(site) / entry['code'] for entry in entries}
    for path in (Path(site) / 'gallery-sources').rglob('*.zi'):
        if path not in expected: path.unlink()
    def string_function(name, value):
        result = f'{name} :: (index: s32) -> string {{\n'
        for entry in entries:
            text = value(entry)
            if '\n' in text:
                marker = 'GALLERY_SOURCE_' + str(entry['index'])
                result += f"    if index == {entry['index']} {{ return #string {marker}\n{text}\n{marker}\n    }}\n"
            else:
                result += f"    if index == {entry['index']} {{ return {json.dumps(text, ensure_ascii=False)} }}\n"
        return result + '    return ""\n}\n'
    registry = 'using UI :: #import "kryon/Widgets";\n#import "palette"\n' + '\n'.join(imports) + '\n'
    registry += 'catalog_viewport: Rectangle = .{0.0, 0.0, 480.0, 320.0};\n'
    registry += f'GallerySceneCount :: {len(entries)};\n'
    registry += 'GalleryCompilerSearch :: () -> string {\n    return #string GALLERY_COMPILER_SEARCH\n' + '\n'.join(compiler_paths(modules)) + '\nGALLERY_COMPILER_SEARCH\n}\n'
    registry += string_function('GallerySceneName', lambda e: e['name'])
    registry += string_function('GallerySceneCode', lambda e: (Path(site) / e['code']).read_text())
    registry += string_function('GallerySceneId', lambda e: e['id'])
    registry += string_function('GallerySceneSummary', lambda e: e['summary'])
    core = {'kryon', 'inputs', 'overlays'}
    registry += string_function('GallerySceneLabel', lambda e: (
        '' if e['moduleId'] in core else e['module'] + ' / ') + e['category'] + ' · ' + e['name'])
    navigation = sorted(entries, key=lambda e: (e['moduleId'] not in ('kryon', 'inputs', 'overlays'), '' if e['moduleId'] in ('kryon', 'inputs', 'overlays') else e['module'], e['category'], e['name']))
    registry += string_function('GallerySceneGroup', lambda e: (
        '' if e['moduleId'] in core else e['module'] + ' / ') + e['category'])
    registry += 'GalleryNavigationScene :: (position: s32) -> s32 {\n'
    for position, entry in enumerate(navigation):
        registry += f"    if position == {position} {{ return {entry['index']} }}\n"
    registry += '    return 0\n}\n'

    registry += 'InstallGalleryPalette :: (dark: bool) {\n    InstallCatalogPalette(dark)\n' + '\n'.join(styles) + '\n}\n'
    registry += 'catalog_reduced_motion: bool;\n'
    registry += 'SetGalleryPreview :: (bounds: Rectangle, live: bool, now: float32, reduced_motion: bool) {\n    catalog_viewport = bounds; catalog_reduced_motion = reduced_motion\n' + '}\n'
    registry += 'ComposeGallery :: (session: Session, scene: s32, disabled: bool) {\n' + '\n'.join(dispatch) + '\n}\n'
    write(output / 'catalog.zi', registry)
    write(output / 'modules.json', json.dumps(modules, indent=2) + '\n')
    for entry in entries:
        related = [other['id'] for other in entries if other['id'] != entry['id'] and (
            other['category'] == entry['category'] or set(other['api']) & set(entry['api']))]
        entry['related'] = related[:6]
    write(Path(site) / 'gallery-data.js', 'window.galleryModules = ' + json.dumps([{'id': m['id'], 'label': m['label']} for m in modules]) + ';\nwindow.galleryScenes = ' + json.dumps(entries, ensure_ascii=False, indent=2) + ';\n')
    return modules, entries


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--modules', help='Comma-separated module IDs for an isolated build')
    args = parser.parse_args()
    modules, entries = prepare(None if args.modules is None else set(args.modules.split(',')))
    print('Prepared module integrations: ' + ', '.join(m['label'] for m in modules))
