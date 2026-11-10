"""Tap the test phone's actual IME keys; adb input text bypasses IME edits."""
import re
import time
import xml.etree.ElementTree as ET


def wait_keyboard_visible(device, expected):
    deadline = time.monotonic() + 12
    while True:
        state = device('shell', 'dumpsys', 'input_method').decode(errors='replace')
        shown = re.findall(r'\bmInputShown=(true|false)', state)
        if shown and (shown[-1] == 'true') == expected:
            return
        assert time.monotonic() < deadline, 'Phone IME visibility did not become ' + str(expected) + '\n' + state[-2000:]
        time.sleep(.2)


def soft_keyboard_text(device, text):
    dump = device('shell', 'uiautomator', 'dump', '/dev/tty').decode(errors='replace')
    start = dump.find('<?xml')
    end = dump.rfind('</hierarchy>')
    assert start >= 0 and end >= start, 'Phone did not expose its IME accessibility tree'
    root = ET.fromstring(dump[start:end + len('</hierarchy>')])
    keys = {}
    for node in root.iter('node'):
        if 'inputmethod' not in node.get('package', ''):
            continue
        label = node.get('content-desc') or node.get('text') or ''
        if len(label) == 1 and label.isascii() and label.isalpha():
            bounds = list(map(int, re.findall(r'\d+', node.get('bounds', ''))))
            if len(bounds) == 4:
                left, top, right, bottom = bounds
                keys[label.lower()] = (round((left + right) / 2), round((top + bottom) / 2))
    assert all(letter in keys for letter in text), 'The active phone IME did not expose the requested letter keys'
    for letter in text:
        x, y = keys[letter]
        device('shell', 'input', 'tap', str(x), str(y))
