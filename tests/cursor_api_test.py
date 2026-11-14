"""Persistent cursor requests, the application cursor and real SDL bitmap
cursors (premultiplied, cached, steady) on private Xvfb, then the same
requests as CSS cursors in Canvas and DOM pages in a private headless Chromium."""
import ctypes as c
import os
from pathlib import Path
import shutil
import subprocess
import sys
import time
from PIL import Image
from toolchain import UI_MODULE_ARGS, ROOT, ZIRAN_ROOT, ZIRAN

OUTPUT = ROOT / 'build/test/cursor-api'
ENV = dict(os.environ)
for name in ('DISPLAY','WAYLAND_DISPLAY','XAUTHORITY','GDK_DISPLAY','DBUS_SESSION_BUS_ADDRESS',
             'SESSION_MANAGER','DESKTOP_SESSION','XDG_SESSION_ID','XDG_SESSION_TYPE',
             'XDG_CURRENT_DESKTOP','XDG_SESSION_DESKTOP','XDG_RUNTIME_DIR',
             'GNOME_DESKTOP_SESSION_ID','KDE_FULL_SESSION',
             'KRYON_CAPTURE_PATH','KRYON_TEST_DRIVER'):
    ENV.pop(name, None)
ENV.update(YUE_DESKTOP_RECOVERY='0', TMPDIR=str(OUTPUT), SDL_RENDER_DRIVER='software')
ENV['KRYON_APP_DATA'] = str(OUTPUT / 'portable-data')
ENV.setdefault('EM_CACHE', str(ROOT / 'build/emscripten-cache'))

def run(args, cwd=ROOT, **kw):
    try:
        return subprocess.run(list(map(str,args)),cwd=cwd,env=ENV,check=True,**kw)
    except subprocess.CalledProcessError as failure:
        print(f'Cursor local test command failed ({failure.returncode}): {args[0]} in {cwd}', file=sys.stderr)
        raise SystemExit(failure.returncode)

def wait(check, message):
    end = time.monotonic() + 20
    while time.monotonic() < end:
        if check(): return
        time.sleep(.04)
    raise AssertionError(message)

class CursorImage(c.Structure):
    _fields_ = [('x',c.c_short),('y',c.c_short),('width',c.c_ushort),('height',c.c_ushort),
                ('xhot',c.c_ushort),('yhot',c.c_ushort),('serial',c.c_ulong),
                ('pixels',c.POINTER(c.c_ulong)),('atom',c.c_ulong),('name',c.c_char_p)]

