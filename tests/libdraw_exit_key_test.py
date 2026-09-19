"""Run under a private Xvfb display with the built libdraw smoke binary."""
import os
import subprocess
import sys
import time

for mode in ('disabled', 'custom', 'default'):
    environment = dict(os.environ, KRYON_LIBDRAW_EXIT_KEY_TEST=mode)
    process = subprocess.Popen([sys.argv[1]], env=environment)
    try:
        deadline = time.monotonic() + 5
        window = ''
        while time.monotonic() < deadline and process.poll() is None:
            window = subprocess.run(['xdotool', 'search', '--onlyvisible', '--name',
                                     '^Kryon exit key$'], capture_output=True, text=True).stdout.strip()
            if window:
                break
            time.sleep(.05)
        if not window:
            raise AssertionError('exit-key test window did not map')
        subprocess.run(['xdotool', 'windowfocus', '--sync', window, 'key', 'Escape'], check=True)
        if mode == 'custom':
            time.sleep(.2)
            if process.poll() is not None:
                raise AssertionError('Escape ignored the configured custom exit key')
            subprocess.run(['xdotool', 'key', 'q'], check=True)
        if process.wait(timeout=8) != 0:
            raise AssertionError('exit-key test failed: ' + mode)
    finally:
        if process.poll() is None:
            process.kill()
            process.wait()
print('libdraw disabled, custom and default exit keys passed')
