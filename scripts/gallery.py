"""Build-time module registration and editable examples for the Ziran gallery."""
import argparse
import json
import re
import subprocess
from pathlib import Path
import tomllib

ROOT = Path(__file__).resolve().parents[1]
PROJECT = ROOT / 'examples/gallery'
SITE = ROOT / 'docs/site'

# These scenes share a source module while keeping their distinct composition.
SCENE_FRAMES = {
    'kryon/1': 'GalleryFrame',
    'kryon/2': 'GalleryFrame',
    'kryon/4': 'GalleryFrame',
    'kryon/5': 'GalleryFrame',
    'kryon/6': 'GalleryFrame',
    'kryon/7': 'GalleryFrame',
    'kryon/8': 'GalleryFrame',
    'kryon/9': 'GalleryFrame',
    'kryon/12': 'GalleryFrame',
    'kryon/13': 'GalleryFrame',
    'kryon/14': 'GalleryFrame',
    'kryon/15': 'GalleryFrame',
    'kryon/16': 'GalleryFrame',
    'kryon/21': 'GalleryFrame',
    'kryon/22': 'GalleryFrame',
    'kryon/26': 'GalleryFrame',
    'kryon/27': 'GalleryFrame',
    'kryon/28': 'GalleryFrame',
    'kryon/29': 'GalleryFrame',
    'kryon/30': 'GalleryFrame',
    'kryon/35': 'GalleryFrame',
    'kryon/36': 'GalleryFrame',
    'kryon/37': 'GalleryFrame',
    'kryon/38': 'GalleryFrame',
    'kryon/39': 'GalleryFrame',
    'kryon/40': 'GalleryFrame',
    'kryon/49': 'GalleryFrame',
    'kryon/50': 'GallerySelectionFrame',
    'kryon/52': 'GalleryFrame',
    'kryon/53': 'GalleryFrame',
    'kryon/54': 'GalleryFrame',
    'kryon/3': 'GalleryFrame',
    'kryon/10': 'GalleryFrame',
    'kryon/18': 'GalleryFrame',
    'kryon/19': 'GalleryFrame',
    'kryon/20': 'GalleryFrame',
    'kryon/42': 'GalleryFrame',
    'kryon/11': 'GalleryMultiFrame',
    'kryon/47': 'GalleryOpenFrame',
    'kryon/17': 'GalleryFrame',
    'kryon/31': 'GalleryFrame',
    'kryon/41': 'GalleryFrame',
    'kryon/44': 'GalleryFrame',
    'kryon/48': 'GalleryFrame',
    'kryon/24': 'GalleryFrame',
    'kryon/25': 'GalleryFrame',
    'kryon/32': 'GalleryFrame',
    'kryon/33': 'GalleryFrame',
    'kryon/34': 'GalleryFrame',
    'kryon/45': 'GalleryFrame',
    'kryon/23': 'GalleryFrame',
    'kryon/43': 'GalleryFrame',
    'kryon/51': 'GalleryFrame',
    'overlays/0': 'GalleryFrame',
    'overlays/1': 'GalleryFrame',
    'overlays/2': 'GalleryFrame',
    'overlays/3': 'GalleryFrame',
    'overlays/4': 'GalleryFrame',
    'overlays/5': 'GalleryFrame',
    'inputs/0': 'GalleryFrame',
    'inputs/1': 'GalleryFrame',
    'inputs/2': 'GalleryFrame',
    'inputs/3': 'GalleryFrame',
    'inputs/4': 'GalleryFrame',
    'inputs/5': 'GalleryFrame',
    'plot/0': 'GalleryFrame',
    'plot/1': 'GalleryFrame',
    'plot/2': 'GalleryFrame',
    'plot/3': 'GalleryFrame',
    'plot/4': 'GalleryFrame',
    'plot/5': 'GalleryFrame',
    'data_views/0': 'GalleryFrame',
    'data_views/1': 'GalleryFrame',
    'data_views/2': 'GalleryFrame',
}

# Static catalog captures keep each authored scene's fixed preview geometry;
# live previews use its ordinary interactive frame callback.
SCENE_CAPTURES = {
    'kryon/0': 'GalleryCoreFrame',
    'kryon/46': 'GalleryEmphasisFrame',
    'kryon/43': 'GalleryCaptureFrame',
    'kryon/47': 'GalleryStaticOpenFrame',
    'overlays/1': 'GalleryCaptureFrame',
    'overlays/2': 'GalleryCaptureFrame',
    'overlays/3': 'GalleryCaptureFrame',
    'overlays/4': 'GalleryCaptureFrame',
    'overlays/5': 'GalleryCaptureFrame',
}


