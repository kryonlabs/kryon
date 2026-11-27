"""Build the selected Frame source through the project tool without module aliases."""
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import tomllib

from image_test_tools import png_pixels, rgb
from toolchain import ROOT, ZIRAN, ZIRAN_ROOT, kryon_test_project

# `ziran run` hands a bundle only to a provider registered for its toolchain.
IDENTITY = subprocess.check_output([str(ZIRAN), "version", "--identity"], text=True).strip()


def run(arguments, project, expected=0):
    environment = os.environ.copy()
    for key in ('DISPLAY', 'WAYLAND_DISPLAY', 'XAUTHORITY', 'DBUS_SESSION_BUS_ADDRESS'):
        environment.pop(key, None)
    result = subprocess.run([str(value) for value in arguments], cwd=project,
                            env=environment, capture_output=True, text=True, timeout=180)
    assert result.returncode == expected, (arguments, result.returncode, result.stdout[-1500:], result.stderr[-5000:])
    return result


def check_ui_map(project):
    catalog = json.loads((ROOT / 'docs/ui-map.json').read_text())
    assert (catalog['version'], catalog['backend']) == (2, 'native')
    selectors = [selector for entry in catalog['elements'] for selector in entry['html']]
    assert len(selectors) == len(set(selectors)), 'duplicate HTML selector in UI map'
    exports = tomllib.loads((ROOT / 'ziran.toml').read_text())['exports']
    imports = {}

    def widget_alias(path):
        package, module = path.split('/')
        assert package == 'kryon' and module in exports, path
        return imports.setdefault(path, f'Widget{len(imports)}')

    calls = []
    for index, entry in enumerate(catalog['elements'] + catalog['layouts']):
        alias = widget_alias(entry['import'])
        props = ', '.join(f'.{name} = {json.dumps(value)}' for name, value in entry.get('props', {}).items())
        arguments = ['session']
        if 'state' in entry:
            calls.append(f'    state{index}: {alias}.{entry["state"]}')
            arguments.append(f'*state{index}')
        arguments.append('.{' + props + '}')
        for name, item in entry.get('lists', ()):
            if '/' in item:
                path, type_name = item.split('.')
                item = f'{widget_alias(path)}.{type_name}'
            elif item != 'bool':
                item = f'{alias}.{item}'
            calls.append(f'    {name}{index}: []{item}')
            arguments.append(f'{name}{index}')
        call = f'{alias}.{entry["call"]}({", ".join(arguments)})'
        fields = [entry[key].removeprefix('result.') for key in ('scope', 'content', 'event') if key in entry]
        if fields:
            calls.append(f'    result{index} := {call}')
            calls.extend(f'    unused result{index}.{field}' for field in fields)
        else:
            calls.append(f'    {call}')
    source = project / 'src/ui_map_check.zi'
    source.write_text('\n'.join([
        *(f'{alias} :: #import "{module}";' for module, alias in imports.items()),
        '#import "kryon/session"', '#program_export', 'CheckUiMap :: (session: Session) {',
        *calls, '}', '',
    ]))
    run([ZIRAN, 'ir', '--project', '--root', source.parent, '-o', project / 'map-ir', source], project)
    source.unlink()


