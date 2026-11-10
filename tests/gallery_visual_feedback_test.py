"""Render maintained gallery examples through the production native host."""
import hashlib
import json
import os
from pathlib import Path
import subprocess
import sys
import time
from PIL import Image

ROOT = Path(__file__).resolve().parents[1]
sys.path[:0] = [str(ROOT / 'scripts'), str(ROOT / 'tests')]
from gallery import prepare
from toolchain import ZIRAN_ROOT

OUTPUT = ROOT / 'build/scratch/gallery-visual-feedback'
BINARY = OUTPUT / 'gallery-testhost'
OWNER_BINARY = ROOT / 'examples/gallery/build/gallery-desktop'
EVIDENCE = ROOT / 'backup/cui-motion/visual-feedback'

def sha(path):
    value = hashlib.sha256()
    with path.open('rb') as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b''):
            value.update(block)
    return value.hexdigest()

def build():
    OUTPUT.mkdir(parents=True, exist_ok=True)
    previous = sha(OWNER_BINARY)
    prepare(site=OUTPUT / 'site')
    command = ['make', '--no-print-directory', '-f', str(ROOT / 'mk/ziran-project.mk'),
        'build', 'PROJECT_NAME=gallery', 'PROJECT_ENTRY=src/app.zi',
        'PROJECT_BACKEND=desktop', 'PROJECT_CODEGEN=c99', 'PROJECT_PROFILE=desktop',
        'PROJECT_HOST=src/desktop.zi', 'PROJECT_ASSETS=assets',
        'PROJECT_EXTRA_SOURCES=build/catalog/catalog.zi', 'ZIRAN_PACKAGE_ID=gallery',
        'KRYON_DIR=' + str(ROOT), 'ZIRAN_DIR=' + str(ZIRAN_ROOT),
        'PROGRAM=' + str(BINARY), 'GEN_DIR=' + str(OUTPUT / 'generated')]
    subprocess.run(command, cwd=ROOT / 'examples/gallery', check=True)
    assert sha(OWNER_BINARY) == previous, 'The running gallery executable changed'
    print('Production gallery test host built; owner executable preserved', flush=True)

def environment():
    scrub = {'DISPLAY', 'WAYLAND_DISPLAY', 'WAYLAND_SOCKET', 'XAUTHORITY',
        'DBUS_SESSION_BUS_ADDRESS', 'SESSION_MANAGER', 'DESKTOP_STARTUP_ID',
        'XDG_SESSION_ID', 'XDG_SESSION_TYPE', 'KRYON_CAPTURE_PATH'}
    env = {key: value for key, value in os.environ.items() if key not in scrub}
    env.update(YUE_DESKTOP_RECOVERY='0', SDL_RENDER_DRIVER='software',
        LIBGL_ALWAYS_SOFTWARE='1', KRYON_FONT_PATH=str(ROOT / 'assets/fonts/LiberationSans-Regular.ttf'))
    return env

def scenes():
    data = (OUTPUT / 'site/gallery-data.js').read_text()
    return json.JSONDecoder().raw_decode(data.split('window.galleryScenes = ', 1)[1])[0]

def static_captures(selected=None):
    EVIDENCE.mkdir(parents=True, exist_ok=True)
    names = {item['name']: item['index'] for item in scenes()}
    for name in ['OneTimeCode', 'ScrubField', 'Direction pad', 'Joystick']:
        if selected and name not in selected:
            continue
        label = 'DirectionPad' if name == 'Direction pad' else name
        png = OUTPUT / (label + '-after.png')
        env = {**environment(), 'KRYON_CATALOG_SCENE': str(names[name]),
            'KRYON_CATALOG_THEME': 'light', 'KRYON_CAPTURE_PATH': str(png)}
        subprocess.run(['xvfb-run', '-a', str(BINARY)], cwd=ROOT, env=env, check=True)
        with Image.open(png) as image:
            assert image.size == (480, 320)
            image.save(EVIDENCE / (label + '-after.webp'), 'WEBP', lossless=True)
        print('Actual gallery Frame/Cairo capture:', label, flush=True)