def write(path, text):
    path.parent.mkdir(parents=True, exist_ok=True)
    data = text if isinstance(text, bytes) else text.encode('utf-8')
    if not path.exists() or path.read_bytes() != data:
        path.write_bytes(data)


def scene_metadata(source):
    # Catalog hints come from authored composition; compilation uses the typed
    # frontend. Missing scenes stay visible in the inventory and fail publication.
    if not source.is_file():
        return [], []
    text = source.read_text()
    apis = list(dict.fromkeys(re.findall(r'<([A-Z][A-Za-z0-9_.]*)\b', text)))
    options = []
    for fields in re.findall(r'\bprops=\{\{([^}]*)', text):
        for name in re.findall(r'\b([a-z][A-Za-z0-9_]*)\s*:', fields):
            if name not in options:
                options.append(name)
    return apis, options


def enabled_modules(only=None):
    dependencies = tomllib.loads((PROJECT / 'ziran.toml').read_text())['dependencies']
    modules = tomllib.loads((PROJECT / 'modules.toml').read_text())['modules']
    return [m for m in modules if m['dependency'] in dependencies and (only is None or m['id'] in only)]


def shared_example_exports(core=ROOT):
    manifest = tomllib.loads((core / 'ziran.toml').read_text())
    return {name.removeprefix('examples/'): Path(path) for name, path in manifest['exports'].items()
            if name.startswith('examples/')}


def package_roots(dependency, package):
    manifest = tomllib.loads((package / 'ziran.toml').read_text())
    roots = [Path(root) for root in manifest['package']['module_roots']]
    base = Path('/packages') / dependency
    return [(package / root, base / (root.relative_to(roots[0])
             if root.is_relative_to(roots[0]) else root)) for root in roots]


def compiler_paths(modules):
    paths = []
    for module in modules:
        dependency = module['dependency']
        package = compiler_package(dependency)
        aliases = export_aliases(dependency, package)
        if aliases:
            name = tomllib.loads((package / 'ziran.toml').read_text())['package']['name']
            for alias in dict.fromkeys((dependency, name)):
                paths.append(alias + '=/exports/' + dependency)
            # The compiler scopes module identities by the longest named
            # search mount, so grouped export aliases need their own longer
            # scope entries: kryon/button and kryon/tsx/button otherwise
            # collide on the kryon__button identity of their shared file leaf.
            for group in sorted({alias_name.split('/')[0] for alias_name, _ in aliases if '/' in alias_name}):
                paths.append(dependency + '_' + group + '=/exports/' + dependency + '/' + group)
            plain = PROJECT / 'build/catalog/exports' / dependency / '_plain'
            if plain.is_dir():
                # Package-scoped plain imports only search mounts named
                # after the importer's own package, in order, so this stub
                # mount must be the first <dependency>_source entry: the
                # stubs bind as <dependency>_source__<Dir> and pull the real
                # directory modules through their dedicated
                # <dependency>_source_<Dir> mounts without touching the
                # export aliases. The bare copy serves non-scoped owners and
                # must precede every bare source mount below.
                paths.append(dependency + '_source=/exports/' + dependency + '/_plain')
                paths.append('/exports/' + dependency + '/_plain')
    roots = tomllib.loads((ROOT / 'ziran.toml').read_text())['package']['module_roots']
    shared_roots = {path.parent.as_posix() for path in shared_example_exports().values()}
    for root in roots:
        if root in shared_roots:
            virtual = '/kryon/' + root
        elif Path(root).is_relative_to('src'):
            virtual = '/kryon/' + Path(root).relative_to('src').as_posix()
        else:
            continue
        paths.extend([virtual, 'kryon_source=' + virtual])
        # Directory modules all share the file leaf `module`; give each one
        # a longer scope mount so their identities stay distinct, matching
        # the per-dependency loop below. These mounts are also the route the
        # _plain stubs use to reach the real modules.
        base = ROOT / root
        for child in sorted(base.iterdir()) if base.is_dir() else []:
            if (child / 'module.zi').is_file():
                paths.append('kryon_source_' + child.name + '=' + virtual + '/' + child.name)
    for module in modules:
        if module['dependency'] == 'kryon': continue
        dependency = module['dependency']
        for source, root in package_roots(dependency, compiler_package(dependency)):
            paths.extend([root.as_posix(), dependency + '_source=' + root.as_posix()])
            # See the kryon loop above: distinct scope per directory module.
            for child in sorted(source.iterdir()) if source.is_dir() else []:
                if (child / 'module.zi').is_file():
                    paths.append(dependency + '_source_' + child.name + '=' + (root / child.name).as_posix())
    # Authored scene sources live under examples/shared; the compiler search
    # cites that mount so bundled source survives edits at its published path.
    paths.extend(['examples=/examples', '/std'])
    return list(dict.fromkeys(paths))


