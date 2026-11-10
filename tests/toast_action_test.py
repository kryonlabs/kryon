"""Actual package Toast action through retained Button input, native and portable."""
import os
from pathlib import Path
import subprocess
from toolchain import ROOT, ZIRAN, kryon_test_project

for name in ('DISPLAY', 'WAYLAND_DISPLAY', 'XAUTHORITY', 'DBUS_SESSION_BUS_ADDRESS', 'SESSION_MANAGER', 'GDK_DISPLAY'):
    os.environ.pop(name, None)
os.environ['YUE_DESKTOP_RECOVERY'] = '0'
work = ROOT / 'build/scratch/toast-action'
work.mkdir(parents=True, exist_ok=True)
guard = work / '.running'
guard.mkdir()
try:
    project = work / 'project'
    package_id = kryon_test_project(project)
    for name in ('toast_action_test', 'ziran_test_driver_host', 'text_input_platform_host'):
        source = (ROOT / f'tests/{name}.zi').read_text()
        for module in ('session', 'drawing_props', 'geometry'):
            source = source.replace(chr(35)+'import "'+module+'"', chr(35)+'import "kryon/'+module+'"')
        (project / f'src/{name}.zi').write_text(source)
    def run(args):
        subprocess.run([str(arg) for arg in args], cwd=project, check=True)
    entry = 'toast_action_test:main'
    source = project / 'src/toast_action_test.zi'
    ir = work / 'ir'
    run([ZIRAN, 'ir', '--project', '--offline', '-o', ir, source])
    effects = [('font_metrics', 'MeasureGlyphWidth'), ('font_metrics', 'MeasureGlyphLineHeight'),
               ('image_raster', 'ImageWidth'), ('image_raster', 'ImageHeight')]
    effects += [(module, effect) for module, names in {'paint_queue': ['RasterImage', 'RasterImageMesh'], 'raster': ['RasterLine'], 'raster_shape': ['RasterRoundedRectangle', 'RasterRoundedRectangleOutline'], 'raster_text': ['RasterText', 'RasterTextClipped', 'RasterTextRotated'], 'raster_triangles': ['RasterTriangles']}.items() for effect in names]
    bindings = []
    for module, effect in effects:
        bindings += ['--bind', f'{package_id}_{module}:{effect}=ziran_test_driver_host:{effect}']
    bindings += ['--bind', f'{package_id}_text_input_platform:SetPlatformTextKeyboardVisible=text_input_platform_host:SetPlatformTextKeyboardVisible']
    for form, fixture in [('source', source), ('saved', ir / 'toast_action_test.zir')]:
        options = ['--project', '--offline'] if form == 'source' else ['--root', ir, '--module-path', ir]
        output = work / ('c-' + form)
        run([ZIRAN, 'build', *options, '--target=c', '--exe', '--entry', entry, '-o', output, fixture])
        run([output / 'toast_action_test'])
        bundle = work / (form + '.zx')
        run([ZIRAN, 'bundle', *options, *bindings, '--entry', entry, '-o', bundle, fixture])
        result = subprocess.check_output([str(ZIRAN), 'run', str(bundle)], cwd=project, text=True)
        assert result.strip() == '0', result[-1000:]
        print('Toast action ' + form + ': native C and portable pointer/keyboard/lifecycle passed', flush=True)
finally:
    guard.rmdir()
