"""Keyboard events on the actual plan9port window, on a private X display."""
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import time


def wait_for(test, message):
    deadline = time.monotonic() + 5
    while time.monotonic() < deadline:
        value = test()
        if value:
            return value
        time.sleep(.03)
    raise AssertionError(message)


with tempfile.TemporaryDirectory(prefix='kryon-keyboard-') as directory:
    path = Path(directory) / 'events'
    process = subprocess.Popen([sys.argv[1]], env=dict(os.environ, KRYON_LIBDRAW_INPUT_TEST=str(path)))
    try:
        window = wait_for(lambda: subprocess.run(
            ['xdotool', 'search', '--onlyvisible', '--name', '^Kryon keyboard input$'],
            capture_output=True, text=True).stdout.strip(), 'keyboard window did not map')
        subprocess.run(['xdotool', 'windowfocus', '--sync', window], check=True)
        wait_for(lambda: 'focus 1\n' in path.read_text(), 'focus was not reported')
        subprocess.run(['xdotool', 'keydown', 'ctrl', 'keydown', 'c', 'sleep', '.15',
                        'keyup', 'c', 'keyup', 'ctrl'], check=True)
        wait_for(lambda: 'release-control 0\n' in path.read_text(), 'Control remained stuck')
        assert 'copy 1\n' in path.read_text(), path.read_text()
        assert 'release-c 0\n' in path.read_text(), path.read_text()
        subprocess.run(['xdotool', 'key', '--delay', '0', 'ctrl+c'], check=True)
        wait_for(lambda: path.read_text().count('copy 1\n') == 2, 'quick shortcut lost modifiers')
        subprocess.run(['xdotool', 'keydown', 'shift', 'keydown', 'a', 'sleep', '.15',
                        'keyup', 'a', 'keyup', 'shift', 'key', 'Delete'], check=True)
        wait_for(lambda: 'delete 0\n' in path.read_text(), 'Delete did not arrive separately from Backspace')
        subprocess.run(['xdotool', 'key', 'BackSpace'], check=True)
        wait_for(lambda: 'backspace 0\n' in path.read_text(), 'Backspace did not arrive')
        assert 'a 1\n' in path.read_text(), path.read_text()
        assert 'release-a 0\n' in path.read_text(), path.read_text()
        assert 'delete 0\n' in path.read_text(), path.read_text()
        subprocess.run(['xdotool', 'keydown', 'a', 'sleep', '.1', 'windowfocus', '0', 'keyup', 'a'], check=True)
        wait_for(lambda: path.read_text().count('release-a 0\n') >= 2, 'focus loss left A down')
        subprocess.run(['xdotool', 'windowfocus', '--sync', window, 'key', 'q'], check=True)
        assert process.wait(timeout=5) == 0
    finally:
        if(path.exists()):
            print(path.read_text(), end='')
        if process.poll() is None:
            process.kill()
            process.wait()

print('libdraw shortcuts, key releases, Delete/Backspace and focus loss passed')