def compiler_package(dependency, ziran='ziran', core=ROOT):
    if dependency == 'kryon': return Path(core)
    command = [ziran, 'pkg', 'path', dependency]
    if not (PROJECT / 'ziran.local.toml').exists(): command.append('--locked')
    return Path(subprocess.check_output(command, cwd=PROJECT, text=True).strip())


def compiler_embeds(ziran='ziran', core=ROOT):
    modules = enabled_modules()
    embeds = [str(PROJECT / 'src/palette.zi') + '@/palette.zi', str(ROOT / 'examples/shared/widget_context.zi') + '@/widget_context.zi']
    for module in modules:
        package = compiler_package(module['dependency'], ziran, core)
        aliases = export_aliases(module['dependency'], package)
        if module['dependency'] != 'kryon':
            # The wasm loader probes plain imports from the importing file's
            # own directory before the search mounts, so raw package sources
            # would bind directory modules under a scope the link stage never
            # sees. Embed the mirrored tree instead: it carries a stub
            # <Dir>.zi beside every directory module whose import rewrites to
            # the same identity the plain import expects. Every module root
            # maps under the same /packages/<dependency> base, so one embed
            # covers them all and the export aliases stay unchanged.
            mirror = source_mirror(module['dependency'], package)
            if any(path.is_file() for path in mirror.rglob('*')):
                embeds.append(str(mirror) + '@' + '/packages/' + module['dependency'])
            else:
                roots = package_roots(module['dependency'], package)
                for source, virtual in roots:
                    if not any(source != other and source.is_relative_to(other) for other, _ in roots):
                        embeds.append(str(source) + '@' + virtual.as_posix())
        if aliases:
            alias_root = PROJECT / 'build/catalog/exports' / module['dependency']
            embeds.append(str(alias_root) + '@/exports/' + module['dependency'])
    shared_roots = sorted({path.parent for path in shared_example_exports(core).values()})
    for root in shared_roots:
        embeds.append(str(core / root) + '@/kryon/' + root.as_posix())
    embeds.append(str(ROOT / 'examples/shared') + '@/examples/shared')
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
    # A plain import loads <dir>/module.zi by file leaf, which would merge
    # every directory module into one identity; write an importable stub
    # per directory so each keeps its own name.
    plain = PROJECT / 'build/catalog/exports' / dependency / '_plain'
    stubs = []
    for root in roots:
        if not root.is_dir(): continue
        for child in sorted(root.iterdir()):
            if (child / 'module.zi').is_file():
                stub = plain / (child.name + '.zi')
                write(stub, 'using ' + child.name + ' :: #import ' + chr(34) + dependency + '_source_' + child.name + '/module' + chr(34) + ';\n')
                stubs.append((child.name, stub))
    expected = {path for _, path in aliases} | {path for _, path in stubs}
    for stale in (PROJECT / 'build/catalog/exports' / dependency).rglob('*.zi'):
        if stale not in expected: stale.unlink()
    return aliases + stubs


