#!/usr/bin/env python3
import http.server
import socketserver
import sys
import os

PORT = int(sys.argv[1]) if len(sys.argv) > 1 else 3000
ROOT = sys.argv[2] if len(sys.argv) > 2 else "./build"

os.chdir(ROOT)

class MyHTTPRequestHandler(http.server.SimpleHTTPRequestHandler):
    def log_message(self, format, *args):
        pass  # Suppress log spam

print(f"\n╔══════════════════════════════════════════════════════════════╗")
print(f"║  Kryon Dev Server                                            ║")
print(f"╠══════════════════════════════════════════════════════════════╣")
print(f"║  Local:   http://127.0.0.1:{PORT}                            ║")
print(f"║  Root:    {ROOT.ljust(50)}║")
print(f"╠══════════════════════════════════════════════════════════════╣")
print(f"║  Press Ctrl+C to stop                                        ║")
print(f"╚══════════════════════════════════════════════════════════════╝\n")

with socketserver.TCPServer(("", PORT), MyHTTPRequestHandler) as httpd:
    httpd.serve_forever()
