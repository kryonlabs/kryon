#!/usr/bin/env python3
"""Export complete interactive gallery examples as self-contained ZX files."""
import argparse
import json
import os
from pathlib import Path
import subprocess

import gallery


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, default=gallery.ROOT / 'build/gallery-zx')
    parser.add_argument('--example', action='append', help='Example ID, for example kryon/0')
    parser.add_argument('--ziran', default=os.environ.get('ZIRAN', 'ziran'))
    args = parser.parse_args()
    output = args.output.resolve()
    _, entries = gallery.prepare(inventory=True)
    if args.example:
        selected = set(args.example)
        unknown = selected - {entry['id'] for entry in entries}
        if unknown:
            parser.error('Unknown example IDs: ' + ', '.join(sorted(unknown)))
        entries = [entry for entry in entries if entry['id'] in selected]
    for entry in entries:
        _, source = gallery.example_location(entry)
        if not entry['available'] or not source.is_file():
            parser.error(f"Cannot export {entry['id']}: available authored TSX is required at {source}")

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
    index = []
    for entry in entries:
        # The authored TSX module is an input to the ordinary project compiler.
        # Its native main owns the standalone window; no app.zi is synthesized.
        project, source = gallery.example_location(entry)
        root = project / 'src' if project == gallery.PROJECT else source.parent
        module = source.relative_to(root).with_suffix('').as_posix()
        bundle = output / (entry['id'] + '.zx')
        bundle.parent.mkdir(parents=True, exist_ok=True)
        subprocess.run([compiler, 'bundle', '--project', '--define', 'KRYON_ZX', '--root', str(root),
                        '--entry', module + ':main',
                        '--asset-dir', 'assets=' + str(gallery.PROJECT / 'assets'),
                        '-o', str(bundle), str(source)],
                       cwd=project, env=compile_env, check=True)
        index.append({**entry, 'bundle': bundle.relative_to(output).as_posix()})
        print('Exported ' + entry['id'] + ': ' + entry['name'], flush=True)
    (output / 'index.json').write_text(json.dumps(index, ensure_ascii=False, indent=2) + '\n')
    print(f'Exported {len(index)} complete ZX examples to {output}')


if __name__ == '__main__':
    main()
