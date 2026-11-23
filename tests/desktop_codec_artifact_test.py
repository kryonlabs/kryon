"""Build and consume the desktop codec outputs through ordinary artifacts."""
import argparse
import json
import os
from pathlib import Path
import subprocess
import sys

from toolchain import ROOT, ZIRAN, ZIRAN_ROOT, pinned, kryon_only

sys.path.insert(0, str(ZIRAN_ROOT / 'scripts'))
sys.path.insert(0, str(ROOT / 'scripts'))
from ziran_artifacts import Artifact
from renderer_artifact import renderer_metadata, renderer_link

DECODER_ARCHIVES = ['libkryonwebp.a', 'libavif.a', 'libopus.a']
ENCODER_ARCHIVES = ['libkryonwebp.a', 'libavif.a', 'libaom.a', 'libopus.a']


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--encode', action='store_true',
                        help='also build the AVIF encode artifact and link a consumer against it')
    args = parser.parse_args()
    out = ROOT / 'build/scratch/desktop-codec-artifact'
    project = out / 'consumer'
    project.mkdir(parents=True, exist_ok=True)
    env = os.environ.copy()
    for key in ('DISPLAY', 'WAYLAND_DISPLAY', 'XAUTHORITY', 'DBUS_SESSION_BUS_ADDRESS', 'SESSION_MANAGER'):
        env.pop(key, None)
    env['YUE_DESKTOP_RECOVERY'] = '0'
    (project / 'ziran.toml').write_text(pinned('''[package]
name = "desktop_codec_consumer"
[toolchain]
git = "https://github.com/ziranlang/ziran.git"
ref = "master"
[dependencies.kryon]
git = "https://github.com/kryonlabs/kryon.git"
ref = "master"
'''))
    overrides = {'kryon': str(ROOT), 'ziran': str(ZIRAN_ROOT)}
    for package in ('audio', 'avif', 'webp', 'image_codec'):
        overrides[package] = subprocess.check_output(
            [str(ZIRAN), 'pkg', 'path', package, '--offline'], cwd=ROOT, env=env, text=True).strip()
    (project / 'ziran.local.toml').write_text(kryon_only('[overrides]\n' + ''.join(
        f'{name} = {json.dumps(path)}\n' for name, path in overrides.items())))
    subprocess.run([str(ZIRAN), 'lock', '--offline'], cwd=project, env=env, check=True)

    recipe_archives = {
        'renderer-desktop': DECODER_ARCHIVES,
        'renderer-desktop-encode': ENCODER_ARCHIVES,
    }

    def artifact(recipe):
        descriptor = Path(subprocess.check_output([str(ZIRAN), 'pkg', 'artifact',
            'kryon:' + recipe, '--platform', 'linux-x86_64-gnu', '--offline'],
            cwd=project, env=env, text=True).strip())
        resolved = Artifact(descriptor)
        meta = renderer_metadata(resolved)
        assert [Path(path).name for path in meta['archives']] == recipe_archives[recipe]
        return resolved, meta

    default, meta = artifact('renderer-desktop')
    assert 'resources/LICENSE-libaom' not in default.rows
    assert 'resources/PATENTS-libaom' not in default.rows
    for license_name in ('LICENSE-opus', 'LICENSE-libwebp', 'PATENTS-libwebp',
                         'LICENSE-libavif', 'LICENSE-dav1d'):
        assert default.output('resources/' + license_name).stat().st_size > 0
    # The source and saved-IR paths must both use exactly these artifact outputs.
    source = ROOT / 'tests/desktop_codec_artifact_test.zi'
    ir = out / 'ir'
    subprocess.run([str(ZIRAN), 'ir', '--project', '--root', str(ROOT / 'tests'),
        '--module-path', str(ROOT / 'tests'), '-o', str(ir), str(source)], cwd=ROOT, env=env, check=True)

    def consume(name, meta, forms):
        flags = renderer_link(meta)
        assert not any('raylib' in flag.lower() for flag in flags)
        for form in forms:
            generated = out / (name + '-' + form)
            command = [str(ZIRAN), 'build', '--target=c', '-o', str(generated)]
            if form == 'source':
                command += ['--project', '--root', str(ROOT / 'tests'), '--module-path', str(ROOT / 'tests'), str(source)]
            else:
                command += ['--root', str(ir), '--module-path', str(ir), str(ir / 'desktop_codec_artifact_test.zir')]
            subprocess.run(command, cwd=ROOT, env=env, check=True)
            sources = sorted(generated.rglob('*.c'))
            binary = generated / 'test'
            link = [env.get('CC', 'cc'), '-std=c11', '-O1', '-ffunction-sections', '-fdata-sections',
                    '-I' + str(ZIRAN_ROOT / 'include'),
                    *['-iquote' + str(path) for path in sorted({source.parent for source in sources})],
                    *sources, '-Wl,--gc-sections', *flags, '-o', binary]
            (out / (name + '-' + form + '-link.json')).write_text(json.dumps(list(map(str, link)), indent=2) + '\n')
            subprocess.run(list(map(str, link)), cwd=ROOT, env=env, check=True)
            dependencies = subprocess.check_output(['ldd', str(binary)], env=env, text=True)
            assert 'raylib' not in dependencies.lower()
            subprocess.run([str(binary)], cwd=ROOT, env=env, check=True, timeout=30)

    consume('desktop', meta, ('source', 'saved'))
    print('Desktop artifact descriptor, dav1d-only AVIF decode, WebP and Opus passed', flush=True)
    if args.encode:
        encode, encode_meta = artifact('renderer-desktop-encode')
        assert encode.output('resources/LICENSE-libaom').stat().st_size > 0
        assert encode.output('resources/PATENTS-libaom').stat().st_size > 0
        consume('encode', encode_meta, ('source',))
        print('AVIF encode artifact descriptor and native consumer passed', flush=True)


if __name__ == '__main__':
    main()
