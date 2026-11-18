#!/usr/bin/env python3
"""Export complete interactive gallery examples as self-contained ZX files."""
import argparse
import json
import os
from pathlib import Path
import subprocess
import tomllib

import gallery


def scoped(package, module):
    """Name a module loaded through `--module-path PACKAGE=DIR`, as Ziran does."""
    digest = 1469598103934665603
    for byte in package.encode():
        digest = ((digest ^ byte) * 1099511628211) & 0xffffffffffffffff
    return f'p{digest:016x}_{module}'


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, default=gallery.ROOT / 'build/gallery-zx')
    parser.add_argument('--example', action='append', help='Example ID, for example kryon/0')
    parser.add_argument('--ziran', default=os.environ.get('ZIRAN', 'ziran'))
    args = parser.parse_args()
    output = args.output.resolve()
    modules, entries = gallery.prepare(catalog=output / 'catalog', site=output / 'sources')

    def package(name):
        command = [args.ziran, 'pkg', 'path', name]
        if not (gallery.PROJECT / 'ziran.local.toml').exists():
            command.append('--locked')
        return Path(subprocess.check_output(command, cwd=gallery.PROJECT, text=True).strip())

    # The installed command resolves packages; the project's configured
    # compiler writes the bundle. PATH may carry a different IR format.
    toolchain = package('ziran')
    compiler = toolchain / 'build/bin/ziran'
    if not compiler.is_file():
        parser.error('Build the gallery portable profile before exporting its ZXs')
    compile_env = {**os.environ, 'ZIRAN_ROOT': str(toolchain)}
    paths = ['--root', str(output / 'app'), '--module-path', str(gallery.PROJECT / 'src'),
             '--module-path', str(gallery.ROOT / 'src/ui'),
             '--module-path', str(gallery.ROOT / 'src/backend'),
             '--module-path', str(gallery.ROOT / 'examples/shared')]
    renames = {}
    for module in modules:
        name = module['dependency']
        source_package = package(name)
        manifest = tomllib.loads((source_package / 'ziran.toml').read_text())
        roots = [source_package / root for root in manifest['package']['module_roots']]
        for root in roots:
            paths += ['--module-path', name + '=' + str(root)]
            paths += ['--module-path', str(root)]
        # An export may name a module other than its same-named file, as
        # `modal` names modal_frame.zi, or a module with no file of its name.
        aliases = output / 'exports' / name
        for exported, target in manifest.get('exports', {}).items():
            source = source_package / target
            if any(source in (root / (exported + '.zi'), root / exported / 'module.zi') for root in roots):
                continue
            # Name the target as it is known in the deepest root holding it:
            # a slash in an import names another package.
            root = max((root for root in roots if source.is_relative_to(root)),
                       key=lambda root: len(root.parts), default=None)
            if root is None:
                continue
            local = source.relative_to(root).with_suffix('').as_posix()
            if any((root / (exported + '.zi')).exists() or (root / exported / 'module.zi').exists() for root in roots):
                # The package keeps its own same-named module; the example
                # imports the export's target by that target's name.
                renames['#import "' + name + '/' + exported + '"'] = '#import "' + name + '/' + local + '"'
            else:
                facade = aliases / (exported + '.zi')
                facade.parent.mkdir(parents=True, exist_ok=True)
                facade.write_text('using Export :: #import "' + local + '";\n')
        if aliases.is_dir():
            paths += ['--module-path', name + '=' + str(aliases)]
    (output / 'app').mkdir(exist_ok=True)
    index = []
    for entry in entries:
        if args.example and entry['id'] not in args.example:
            continue
        if not entry['available']:
            raise SystemExit('Cannot export an incomplete example: ' + entry['id'])
        source = (output / 'sources' / entry['code']).read_text()
        source = source.replace('#import "preview_protocol"\n', '')
        source = source.replace('    example_input := PreviewInputRead()\n', '')
        source = source.replace('example_input.theme == 2', 'false')
        source = source.replace('example_input.disabled', 'false')
        source = source.replace('scene_image = "catalog-image.png"', 'scene_image = "assets/pfp.webp"')
        for package_import, target_import in renames.items():
            source = source.replace(package_import, target_import)
        app = output / 'app/app.zi'
        app.write_text(source)
        bundle = output / (entry['id'] + '.zx')
        bundle.parent.mkdir(parents=True, exist_ok=True)
        subprocess.run([compiler, 'bundle', *paths, '--entry', scoped('kryon', 'zx_run') + ':main',
                        '--bind', scoped('kryon', 'frame_entry') + ':ProjectFrame=app:Frame',
                        '--asset-dir', 'assets=' + str(gallery.PROJECT / 'assets'),
                        '-o', str(bundle), str(gallery.ROOT / 'src/backend/zx_run.zi'), str(app)],
                       cwd=gallery.PROJECT, env=compile_env, check=True)
        index.append({**entry, 'bundle': bundle.relative_to(output).as_posix()})
        print('Exported ' + entry['id'] + ': ' + entry['name'], flush=True)
    (output / 'index.json').write_text(json.dumps(index, ensure_ascii=False, indent=2) + '\n')
    print(f'Exported {len(index)} complete ZX examples to {output}')


if __name__ == '__main__':
    main()
