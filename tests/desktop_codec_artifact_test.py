"""Build and consume the desktop codec outputs through an ordinary artifact."""
import json
import os
from pathlib import Path
import subprocess
import sys

from toolchain import ROOT, ZIRAN, ZIRAN_ROOT

sys.path.insert(0, str(ZIRAN_ROOT / 'scripts'))
sys.path.insert(0, str(ROOT / 'scripts'))
from ziran_artifacts import Artifact
from renderer_artifact import renderer_metadata, renderer_link


def main():
    out = ROOT / 'build/scratch/desktop-codec-artifact'
    project = out / 'consumer'
    project.mkdir(parents=True, exist_ok=True)
    env = os.environ.copy()
    for key in ('DISPLAY', 'WAYLAND_DISPLAY', 'XAUTHORITY', 'DBUS_SESSION_BUS_ADDRESS', 'SESSION_MANAGER'):
        env.pop(key, None)
    env['YUE_DESKTOP_RECOVERY'] = '0'
    (project / 'ziran.toml').write_text('''[package]
name = "desktop_codec_consumer"
[toolchain]
git = "https://github.com/ziranlang/ziran.git"
ref = "master"
[dependencies.kryon]
git = "https://github.com/kryonlabs/kryon.git"
ref = "master"
''')
    overrides = {'kryon': str(ROOT), 'ziran': str(ZIRAN_ROOT)}
    for package in ('audio', 'avif', 'webp', 'image_codec'):
        overrides[package] = subprocess.check_output(
            [str(ZIRAN), 'pkg', 'path', package, '--offline'], cwd=ROOT, env=env, text=True).strip()
    (project / 'ziran.local.toml').write_text('[overrides]\n' + ''.join(
        f'{name} = {json.dumps(path)}\n' for name, path in overrides.items()))
    subprocess.run([str(ZIRAN), 'lock', '--offline'], cwd=project, env=env, check=True)
    descriptor = Path(subprocess.check_output([str(ZIRAN), 'pkg', 'artifact',
        'kryon:renderer-desktop', '--platform', 'linux-x86_64-gnu', '--offline'],
        cwd=project, env=env, text=True).strip())
    artifact = Artifact(descriptor)
    meta = renderer_metadata(artifact)
    assert [Path(path).name for path in meta['archives']] == [
        'libkryonwebp.a', 'libavif.a', 'libaom.a', 'libopus.a']
    flags = renderer_link(meta)
    assert not any('raylib' in flag.lower() for flag in flags)
    for license_name in ('LICENSE-opus', 'LICENSE-libwebp', 'PATENTS-libwebp',
                         'LICENSE-libavif', 'LICENSE-libaom', 'PATENTS-libaom', 'LICENSE-dav1d'):
        assert artifact.output('resources/' + license_name).stat().st_size > 0
    # The source and saved-IR paths must both use exactly these artifact outputs.
    source = ROOT / 'tests/desktop_codec_artifact_test.zi'
    ir = out / 'ir'
    subprocess.run([str(ZIRAN), 'ir', '--project', '--root', str(ROOT / 'tests'),
        '--module-path', str(ROOT / 'tests'), '-o', str(ir), str(source)], cwd=ROOT, env=env, check=True)
    for form in ('source', 'saved'):
        generated = out / form
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
        (out / (form + '-link.json')).write_text(json.dumps(list(map(str, link)), indent=2) + '\n')
        subprocess.run(list(map(str, link)), cwd=ROOT, env=env, check=True)
        dependencies = subprocess.check_output(['ldd', str(binary)], env=env, text=True)
        assert 'raylib' not in dependencies.lower()
        subprocess.run([str(binary)], cwd=ROOT, env=env, check=True, timeout=30)
    print('Desktop artifact descriptor, codec archives/licenses and source/saved-IR native consumers passed')


if __name__ == '__main__':
    main()
