"""Compile gallery registrations without the omitted packages' module paths."""
import json
import os
from pathlib import Path
import subprocess
import sys
import tomllib

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'scripts'))
import gallery

PROJECT = gallery.PROJECT
OUTPUT = ROOT / 'build/scratch/gallery-modules'
ENV = {k: v for k, v in os.environ.items() if k not in {'DISPLAY', 'WAYLAND_DISPLAY', 'XAUTHORITY', 'DBUS_SESSION_BUS_ADDRESS'}}


def package(name):
    command = ['ziran', 'pkg', 'path', name]
    if not (PROJECT / 'ziran.local.toml').exists(): command.append('--locked')
    return Path(subprocess.check_output(command, cwd=PROJECT, env=ENV, text=True).strip())


ziran = package('ziran')
core = package('kryon')
results = []
for label, selected in [('core-only', {'kryon'}), ('without-plot', {'kryon', 'data_views'}), ('without-data-views', {'kryon', 'plot'})]:
    target = OUTPUT / label
    gallery.prepare(catalog=target / 'catalog', site=target / 'site')
    modules, entries = gallery.prepare(selected, catalog=target / 'catalog', site=target / 'site')
    assert {m['id'] for m in modules} == selected
    registry = (target / 'catalog/catalog.zi').read_text()
    omitted = {'plot', 'data_views'} - selected
    for name in omitted:
        assert name + '_examples' not in registry
        assert not any(e['moduleId'] == name for e in entries)
    assert {p.relative_to(target / 'site').as_posix() for p in (target / 'site/gallery-sources').rglob('*.tsx')} == {entry['code'] for entry in entries}
    roots = [PROJECT / 'src', 'gallery=' + str(PROJECT / 'src'), target / 'catalog', ziran / 'std']
    # Each package's public names are its manifest exports, which need not
    # match its source files (kryon/button_group is buttongroup/buttongroup.zi):
    # an alias module per export imports the source through <name>_source.
    for name in selected:
        path = core if name == 'kryon' else package(name)
        manifest = tomllib.loads((path / 'ziran.toml').read_text())
        for source in (path / root for root in manifest['package']['module_roots']):
            roots.extend([source, name + '_source=' + str(source)])
        if gallery.export_aliases(name, path):
            for alias in dict.fromkeys((name, manifest['package']['name'])):
                roots.append(alias + '=' + str(PROJECT / 'build/catalog/exports' / name))
    command = [str(ziran / 'build/bin/zi2zir'), '--root', str(PROJECT / 'tests')]
    for root in roots: command.extend(['--module-path', str(root)])
    command.extend(['--entry', 'modules:main', '-o', str(target / 'ir'), str(PROJECT / 'tests/modules.zi')])
    subprocess.run(command, env=ENV, check=True)
    results.append({'configuration': label, 'modules': sorted(selected), 'examples': len(entries), 'compiled': True})
    print('Passed ' + label + ': only configured module imports and examples', flush=True)
OUTPUT.mkdir(parents=True, exist_ok=True)
(OUTPUT / 'results.json').write_text(json.dumps(results, indent=2) + '\n')
