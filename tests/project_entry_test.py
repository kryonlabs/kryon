"""Build the selected Frame source through the project tool without module aliases."""
import os
from pathlib import Path
import subprocess
import tempfile

from toolchain import ROOT, ZIRAN, ZIRAN_ROOT


def run(arguments, project, expected=0):
    environment = os.environ.copy()
    for key in ('DISPLAY', 'WAYLAND_DISPLAY', 'XAUTHORITY', 'DBUS_SESSION_BUS_ADDRESS'):
        environment.pop(key, None)
    result = subprocess.run([str(value) for value in arguments], cwd=project,
                            env=environment, capture_output=True, text=True)
    assert result.returncode == expected, (arguments, result.returncode, result.stdout, result.stderr)
    return result


def main():
    scratch = ROOT / 'build/scratch'
    scratch.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix='project-entry-', dir=scratch) as temporary:
        project = Path(temporary)
        (project / 'ziran.local.toml').write_text(
            f'[overrides]\nkryon = "{ROOT}"\nziran = "{ZIRAN_ROOT}"\n')
        for entry, package_entry in [('app.zi', 'app.zi'), ('button.zi', 'button.zi'),
                                     ('button.zi', 'main.zi')]:
            manifest = f'''[package]
name = "entry_probe"
entry = "{package_entry}"
module_roots = ["."]
tool = "kryon"
[toolchain]
git = "https://github.com/ziranlang/ziran.git"
ref = "master"
[dependencies.kryon]
git = "https://github.com/kryonlabs/kryon.git"
ref = "master"
[tool.kryon]
entry = "{entry}"
default_profile = "native"
[tool.kryon.profiles.native]
backend = "terminal"
[tool.kryon.profiles.portable]
backend = "pixmap"
codegen = "zx"
'''
            (project / 'ziran.toml').write_text(manifest)
            selected = project / entry
            selected.write_text(r'''#import "kryon/geometry"
#import "kryon/session"
#program_export
Frame :: (session: Session, viewport: Rectangle) -> s32 {
    print("selected frame\n")
    return 42
}
''')
            if package_entry != entry:
                (project / package_entry).write_text('main :: () -> s32 { return 0 }\n')
            run([ZIRAN, 'lock'], project)
            run([ZIRAN, 'check'], project)
            run([ZIRAN, 'build'], project)
            result = run([project / 'build/entry_probe-native'], project, expected=42)
            assert 'selected frame' in result.stdout, result.stdout
            run([ZIRAN, 'tool', 'kryon', 'build', 'portable'], project)
            result = run([ZIRAN, 'run', project / 'build/entry_probe-portable.zx'],
                         project)
            assert 'selected frame' in result.stdout and result.stdout.strip().endswith('42'), result.stdout
            selected.write_text('Frame :: ( {\n')
            result = run([ZIRAN, 'check'], project, expected=2)
            assert entry in result.stderr, result.stderr
            selected.unlink()
            if package_entry != entry:
                (project / package_entry).unlink()
        # A project-owned host imports its own Frame graph and needs no
        # implicit built-in frame capability or make-variable workaround.
        custom_manifest = manifest.replace('entry = "main.zi"', 'entry = "button.zi"')
        custom_manifest = custom_manifest.replace('backend = "terminal"', 'backend = "pixmap"')
        custom_manifest = custom_manifest.replace('backend = "pixmap"',
            'backend = "pixmap"' + chr(10) + 'host = "runner.zi"')
        (project / 'ziran.toml').write_text(custom_manifest)
        (project / 'button.zi').write_text('#import "kryon/geometry"' + chr(10) +
            '#import "kryon/session"' + chr(10) +
            '#program_export' + chr(10) +
            'Frame :: (session: Session, viewport: Rectangle) -> s32 { return 42 }' + chr(10))
        (project / 'runner.zi').write_text('#import "kryon/geometry"' + chr(10) +
            '#import "kryon/session"' + chr(10) +
            '#import "button"' + chr(10) + '#import "kryon/pixmap"' + chr(10) + '#program_export' + chr(10) +
            'main :: () -> s32 {' + chr(10) + '    session: Session' + chr(10) +
            '    viewport: Rectangle' + chr(10) +
            '    if PixmapWorkerCount() < 1 { return 2 }' + chr(10) +
            '    if Frame(session, viewport) != 42 { return 1 }' + chr(10) +
            '    return 0' + chr(10) + '}' + chr(10))
        run([ZIRAN, 'lock'], project)
        run([ZIRAN, 'check'], project)
        run([ZIRAN, 'build'], project)
        run([project / 'build/entry_probe-native'], project)
        run([ZIRAN, 'tool', 'kryon', 'build', 'portable'], project)
        result = run([ZIRAN, 'run', project / 'build/entry_probe-portable.zx'], project)
        assert result.stdout.strip() == '0', result.stdout
    print('Selected entries and custom host: native and portable local tests passed')


if __name__ == '__main__':
    main()
