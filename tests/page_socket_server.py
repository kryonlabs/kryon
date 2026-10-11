#!/usr/bin/env python3
"""A minimal WebSocket server for the page_socket test; prints its port.

/events answers with the first offered subprotocol and sends two text
messages, /big one longer than the page's capacity, /binary a binary one.
The offered subprotocols are recorded in the second message of /events.
"""
import base64
import hashlib
import socketserver
import struct
import sys
import time

GUID = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11"


def frame(opcode, payload):
    header = bytes([0x80 | opcode])
    if len(payload) < 126:
        header += bytes([len(payload)])
    else:
        header += bytes([126]) + struct.pack(">H", len(payload))
    return header + payload


class Handler(socketserver.BaseRequestHandler):
    def handle(self):
        request = b""
        while b"\r\n\r\n" not in request:
            chunk = self.request.recv(4096)
            if not chunk:
                return
            request += chunk
        lines = request.decode("latin-1").split("\r\n")
        path = lines[0].split(" ")[1]
        headers = {}
        for line in lines[1:]:
            if ": " in line:
                name, value = line.split(": ", 1)
                headers[name.lower()] = value
        accept = base64.b64encode(hashlib.sha1((headers["sec-websocket-key"] + GUID).encode()).digest()).decode()
        offered = [value.strip() for value in headers.get("sec-websocket-protocol", "").split(",") if value.strip()]
        reply = ("HTTP/1.1 101 Switching Protocols\r\nUpgrade: websocket\r\nConnection: Upgrade\r\n"
                 "Sec-WebSocket-Accept: %s\r\n" % accept)
        if offered:
            reply += "Sec-WebSocket-Protocol: %s\r\n" % offered[0]
        self.request.sendall((reply + "\r\n").encode())
        if path == "/events":
            self.request.sendall(frame(1, b"hello"))
            self.request.sendall(frame(1, ",".join(offered).encode()))
        elif path == "/big":
            self.request.sendall(frame(1, b"x" * 100))
        elif path == "/binary":
            self.request.sendall(frame(2, b"\x00\x01"))
        time.sleep(5)


class Server(socketserver.ThreadingTCPServer):
    daemon_threads = True
    allow_reuse_address = True


server = Server(("127.0.0.1", 0), Handler)
print(server.server_address[1], flush=True)
sys.stdout.close()
server.serve_forever()
