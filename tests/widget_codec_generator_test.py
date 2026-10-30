"""Exercise generated native codecs with partial and legacy provided modules."""
from pathlib import Path
import os
import shlex
import subprocess
import sys

repo, work, ziran_root = map(Path, sys.argv[1:])
ziran = ziran_root / 'build/bin/ziran'
generator = repo / 'scripts/generate-widget-codecs.py'
shared = ['--import', '"std/bundle_host"', '--import', '"std/byte_text_linux"',
          '--import', '"hosted_values"', '--import', '"widget_codec_leaf"']


def run(command):
    return subprocess.run([str(part) for part in command], check=True)


def generate(output, arguments, imports=()):
    run([sys.executable, generator, '--output', output, *shared, *imports, *arguments])


partial = work / 'partial_leaf_codec.zi'
legacy = work / 'legacy_leaf_codec.zi'
leaf_args = ['--source', repo / 'tests/widget_codec_leaf.zi', '--root', 'CodecLeaf']
generate(partial, [*leaf_args, '--defer-method', 'WidgetEncodeCodecLeaf',
                   '--defer-method', 'WidgetValidCodecLeaf'])
generate(legacy, leaf_args)
assert '// Widget codec schema:' in partial.read_text()
assert '// Widget codec schema:' not in legacy.read_text()
for label, provided in [('partial', partial), ('legacy', legacy)]:
    output = work / (label + '_envelope_codec.zi')
    generate(output, ['--source', repo / 'tests/widget_codec_envelope.zi',
                      '--root', 'CodecEnvelope', '--uses', provided],
             ['--import', '"widget_codec_envelope"', '--import', '"' + provided.stem + '"'])
    text = output.read_text()
    for method in ['WidgetEncodeCodecLeaf', 'WidgetValidCodecLeaf']:
        assert (method + ' ::' in text) == (label == 'partial')
    assert 'WidgetDecodeCodecLeaf ::' not in text
    case = work / label
    case.mkdir()
    source = (repo / 'tests/widget_codec_partial_test.zi').read_text()
    source = source.replace('partial_leaf_codec', provided.stem)
    source = source.replace('partial_envelope_codec', output.stem)
    (case / 'codec_case.zi').write_text(source)
    paths = ['--root', case, '--module-path', repo / 'tests', '--module-path', work,
             '--module-path', repo / 'src/backend', '--module-path', ziran_root / 'std']
    run([ziran, 'ir', *paths, '-o', case / 'ir', case / 'codec_case.zi'])
    for mode in ['source', 'saved']:
        inputs = paths if mode == 'source' else ['--root', case / 'ir', '--module-path', case / 'ir']
        module = case / 'codec_case.zi' if mode == 'source' else case / 'ir/codec_case.zir'
        for target, compiler, standard in [('c', 'CC', 'c11')]:
            native = case / (target + '-' + mode)
            run([ziran, 'build', '--target=' + target, '--entry', 'codec_case:main',
                 *inputs, '-o', native, module])
            suffix = '.c'
            command = shlex.split(os.environ.get(compiler, 'cc'))
            run([*command, '-O2', '-std=' + standard, '-ffunction-sections', '-fdata-sections',
                 '-I' + str(ziran_root / 'include'), '-I' + str(native),
                 *sorted(native.glob('*' + suffix)), '-Wl,--gc-sections', '-lm',
                 '-o', native / 'run'])
            run([native / 'run'])
# A bad schema must not generate writes past the provided storage layout.
broken = work / 'broken.zi'
broken.write_text(partial.read_text().replace('fields: [2]HostField', 'fields: [1]HostField'))
result = subprocess.run([sys.executable, str(generator), '--source',
                         str(repo / 'tests/widget_codec_envelope.zi'), '--root', 'CodecEnvelope',
                         '--uses', str(broken), '--output', str(work / 'broken_output.zi')],
                        text=True, capture_output=True)
assert result.returncode != 0 and 'metadata does not match its storage' in result.stderr
# Importing a module for reuse must never overwrite that same module.
before = partial.read_bytes()
result = subprocess.run([sys.executable, str(generator), '--source',
                         str(repo / 'tests/widget_codec_leaf.zi'), '--root', 'CodecLeaf',
                         '--uses', str(partial), '--output', str(partial)], text=True, capture_output=True)
assert result.returncode != 0 and partial.read_bytes() == before
print('Partial and legacy codec providers: C source/saved IR, nested slices, fixed arrays, UTF-8, reordered fields and invalid data passed')
