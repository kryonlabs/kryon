#!/usr/bin/env python3
"""Packaging reads maintained example edits and never rewrites their source."""
import re
from pathlib import Path
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'scripts'))
from gallery import example_location, prepare, shared_example_exports


def main():
    scratch = ROOT / 'build/scratch/widget-example-source'
    scratch.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(dir=scratch) as directory:
        fixture = Path(directory)
        examples = fixture / 'examples'
        _, inventory = prepare(inventory=True)
        def source_for(entry):
            if entry['sourcePath'].startswith('packages/'):
                return example_location(entry)[1]
            return examples / Path(entry['sourcePath']).relative_to('examples')
        # Source publication must fail before changing the previous output when
        # a maintained native screen is missing, even if a .zi app exists.
        button = source_for(next(entry for entry in inventory if entry['id'] == 'kryon/0'))
        button.parent.mkdir(parents=True)
        legacy = button.with_suffix('.zi')
        legacy.write_text('old Ziran app must not be published')
        site, catalog = fixture / 'site', fixture / 'catalog'
        sentinel = site / 'gallery-sources/previous.tsx'
        sentinel.parent.mkdir(parents=True)
        sentinel.write_text('previous published source')
        try:
            prepare(catalog=catalog, site=site, examples=examples)
        except ValueError as error:
            assert 'Native TSX gallery sources are required' in str(error)
            assert str(button) in str(error)
        else:
            raise AssertionError('Missing authored TSX silently accepted a legacy app')
        assert sentinel.read_text() == 'previous published source'
        assert not catalog.exists(), 'Partial catalog published before validation'
        # These are temporary authored-source fixtures, not generated product
        # screens. The publisher must preserve every byte, name and variant.
        code = ('import * as App from "kryon/app";\n'
                'import * as Text from "kryon/text";\n'
                'export function SetExampleOptions(disabled: boolean, reducedMotion: boolean): void {}\n'
                'export function Frame(frame: App.Frame): void {\n'
                '    Text.Draw(frame, {text: "Maintained fixture edit"});\n' + '}\n')
        for entry in inventory:
            if entry['sourcePath'].startswith('packages/'):
                continue
            path = source_for(entry)
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_text(code)
        button.write_bytes(('/* Maintained café edit */\n' + code).replace('\n', '\r\n').encode('utf-8'))
        originals = {path: path.read_bytes() for path in examples.rglob('*') if path.is_file()}
        originals.update({source_for(entry): source_for(entry).read_bytes() for entry in inventory
                          if entry['sourcePath'].startswith('packages/')})
        _, entries = prepare(catalog=catalog, site=site, examples=examples)
        assert len(entries) == len(inventory)
        for field in ('id', 'name', 'moduleId', 'images', 'available'):
            assert [entry[field] for entry in entries] == [entry[field] for entry in inventory], field
        assert all(path.read_bytes() == source for path, source in originals.items()), 'Packaging overwrote authored source'
        assert not sentinel.exists(), 'Stale publication survived successful preparation'
        assert not list((site / 'gallery-sources').rglob('*.zi'))
        for entry in entries:
            source = source_for(entry)
            assert (site / entry['code']).read_bytes() == source.read_bytes(), entry['id']
            assert entry['code'].endswith('.tsx') and entry['language'] == 'tsx'
        variants = [entry for entry in entries if entry['name'] in ('Button', 'Button emphasis')]
        assert len(variants) == 2 and len({entry['sourcePath'] for entry in variants}) == 1
        registry = (catalog / 'catalog.zi').read_text()
        assert not list(catalog.glob('usage_*.zi')), 'Generated screen source returned'
        shared_modules = {path.as_posix(): 'kryon/examples/' + slug
                          for slug, path in shared_example_exports().items()}
        aliases = {}
        frames = {}
        for entry in entries:
            line = next(line for line in registry.splitlines()
                        if line.startswith(f"    if scene == {entry['index']} {{"))
            match = re.search(r'App.CallDraw\((Usage\d+)\.([A-Za-z]+),', line)
            assert match, ('authored Frame dispatch missing', entry['id'])
            frames[entry['id']] = match[2]
            prior = aliases.setdefault(entry['sourcePath'], match[1])
            assert prior == match[1], 'Source variants lost shared module identity'
            module = shared_modules.get(entry['sourcePath'])
            if entry['sourcePath'].startswith('packages/'):
                module = entry['moduleId'] + '/examples/' + Path(entry['sourcePath']).stem.removesuffix('_scene')
            if module is not None:
                assert f'{match[1]}.SetExampleOptions(disabled, catalog_reduced_motion);' in line
            else:
                module = 'gallery/' + Path(entry['sourcePath']).relative_to('examples/gallery/src').with_suffix('').as_posix()
            assert f'{match[1]} :: #import "{module}";' in registry
        assert len({frames[entry['id']] for entry in variants}) == 2, 'Distinct button compositions dispatched the same frame'
        prepare(catalog=catalog, site=site, examples=examples)
        assert all(path.read_bytes() == source for path, source in originals.items()), 'Repeated preparation overwrote authored source'
    print('Local example source test passed: edits preserved and shared by catalog variants/site snippets')


if __name__ == '__main__': main()
