"""Native own-window cursor initialization and copied RGBA icon, source/saved."""
import ctypes as C
import ctypes.util
import json
import os
from pathlib import Path
import shlex
import subprocess
import sys
import time

ROOT=Path(__file__).resolve().parents[1]
WORK=ROOT/'build/scratch/desktop-native'

def run(args,**kw):
    subprocess.run(list(map(str,args)),check=True,**kw)

def drive(probe):
    assert os.environ['KRYON_PRIVATE_XVFB']=='1' and os.environ['YUE_DESKTOP_RECOVERY']=='0'
    class Image(C.Structure):
        _fields_=[('x',C.c_short),('y',C.c_short),('width',C.c_ushort),('height',C.c_ushort),
                  ('xhot',C.c_ushort),('yhot',C.c_ushort),('serial',C.c_ulong),
                  ('pixels',C.POINTER(C.c_ulong)),('atom',C.c_ulong),('name',C.c_char_p)]
    def fn(lib,name,result,*args):
        f=getattr(lib,name);f.restype=result;f.argtypes=args;return f
    ptr=C.c_void_p; x=C.CDLL('libX11.so.6');fix=C.CDLL('libXfixes.so.3')
    sdl=C.CDLL(ctypes.util.find_library('SDL2'))
    connection=fn(x,'XOpenDisplay',ptr,C.c_char_p)(os.environ['DISPLAY'].encode());assert connection
    def cursor():
        fn(x,'XSync',None,ptr,C.c_int)(connection,0)
        image=fn(fix,'XFixesGetCursorImage',C.POINTER(Image),ptr)(connection);assert image
        v=image.contents
        result=(v.width,v.height,v.xhot,v.yhot,tuple(v.pixels[i]&0xffffffff for i in range(v.width*v.height)))
        fn(x,'XFree',None,ptr)(image)
        return result
    def xdo(*args):
        return subprocess.check_output(['xdotool',*map(str,args)],text=True).strip()
    assert fn(sdl,'SDL_Init',C.c_int,C.c_uint)(0x20)==0
    window=fn(sdl,'SDL_CreateWindow',ptr,C.c_char_p,C.c_int,C.c_int,C.c_int,C.c_int,C.c_uint)(b'Arrow oracle',0,0,160,100,4)
    assert window
    arrow=fn(sdl,'SDL_CreateSystemCursor',ptr,C.c_int)(0);assert arrow
    fn(sdl,'SDL_SetCursor',None,ptr)(arrow)
    xdo('mousemove',30,30)
    for _ in range(20): fn(sdl,'SDL_PumpEvents',None)();time.sleep(.005)
    expected=cursor()
    with (probe.parent/'native-window.log').open('w') as log:
        child=subprocess.Popen([str(probe)],stdout=log,stderr=subprocess.STDOUT)
        try:
            window_id='';end=time.monotonic()+3
            while not window_id and time.monotonic()<end:
                found=subprocess.run(['xdotool','search','--onlyvisible','--pid',str(child.pid)],capture_output=True,text=True)
                if found.stdout.strip(): window_id=found.stdout.splitlines()[0]
                else: time.sleep(.01)
            assert window_id, 'Native fixture did not open'
            xdo('windowmove',window_id,200,100);xdo('mousemove','--window',window_id,120,80)
            end=time.monotonic()+1
            while cursor()!=expected and time.monotonic()<end: time.sleep(.01)
            actual=cursor()
            assert actual==expected,('Initial window cursor differs from SDL native arrow',expected[:4],actual[:4])
            # Xlib's format-32 property values occupy native unsigned longs.
            atom=fn(x,'XInternAtom',C.c_ulong,ptr,C.c_char_p,C.c_int)(connection,b'_NET_WM_ICON',0)
            actual_type=C.c_ulong();fmt=C.c_int();count=C.c_ulong();remaining=C.c_ulong();data=ptr()
            status=fn(x,'XGetWindowProperty',C.c_int,ptr,C.c_ulong,C.c_ulong,C.c_long,C.c_long,
                      C.c_int,C.c_ulong,C.POINTER(C.c_ulong),C.POINTER(C.c_int),C.POINTER(C.c_ulong),
                      C.POINTER(C.c_ulong),C.POINTER(ptr))(connection,int(window_id),atom,0,1024,0,0,
                      C.byref(actual_type),C.byref(fmt),C.byref(count),C.byref(remaining),C.byref(data))
            assert status==0 and fmt.value==32 and count.value==6
            values=tuple(C.cast(data,C.POINTER(C.c_ulong))[i]&0xffffffff for i in range(count.value))
            fn(x,'XFree',None,ptr)(data)
            assert values==(2,2,0xffff0000,0x8000ff00,0xff0000ff,0x000c2238),values
            assert child.wait(timeout=8)==0,'Native fixture failed'
            (probe.parent/'native-window.json').write_text(json.dumps({'native_arrow_exact':True,'icon_argb':values},indent=2))
        finally:
            if child.poll() is None: child.terminate();child.wait(timeout=5)
    fn(sdl,'SDL_SetCursor',None,ptr)(fn(sdl,'SDL_GetDefaultCursor',ptr)())
    fn(sdl,'SDL_FreeCursor',None,ptr)(arrow)
    fn(sdl,'SDL_DestroyWindow',None,ptr)(window);fn(sdl,'SDL_Quit',None)()
    fn(x,'XCloseDisplay',None,ptr)(connection)

