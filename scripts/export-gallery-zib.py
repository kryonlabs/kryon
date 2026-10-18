#!/usr/bin/env python3
"""Export complete interactive gallery examples as self-contained ZIB files."""
import argparse
import json
import os
from pathlib import Path
import subprocess
import tomllib

import gallery


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, default=gallery.ROOT / 'build/gallery-zib')
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
        parser.error('Build the gallery portable profile before exporting its ZIBs')
    compile_env = {**os.environ, 'ZIRAN_ROOT': str(toolchain)}
    paths = ['--root', str(output / 'app'), '--module-path', str(gallery.PROJECT / 'src'),
             '--module-path', str(gallery.ROOT / 'src/ui'),
             '--module-path', str(gallery.ROOT / 'src/backend')]
    for module in modules:
        name = module['dependency']
        source_package = package(name)
        manifest = tomllib.loads((source_package / 'ziran.toml').read_text())
        for root in manifest['package']['module_roots']:
            paths += ['--module-path', name + '=' + str(source_package / root)]
            paths += ['--module-path', str(source_package / root)]
        # Exports may name a facade rather than a same-named source file.
        # Materialize only those tiny import facades in this build output.
        aliases = output / 'exports' / name
        for exported, target in manifest.get('exports', {}).items():
            for root in manifest['package']['module_roots']:
                source = source_package / root
                if (source / (exported + '.zi')).exists() or (source / exported / 'module.zi').exists():
                    break
            else:
                for root in manifest['package']['module_roots']:
                    try:
                        local = (source_package / target).relative_to(source_package / root).with_suffix('').as_posix()
                    except ValueError:
                        continue
                    aliases.mkdir(parents=True, exist_ok=True)
                    (aliases / (exported + '.zi')).write_text('using Export :: #import "' + local + '";\n')
                    break
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
        source = source.replace('scene_image = "catalog-image.png"', 'scene_image = "assets/pfp.png"')
        app = output / 'app/app.zi'
        app.write_text(source)
        bundle = output / (entry['id'] + '.zib')
        bundle.parent.mkdir(parents=True, exist_ok=True)
        subprocess.run([compiler, 'bundle', *paths, '--entry', 'zib_run:main',
                        '--asset-dir', 'assets=' + str(gallery.PROJECT / 'assets'),
                        '-o', str(bundle), str(gallery.ROOT / 'src/backend/zib_run.zi')],
                       cwd=gallery.PROJECT, env=compile_env, check=True)
        index.append({**entry, 'bundle': bundle.relative_to(output).as_posix()})
        print('Exported ' + entry['id'] + ': ' + entry['name'], flush=True)
    (output / 'index.json').write_text(json.dumps(index, ensure_ascii=False, indent=2) + '\n')
    print(f'Exported {len(index)} complete ZIB examples to {output}')


if __name__ == '__main__':
    main()
