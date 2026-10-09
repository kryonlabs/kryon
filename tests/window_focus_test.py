"""Read focus in native and portable apps on owned private-display windows."""
import ctypes as c
import os
from pathlib import Path
import subprocess
import sys
import time

from PIL import Image
from toolchain import ROOT, ZIRAN_ROOT, ZIRAN

OUTPUT = ROOT / 'build/test/window-focus'
APP = '''using UI :: #import "kryon/Widgets";
#import "kryon/Window"

#program_export
Frame :: (session: Session, viewport: Rectangle) -> s32 {
    props: BoxProps; props.key = 1; props.bounds = viewport
    props.fill = ifx WindowFocused() then Color.{0,200,0,255} else Color.{200,0,0,255}
    Box(session, props)
    return 0
}
'''


def wait(check, message):
    end = time.monotonic() + 20
    while time.monotonic() < end:
        if check(): return
        time.sleep(.05)
    raise AssertionError(message)


def drive(mode):
    assert os.environ.get('KRYON_PRIVATE_XVFB') == '1'
    commands = {'native': [OUTPUT/'build/window_focus-desktop'],
                'portable': [ROOT/'build/bin/zib', OUTPUT/'build/window_focus-portable.zib']}
    child = subprocess.Popen(list(map(str, commands[mode])), cwd=OUTPUT,
                             stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    x = c.CDLL('libX11.so.6')
    x.XOpenDisplay.restype = c.c_void_p
    x.XDefaultRootWindow.argtypes = [c.c_void_p]; x.XDefaultRootWindow.restype = c.c_ulong
    x.XCreateSimpleWindow.argtypes = [c.c_void_p,c.c_ulong,c.c_int,c.c_int,c.c_uint,c.c_uint,c.c_uint,c.c_ulong,c.c_ulong]
    x.XCreateSimpleWindow.restype = c.c_ulong
    x.XMapWindow.argtypes = [c.c_void_p,c.c_ulong]
    x.XSetInputFocus.argtypes = [c.c_void_p,c.c_ulong,c.c_int,c.c_ulong]
    x.XDestroyWindow.argtypes = [c.c_void_p,c.c_ulong]
    x.XFlush.argtypes = [c.c_void_p]; x.XCloseDisplay.argtypes = [c.c_void_p]
    connection = x.XOpenDisplay(None); assert connection
    secondary = x.XCreateSimpleWindow(connection,x.XDefaultRootWindow(connection),900,600,100,80,0,0,0)
    x.XMapWindow(connection,secondary); x.XFlush(connection)
    window = None
    def locate():
        nonlocal window
        if child.poll() is not None:
            raise AssertionError(child.stdout.read().decode()+child.stderr.read().decode())
        result = subprocess.run(['xdotool','search','--onlyvisible','--pid',str(child.pid)],capture_output=True,text=True)
        if result.returncode == 0 and result.stdout.strip(): window = result.stdout.splitlines()[0]; return True
        return False
    def color(expected):
        assert child.poll() is None, child.stderr.read().decode()
        image = OUTPUT/(mode+'.png')
        subprocess.run(['import','-window',window,str(image)],check=True,timeout=5)
        with Image.open(image) as picture: return picture.convert('RGB').getpixel((10,10)) == expected
    try:
        wait(locate,'Focus probe did not open its window')
        subprocess.run(['xdotool','windowmove',window,'0','0'],check=True)
        subprocess.run(['xdotool','windowfocus',window],check=True)
        wait(lambda: color((0,200,0)),'Own focused window reported inactive')
        x.XSetInputFocus(connection,secondary,1,0); x.XFlush(connection)
        wait(lambda: color((200,0,0)),'Focus loss did not reach the app')
        subprocess.run(['xdotool','windowfocus',window],check=True)
        wait(lambda: color((0,200,0)),'Returning focus did not reach the app')
        subprocess.run(['xdotool','windowunmap',window],check=True); time.sleep(.3)
        subprocess.run(['xdotool','windowmap',window],check=True)
        x.XSetInputFocus(connection,secondary,1,0); x.XFlush(connection)
        wait(lambda: color((200,0,0)),'An inactive remapped window reported focus')
        subprocess.run(['xdotool','windowfocus',window],check=True)
        wait(lambda: color((0,200,0)),'Remapped window did not regain focus')
    finally:
        if child.poll() is None: child.terminate()
        try: child.wait(timeout=5)
        except subprocess.TimeoutExpired: child.kill(); child.wait(timeout=5)
        child.stdout.close(); child.stderr.close()
        x.XDestroyWindow(connection,secondary); x.XCloseDisplay(connection)
    print('Private Xvfb '+mode+': own-window focus, loss, return and remapping passed')


def main():
    OUTPUT.mkdir(parents=True,exist_ok=True)
    (OUTPUT/'src').mkdir(exist_ok=True)
    (OUTPUT/'src/app.zi').write_text(APP)
    (OUTPUT/'ziran.toml').write_text('''[package]
name = "window_focus"
entry = "src/app.zi"
module_roots = ["src"]
bridge_modules = ["app"]

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

[tool.kryon.profiles.portable]
backend = "desktop"
codegen = "zib"
''')
    (OUTPUT/'ziran.local.toml').write_text(f'[overrides]\nkryon = "{ROOT}"\nziran = "{ZIRAN_ROOT}"\n')
    env = dict(os.environ)
    for key in ['DISPLAY','WAYLAND_DISPLAY','XAUTHORITY','DBUS_SESSION_BUS_ADDRESS','SESSION_MANAGER']: env.pop(key,None)
    env['YUE_DESKTOP_RECOVERY'] = '0'
    subprocess.run(['make','zib-player'],cwd=ROOT,env=env,check=True)
    subprocess.run([str(ZIRAN),'lock'],cwd=OUTPUT,env=env,check=True)
    for mode, profile in [('native','desktop'),('portable','portable')]:
        subprocess.run([str(ZIRAN),'tool','kryon','build','--profile',profile],cwd=OUTPUT,env=env,check=True)
        subprocess.run(['xvfb-run','-a','-s','-screen 0 1400x1000x24','env','KRYON_PRIVATE_XVFB=1',sys.executable,str(Path(__file__).resolve()),'--drive',mode],env=env,check=True)


if __name__ == '__main__':
    if len(sys.argv) == 3 and sys.argv[1] == '--drive': drive(sys.argv[2])
    else: main()