def source_mirror(dependency, package):
    """Browser embed of a dependency's sources with importable dir stubs.

    The ordinary loader resolves plain imports from the importing file's own
    directory first, so a package file beside a directory module would load
    <dir>/module.zi under the directory's own scope while the import is
    rewritten to <package>__<Name>; the identities disagree and linking
    fails. Mirroring the sources with a stub <Dir>.zi file beside every
    directory module lets that first probe bind the expected identity,
    matching the package map's export boundary on native builds.
    """
    manifest = tomllib.loads((package / 'ziran.toml').read_text())
    roots = [Path(root) for root in manifest['package']['module_roots']]
    mirror = PROJECT / 'build/catalog/mirrors' / dependency
    expected = set()
    for index, root_rel in enumerate(roots):
        source = package / root_rel
        if not source.is_dir(): continue
        suffix = root_rel.relative_to(roots[0]) if root_rel.is_relative_to(roots[0]) else root_rel
        stubs = {}
        for child in sorted(source.iterdir()):
            if child.is_dir() and (child / 'module.zi').is_file() and not (source / (child.name + '.zi')).is_file():
                stubs[child.name] = 'using ' + child.name + ' :: #import ' + chr(34) + dependency + '_source_' + child.name + '/module' + chr(34) + ';\n'
        for path in sorted(source.rglob('*')):
            if not path.is_file(): continue
            local = path.relative_to(source)
            target = mirror / (suffix if suffix != Path('.') else Path()) / local
            write(target, path.read_bytes())
            expected.add(target)
        for name, text in stubs.items():
            target = mirror / (suffix if suffix != Path('.') else Path()) / (name + '.zi')
            write(target, text)
            expected.add(target)
    for stale in mirror.rglob('*'):
        if stale.is_file() and stale not in expected: stale.unlink()
    return mirror


def example_source(examples_root, slug, shared_exports):
    if slug in shared_exports:
        return examples_root / shared_exports[slug].relative_to('examples')
    return examples_root / 'gallery/src/examples' / (slug + '.tsx')


def publish_source(entry, examples_root, site, module_id, local, available, *, package=None,
                   dependency='kryon', repository='https://git.waozi.xyz/kryonlabs/kryon.git'):
    relative = entry.relative_to(package) if package is not None else Path('examples') / entry.relative_to(examples_root)
    source_path = relative
    if package is not None:
        source_path = next((virtual / entry.relative_to(root) for root, virtual in package_roots(dependency, package)
                            if entry.is_relative_to(root)), None)
        if source_path is None:
            raise ValueError(f'Exported example must belong to a declared module root: {entry}')
        source_path = source_path.relative_to('/')
    return {'source': repository.removesuffix('.git') + '/src/branch/master/' + relative.as_posix(),
            'sourcePath': source_path.as_posix(),
            'code': 'gallery-sources/' + module_id + '/' + str(local) + '.tsx',
            'language': 'tsx', 'available': available,
            'status': 'Available in the maintained gallery' if available else 'Not available in the current build'}


def example_location(entry):
    source = Path(entry['sourcePath'])
    if source.parts[0] == 'packages':
        dependency = source.parts[1]
        package = compiler_package(dependency)
        virtual_source = Path('/') / source
        roots = [(root, virtual) for root, virtual in package_roots(dependency, package)
                 if virtual_source.is_relative_to(virtual)]
        if not roots:
            raise ValueError(f'Example is outside the declared package roots: {source}')
        root, virtual = max(roots, key=lambda item: len(item[1].parts))
        return package, root / virtual_source.relative_to(virtual)
    source = ROOT / source
    return (PROJECT if source.is_relative_to(PROJECT / 'src') else ROOT), source


