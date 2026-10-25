#!/usr/bin/env python3
"""A local HTTP server for the http_transfer tests; prints its port."""
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
import sys
import time


class Handler(BaseHTTPRequestHandler):
    def log_message(self, *arguments):
        pass

    def answer(self, status, body):
        self.send_response(status)
        self.send_header('Content-Type', 'text/plain')
        self.send_header('Content-Length', str(len(body)))
        self.send_header('Access-Control-Allow-Origin', '*')
        self.send_header('Access-Control-Allow-Headers', 'Authorization, X-Test, Content-Type')
        self.end_headers()
        self.wfile.write(body)

    def do_OPTIONS(self):
        self.answer(204, b'')

    def do_GET(self):
        if self.path == '/hello':
            self.answer(200, b'hello world')
        elif self.path == '/binary':
            self.answer(200, b'\x00\x01\xff\x00z')
        elif self.path == '/big':
            self.answer(200, b'x' * 100000)
        elif self.path == '/slow':
            time.sleep(3)
            self.answer(200, b'late answer')
        elif self.path == '/redirect-stream':
            self.send_response(302)
            self.send_header('Location', '/stream')
            self.send_header('Content-Length', '3')
            self.end_headers()
            self.wfile.write(b'old')
        elif self.path == '/stream':
            self.send_response(200)
            self.send_header('Content-Length', '100000')
            self.end_headers()
            try:
                for _ in range(100):
                    self.wfile.write(b'x' * 1000)
                    self.wfile.flush()
                    time.sleep(.01)
            except (BrokenPipeError, ConnectionResetError):
                pass
        else:
            self.answer(404, b'missing')

    def do_POST(self):
        length = int(self.headers.get('Content-Length') or 0)
        body = self.rfile.read(length)
        reply = '%s|%s|%s|%s' % (self.headers.get('Authorization'), self.headers.get('X-Test'),
                                 self.headers.get('Content-Type'), body.decode())
        self.answer(201, reply.encode())


server = ThreadingHTTPServer(('127.0.0.1', 0), Handler)
print(server.server_address[1], flush=True)
sys.stdout.close()
server.serve_forever()