def drive(mode):
    assert os.environ.get('KRYON_PRIVATE_XVFB') == '1'
    display = os.environ['DISPLAY']
    assert display != ':0'
    x = c.CDLL('libX11.so.6'); fix = c.CDLL('libXfixes.so.3')
    x.XOpenDisplay.argtypes=[c.c_char_p]; x.XOpenDisplay.restype=c.c_void_p
    x.XFree.argtypes=[c.c_void_p]; x.XSync.argtypes=[c.c_void_p,c.c_int]
    x.XCloseDisplay.argtypes=[c.c_void_p]
    x.XDefaultRootWindow.argtypes=[c.c_void_p]; x.XDefaultRootWindow.restype=c.c_ulong
    x.XCreateSimpleWindow.argtypes=[c.c_void_p,c.c_ulong,c.c_int,c.c_int,c.c_uint,c.c_uint,c.c_uint,c.c_ulong,c.c_ulong]
    x.XCreateSimpleWindow.restype=c.c_ulong
    x.XMapWindow.argtypes=[c.c_void_p,c.c_ulong]
    x.XSetInputFocus.argtypes=[c.c_void_p,c.c_ulong,c.c_int,c.c_ulong]
    x.XDestroyWindow.argtypes=[c.c_void_p,c.c_ulong]
    fix.XFixesGetCursorImage.argtypes=[c.c_void_p]; fix.XFixesGetCursorImage.restype=c.POINTER(CursorImage)
    connection=x.XOpenDisplay(display.encode()); assert connection
    class Data(c.Union):
        _fields_=[('values',c.c_long*5),('pad',c.c_char*20)]
    class Message(c.Structure):
        _fields_=[('type',c.c_int),('serial',c.c_ulong),('send_event',c.c_int),
                  ('display',c.c_void_p),('window',c.c_ulong),('message_type',c.c_ulong),
                  ('format',c.c_int),('data',Data)]
    x.XInternAtom.argtypes=[c.c_void_p,c.c_char_p,c.c_int]; x.XInternAtom.restype=c.c_ulong
    x.XSendEvent.argtypes=[c.c_void_p,c.c_ulong,c.c_int,c.c_long,c.c_void_p]
    def close_window(window):
        event=Message(type=33,send_event=1,display=connection,window=int(window),format=32)
        event.message_type=x.XInternAtom(connection,b'WM_PROTOCOLS',0)
        event.data.values[0]=x.XInternAtom(connection,b'WM_DELETE_WINDOW',0)
        x.XSendEvent(connection,int(window),0,0,c.byref(event)); x.XSync(connection,0)
    def cursor():
        x.XSync(connection,0)
        p=fix.XFixesGetCursorImage(connection); assert p
        v=p.contents
        result=(v.width,v.height,v.xhot,v.yhot,tuple(v.pixels[i] & 0xffffffff for i in range(v.width*v.height)),v.serial)
        x.XFree(p)
        return result
    outside=cursor()
    command=[OUTPUT/'build/cursor_probe-desktop'] if mode=='native' else [ROOT/'build/bin/zx',OUTPUT/'build/cursor_probe-portable.zx']
    with (OUTPUT/f'{mode}.log').open('w') as log:
        child=subprocess.Popen(list(map(str,command)),cwd=OUTPUT,stdout=log,stderr=subprocess.STDOUT)
        window=None
        def locate():
            nonlocal window
            assert child.poll() is None, f'{mode} exited; inspect {OUTPUT / (mode+".log")}'
            found=subprocess.run(['xdotool','search','--onlyvisible','--pid',str(child.pid)],capture_output=True,text=True)
            if found.returncode==0 and found.stdout.strip(): window=found.stdout.splitlines()[0]; return True
            return False
        def xdo(*args): subprocess.run(['xdotool',*map(str,args)],check=True,timeout=5)
        def key(n): xdo('key','--window',window,str(n))
        def bitmap(colour,size,hot):
            value=cursor()
            return value[:4]==(*size,*hot) and any((p&0xffffff)==colour and p>>24==255 for p in value[4])
        def hand(hot=(3,5)): return bitmap(0xe02040,(32,24),hot)
        def tool(): return bitmap(0x20a040,(32,24),(4,20))
        def app(): return bitmap(0x2040e0,(32,32),(7,1))
        def hidden(): return not any(cursor()[4])
        def premultiplied():
            # Xcursor composites premultiplied ARGB; a colour channel above
            # its alpha shows as a light fringe over dark pictures.
            pixels=cursor()[4]
            semi=[p for p in pixels if 0 < p>>24 < 255]
            assert semi, 'Cursor lost its antialiased edge'
            bad=[hex(p) for p in pixels if max((p>>16)&255,(p>>8)&255,p&255) > p>>24]
            assert not bad, f'Cursor pixels are not premultiplied (light fringe): {bad[:6]}'
        def steady(seconds=.6):
            # A repeated request must not re-create or re-select the cursor.
            first=cursor(); end=time.monotonic()+seconds
            while time.monotonic()<end:
                assert cursor()[5]==first[5], 'Cursor changed while its request stayed the same'
                time.sleep(.03)
        try:
            wait(locate,'No owned cursor window')
            # Move only our window around the stationary private-display pointer.
            xdo('windowmove',window,160,120); xdo('windowfocus',window)
            try:
                wait(hand,'Bitmap, output dimensions, hotspot or red pixels differ')
            except AssertionError:
                measured=cursor()
                print('Measured private cursor:', measured[:4], 'colors:', sorted(set(measured[4]))[:8],flush=True)
                raise
            premultiplied(); steady()
            # The empty request falls back to the application cursor natively
            # (a hosted guest's player sets none, so the system pointer shows).
            key(3)
            if mode=='native': wait(app,'Withdrawn request did not show the application cursor'); premultiplied(); steady()
            else: wait(lambda: not hand() and not hidden(),'Withdrawn request did not show the system pointer')
            key(4); wait(lambda: not hand() and not app() and not hidden(),'Missing asset did not fall back visibly')
            baseline=cursor()
            key(1); wait(hand,'Custom cursor did not resume')
            key(5); wait(lambda: hand((9,5)),'Changed hotspot was not applied')
            key(2); wait(hidden,'Explicit hidden request did not hide cursor')
            key(8); wait(tool,'Tool cursor did not replace the hidden request')
            key(6); time.sleep(.3); assert tool(), 'Refused request changed the cursor'
            # Alternating two bitmaps reuses both cached cursors.
            serials=set()
            for _ in range(12):
                key(7); time.sleep(.05); serials.add(cursor()[5])
            assert len(serials)<=2, f'Switching cursors re-created them: {len(serials)} serials'
            key(1); wait(hand,'Custom request did not recover')
            xdo('windowmove',window,900,650)
            wait(lambda: cursor()[:5]==outside[:5],'Leave did not show the desktop cursor')
            xdo('windowmove',window,160,120)
            wait(hand,'Enter did not show the custom cursor')
            # This second window belongs to our test's private X connection.
            other=x.XCreateSimpleWindow(connection,x.XDefaultRootWindow(connection),900,650,120,80,0,0,0)
            assert other
            x.XMapWindow(connection,other); x.XSync(connection,0)
            try:
                # Keyboard focus elsewhere leaves the pointer over our window:
                # its cursor stays exactly the same.
                x.XSetInputFocus(connection,other,1,0); x.XSync(connection,0)
                time.sleep(.3); assert hand(), 'Focus loss changed the cursor under the pointer'
                steady(.3)
                xdo('windowfocus',window); assert hand()
            finally:
                x.XDestroyWindow(connection,other); x.XSync(connection,0)
            # Mode 9: the pointing cursor only over the press and over the
            # painted target the app marks with CursorHover, the calm cursor
            # between them, and no re-selection while the pointer rests. The
            # pointer of this private display moves; the desktop's never does.
            def shell(command,*args):
                found=subprocess.run(['xdotool',command,'--shell',*map(str,args)],capture_output=True,text=True,check=True,timeout=5)
                return dict(line.split('=',1) for line in found.stdout.split())
            home=shell('getmouselocation'); frame=shell('getwindowgeometry',window)
            wx,wy,ww,wh=(int(frame[name]) for name in ('X','Y','WIDTH','HEIGHT'))
            def hover(fraction): xdo('mousemove',wx+int(ww*fraction),wy+wh//2)
            def point(): return bitmap(0xe0a020,(32,32),(8,1))
            key(9)
            if mode=='native':
                for fraction,shown,what in ((.5,app,'nothing'),(.1,point,'the press'),(.5,app,'nothing again'),
                                            (.9,point,'the painted target'),(.6,app,'the edge of the target')):
                    hover(fraction); wait(shown,f'Mode 9 over {what} showed the wrong cursor'); premultiplied(); steady(.3)
            else:
                # A hosted guest's player sets no application cursors: the
                # system pointer and clickable shapes show instead.
                hover(.5); wait(lambda: not hand() and not hidden(),'Mode 9 hid or kept the request'); calm=cursor()[:5]
                hover(.1); wait(lambda: cursor()[:5]!=calm,'The press did not show the clickable shape'); clickable=cursor()[:5]
                steady(.3)
                hover(.9); wait(lambda: cursor()[:5]==clickable,'The painted target did not show the clickable shape')
                steady(.3)
                hover(.5); wait(lambda: cursor()[:5]==calm,'Leaving the target kept the clickable shape')
            xdo('mousemove',home['X'],home['Y'])
            key(1); wait(hand,'Custom request did not resume after the hover bands')
            key(3); key(4); wait(lambda: cursor()[:5]==baseline[:5],'Missing asset did not fall back visibly')
            key(2); wait(hidden,'Hide before exit failed')
            if mode == 'portable':
                extracted=[path for path in OUTPUT.glob('zx-*') if path.is_dir()]
                assert len(extracted)==1, 'Hosted assets did not use the fixture TMPDIR'
                assert (extracted[0]/'assets/hand.png').is_file()
                assert extracted[0].stat().st_mode & 0o777 == 0o700
            close_window(window)
            child.wait(timeout=10)
            assert child.returncode==0
            wait(lambda: cursor()[:5]==outside[:5],'Exit left hidden cursor')
            if mode == 'portable':
                assert not any(path.is_dir() for path in OUTPUT.glob('zx-*')), 'Hosted extraction was not cleaned up'
        finally:
            if child.poll() is None:
                child.terminate()
                try: child.wait(timeout=5)
                except subprocess.TimeoutExpired: child.kill(); child.wait(timeout=5)
            x.XCloseDisplay(connection)
    print(f'Private Xvfb {mode}: premultiplied bitmaps, app and pointing cursors, hover marks, steady repeats, cached switching, hotspots, hide/missing/refused, leave, focus and exit passed',flush=True)

def main():
    OUTPUT.mkdir(parents=True,exist_ok=True)
    run(['git','check-ignore',OUTPUT/'probe'])
    for name in ('model-ir','host-ir','model-c','model-go','host-c'):
        shutil.rmtree(OUTPUT/name,ignore_errors=True)
    (OUTPUT/'src').mkdir(exist_ok=True); (OUTPUT/'assets').mkdir(exist_ok=True)
    # Opaque colour inside a soft, light edge: the edge must reach X11
    # premultiplied, never as a light fringe.
    for name,colour in (('hand',(224,32,64)),('tool',(32,160,64)),('app',(32,64,224)),('point',(224,160,32))):
        image=Image.new('RGBA',(1024,1024),(255,255,255,0))
        image.paste(Image.new('RGBA',(896,896),(255,255,255,96)),(64,64))
        image.paste(Image.new('RGBA',(768,768),(*colour,255)),(128,128))
        image.save(OUTPUT/f'assets/{name}.png')
    shutil.copyfile(ROOT/'tests/cursor_app.zi',OUTPUT/'src/app.zi')
    (OUTPUT/'ziran.toml').write_text('''[package]
name = "cursor_probe"
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
[tool.kryon.profiles.web]
backend = "canvas"
assets = "assets"
''')
    (OUTPUT/'ziran.local.toml').write_text(f'[overrides]\nkryon = "{ROOT}"\nziran = "{ZIRAN_ROOT}"\n')
    run([ZIRAN,'lock'],OUTPUT)
    for profile in ('desktop','web'):
        run([ZIRAN,'tool','kryon','check','--profile',profile],OUTPUT)
    common=['--project','--root',ROOT/'tests','--module-path',ROOT/'tests']
    source=ROOT/'tests/cursor_model_test.zi'
    run([ZIRAN,'ir',*common,'-o',OUTPUT/'model-ir',source])
    for form in ('source','saved'):
        entry=source if form=='source' else OUTPUT/'model-ir/cursor_model_test.zir'
        paths=common if form=='source' else ['--root',OUTPUT/'model-ir']
        bundle=OUTPUT/f'model-{form}.zx'
        run([ZIRAN,'bundle',*paths,'--entry','cursor_model_test:Answer','-o',bundle,entry])
        assert run([ZIRAN,'run',bundle],capture_output=True,text=True).stdout.strip()=='42'
        for target in ('c','go'):
            gen=OUTPUT/f'model-{target}'
            run([ZIRAN,'build','--target='+target,*(['--pkg','main'] if target=='go' else []),'--exe',*paths,'--entry','cursor_model_test:main','-o',gen,entry])
            if target=='go':
                run(['go','build','-o',gen/'cursor_model_test',*sorted(gen.glob('*.go'))])
            run([gen/'cursor_model_test'])
    host_common=['--project','--root',ROOT/'tests','--module-path',ROOT/'tests']
    host=ROOT/'tests/cursor_host_test.zi'
    noop=ROOT/'tests/ziran_widget_noop_host.zi'
    clipboard=ROOT/'tests/system_clipboard_host.zi'
    run([ZIRAN,'ir',*host_common,'-o',OUTPUT/'host-ir',host,noop,clipboard])
    for form in ('source','saved'):
        paths=host_common if form=='source' else ['--root',OUTPUT/'host-ir']
        roots=[host,noop,clipboard] if form=='source' else [OUTPUT/'host-ir/cursor_host_test.zir',OUTPUT/'host-ir/ziran_widget_noop_host.zir',OUTPUT/'host-ir/system_clipboard_host.zir']
        gen=OUTPUT/'host-c'
        run([ZIRAN,'build','--target=c','--no-main',*paths,'--entry','cursor_host_test:main','-o',gen,*roots])
        run([os.environ.get('CC','cc'),'-O1','-ffunction-sections','-fdata-sections',
             '-I'+str(gen),'-I'+str(ZIRAN_ROOT/'include'),*sorted(gen.glob('*.c')),
             ZIRAN_ROOT/'build/libziran.a','-Wl,--gc-sections','-lm','-lpthread','-o',OUTPUT/'host-test'])
        run([OUTPUT/'host-test'])
    run(['make','zx-player'])
    for mode in ('native','portable'):
        if mode == 'native':
            run([ZIRAN,'tool','kryon','build','--profile','desktop'],OUTPUT)
        else:
            run([ZIRAN,'bundle','--project','--define','KRYON_HOSTED_UI',
                 '--entry','app:main','--asset-dir','assets=assets',
                 '-o',OUTPUT/'build/cursor_probe-portable.zx','src/app.zi'],OUTPUT)
            run([ROOT/'build/bin/zx','--check',OUTPUT/'build/cursor_probe-portable.zx'])
            for invalid in ('relative',str(OUTPUT/'missing-cursor-tmp'),'/'+'x'*4096):
                rejected=subprocess.run([ROOT/'build/bin/zx',OUTPUT/'build/cursor_probe-portable.zx'],
                    cwd=OUTPUT,env=ENV | {'TMPDIR':invalid},capture_output=True,timeout=15)
                assert rejected.returncode == 1, 'Invalid extraction TMPDIR was accepted'
        run(['xvfb-run','-a','-s','-screen 0 1400x1000x24 -nolisten tcp','env','KRYON_PRIVATE_XVFB=1',sys.executable,Path(__file__).resolve(),'--drive',mode])
    # Canvas and DOM: the same requests as CSS cursors in a private headless
    # Chromium. The short profile path keeps its sockets below 108 bytes.
    for profile in ('web',):
        run([ZIRAN,'tool','kryon','build','--profile',profile],OUTPUT)
        run([sys.executable,ROOT/'tests/cursor_browser.py',OUTPUT/f'build/cursor_probe-{profile}.html',ROOT/f'build/scratch/cw-{profile}'])
    print('Cursor source/saved IR, C, Go and portable model passed',flush=True)

if __name__=='__main__':
    if len(sys.argv)==3 and sys.argv[1]=='--drive': drive(sys.argv[2])
    else: main()
