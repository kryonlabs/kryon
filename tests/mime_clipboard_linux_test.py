#!/usr/bin/env python3
"""Private native MIME selection interoperability, source and saved IR."""
import ctypes as C
import os
from pathlib import Path
import select
import struct
import subprocess
import sys
import threading
import time

ROOT = Path(__file__).resolve().parents[1]
WORK = ROOT / 'build/scratch/mime-clipboard'
SCRUB = ('DISPLAY', 'WAYLAND_DISPLAY', 'WAYLAND_SOCKET', 'XAUTHORITY',
         'DBUS_SESSION_BUS_ADDRESS', 'SESSION_MANAGER', 'ICEAUTHORITY')
URI = 'file:///tmp/世界\r\n'.encode()
COPIED = 'cut\nfile:///tmp/世界\n'.encode()
BINARY = bytes(i % 251 for i in range(1048576))


def run(command, **kw):
    result = subprocess.run(list(map(str, command)), stdout=subprocess.PIPE,
                            stderr=subprocess.PIPE, timeout=180, **kw)
    if result.returncode:
        raise AssertionError(f'{command}: {result.returncode}\n{result.stderr.decode(errors="replace")[-6000:]}')
    return result.stdout


def line(process):
    assert select.select([process.stdout], [], [], 5)[0], 'probe response timeout'
    data = process.stdout.readline()
    assert data, f'probe exited {process.poll()}: {process.stderr.read(2000)!r}'
    return [int(n) for n in data.split()]


def command(process, value):
    process.stdin.write(value)
    process.stdin.flush()
    return line(process)


class Request(C.Structure):
    _fields_ = [('type', C.c_int), ('serial', C.c_ulong), ('send', C.c_int),
                ('display', C.c_void_p), ('owner', C.c_ulong),
                ('requestor', C.c_ulong), ('selection', C.c_ulong),
                ('target', C.c_ulong), ('property', C.c_ulong), ('time', C.c_ulong)]


class Notify(C.Structure):
    _fields_ = Request._fields_[:4] + Request._fields_[5:]


class Property(C.Structure):
    _fields_ = Request._fields_[:4] + [('window', C.c_ulong), ('atom', C.c_ulong),
                                     ('time', C.c_ulong), ('state', C.c_int)]


