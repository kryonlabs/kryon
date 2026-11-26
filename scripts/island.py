#!/usr/bin/python3
"""Kryon WebKit island: approved staged pages, owned X child, bounded JSON packets."""
import argparse
import ctypes as c
from functools import partial
from http.server import SimpleHTTPRequestHandler, ThreadingHTTPServer
import json
import os
from pathlib import Path
import re
import signal
import socket
import threading
from urllib.parse import urlsplit

MAX_PACKET = 4096
COMMANDS = {'spin', 'state', 'focus', 'seek', 'play', 'pause', 'select', 'next', 'previous', 'stop', 'volume'}


def validate_command(value):
    if not isinstance(value, dict) or type(value.get('v')) is not int or value['v'] != 1 or value.get('type') not in COMMANDS:
        raise ValueError('Unsupported island protocol version or command')
    if len(json.dumps(value).encode()) > MAX_PACKET:
        raise ValueError('Island command is too large')
    return value


class Assets(SimpleHTTPRequestHandler):
    def log_message(self, *args):
        pass

    def do_GET(self):
        if any(part.startswith('.') for part in Path(urlsplit(self.path).path).parts):
            self.send_error(404)
            return
        super().do_GET()

    def list_directory(self, path):
        self.send_error(404)
        return None

    def send_head(self):
        self.range_remaining = None
        requested = self.headers.get('Range', '')
        match = re.fullmatch(r'bytes=(\d*)-(\d*)', requested)
        path = Path(self.translate_path(self.path))
        if not match or not any(match.groups()) or not path.is_file():
            return super().send_head()
        try:
            stream = path.open('rb')
        except OSError:
            self.send_error(404)
            return None
        size = os.fstat(stream.fileno()).st_size
        first, last = match.groups()
        if first:
            start = int(first)
            end = min(int(last), size - 1) if last else size - 1
        else:
            suffix = int(last)
            start, end = max(0, size - suffix), size - 1
        if size == 0 or start >= size or end < start:
            stream.close()
            self.send_response(416)
            self.send_header('Content-Range', f'bytes */{size}')
            self.send_header('Content-Length', '0')
            self.end_headers()
            return None
        stream.seek(start)
        self.range_remaining = end - start + 1
        self.send_response(206)
        self.send_header('Content-Type', self.guess_type(str(path)))
        self.send_header('Content-Range', f'bytes {start}-{end}/{size}')
        self.send_header('Content-Length', str(self.range_remaining))
        self.end_headers()
        return stream

    def copyfile(self, source, output):
        if self.range_remaining is None:
            return super().copyfile(source, output)
        remaining = self.range_remaining
        while remaining:
            data = source.read(min(65536, remaining))
            if not data:
                break
            output.write(data)
            remaining -= len(data)

    def end_headers(self):
        self.send_header('Accept-Ranges', 'bytes')
        self.send_header('Cache-Control', 'no-store')
        self.send_header('X-Content-Type-Options', 'nosniff')
        self.send_header('Content-Security-Policy',
            "default-src 'self'; script-src 'self'; style-src 'self' 'unsafe-inline'; "
            "img-src 'self' data: blob:; media-src 'self' blob:; connect-src 'self'; "
            "object-src 'none'; frame-src 'none'; base-uri 'none'")
        super().end_headers()