def alignment_pixels():
    def ink(path,box):
        image = Image.open(path).convert('RGB').crop(box)
        points = [(x+box[0],y+box[1]) for y in range(image.height) for x in range(image.width)
            if max(image.getpixel((x,y))) < 120]
        assert len(points)>8,(path,box,'No glyph ink')
        return (min(x for x,y in points),min(y for x,y in points),max(x for x,y in points),max(y for x,y in points))
    centers = []
    for x in [95,142,189,250,297,344]:
        bounds = ink(EVIDENCE/'OneTimeCode-after.webp',(x+5,101,x+36,139))
        center = ((bounds[0]+bounds[2])/2,(bounds[1]+bounds[3])/2)
        assert abs(center[0]-(x+20))<=2.5 and abs(center[1]-120)<=3,(bounds,center)
        centers.append(center)
    scrub = []
    for box in [(31,102,135,132),(340,102,371,132),(377,102,425,132)]:
        bounds = ink(EVIDENCE/'ScrubField-after.webp',box)
        scrub.append((bounds[1]+bounds[3])/2)
    assert max(scrub)-min(scrub)<=2.5,scrub
    for name in ['DirectionPad','Joystick']:
        ink(EVIDENCE/(name+'-after.webp'),(20,268,100,296))
        ink(EVIDENCE/(name+'-after.webp'),(170,268,201,296))
    print('Real font pixel alignment: OTP centers',centers,'ScrubField ink centers',scrub,flush=True)