def preview_test():
    scratch = ROOT / 'build/scratch'
    scratch.mkdir(parents=True, exist_ok=True)
    # The preview driver captures offscreen and refuses other SDL drivers.
    os.environ['SDL_VIDEODRIVER'] = 'dummy'
    with tempfile.TemporaryDirectory(prefix='project-preview-', dir=scratch) as temporary:
        project = Path(temporary)
        kryon_test_project(project)
        manifest = project / 'ziran.toml'
        manifest.write_text(manifest.read_text().replace('module_roots = ["src"]',
                                                         'module_roots = ["src", "previews"]'))
        with manifest.open('a') as config:
            config.write('''
[tool.kryon]
default_profile = "desktop"
[tool.kryon.profiles.desktop]
backend = "desktop"
[tool.kryon.profiles.terminal]
backend = "terminal"
''')
        run([ZIRAN, 'lock', '--offline'], project)
        fixtures = project / 'previews'
        fixtures.mkdir()
        check_ui_map(project)
        fixture = fixtures / 'controls.tsx'
        source = r'''import { Button } from "kryon/tsx/button";
import * as Snapshot from "kryon/frame_snapshot";
import * as App from "kryon/app";
import { Session } from "kryon/session";

// Non-neutral input draws nothing, so the preview check fails visibly.
export function Frame(frame: App.Frame): void {
    const session: Session = {slot: frame.slot, epoch: frame.epoch};
    const viewport = frame.viewport;
    const input = Snapshot.CaptureFrameInput(session);
    const neutral = input.available && input.elapsed_ms === s64(0) && input.delta_ms === s64(0) &&
        !input.focused && input.pointer.x === float32(-1) && input.pointer.y === float32(-1);
    if (!neutral) { return; }
    <Button session={session} props={{key: u64(9007199254740993n),
        bounds: {x: viewport.width - 135, y: 20, width: 120, height: 36},
        label: "Save \"café\"", id: s32(19)}}/>;
}
export function main(): s32 { return App.Run({title: "preview"}, Frame); }
'''
        fixture.write_text(source, encoding='utf-8')
        output = project / 'output space'
        output.mkdir()
        front_door = ROOT / 'build/bin/kryon'
        cli = [ZIRAN, 'tool', 'kryon', 'preview']
        assert 'previews/FIXTURE.tsx' in run([front_door, 'preview', '--help'], project).stdout
        light = ['controls', '--width', '321', '--height', '179', '--theme', 'light']
        args = [*light, '--output', 'output space/light.png']
        run(['env', f"PATH={ZIRAN.parent}:{os.environ['PATH']}", 'DISPLAY=invalid-for-preview',
             'WAYLAND_DISPLAY=invalid-for-preview', 'SDL_VIDEODRIVER=dummy',
             front_door, 'preview', *args], project)
        run([*cli, *light, '--output', 'output space/repeat'], project)
        run([*cli, 'controls', '--theme', 'dark', '--width', '173', '--height', '97',
             '--output', 'output space/dark', '--profile', 'desktop'], project)

        assert (output / 'light.png').read_bytes() == (output / 'repeat.png').read_bytes()
        baseline = (output / 'light.json').read_bytes()
        assert baseline == (output / 'repeat.json').read_bytes()
        themes = []
        for name, width, height in [('light', 321, 179), ('dark', 173, 97)]:
            png = png_pixels(output / f'{name}.png')
            assert png[:2] == (width, height)
            themes.append(rgb(png, 1, 1))
            tree = json.loads((output / f'{name}.json').read_bytes())
            assert (tree['version'], tree['width'], tree['height']) == (1, width, height)
            root, button = tree['nodes']
            assert (root['index'], root['parent'], root['kind']) == (0, -1, 'screen')
            assert (root['x'], root['y'], root['width'], root['height']) == (0, 0, width, height)
            assert (button['parent'], button['role'], button['label'], button['key']) == (
                0, 'button', 'Save "café"', 9007199254740993), button
            assert button['focus_id'] == 19
            assert (button['x'], button['y'], button['width'], button['height']) == (
                width - 135, 20, 120, 36)
            assert button['visible'] and button['enabled']
        assert sum(themes[0]) > 650 and sum(themes[1]) < 200, themes

        run([*cli, 'controls', '--width', '173,321,760', '--height', '179', '--theme', 'light',
             '--output', 'output space/responsive.png'], project)
        for width in (173, 321, 760):
            prefix = output / f'responsive-{width}'
            assert png_pixels(prefix.with_suffix('.png'))[:2] == (width, 179)
            tree = json.loads(prefix.with_suffix('.json').read_bytes())
            assert (tree['width'], tree['height'], tree['nodes'][1]['x']) == (width, 179, width - 135)
        assert (output / 'responsive-321.png').read_bytes() == (output / 'light.png').read_bytes()
        assert (output / 'responsive-321.json').read_bytes() == baseline

        assert 'fixture not found: previews/missing.tsx' in run(
            [*cli, 'missing', *args[1:]], project, expected=2).stderr
        for invalid in ('4097', '173,', ',173', '173,,321', '173,4097'):
            assert 'integers from 1 through 4096' in run(
                [*cli, 'controls', '--width', invalid, *args[3:]], project, expected=2).stderr
        assert 'same width more than once' in run(
            [*cli, 'controls', '--width', '173,0173', *args[3:]], project, expected=2).stderr
        assert 'desktop C99' in run([*cli, *args, '--profile', 'terminal'], project, expected=2).stderr
        assert 'parent exists and is writable' in run(
            [*cli, *light, '--output', 'missing directory/preview'], project, expected=2).stderr
        fixture.write_text('export function Frame( {\n')
        broken = run([*cli, *args], project, expected=2)
        assert 'controls.tsx' in broken.stderr and 'preview build or render failed' in broken.stderr, broken.stderr
        assert (output / 'light.json').read_bytes() == baseline
        assert (output / 'light.png').read_bytes() == (output / 'repeat.png').read_bytes()
        assert not list(output.glob('.kryon-preview.*'))
    print('Preview: responsive PNG and JSON batch, two themes, byte-identical repeats, widget geometry and actionable failures passed')


