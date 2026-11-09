"""Manifest system libraries reach the real native project linker, without a display."""
import os
from pathlib import Path
import subprocess
import tempfile

from toolchain import ZIRAN_ROOT

ROOT = Path(__file__).resolve().parents[1]
ZIRAN = ZIRAN_ROOT / 'build/bin/ziran'


def main():
    env = os.environ.copy()
    for key in ('DISPLAY', 'WAYLAND_DISPLAY', 'XAUTHORITY', 'DBUS_SESSION_BUS_ADDRESS',
                'LDLIBS', 'MAKEFLAGS', 'MFLAGS', 'MAKELEVEL'):
        env.pop(key, None)
    env['YUE_DESKTOP_RECOVERY'] = '0'
    scratch = ROOT / 'build/scratch/project-system-links'
    subprocess.run(['git', 'check-ignore', str(scratch)], cwd=ROOT, check=True,
                   stdout=subprocess.DEVNULL)
    scratch.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(dir=scratch) as temporary:
        project = Path(temporary)
        (project / 'src').mkdir()
        (project / 'ziran.toml').write_text('''[package]
name = "system_links_probe"
tool = "kryon"
entry = "src/app.zi"
module_roots = ["src"]
bridge_modules = ["app"]
links = ["gtk-3", "gdk-3", "gio-2.0", "gobject-2.0", "glib-2.0", "pango-1.0", "cairo", "gdk_pixbuf-2.0"]
[toolchain]
git = "https://github.com/ziranlang/ziran.git"
ref = "master"
[dependencies.kryon]
git = "https://github.com/kryonlabs/kryon.git"
ref = "master"
[tool.kryon]
default_profile = "native"
[tool.kryon.profiles.native]
backend = "terminal"
host = "src/runner.zi"
''')
        (project / 'ziran.local.toml').write_text(
            f'[overrides]\nkryon = "{ROOT}"\nziran = "{ZIRAN_ROOT}"\n')
        (project / 'src/app.zi').write_text('Value :: 42;\n')
        # Type registration and version queries require linkage but open no windows.
        (project / 'src/runner.zi').write_text('''native :: #system_library "native";
GtkType :: () -> u64 #foreign native "gtk_widget_get_type";
GdkType :: () -> u64 #foreign native "gdk_display_manager_get_type";
GioType :: () -> u64 #foreign native "g_application_get_type";
ObjectType :: () -> u64 #foreign native "g_object_get_type";
GlibVersion :: () -> u32 #foreign native "g_get_num_processors";
PangoVersion :: () -> s32 #foreign native "pango_version";
CairoVersion :: () -> s32 #foreign native "cairo_version";
PixbufType :: () -> u64 #foreign native "gdk_pixbuf_get_type";
#program_export
main :: () -> s32 {
    if GtkType() == 0 || GdkType() == 0 || GioType() == 0 || ObjectType() == 0 ||
       GlibVersion() == 0 || PangoVersion() == 0 || CairoVersion() == 0 || PixbufType() == 0 { return 1 }
    print("system libraries linked\\n")
    return 0
}
''')
        for command in ([str(ZIRAN), 'lock'], [str(ZIRAN), 'build'],
                        [str(project / 'build/system_links_probe-native')]):
            subprocess.run(command, cwd=project, env=env, check=True)
    print('Manifest system library lock, native project link and execution passed')


if __name__ == '__main__':
    main()