def prepare(only=None, *, catalog=None, site=SITE, examples=None, inventory=False):
    examples_root = Path(examples) if examples else ROOT / "examples"
    shared_exports = shared_example_exports()
    shared_imports = {path.as_posix(): 'kryon/examples/' + slug for slug, path in shared_exports.items()}
    modules = enabled_modules(only)
    assert modules and modules[0]['id'] == 'kryon', 'The core Kryon module is required'
    dependencies = tomllib.loads((PROJECT / 'ziran.toml').read_text())['dependencies']
    optional = {}
    for module in modules:
        dependency = module['dependency']
        if dependency != 'kryon' and dependency not in optional:
            package = compiler_package(dependency)
            optional[dependency] = (package, shared_example_exports(package))
    entries, imports, dispatch, styles = [], [], [], []
    sources = {}
    output = Path(catalog) if catalog else PROJECT / 'build/catalog'
    for number, module in enumerate(modules):
        source = PROJECT / module['source']
        text = source.read_text()
        names = [(int(m[1]), m[2]) for m in re.finditer(r'if index == (\d+) \{ return "([^"]+)" \}', text)]
        assert [i for i, _ in names] == list(range(len(names))), source
        alias = 'Examples' + str(number)
        if module.get('styles'):
            imports.append(f'{alias} :: #import "{module["module"]}";')
        if module.get('styles'): styles.append(f'    {alias}.{module["styles"]}(dark)')
        for local, name in names:
            category = next((key for key, indices in module.get('categories', {}).items() if local in indices), module['category'])
            index = len(entries)
            slug = re.sub(r'[^a-z0-9]+', '-', name.lower()).strip('-')
            slug = {'modern-style':'stylepicker', 'button-emphasis':'button', 'dropdown-open':'dropdown', 'circle-ring-triangle-line':'shape', 'page-section-heading-paragraphtext':'page', 'text-selection':'text', 'transition-fade':'transition', 'listboxmulti':'listbox', 'plot-bars':'plotbars', 'plot-lines':'plotlines', 'separator-and-bullet':'separator'}.get(slug, slug)
            dependency = module['dependency']
            if dependency in optional:
                package, exports = optional[dependency]
                if slug not in exports:
                    raise ValueError(f'{dependency} must export its maintained TSX example: examples/{slug}')
                entry = package / exports[slug]
                publication = publish_source(entry, examples_root, site, module['id'], local,
                                             local not in module.get('unavailable', []), package=package,
                                             dependency=dependency, repository=dependencies[dependency]['git'])
                shared_imports[publication['sourcePath']] = dependency + '/examples/' + slug
            else:
                entry = example_source(examples_root, slug, shared_exports)
                publication = publish_source(entry, examples_root, site, module['id'], local,
                                             local not in module.get('unavailable', []))
            sources[publication['sourcePath']] = entry
            apis, options = scene_metadata(entry)
            entries.append({'index': index, 'id': module['id'] + '/' + str(local), 'name': name,
                            'module': module['label'], 'moduleId': module['id'], 'category': category,
                            **publication,
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
        'chip': 'Chip', 'listitem': 'List item', 'skeleton': 'Skeleton',
        'buttongroup': 'Button group', 'confirmbutton': 'Confirm button',
        'copybutton': 'Copy button', 'countdown': 'Countdown', 'emptystate': 'Empty state',
        'keycap': 'Keycap', 'breadcrumb': 'Breadcrumb', 'svgicon': 'SVG icon',
        'gridmulti': 'Grid multi', 'holdbutton': 'Hold button', 'dpad': 'Direction pad',
        'joystick': 'Joystick', 'swipearea': 'Swipe area', 'loading': 'Loading indicator',
        'loadingscreen': 'Loading screen', 'checklist': 'Onboarding checklist',
        'progressring': 'Progress ring', 'timeline': 'Timeline',
        'carousel': 'Carousel', 'tilemap': 'Tile map',
    }
    component_categories = {
        'Actions': ['buttongroup', 'confirmbutton', 'copybutton', 'holdbutton'],
        'Feedback': ['alert', 'badge', 'banner', 'countdown', 'emptystate', 'loading', 'loadingscreen', 'skeleton', 'checklist', 'progressring', 'timeline'],
        'Navigation': ['breadcrumb'],
        'Inputs': ['chip', 'dpad', 'joystick', 'swipearea'],
        'Layout': ['gridmulti'],
        'Media & graphics': ['avatar', 'svgicon', 'carousel', 'tilemap'],
        'Content': ['keycap', 'listitem'],
    }
    core_count = sum(entry["moduleId"] == "kryon" for entry in entries)
    for extra, (slug, name) in enumerate(additional.items()):
        entry = example_source(examples_root, slug, shared_exports)
        index = len(entries)
        publication = publish_source(entry, examples_root, site, 'kryon', slug, True)
        sources[publication['sourcePath']] = entry
        entries.append({'index': index, 'id': 'kryon/' + str(core_count + extra), 'name': name,
                        'module': 'Kryon', 'moduleId': 'kryon', 'category': next(category for category, slugs in component_categories.items() if slug in slugs),
                        **publication,
                        'api': [name.title().replace(' ', '')], 'options': [], 'keyboard': '',
                        'summary': f'A maintained {name} usage example.',
                        'alt': f'Live {name} example',
                        'images': {theme: f'assets/components/kryon-{core_count + extra}-{theme}.webp' for theme in ('light', 'dark')}, 'note': ''})
    for entry in entries:
        entry['related'] = [other['id'] for other in entries if other['id'] != entry['id'] and (
            other['category'] == entry['category'] or set(other['api']) & set(entry['api']))][:6]
    # Inventory supports focused builds and diagnostics without publishing a partial site.
    if inventory: return modules, entries
    missing = [str(path) for path in sources.values() if not path.is_file()]
    if missing:
        raise ValueError('Native TSX gallery sources are required; source publication stopped.\n'
                         + '\n'.join(missing))
    # The catalog uses ordinary package imports for authored screens.
    render_modules = {}
    for entry in entries:
        path = entry['sourcePath']
        if path not in render_modules:
            alias = 'Usage' + str(len(render_modules))
            module_name = shared_imports.get(path)
            if module_name is None:
                module_name = 'gallery/' + Path(path).relative_to('examples/gallery/src').with_suffix('').as_posix()
            imports.append(f'{alias} :: #import "{module_name}";')
            render_modules[path] = alias
        alias = render_modules[path]
        options = f'{alias}.SetExampleOptions(disabled, catalog_reduced_motion); ' if path in shared_imports else ''
        frame = SCENE_FRAMES.get(entry['id'], 'Frame')
        draw = f'{alias}.{frame}'
        capture = SCENE_CAPTURES.get(entry['id'])
        if capture:
            call = (f'if catalog_live {{ App.CallDraw({draw}, session.slot, session.epoch, catalog_viewport); }} '
                    f'else {{ App.CallDraw({alias}.{capture}, session.slot, session.epoch, catalog_viewport); }}')
        else:
            call = f'App.CallDraw({draw}, session.slot, session.epoch, catalog_viewport);'
        dispatch.append(f"    if scene == {entry['index']} {{ {options}{call} return }}")
        write(Path(site) / entry['code'], sources[path].read_bytes())
    expected = {Path(site) / entry['code'] for entry in entries}
    for pattern in ('*.zi', '*.tsx'):
        for path in (Path(site) / 'gallery-sources').rglob(pattern):
            if path not in expected: path.unlink()
    for stale in output.glob('usage_*.zi'): stale.unlink()
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
    # The typed entry `kryon/app_types` is package-internal; `kryon/app`
    # re-exports Window, Frame, Draw and CallDraw for consumers.
    registry = 'App :: #import "kryon/app";\n#import "kryon/geometry"\n#import "kryon/session"\n#import "palette"\n' + '\n'.join(imports) + '\n'
    registry += 'catalog_viewport: Rectangle = .{0.0, 0.0, 480.0, 320.0};\n'
    registry += f'GallerySceneCount :: {len(entries)};\n'
    registry += 'GalleryCompilerSearch :: () -> string {\n    return #string GALLERY_COMPILER_SEARCH\n' + '\n'.join(compiler_paths(modules)) + '\nGALLERY_COMPILER_SEARCH\n}\n'
    registry += string_function('GallerySceneName', lambda e: e['name'])
    registry += string_function('GallerySceneCode', lambda e: sources[e['sourcePath']].read_bytes().decode('utf-8'))
    registry += string_function('GallerySceneId', lambda e: e['id'])
    registry += string_function('GallerySceneSourcePath', lambda e: e['sourcePath'])
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
    registry += 'catalog_live: bool;\n'
    registry += 'SetGalleryPreview :: (bounds: Rectangle, live: bool, now: float32, reduced_motion: bool) {\n    catalog_viewport = bounds; catalog_reduced_motion = reduced_motion; catalog_live = live\n' + '}\n'
    registry += 'ComposeGallery :: (session: Session, scene: s32, disabled: bool) {\n' + '\n'.join(dispatch) + '\n}\n'
    write(output / 'catalog.zi', registry)
    write(output / 'modules.json', json.dumps(modules, indent=2) + '\n')
    write(Path(site) / 'gallery-data.js', 'window.galleryModules = ' + json.dumps([{'id': m['id'], 'label': m['label']} for m in modules]) + ';\nwindow.galleryScenes = ' + json.dumps(entries, ensure_ascii=False, indent=2) + ';\n')
    return modules, entries


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--modules', help='Comma-separated module IDs for an isolated build')
    args = parser.parse_args()
    modules, entries = prepare(None if args.modules is None else set(args.modules.split(',')))
    print('Prepared module integrations: ' + ', '.join(m['label'] for m in modules))