def main():
    scratch = ROOT / 'build/scratch'
    scratch.mkdir(parents=True, exist_ok=True)
    # Register this checkout's zx player for its exact toolchain identity;
    # an installed runtime from another ziran build must not decide here.
    providers = scratch / 'entry runtime providers'
    providers.mkdir(parents=True, exist_ok=True)
    (providers / 'kryon-desktop.host').write_text(f'{ROOT / "build/bin/zx"}\nziran {IDENTITY}\n')
    os.environ['ZIRAN_HOST_PATH'] = str(providers)
    with tempfile.TemporaryDirectory(prefix='project-entry-', dir=scratch) as temporary:
        project = Path(temporary)
        # Checked packages lock only against the exact toolchain Kryon pins;
        # this checkout still supplies the Kryon source under test.
        (project / 'ziran.local.toml').write_text(f'[overrides]\nkryon = "{ROOT}"\n')
        toolchain = tomllib.loads((ROOT / 'ziran.toml').read_text())['toolchain']
        revision = subprocess.check_output(['git', 'rev-parse', '--verify', 'HEAD'],
                                           cwd=ROOT, text=True).strip()
        for entry, package_entry in [('app.tsx', 'app.tsx'), ('button.tsx', 'button.tsx'),
                                     ('button.tsx', 'main.zi')]:
            override = f'entry = "{entry}"\n' if entry != package_entry else ''
            manifest = f'''[package]
name = "entry_probe"
entry = "{package_entry}"
module_roots = ["."]
tool = "kryon"
[toolchain]
git = "{toolchain['git']}"
ref = "{toolchain['ref']}"
[dependencies.kryon]
git = "https://git.waozi.xyz/kryonlabs/kryon.git"
ref = "{revision}"
[tool.kryon]
{override}default_profile = "native"
[tool.kryon.profiles.native]
backend = "terminal"
[tool.kryon.profiles.portable]
backend = "desktop"
codegen = "zx"
'''
            (project / 'ziran.toml').write_text(manifest)
            selected = project / entry
            selected.write_text(r'''import * as App from "kryon/app";
import { Session } from "kryon/session";
import { Text } from "kryon/tsx/text";
import { WindowQuit } from "kryon/window";

let announced: boolean = false;

export function Frame(frame: App.Frame): void {
    const session: Session = {slot: frame.slot, epoch: frame.epoch};
    <Text session={session} props={{text: "Entry probe", bounds: frame.viewport}}/>;
    if (!announced) {
        announced = true;
        print("selected frame\n");
        WindowQuit();
    }
}

export function main(): s32 {
    App.Run({title: "entry probe"}, Frame);
    return s32(42);
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
            # The window player owns the bundle's event loop and exit path,
            # so the frame announcement is the portable-run observable.
            result = run(['env', 'HARMONY_DESKTOP_RECOVERY=0', 'xvfb-run', '-a', ZIRAN,
                          'run', project / 'build/entry_probe-portable.zx'], project)
            assert 'selected frame' in result.stdout, result.stdout
            selected.write_text('export function Frame( {\n')
            result = run([ZIRAN, 'check'], project, expected=2)
            assert entry in result.stderr, result.stderr
            selected.unlink()
            if package_entry != entry:
                (project / package_entry).unlink()
        legacy = project / 'legacy.zi'
        legacy.write_text('main :: () -> s32 { return 0 }\n')
        legacy_override = manifest.replace('entry = "button.tsx"', 'entry = "legacy.zi"')
        legacy_default = manifest.replace('entry = "main.zi"', 'entry = "legacy.zi"').replace(override, '')
        for config in (legacy_override, legacy_default):
            (project / 'ziran.toml').write_text(config)
            result = run([ZIRAN, 'check'], project, expected=2)
            assert 'application entries must use .tsx' in result.stderr, result.stderr
        legacy.unlink()

        # A project-owned host imports its own Frame graph and needs no
        # implicit built-in frame capability or make-variable workaround.
        custom_manifest = manifest.replace('entry = "main.zi"', 'entry = "button.tsx"')
        for backend in ('terminal', 'desktop'):
            custom_manifest = custom_manifest.replace(f'backend = "{backend}"',
                f'backend = "{backend}"' + chr(10) + 'host = "runner.zi"')
        (project / 'ziran.toml').write_text(custom_manifest)
        (project / 'button.tsx').write_text('''import { Rectangle } from "kryon/geometry";
import { Session } from "kryon/session";

export function Frame(session: Session, viewport: Rectangle): s32 { return s32(42); }
''')
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
    os.environ.pop('ZIRAN_HOST_PATH', None)
    print('Selected entries and custom host: native and portable local tests passed')


if __name__ == '__main__':
    if sys.argv[1:] != ['--preview-only']:
        main()
    preview_test()
