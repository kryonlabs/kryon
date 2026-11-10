"""Drive the stable native gallery shell on an owned private X display."""
import ctypes as C
import json
import os
from pathlib import Path
import subprocess
import sys
import time

from PIL import Image, ImageChops
from Xlib.display import Display


ROOT = Path(__file__).resolve().parents[1]
BINARY = ROOT / 'examples/gallery/build/gallery-desktop'
OUTPUT = ROOT / 'build/scratch/gallery-desktop-shell'
SCRUB = {
    'DISPLAY', 'WAYLAND_DISPLAY', 'WAYLAND_SOCKET', 'XAUTHORITY',
    'DBUS_SESSION_BUS_ADDRESS', 'SESSION_MANAGER', 'DESKTOP_STARTUP_ID',
    'XDG_SESSION_ID', 'XDG_SESSION_TYPE', 'KRYON_CAPTURE_PATH',
}


class CursorImage(C.Structure):
    _fields_ = [
        ('x', C.c_short), ('y', C.c_short), ('width', C.c_ushort),
        ('height', C.c_ushort), ('xhot', C.c_ushort), ('yhot', C.c_ushort),
        ('serial', C.c_ulong), ('pixels', C.POINTER(C.c_ulong)),
        ('atom', C.c_ulong), ('name', C.c_char_p),
    ]


def environment():
    env = {key: value for key, value in os.environ.items() if key not in SCRUB}
    env.update(
        YUE_DESKTOP_RECOVERY='0', SDL_RENDER_DRIVER='software',
        LIBGL_ALWAYS_SOFTWARE='1',
        KRYON_FONT_PATH=str(ROOT / 'assets/fonts/LiberationSans-Regular.ttf'))
    return env


def rms(left, right, box=None):
    with Image.open(left) as first, Image.open(right) as second:
        if box:
            first = first.crop(box)
            second = second.crop(box)
        if first.size != second.size:
            raise AssertionError((left, right, first.size, second.size))
        difference = ImageChops.difference(first.convert('RGB'), second.convert('RGB'))
        histogram = difference.histogram()
        channels = (histogram[0:256], histogram[256:512], histogram[512:768])
        total = sum(value * value * count for counts in channels
                    for value, count in enumerate(counts))
        pixels = first.size[0] * first.size[1] * 3
        return (total / pixels) ** 0.5