def motion_inputs():
    if os.environ.get('KRYON_PRIVATE_XVFB') != '1':
        env = environment()
        env['KRYON_PRIVATE_XVFB'] = '1'
        subprocess.run(['xvfb-run', '-a', sys.executable, str(Path(__file__).resolve()), '--motion-private'], cwd=ROOT, env=env, check=True)
        return
    from Xlib.display import Display
    from Xlib import X
    assert os.environ['DISPLAY'] != ':0'
    connection = Display(os.environ['DISPLAY'])
    driver = OUTPUT / 'driver'
    driver.mkdir(exist_ok=True)
    for path in driver.iterdir():
        if path.is_file(): path.unlink()
    child_env = environment()
    child_env.update(DISPLAY=os.environ['DISPLAY'], KRYON_TEST_DRIVER=str(driver))
    if 'XAUTHORITY' in os.environ: child_env['XAUTHORITY'] = os.environ['XAUTHORITY']
    serial = 0
    def tree():
        nonlocal serial
        serial += 1
        pending = driver / 'pending'
        pending.write_text(str(serial) + chr(9) + 'tree' + chr(10))
        pending.replace(driver / 'command')
        deadline = time.monotonic() + 4
        while not (driver / 'reply').exists():
            assert time.monotonic() < deadline, 'No test-host tree reply'
            time.sleep(.005)
        reply = json.loads((driver / 'reply').read_text())
        (driver / 'reply').unlink()
        assert reply['ok'], reply
        tree.frame = reply['frame']
        return reply['nodes']
    def xdo(*args):
        subprocess.run(['xdotool', *map(str, args)], check=True, timeout=5)
    with (OUTPUT / 'motion-native.log').open('w') as log:
        child = subprocess.Popen([str(BINARY)], cwd=ROOT, env=child_env, stdout=log, stderr=subprocess.STDOUT)
        try:
            window = None
            deadline = time.monotonic() + 8
            while window is None and time.monotonic() < deadline:
                assert child.poll() is None, 'Native test host exited'
                for candidate in connection.screen().root.query_tree().children:
                    g = candidate.get_geometry()
                    if candidate.get_attributes().map_state == 2 and (g.width,g.height) == (1000,700):
                        window = candidate
                        break
                if window is None: time.sleep(.02)
            assert window is not None
            xdo('windowfocus', '--sync', window.id)
            names = [item['name'] for item in scenes()]
            def select(name):
                row = next(n for n in tree() if n['selected'] and n['x'] == 500 and n['visible'])
                xdo('mousemove', '--window', window.id, row['visible_x'] + 30, row['visible_y'] + row['visible_height']/2, 'click', '1')
                xdo('key', '--window', window.id, '--delay', '30', 'Home', *(['Down'] * names.index(name)))
                time.sleep(.8)
                assert any(n['selected'] and n['label'] == name for n in tree()), name
            def image():
                raw = window.get_image(0, 0, 480, 320, X.ZPixmap, 0xffffffff)
                return Image.frombytes('RGB', (480,320), raw.data, 'raw', 'BGRX')
            def distance(a,b):
                from PIL import ImageChops, ImageStat
                return sum(ImageStat.Stat(ImageChops.difference(a,b)).mean)
            cases = [('TabBar',(90,85)), ('NavigationBar',(240,85)), ('Collapsible',(150,85)), ('Menu',(420,280))]
            results = {}
            for name,(x,y) in cases:
                selected = os.environ.get('KRYON_MOTION_SCENES', '').split(',')
                if selected != [''] and name not in selected: continue
                select(name)
                if name == 'NavigationBar':
                    target = next(n for n in tree() if n['label'] == 'Library' and n['x'] < 480 and n['interactive'])
                    x,y = target['x']+target['width']/2, target['y']+target['height']/2
                xdo('mousemove', '--window', window.id, x, y)
                time.sleep(.12)
                tree()
                first_frame = tree.frame
                first_time = time.monotonic()
                frames = [image()]
                xdo('mousedown','1')
                time.sleep(.018)
                xdo('mouseup','1')
                active_rate = 0.0
                for sample in range(36):
                    frames.append(image())
                    time.sleep(.018)
                    if sample == 11:
                        tree()
                        active_rate = (tree.frame-first_frame)/(time.monotonic()-first_time)
                time.sleep(1.4)
                end = image()
                time.sleep(.15)
                rest = image()
                changed = [i for i,f in enumerate(frames) if distance(f,frames[0]) > .02 and distance(f,end) > .02]
                if len(changed) < 2:
                    frames[0].save(OUTPUT / (name+'-failure-start.png'))
                    end.save(OUTPUT / (name+'-failure-end.png'))
                    (OUTPUT / (name+'-failure-tree.json')).write_text(json.dumps(tree()))
                assert len(changed) >= 2, (name,'No rendered intermediate frames')
                assert distance(frames[0],end) > .05, (name,'No final pixel change')
                assert distance(end,rest) < .01, (name,'Motion did not converge')
                frames[0].save(EVIDENCE / (name+'-motion-start.webp'),'WEBP',lossless=True)
                frames[changed[len(changed)//2]].save(EVIDENCE / (name+'-motion-mid.webp'),'WEBP',lossless=True)
                end.save(EVIDENCE / (name+'-motion-end.webp'),'WEBP',lossless=True)
                tree()
                idle_frame,idle_time = tree.frame,time.monotonic()
                time.sleep(.5)
                tree()
                idle_rate = (tree.frame-idle_frame)/(time.monotonic()-idle_time)
                assert active_rate >= 28, (name,'Active frame rate',active_rate)
                assert idle_rate <= 23, (name,'Idle frame rate',idle_rate)
                results[name] = {'intermediate_frames':len(changed), 'settled_difference':distance(end,rest), 'active_fps':active_rate, 'idle_fps':idle_rate}
                print('Real native input motion:',name,results[name],flush=True)
            for name in ['Direction pad','Joystick']:
                select(name)
                nodes = tree()
                assert any(n['key']==11 and n['label']=='Steps' for n in nodes),name
                initial = int(next(n['label'] for n in nodes if n['key']==12))
                xdo('mousemove','--window',window.id,175,166,'mousedown','1')
                time.sleep(.1)
                xdo('mouseup','1')
                time.sleep(.1)
                advanced = int(next(n['label'] for n in tree() if n['key']==12))
                assert advanced > initial,(name,'Counter did not follow real input',initial,advanced)
                time.sleep(.2)
                assert int(next(n['label'] for n in tree() if n['key']==12)) == advanced,(name,'Counter advanced at rest')
                print('Labeled movement counter follows real input:',name,initial,advanced,flush=True)
            (EVIDENCE / 'native-motion-results.json').write_text(json.dumps(results,indent=2)+'\n')
        finally:
            if child.poll() is None: child.terminate()
            child.wait(timeout=5)
            connection.close()

if __name__ == '__main__':
    if '--motion-private' in sys.argv or '--motion-only' in sys.argv:
        motion_inputs()
    else:
        if '--capture-only' not in sys.argv: build()
        static_captures()
        alignment_pixels()
        if '--capture-only' not in sys.argv: motion_inputs()