class Island:
    def __init__(self, options):
        import gi
        gi.require_version('Gtk', '3.0')
        gi.require_version('WebKit2', '4.1')
        gi.require_version('GdkX11', '3.0')
        from gi.repository import Gtk, GLib, WebKit2, GdkX11
        self.Gtk, self.GLib = Gtk, GLib
        self.socket = socket.socket(fileno=options.fd)
        self.socket.setblocking(False)
        self.ready = False
        self.pending = []
        self.visible = True
        self.focused = None
        self.closed = False
        self.root = (options.assets or Path(__file__).parent / 'webview-assets').resolve()
        if not self.root.is_dir():
            raise ValueError('Build the staged island assets first')
        self.server = ThreadingHTTPServer(('127.0.0.1', 0), partial(Assets, directory=str(self.root)))
        self.server.daemon_threads = True
        threading.Thread(target=self.server.serve_forever, daemon=True).start()
        self.origin = f'http://127.0.0.1:{self.server.server_port}'
        self.parent = options.parent
        self.window = Gtk.Window(title='Kryon webview island')
        self.window.set_default_size(640, 400)
        self.window.set_decorated(False)
        self.window.connect('destroy', lambda *_: self.close())
        manager = WebKit2.UserContentManager()
        manager.connect('script-message-received::kryon', self.message)
        if not manager.register_script_message_handler('kryon'):
            raise RuntimeError('Could not register the island bridge')
        context = WebKit2.WebContext.new_ephemeral()
        self.view = WebKit2.WebView(web_context=context, user_content_manager=manager)
        self.view.get_settings().set_enable_webgl(True)
        self.view.get_settings().set_media_playback_requires_user_gesture(True)
        self.view.connect('decide-policy', self.policy)
        self.view.connect('permission-request', self.permission)
        self.view.connect('load-failed', lambda *_: self.send({'v': 1, 'type': 'error', 'message': 'Island page failed to load'}) or False)
        self.view.connect('web-process-terminated', lambda *_: self.send({'v': 1, 'type': 'error', 'message': 'Island web process terminated'}))
        self.window.add(self.view)
        self.window.realize()
        self.xid = GdkX11.X11Window.get_xid(self.window.get_window())
        self.x = c.CDLL('libX11.so.6')
        self.x.XOpenDisplay.argtypes = [c.c_char_p]
        self.x.XOpenDisplay.restype = c.c_void_p
        self.display = self.x.XOpenDisplay(None)
        if not self.display:
            raise RuntimeError('An X11 display is required for the desktop island')
        self.x.XReparentWindow.argtypes = [c.c_void_p, c.c_ulong, c.c_ulong, c.c_int, c.c_int]
        self.x.XMoveResizeWindow.argtypes = [c.c_void_p, c.c_ulong, c.c_int, c.c_int, c.c_uint, c.c_uint]
        self.x.XFlush.argtypes = [c.c_void_p]
        self.x.XGetInputFocus.argtypes = [c.c_void_p, c.POINTER(c.c_ulong), c.POINTER(c.c_int)]
        self.x.XQueryTree.argtypes = [c.c_void_p, c.c_ulong, c.POINTER(c.c_ulong), c.POINTER(c.c_ulong), c.POINTER(c.POINTER(c.c_ulong)), c.POINTER(c.c_uint)]
        self.x.XFree.argtypes = [c.c_void_p]
        self.x.XCloseDisplay.argtypes = [c.c_void_p]
        # Reparent while still hidden: this child never maps on the live root.
        self.window.get_window().set_override_redirect(True)
        self.x.XReparentWindow(self.display, self.xid, self.parent, 0, 0)
        self.x.XFlush(self.display)
        self.window.show_all()
        if re.fullmatch(r'[a-z0-9-]+', options.content or ''):
            uri = self.origin + '/' + options.content + '.html'
        else:
            raise ValueError('Only staged island content is supported')
        self.view.load_uri(uri)
        GLib.io_add_watch(self.socket.fileno(), GLib.IO_IN | GLib.IO_HUP | GLib.IO_ERR, self.input)
        GLib.timeout_add(100, self.focus)
        GLib.unix_signal_add(GLib.PRIORITY_DEFAULT, signal.SIGTERM, self.close)

    def policy(self, view, decision, kind):
        from gi.repository import WebKit2
        if kind == WebKit2.PolicyDecisionType.NAVIGATION_ACTION:
            if not decision.get_navigation_action().get_request().get_uri().startswith(self.origin + '/'):
                decision.ignore()
                return True
        if kind == WebKit2.PolicyDecisionType.NEW_WINDOW_ACTION:
            decision.ignore()
            return True
        return False

    def permission(self, view, request):
        request.deny()
        return True

    def send(self, value):
        packet = json.dumps(value, separators=(',', ':'), ensure_ascii=True).encode()
        if len(packet) > MAX_PACKET:
            return False
        try:
            return self.socket.send(packet) == len(packet)
        except (BlockingIOError, BrokenPipeError, OSError):
            return False

    def message(self, manager, result):
        try:
            text = result.get_js_value().to_string()
            if len(text.encode()) > MAX_PACKET:
                raise ValueError('Island event is too large')
            value = json.loads(text)
            if not isinstance(value, dict) or value.get('v') != 1 or value.get('type') not in {'ready', 'ack', 'state', 'click', 'error', 'frame'}:
                raise ValueError('Invalid island event')
            if value['type'] == 'ready':
                value['native'] = {'pid': os.getpid(), 'xid': self.xid, 'parent': self.parent}
            self.send(value)
            if value['type'] == 'ready':
                self.ready = True
                waiting, self.pending = self.pending, []
                for command in waiting:
                    self.evaluate(command)
                self.focused = None
                self.focus()
        except (ValueError, AttributeError):
            self.send({'v': 1, 'type': 'error', 'message': 'Invalid island event'})

    def evaluate(self, command):
        if not self.ready:
            if len(self.pending) >= 32:
                raise ValueError('Island is not ready; queue is full')
            self.pending.append(command)
            return
        # JSON encoding keeps data out of executable script source.
        self.view.evaluate_javascript('window.KryonIsland.receive(' + json.dumps(command, ensure_ascii=True) + ')', -1, None, None, None, None, None)

    def input(self, fd, condition):
        try:
            packet = self.socket.recv(MAX_PACKET + 1)
            if not packet:
                self.close()
                return False
            if len(packet) > MAX_PACKET:
                raise ValueError('Island command is too large')
            value = json.loads(packet)
            if isinstance(value, dict) and value.get('v') == 1 and value.get('type') == 'host-bounds':
                x, y, width, height = (value[k] for k in ('x', 'y', 'width', 'height'))
                if any(type(n) is not int or abs(n) > 16384 for n in (x, y, width, height)) or width < 1 or height < 1 or type(value.get('visible')) is not bool:
                    raise ValueError('Invalid island bounds')
                self.visible = value['visible']
                self.x.XMoveResizeWindow(self.display, self.xid, x if self.visible else -width - 1, y if self.visible else -height - 1, width, height)
                self.x.XFlush(self.display)
                self.focused = None
                self.focus()
            else:
                self.evaluate(validate_command(value))
        except BlockingIOError:
            return True
        except (ValueError, KeyError, TypeError, json.JSONDecodeError):
            self.send({'v': 1, 'type': 'error', 'message': 'Invalid island command'})
        except OSError:
            self.close()
            return False
        return True

    def focus(self):
        if self.closed:
            return False
        focused, revert = c.c_ulong(), c.c_int()
        self.x.XGetInputFocus(self.display, c.byref(focused), c.byref(revert))
        window = focused.value
        active = False
        for _ in range(32):
            if window == self.parent or window == self.xid:
                active = self.visible
                break
            if window <= 1:
                break
            root, parent, count = c.c_ulong(), c.c_ulong(), c.c_uint()
            children = c.POINTER(c.c_ulong)()
            if not self.x.XQueryTree(self.display, window, c.byref(root), c.byref(parent), c.byref(children), c.byref(count)):
                break
            if children:
                self.x.XFree(children)
            if parent.value == window:
                break
            window = parent.value
        if active != self.focused:
            self.focused = active
            self.evaluate({'v': 1, 'type': 'focus', 'active': active})
        return True

    def close(self):
        if self.closed:
            return False
        self.closed = True
        self.server.shutdown()
        self.server.server_close()
        self.socket.close()
        self.x.XCloseDisplay(self.display)
        self.Gtk.main_quit()
        return False


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--fd', type=int, required=True)
    parser.add_argument('--parent', type=int, required=True)
    parser.add_argument('--content', default='scene')
    parser.add_argument('--assets', type=Path)
    parser.add_argument('--embedded', action='store_true')
    options = parser.parse_args()
    if options.parent <= 1:
        parser.error('An owned native parent window is required')
    island = Island(options)
    island.Gtk.main()


if __name__ == '__main__':
    main()
