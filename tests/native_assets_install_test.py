"""Declared assets follow the ordinary native build/install route, from neutral cwd."""
import os
from pathlib import Path
import shutil
import stat
import subprocess
import tomllib
from toolchain import ZIRAN_ROOT

ROOT = Path(__file__).resolve().parents[1]
WORK = ROOT / 'build/scratch/native-assets-install'
ZIRAN = ZIRAN_ROOT / 'build/bin/ziran'


def main():
    os.umask(0o077)
    WORK.mkdir(parents=True, exist_ok=True)
    project = WORK / 'project'
    (project / 'src').mkdir(parents=True, exist_ok=True)
    (project / 'assets/icons').mkdir(parents=True, exist_ok=True)
    shutil.copyfile(ROOT / 'icons/ui.webp', project / 'assets/icons/ui.webp')
    (project / 'assets').chmod(0o700)
    (project / 'assets/icons').chmod(0o700)
    (project / 'assets/icons/ui.webp').chmod(0o600)
    source = (ROOT / 'tests/native_asset_image_test.zi').read_text()
    source = source.replace('#import "tree_draw"', '#import "kryon/Frame"').replace('#import "Widgets"', '#import "kryon/Widgets"').replace('#import "cairo_raster"', '#import "kryon/Cairo"')
    (project / 'src/host.zi').write_text(source)
    manifest = '''[package]
name = "asset_probe"
tool = "kryon"
entry = "src/host.zi"
module_roots = ["src"]
[install]
bin = "asset_probe"
[toolchain]
git = "https://github.com/ziranlang/ziran.git"
ref = "master"
[dependencies.kryon]
git = "https://github.com/kryonlabs/kryon.git"
ref = "master"
[tool.kryon]
default_profile = "desktop"
[tool.kryon.profiles.desktop]
backend = "desktop"
host = "src/host.zi"
assets = "assets"
[tool.kryon.install]
profile = "desktop"
'''
    (project / 'ziran.toml').write_text(manifest)
    overrides = {'kryon': ROOT, 'ziran': ZIRAN_ROOT}
    local = ROOT / 'ziran.local.toml'
    if local.exists():
        overrides.update({name: (ROOT / path).resolve() for name, path in tomllib.loads(local.read_text()).get('overrides', {}).items()})
    (project / 'ziran.local.toml').write_text('[overrides]\n' + ''.join(f'{name} = "{path}"\n' for name, path in overrides.items()))
    env = os.environ.copy()
    for name in ('DISPLAY', 'WAYLAND_DISPLAY', 'XAUTHORITY', 'DBUS_SESSION_BUS_ADDRESS', 'SESSION_MANAGER', 'ICEAUTHORITY'):
        env.pop(name, None)
    env['YUE_DESKTOP_RECOVERY'] = '0'
    env['XDG_CONFIG_HOME'] = str(WORK / 'config')
    neutral = WORK / 'neutral'
    neutral.mkdir(exist_ok=True)
    prefix = WORK / 'prefix-private'
    if prefix.exists():
        shutil.rmtree(prefix)
    prefix.mkdir(mode=0o700)

    def run(*args, cwd=project):
        subprocess.run([str(arg) for arg in args], cwd=cwd, env=env, check=True)

    run(ZIRAN, 'lock')
    run(ZIRAN, 'tool', 'kryon', 'build', '--profile', 'desktop')
    program = project / 'build/asset_probe-desktop'
    run(program, cwd=neutral)
    run(ZIRAN, 'install', '--prefix', prefix)
    installed = prefix / 'bin/asset_probe'
    def check_modes(installation, prefix_mode):
        assert stat.S_IMODE(installation.stat().st_mode) == prefix_mode
        for name in ('bin', 'share', 'share/applications', 'share/kryon',
                     'share/kryon/asset_probe', 'share/kryon/asset_probe/assets',
                     'share/kryon/asset_probe/assets/icons'):
            assert stat.S_IMODE((installation / name).stat().st_mode) == 0o755, name
        for name in ('share/kryon/asset_probe/assets/.kryon-prefix',
                     'share/kryon/asset_probe/assets/icons/ui.webp',
                     'share/kryon/asset_probe/app.title',
                     'share/applications/asset_probe.desktop'):
            assert stat.S_IMODE((installation / name).stat().st_mode) == 0o644, name
        assert stat.S_IMODE((installation / 'bin/asset_probe').stat().st_mode) == 0o755
    check_modes(prefix, 0o700)
    public_prefix = WORK / 'prefix-new'
    if public_prefix.exists():
        shutil.rmtree(public_prefix)
    run(ZIRAN, 'install', '--prefix', public_prefix)
    check_modes(public_prefix, 0o755)
    # Prove independence from the original project and its build snapshot.
    shutil.move(project / 'assets', project / 'assets-hidden')
    shutil.move(Path(str(program) + '.assets'), Path(str(program) + '.assets-hidden'))
    try:
        run(installed, cwd=neutral)
        run(public_prefix / 'bin/asset_probe', cwd=neutral)
        assert (prefix / 'share/kryon/asset_probe/assets/icons/ui.webp').read_bytes() == (ROOT / 'icons/ui.webp').read_bytes()
    finally:
        shutil.move(project / 'assets-hidden', project / 'assets')
        shutil.move(Path(str(program) + '.assets-hidden'), Path(str(program) + '.assets'))
    # Removing the declaration and assets must retain assetsless build/install behavior.
    (project / 'ziran.toml').write_text(manifest.replace('assets = "assets"\n', ''))
    (project / 'src/host.zi').write_text('#program_export\nmain :: () -> s32 { return 0 }\n')
    run(ZIRAN, 'install', '--prefix', prefix)
    run(installed, cwd=neutral)
    assert not (prefix / 'share/kryon/asset_probe/assets').exists()
    print('Native declared assets: neutral-cwd rendering, private-umask install modes, preserved prefix boundary and assetsless reinstall passed')


if __name__ == '__main__':
    main()