class Peer(threading.Thread):
    """Independent Xlib owner: deliberately malformed/uncooperative fixtures.

    All its Xlib calls belong to this thread. It never uses an inherited display.
    """
    def __init__(self, mode, data=b'hello\0' + '世界'.encode()):
        super().__init__()
        self.mode, self.data = mode, data
        self.ready, self.stop = threading.Event(), threading.Event()
        self.error = None
        self.start()
        assert self.ready.wait(3)
        if self.error:
            raise self.error

    def run(self):
        try:
            self.serve()
        except BaseException as error:
            self.error = error
            self.ready.set()

    def serve(self):
        x = C.CDLL('libX11.so.6')
        ptr, ul, i = C.c_void_p, C.c_ulong, C.c_int
        def bind(name, result, *args):
            f = getattr(x, name); f.restype = result; f.argtypes = args; return f
        open_ = bind('XOpenDisplay', ptr, C.c_char_p)
        close = bind('XCloseDisplay', i, ptr)
        root = bind('XDefaultRootWindow', ul, ptr)
        create = bind('XCreateSimpleWindow', ul, ptr, ul, i, i, C.c_uint, C.c_uint, C.c_uint, ul, ul)
        atom = bind('XInternAtom', ul, ptr, C.c_char_p, i)
        setowner = bind('XSetSelectionOwner', i, ptr, ul, ul, ul)
        flush = bind('XFlush', i, ptr)
        sync = bind('XSync', i, ptr, i)
        pending = bind('XPending', i, ptr)
        next_ = bind('XNextEvent', i, ptr, ptr)
        send = bind('XSendEvent', i, ptr, ul, i, C.c_long, ptr)
        watch = bind('XSelectInput', i, ptr, ul, C.c_long)
        change = bind('XChangeProperty', i, ptr, ul, ul, ul, i, i, ptr, i)
        # Timed-out requestors may destroy their window while a fixture responds.
        handler_type = C.CFUNCTYPE(i, ptr, ptr)
        handler = handler_type(lambda *_: 0)
        bind('XSetErrorHandler', ptr, handler_type)(handler)
        display = open_(None)
        assert display
        window = create(display, root(display), 0, 0, 1, 1, 0, 0, 0)
        clipboard = atom(display, b'CLIPBOARD', 0)
        incr = atom(display, b'INCR', 0)
        bad = atom(display, b'application/x-wrong', 0)
        setowner(display, clipboard, window, 0)
        sync(display, 0)
        self.window = window
        self.ready.set()
        transfers = {}
        def put(win, prop, kind, fmt, data):
            if fmt == 32:
                storage = (ul * len(data))(*data)
                change(display, win, prop, kind, fmt, 0, storage, len(data))
            else:
                storage = C.create_string_buffer(data)
                change(display, win, prop, kind, fmt, 0, storage, len(data) // (fmt // 8))
        try:
            while not self.stop.is_set():
                if not pending(display):
                    self.stop.wait(.001)
                    continue
                event = (C.c_long * 24)()
                next_(display, event)
                kind = C.cast(event, C.POINTER(i))[0]
                if kind == 30:
                    req = C.cast(event, C.POINTER(Request)).contents
                    prop = req.property or req.target
                    if self.mode == 'timeout':
                        continue
                    if self.mode == 'absent':
                        prop = 0
                    elif self.mode.startswith('incr'):
                        watch(display, req.requestor, 4194304)
                        put(req.requestor, prop, incr, 32,
                            [len(self.data) if self.mode == 'incr' else 1])
                        transfers[(req.requestor, prop)] = [req.target, 0]
                    elif self.mode == 'bad-incr':
                        put(req.requestor, prop, incr, 8, b'1234')
                    else:
                        put(req.requestor, prop, bad if self.mode == 'wrong-type' else req.target,
                            16 if self.mode == 'wrong-format' else 8, self.data)
                    notice = Notify(31, 0, 1, display, req.requestor,
                                    req.selection, req.target,
                                    bad if self.mode == 'wrong-property' else prop, req.time)
                    destination = req.requestor
                    # XSendEvent reads a complete 32-byte wire event from an XEvent.
                    C.memmove(event, C.byref(notice), C.sizeof(notice))
                    send(display, destination, 0, 0, event)
                    if self.mode == 'replace':
                        setowner(display, clipboard, 0, 0)
                    flush(display)
                if kind == 28:
                    prop = C.cast(event, C.POINTER(Property)).contents
                    key = (prop.window, prop.atom)
                    if prop.state == 1 and key in transfers and self.mode != 'incr-stall':
                        target, offset = transfers[key]
                        size = 1 if self.mode == 'incr-trickle' else 8192
                        chunk = self.data[offset:offset + size]
                        put(*key, target, 8, chunk)
                        transfers[key][1] += len(chunk)
                        if not chunk:
                            del transfers[key]
                        flush(display)
        finally:
            close(display)

    def finish(self):
        self.stop.set(); self.join(3)
        assert not self.is_alive()
        if self.error:
            raise self.error


def snapshot_after_replace(owner):
    """Pause a real INCR reader, replace+release publication, finish old bytes."""
    x = C.CDLL('libX11.so.6')
    ptr, ul, i = C.c_void_p, C.c_ulong, C.c_int
    def bind(name, result, *args):
        f = getattr(x, name); f.restype = result; f.argtypes = args; return f
    display = bind('XOpenDisplay', ptr, C.c_char_p)(None)
    assert display
    close = bind('XCloseDisplay', i, ptr)
    flush = bind('XFlush', i, ptr)
    pending = bind('XPending', i, ptr)
    next_ = bind('XNextEvent', i, ptr, ptr)
    atom = bind('XInternAtom', ul, ptr, C.c_char_p, i)
    root = bind('XDefaultRootWindow', ul, ptr)(display)
    window = bind('XCreateSimpleWindow', ul, ptr, ul, i, i, C.c_uint, C.c_uint,
                  C.c_uint, ul, ul)(display, root, 0, 0, 1, 1, 0, 0, 0)
    bind('XSelectInput', i, ptr, ul, C.c_long)(display, window, 4194304)
    target, prop, incr, clipboard = [atom(display, name, 0) for name in
        (b'application/octet-stream', b'_TEST_SNAPSHOT', b'INCR', b'CLIPBOARD')]
    get = bind('XGetWindowProperty', i, ptr, ul, ul, C.c_long, C.c_long, i, ul,
               C.POINTER(ul), C.POINTER(i), C.POINTER(ul), C.POINTER(ul), C.POINTER(ptr))
    delete = bind('XDeleteProperty', i, ptr, ul, ul)
    free = bind('XFree', i, ptr)
    def wait(kind):
        deadline = time.monotonic() + 3
        while time.monotonic() < deadline:
            if not pending(display):
                time.sleep(.001); continue
            event = (C.c_long * 24)()
            next_(display, event)
            if C.cast(event, C.POINTER(i))[0] == kind:
                if kind != 28 or C.cast(event, C.POINTER(Property)).contents.state == 0:
                    return
        raise AssertionError('paused INCR event timeout')
    def value():
        kind, fmt, count, after, data = ul(), i(), ul(), ul(), ptr()
        assert get(display, window, prop, 0, 8192, 0, 0,
                   C.byref(kind), C.byref(fmt), C.byref(count), C.byref(after), C.byref(data)) == 0
        try:
            assert not after.value
            return kind.value, fmt.value, C.string_at(data, count.value) if fmt.value == 8 else b''
        finally:
            if data.value: free(data)
    try:
        bind('XConvertSelection', i, ptr, ul, ul, ul, ul, ul)(display, clipboard, target, prop, window, 0)
        flush(display)
        wait(31)
        assert value()[:2] == (incr, 32)
        assert command(owner, b'p')[0] == 0
        assert command(owner, b'x') == [1]
        result = bytearray()
        while True:
            delete(display, window, prop); flush(display)
            wait(28)
            kind, fmt, chunk = value()
            assert (kind, fmt) == (target, 8)
            result.extend(chunk)
            if not chunk: break
        assert result == BINARY
    finally:
        close(display)


def execute(probe):
    assert os.environ.get('DISPLAY') and os.environ.get('MIME_PRIVATE_TEST') == '1'
    def read(target='application/octet-stream', capacity=1048576, timeout=500):
        output = run([probe], env=dict(os.environ, MODE='read', TARGET=target,
                                      CAPACITY=str(capacity), TIMEOUT=str(timeout)))
        header, body = output.split(b'\n', 1)
        return list(map(int, header.split())), body
    assert read()[0][0] == 1  # no selection owner
    owner = subprocess.Popen([probe], stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                             stderr=subprocess.PIPE, bufsize=0, env=dict(os.environ, MODE='owner'))
    first = line(owner)
    assert first[0] == 0 and first[1]
    try:
        for target, expected in [('text/uri-list', URI), ('x-special/gnome-copied-files', COPIED),
                                 ('application/octet-stream', BINARY), ('application/x-empty', b'')]:
            header, body = read(target)
            assert header == [0, len(expected), first[1] & 0xffffffff] and body == expected, (target, header, len(body))
            # Actual independently installed desktop clipboard client.
            assert run(['xclip', '-selection', 'clipboard', '-target', target, '-out']) == expected
        offered = run(['xclip', '-selection', 'clipboard', '-target', 'TARGETS', '-out'])
        assert b'text/uri-list' in offered and b'x-special/gnome-copied-files' in offered
        assert read('application/x-absent')[0][0] == 1
        assert read(capacity=7)[0][0] == 3
        empty = read('application/x-empty', capacity=0)
        assert empty == ([0, 0, first[1] & 0xffffffff], b''), empty
        assert command(owner, b'i') == [2, first[1]]
        assert command(owner, b'b') == [3, 2, 2, 2, 2, 2, 2, 2, first[1]]
        owner.stdin.write(b's'); owner.stdin.flush()
        assert line(owner) == [0, len(URI), first[1]]
        assert owner.stdout.read(len(URI)) == URI
        assert owner.stdout.read(1) == b'\n'
        snapshot_after_replace(owner)
        newer = command(owner, b'p')
        assert newer[0] == 0 and newer[1] != first[1]
        assert command(owner, b'r') == [0]  # same app, stale generation
        assert command(owner, b'o') == [newer[1]]
        foreign = Peer('normal')
        try:
            assert command(owner, b'x') == [0]
            assert command(owner, b'o') == [foreign.window]
            foreign.data = b'new generation from SAME foreign owner'
            assert command(owner, b'x') == [0]
            incoming = read()
            assert incoming[1] == foreign.data, incoming
        finally:
            foreign.finish()
        latest = command(owner, b'p')
        assert latest[0] == 0
        reopened = command(owner, b'z')
        assert reopened[:2] == [0, 0] and reopened[2] != latest[1], reopened
        assert command(owner, b'x') == [1]
        assert read()[0][0] == 1
        assert command(owner, b'p')[0] == 0
    finally:
        owner.stdin.write(b'q'); owner.stdin.flush()
        assert owner.wait(5) == 0, owner.stderr.read(2000)
    assert read()[0][0] == 1  # close releases owned selection
    cases = [('normal', b'hello\0' + '世界'.encode(), 100, 0),
             ('normal', b'', 0, 0), ('normal', BINARY, 7, 3),
             ('absent', b'', 100, 1), ('timeout', b'', 100, 4),
             ('wrong-type', b'xxxx', 100, 2), ('wrong-format', b'xxxx', 100, 2),
             ('wrong-property', b'xxxx', 100, 2), ('bad-incr', b'', 100, 2),
             ('replace', b'xxxx', 100, 2), ('incr', BINARY, len(BINARY), 0),
             ('incr', BINARY, 0, 3), ('incr-lie', BINARY, 20000, 3),
             ('incr-stall', b'xxxx', 100, 4), ('incr-trickle', BINARY, len(BINARY), 4)]
    for mode, data, capacity, status in cases:
        peer = Peer(mode, data)
        try:
            start = time.monotonic()
            header, body = read(capacity=capacity, timeout=1500 if mode == 'incr' else 150)
            assert header[0] == status, (mode, header)
            assert time.monotonic() - start < 2, mode
            assert body == (data if status == 0 else b''), mode
            if status == 3:
                assert header[1] > capacity
        finally:
            peer.finish()
    print('native MIME: multi-target/Xlib/xclip/INCR/bounds/timeouts/ownership/close PASS', flush=True)


def main():
    if len(sys.argv) == 3 and sys.argv[1] == '--execute':
        execute(Path(sys.argv[2])); return
    from toolchain import ZIRAN, ZIRAN_ROOT
    WORK.mkdir(parents=True, exist_ok=True)
    env = {k: v for k, v in os.environ.items() if k not in SCRUB}
    for key, folder in [('HOME', 'home'), ('XDG_CONFIG_HOME', 'config'), ('XDG_DATA_HOME', 'data'),
                        ('XDG_CACHE_HOME', 'cache'), ('XDG_STATE_HOME', 'state'), ('XDG_RUNTIME_DIR', 'runtime')]:
        path = WORK / folder; path.mkdir(mode=0o700, exist_ok=True); path.chmod(0o700)
        env[key] = str(path)
    env.update(XDG_CONFIG_DIRS=env['XDG_CONFIG_HOME'], XDG_DATA_DIRS=env['XDG_DATA_HOME'],
               YUE_DESKTOP_RECOVERY='0', MIME_PRIVATE_TEST='1', ZIRAN_JOBS='1',
               CMAKE_BUILD_PARALLEL_LEVEL='1', MAKEFLAGS='-j1', LDLIBS='-lxcb', TMPDIR=str(WORK))
    source = ROOT / 'tests/mime_clipboard_probe.zi'
    ir = WORK / 'ir'
    common = ['--module-path', ROOT / 'src/backend', '--module-path', ZIRAN_ROOT / 'std']
    run(['build-queue', '--', ZIRAN, 'ir', '--root', ROOT / 'tests', *common, '-o', ir, source], env=env)
    for form in ('source', 'saved'):
        root, input_ = (ROOT / 'tests', source) if form == 'source' else (ir, ir / 'mime_clipboard_probe.zir')
        output = WORK / 'probe'  # one stable executable, rebuild for each form
        run(['build-queue', '--', ZIRAN, 'build', '--target=c', '--exe', '--root', root,
             '--entry', 'mime_clipboard_probe:main', *([] if form == 'saved' else common),
             '-o', output, input_], env=env)
        probe = output / 'mime_clipboard_probe'
        assert subprocess.run([probe], env=env, timeout=5).returncode == 20
        run(['xvfb-run', '-a', '-n', '430', '-s', '-screen 0 640x480x24 -nolisten tcp',
             'dbus-run-session', '--', sys.executable, __file__, '--execute', probe], env=env)
        print(f'MIME clipboard {form}: PASS', flush=True)


if __name__ == '__main__':
    main()
