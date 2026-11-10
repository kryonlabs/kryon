#!/usr/bin/env python3
"""Packaging reads maintained example edits and never rewrites their source."""
import json
import re
from pathlib import Path
import shutil
import sys
import tempfile
import tomllib

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'scripts'))
from gallery import prepare


def main():
    scratch = ROOT / 'build/scratch/widget-example-source'
    scratch.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(dir=scratch) as directory:
        fixture = Path(directory)
        examples = fixture / 'examples'
        # Copy source fixtures only, without caches, binaries or owner settings.
        for source in (ROOT / 'examples').glob('*/*.zi'):
            if source.name != source.parent.name + '.zi': continue
            manifest = tomllib.loads((source.parent / 'ziran.toml').read_text())['package']
            assert manifest['entry'] == source.name, source
            assert manifest['module_roots'] == ['.'], source
            assert 'bridge_modules' not in manifest, source
            target = examples / source.parent.name / source.name
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(source, target)
        button = examples / 'button/button.zi'
        edit = button.read_text().replace('Continue', 'Maintained fixture edit')
        button.write_text(edit)
        originals = {path: path.read_bytes() for path in examples.glob('*/*.zi')}
        site, catalog = fixture / 'site', fixture / 'catalog'
        _, entries = prepare(catalog=catalog, site=site, examples=examples)
        assert len(entries) == 101
        assert all(path.read_bytes() == source for path, source in originals.items()), 'Packaging overwrote maintained source'
        for entry in entries:
            assert (site / entry['code']).read_bytes() == (ROOT / entry['sourcePath']).read_bytes(), entry['id']
        variants = [entry for entry in entries if entry['name'] in ('Button', 'Button emphasis')]
        assert len(variants) == 2
        for entry in variants:
            assert (site / entry['code']).read_text() == edit
        registry = (catalog / 'catalog.zi').read_text()
        assert '.Compose(session' not in registry, 'default renderer bypasses maintained Frames'
        aliases = {}
        for entry in entries:
            match = re.search(r'if scene == ' + str(entry['index']) + r' \{ (Usage\d+)\.SetExampleOptions.*?\1\.Frame\(session, catalog_viewport\)', registry)
            assert match, ('default Frame dispatch missing', entry['id'])
            alias = match[1]
            module = re.search(alias + r' :: #import "([^"]+)";', registry)[1]
            maintained = (ROOT / entry['sourcePath']).read_text()
            assert (catalog / (module + '.zi')).read_text() == maintained.replace('#program_export\n', ''), entry['id']
            prior = aliases.setdefault(entry['sourcePath'], alias)
            assert prior == alias, 'source variants lost shared module identity'
        button_aliases = {aliases[entry['sourcePath']] for entry in variants}
        assert len(button_aliases) == 1
        module = re.search(next(iter(button_aliases)) + r' :: #import "([^"]+)";', registry)[1]
        assert 'Maintained fixture edit' in (catalog / (module + '.zi')).read_text(), 'edit absent from default render Frame'
        prepare(catalog=catalog, site=site, examples=examples)
        assert all(path.read_bytes() == source for path, source in originals.items()), 'Repeated preparation overwrote maintained source'
    print('Local example source test passed: edits preserved and shared by catalog variants/site snippets')


if __name__ == '__main__': main()