def private_drive():
    assert os.environ.get('KRYON_PRIVATE_XVFB') == '1'
    assert os.environ['DISPLAY'] != ':0'
    OUTPUT.mkdir(parents=True, exist_ok=True)
    assert BINARY.is_file() and os.access(BINARY, os.X_OK), BINARY
    data = (ROOT / 'docs/site/gallery-data.js').read_text()
    names = [item['name'] for item in json.loads(data.split('window.galleryScenes = ', 1)[1].rstrip(';\n'))]
    assert len(names) == 101
    assert 'GallerySceneCount :: 101;' in (
        ROOT / 'examples/gallery/build/catalog/catalog.zi').read_text()

    x11 = C.CDLL('libX11.so.6')
    fixes = C.CDLL('libXfixes.so.3')
    x11.XOpenDisplay.argtypes = [C.c_char_p]
    x11.XOpenDisplay.restype = C.c_void_p
    x11.XSync.argtypes = [C.c_void_p, C.c_int]
    x11.XFree.argtypes = [C.c_void_p]
    fixes.XFixesGetCursorImage.argtypes = [C.c_void_p]
    fixes.XFixesGetCursorImage.restype = C.POINTER(CursorImage)
    connection = x11.XOpenDisplay(os.environ['DISPLAY'].encode())
    assert connection
    xdisplay = Display(os.environ['DISPLAY'])

    def cursor():
        x11.XSync(connection, 0)
        image = fixes.XFixesGetCursorImage(connection)
        assert image
        value = image.contents
        result = (value.width, value.height, value.xhot, value.yhot,
                  tuple(value.pixels[index] & 0xffffffff
                        for index in range(value.width * value.height)),
                  (value.name or b'').decode(errors='replace'))
        x11.XFree(image)
        return result

    def xdo(*args):
        return subprocess.check_output(
            ['xdotool', *map(str, args)], text=True, timeout=5).strip()

    driver = OUTPUT / 'driver'
    driver.mkdir(exist_ok=True)
    for path in driver.iterdir():
        if path.is_file(): path.unlink()
    serial = 0

    def tree():
        nonlocal serial
        serial += 1
        pending = driver / 'pending'
        pending.write_text(str(serial) + chr(9) + 'tree' + chr(10))
        pending.replace(driver / 'command')
        deadline = time.monotonic() + 4
        while not (driver / 'reply').exists():
            assert time.monotonic() < deadline, 'No native widget-tree reply'
            time.sleep(0.02)
        reply = json.loads((driver / 'reply').read_text())
        (driver / 'reply').unlink()
        assert reply['ok'], reply
        return reply['nodes']

    def selected_scene(name):
        nodes = tree()
        sidebar = next(node for node in nodes if node['key'] == 9003)
        assert sidebar['x'] == 500 and sidebar['height'] == 652, sidebar
        assert any(node['label'] == name and node['selected'] for node in nodes), (name, nodes)
        return nodes

    log_path = OUTPUT / 'gallery-desktop.log'
    with log_path.open('w') as log:
        child_env = environment()
        child_env['KRYON_TEST_DRIVER'] = str(driver)
        child_env['DISPLAY'] = os.environ['DISPLAY']
        if 'XAUTHORITY' in os.environ:
            child_env['XAUTHORITY'] = os.environ['XAUTHORITY']
        child = subprocess.Popen([str(BINARY)], cwd=ROOT, env=child_env,
                                 stdout=log, stderr=subprocess.STDOUT)
        window = ''

        def capture(name):
            path = OUTPUT / f'{name}.png'
            subprocess.run(['import', '-window', window, f'PNG32:{path}'],
                           check=True, timeout=5)
            return path

        def move(x, y):
            xdo('mousemove', '--window', window, x, y)
            time.sleep(0.08)

        def click(x, y):
            xdo('mousemove', '--window', window, x, y, 'click', '1')
            time.sleep(0.08)

        def choose_scene(steps):
            row = next(node for node in tree() if node['selected'] and node['x'] == 500 and node['visible'])
            click(row['visible_x'] + 40, row['visible_y'] + row['visible_height'] / 2)
            for step in steps:
                xdo('key', '--window', window, '--delay', '40', step)
                time.sleep(0.08)
            time.sleep(0.15)

        try:
            deadline = time.monotonic() + 8
            while not window and time.monotonic() < deadline:
                root = xdisplay.screen().root
                for candidate in root.query_tree().children:
                    geometry = candidate.get_geometry()
                    if (candidate.get_attributes().map_state == 2 and
                            candidate.query_tree().parent.id == root.id and
                            geometry.width == 1000 and geometry.height == 700):
                        window = str(candidate.id)
                        break
                if not window and child.poll() is not None:
                    raise AssertionError(log_path.read_text())
                if not window:
                    time.sleep(0.02)
            assert window, 'Private gallery window did not open'

            def steady_cursor():
                first = cursor()
                for _ in range(20):
                    if cursor()[:5] == first[:5]:
                        return first
                    time.sleep(0.02)
                return cursor()

            xdo('windowmove', window, 80, 60)
            xdo('windowfocus', '--sync', window)
            time.sleep(0.25)
            buttons = [n for n in tree() if n['label'] == 'Continue' and n['kind'] == 'button']
            assert len(buttons) == 12, 'native default does not render maintained Button Frame'
            click(buttons[0]['x'] + 40, buttons[0]['y'] + 20)
            assert any(n['key'] == 2 and n['label'] == '1' for n in tree()), 'native maintained counter did not increment'
            initial = capture('initial')
            with Image.open(initial) as image:
                assert image.size == (1000, 700)

            move(480, 100)
            background = steady_cursor()
            move(550, 39)
            navigation = steady_cursor()
            assert navigation != background, (background, navigation)
            assert 'text' not in navigation[-1].lower(), navigation

            selected_scene('Button')
            choose_scene(('Home',) + ('Down',) * names.index('CommandPalette'))
            selected_scene('CommandPalette')
            assert not any(node['label'] == 'Search commands' for node in tree())
            second_scene = capture('second-scene')
            assert rms(initial, second_scene) > 2

            click(392, 656)
            dark = capture('dark')
            assert rms(second_scene, dark) > 2
            click(392, 656)

            palette_closed = capture('palette-closed')
            move(101, 145)
            trigger = steady_cursor()
            assert trigger == navigation, (navigation, trigger)
            click(101, 145)
            palette_open = capture('palette-open')
            assert any(node['label'] == 'Search commands' for node in tree())
            query = next(node for node in tree() if node['label'] == 'Search commands')
            assert query['y'] >= 64 and query['y'] + query['height'] <= 272, query
            for letter in 'format':
                xdo('key', '--window', window, '--delay', '40', letter)
                time.sleep(0.04)
            time.sleep(0.15)
            palette_typed = capture('palette-typed')
            assert any(node['label'] == 'format' for node in tree()), tree()
            xdo('key', '--window', window, '--delay', '40', 'Escape')
            time.sleep(0.15)
            capture('palette-closed-again')
            assert not any(node['label'] == 'format' for node in tree())
            click(101, 145)
            time.sleep(0.15)
            palette_reopened = capture('palette-reopened')
            assert any(node['label'] == 'format' for node in tree())

            assert rms(palette_closed, palette_open, (24, 64, 456, 272)) > 2
            assert rms(palette_open, palette_typed, (24, 64, 456, 272)) > 1
            assert rms(palette_typed, palette_reopened, (24, 64, 456, 272)) < 1
            assert rms(palette_closed, palette_open, (0, 620, 480, 700)) < 1
            assert rms(palette_closed, palette_open, (0, 0, 1000, 64)) < 1
            move(240, 160)
            option = steady_cursor()
            assert option == navigation, (navigation, option)

            xdo('key', '--window', window, '--delay', '40', 'Escape')
            time.sleep(0.15)
            choose_scene(('Home', 'Down'))
            selected_scene('TextField')
            move(100, 86)
            editable = steady_cursor()
            assert editable != navigation, (navigation, editable)
            assert ('text' in editable[-1].lower() or
                    editable[:4] != navigation[:4]), editable
            click(100, 86)
            before = [node['value'] for node in tree() if node['focused']]
            xdo('type', '--window', window, '--delay', '40', 'native input')
            time.sleep(0.15)
            after = [node['value'] for node in tree() if node['focused']]
            assert after != before and any('native input' in value for value in after), after

            option = next(node for node in tree() if node['label'] == 'TextArea' and node['x'] == 500 and node['visible'])
            click(option['visible_x'] + 40, option['visible_y'] + option['visible_height'] / 2)
            selected_scene('TextArea')

            def visible_rows():
                return [node['label'] for node in tree() if node['x'] == 500 and node['kind'] == 'selectable' and node['visible']]
            rows_before = visible_rows()
            move(700, 350)
            xdo('click', '--repeat', 5, '--delay', 40, 5)
            time.sleep(0.15)
            assert visible_rows() != rows_before, 'Sidebar wheel did not scroll'
            xdo('click', '--repeat', 5, '--delay', 40, 4)
            time.sleep(0.15)
            assert visible_rows() == rows_before, 'Sidebar wheel did not return'

            reviews = ((initial, 'gallery-shell'),
                       (palette_reopened, 'gallery-palette'))
            for source, name in reviews:
                with Image.open(source) as image:
                    image.save(OUTPUT / f'{name}.webp', 'WEBP',
                               quality=86, method=6)
            for index, name in enumerate(names):
                choose_scene(('Home',) if index == 0 else ('Down',))
                nodes = selected_scene(name)
                if name in ('Button', 'Button emphasis'):
                    assert sum(n['kind'] == 'button' and n['label'] == 'Continue' for n in nodes) == 12
                    assert any(n['key'] == 2 and n['label'] == '1' for n in nodes), 'source variant lost shared state'
                if name == 'Empty state':
                    checkbox = next(n for n in nodes if n['key'] == 9004)
                    click(checkbox['x'] + 20, checkbox['y'] + 20)
                    button = next(n for n in tree() if n['label'] == 'Create project')
                    assert not button['enabled']
                    click(button['x'] + 40, button['y'] + 20)
                    assert not any(n['label'] == 'Project created' for n in tree()), 'disabled action fired'
                    click(checkbox['x'] + 20, checkbox['y'] + 20)
                    click(button['x'] + 40, button['y'] + 20)
                    assert any(n['label'] == 'Project created' for n in tree()), 'reenabled action did not fire'
                    click(checkbox['x'] + 20, checkbox['y'] + 20)
                if name == 'Breadcrumb':
                    buttons = [n for n in nodes if n['kind'] == 'button' and n['x'] < 500]
                    assert buttons and all(not n['enabled'] for n in buttons), 'disabled flag lost on scene change'
                    checkbox = next(n for n in nodes if n['key'] == 9004)
                    click(checkbox['x'] + 20, checkbox['y'] + 20)
                if name in ('Image', 'Carousel', 'Tile map'):
                    from site_browser_test import assert_picture_pixels
                    images = [node for node in nodes if node['kind'] == 'image' and node['visible']]
                    assert images, (name, 'no visible image widget')
                    node = max(images, key=lambda n: n['visible_width'] * n['visible_height'])
                    left, top = node['visible_x'], node['visible_y']
                    width, height = node['visible_width'], node['visible_height']
                    box = (left + width*.15, top + height*.15, left + width*.85, top + height*.85)
                    time.sleep(.2)
                    with Image.open(capture('image-' + name.lower().replace(' ', '-'))) as image:
                        assert_picture_pixels(image, box, name)
                        image.convert('RGB').save(OUTPUT / (name.lower().replace(' ', '-') + '.webp'), 'WEBP', quality=82)
            def tab_until(label, backwards=False):
                visited = []
                for _ in range(32):
                    xdo('key', '--window', window, 'shift+Tab' if backwards else 'Tab')
                    time.sleep(0.08)
                    focused = [(node['key'], node['kind'], node['label']) for node in tree() if node['focused']]
                    visited.append(focused)
                    if any(item[2] == label for item in focused):
                        print('Tab focus path:', visited)
                        return
                raise AssertionError((label, visited))
            tab_until('Disabled')
            tab_until('Dark')
            tab_until(names[-1], backwards=True)
            choose_scene(('Home',))
            selected_scene('Button')
            print('All 101 scenes selected through the native sidebar')
            print('Native gallery private-display shell, sidebar navigation, cursors '
                  'and persistent palette passed')
            print('Review images:', OUTPUT / 'gallery-shell.webp',
                  OUTPUT / 'gallery-palette.webp')
        finally:
            if child.poll() is None:
                child.terminate()
                try:
                    child.wait(timeout=5)
                except subprocess.TimeoutExpired:
                    child.kill()
                    child.wait(timeout=5)


def main():
    subprocess.run([
        'xvfb-run', '-a', '-n', '410',
        '-s', '-screen 0 1100x800x24 -nolisten tcp',
        'env', 'KRYON_PRIVATE_XVFB=1', sys.executable,
        Path(__file__).resolve(), '--private',
    ], cwd=ROOT, env=environment(), check=True)


if __name__ == '__main__':
    if len(sys.argv) == 2 and sys.argv[1] == '--private':
        private_drive()
    else:
        main()