def main():
    if len(sys.argv)>1 and sys.argv[1]=='--drive': return drive(Path(sys.argv[2]))
    from toolchain import ZIRAN,ZIRAN_ROOT
    WORK.mkdir(parents=True,exist_ok=True)
    env=os.environ.copy()
    for key in ('DISPLAY','WAYLAND_DISPLAY','WAYLAND_SOCKET','XAUTHORITY','DBUS_SESSION_BUS_ADDRESS',
                'GDK_DISPLAY','SESSION_MANAGER','ICEAUTHORITY'):env.pop(key,None)
    env.update(YUE_DESKTOP_RECOVERY='0',TMPDIR=str(WORK),KRYON_PRIVATE_XVFB='1',SDL_VIDEODRIVER='x11')
    source=ROOT/'tests/desktop_native_test.zi'
    common=['--root',ROOT/'tests','--module-path',ROOT/'src/ui','--module-path',ROOT/'src/backend','--module-path',ZIRAN_ROOT/'std']
    for package in ('webp','image_codec','avif'):
        package_root=Path(subprocess.check_output([str(ZIRAN),'pkg','path',package],cwd=ROOT,text=True).strip())
        common+=['--module-path',package+'='+str(package_root/'src')]
    run([ZIRAN,'ir',*common,'-o',WORK/'ir',source],env=env)
    flags=shlex.split(subprocess.check_output(['pkg-config','--cflags','--libs','sdl2','cairo','freetype2','pangocairo','fontconfig','gio-2.0'],text=True))
    for form in ('source','saved'):
        folder=WORK/form;folder.mkdir(exist_ok=True)
        paths=common if form=='source' else ['--root',WORK/'ir']
        entry=source if form=='source' else WORK/'ir/desktop_native_test.zir'
        run([ZIRAN,'build','--target=c','--no-main',*paths,'--entry','desktop_native_test:main','-o',folder,entry],env=env)
        run([os.environ.get('CC','cc'),'-O1','-ffunction-sections','-fdata-sections','-I'+str(folder),
             '-I'+str(ZIRAN_ROOT/'include'),*sorted(folder.glob('*.c')),ZIRAN_ROOT/'build/libziran.a',
             '-Wl,--gc-sections','-lm','-lpthread',*flags,'-o',folder/'probe'],env=env)
        run(['dbus-run-session','--','xvfb-run','-a',sys.executable,__file__,'--drive',folder/'probe'],env=env)
    print('Native initial arrow and copied RGBA window icon: source/saved passed',flush=True)

if __name__=='__main__':main()
