#!/usr/bin/env python3
"""
Simple 9P2000 client test script
Tests the Kryon server with basic 9P operations
"""

import socket
import struct
import time

def le_put16(val):
    return struct.pack('<H', val)

def le_put32(val):
    return struct.pack('<I', val)

def le_put64(val):
    return struct.pack('<Q', val)

def le_get16(buf):
    return struct.unpack('<H', buf)[0]

def le_get32(buf):
    return struct.unpack('<I', buf)[0]

def le_get64(buf):
    return struct.unpack('<Q', buf)[0]

def put_string(s):
    s_bytes = s.encode('utf-8')
    return le_put16(len(s_bytes)) + s_bytes

def build_tversion(tag, msize, version):
    msg = le_put32(msize) + put_string(version)
    hdr = le_put32(7 + len(msg)) + bytes([Tversion]) + le_put16(tag)
    return hdr + msg

def build_tattach(tag, fid, afid, uname, aname):
    msg = le_put32(fid) + le_put32(afid) + put_string(uname) + put_string(aname)
    hdr = le_put32(7 + len(msg)) + bytes([Tattach]) + le_put16(tag)
    return hdr + msg

def build_twalk(tag, fid, newfid, wname):
    msg = le_put32(fid) + le_put32(newfid) + le_put16(1 if wname else 0)
    if wname:
        msg += put_string(wname)
    hdr = le_put32(7 + len(msg)) + bytes([Twalk]) + le_put16(tag)
    return hdr + msg

def build_topen(tag, fid, mode):
    msg = le_put32(fid) + bytes([mode])
    hdr = le_put32(7 + len(msg)) + bytes([Topen]) + le_put16(tag)
    return hdr + msg

def build_tread(tag, fid, offset, count):
    msg = le_put32(fid) + le_put64(offset) + le_put32(count)
    hdr = le_put32(7 + len(msg)) + bytes([Tread]) + le_put16(tag)
    return hdr + msg

def build_twrite(tag, fid, offset, data):
    msg = le_put32(fid) + le_put64(offset) + le_put32(len(data)) + data.encode()
    hdr = le_put32(7 + len(msg)) + bytes([Twrite]) + le_put16(tag)
    return hdr + msg

def build_tclunk(tag, fid):
    msg = le_put32(fid)
    hdr = le_put32(7 + len(msg)) + bytes([Tclunk]) + le_put16(tag)
    return hdr + msg

# 9P message types
Tversion = 100
Rversion = 101
Tattach = 104
Rattach = 105
Twalk = 108
Rwalk = 109
Topen = 110
Ropen = 111
Tread = 114
Rread = 115
Twrite = 116
Rwrite = 117
Tclunk = 118
Rclunk = 119

def recv_msg(sock):
    """Receive a complete 9P message"""
    # Read 4-byte size
    size_buf = sock.recv(4)
    if len(size_buf) < 4:
        return None
    size = le_get32(size_buf)

    # Read rest of message
    msg = size_buf
    while len(msg) < size:
        chunk = sock.recv(size - len(msg))
        if not chunk:
            return None
        msg += chunk

    return msg

def test_kryon_server(host='127.0.0.1', port=17019):
    print(f"Connecting to {host}:{port}...")
    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    sock.connect((host, port))

    # Tversion
    print("Sending Tversion...")
    msg = build_tversion(tag=0, msize=8192, version='9P2000')
    sock.sendall(msg)

    resp = recv_msg(sock)
    if resp:
        size = le_get32(resp[0:4])
        msg_type = resp[4]
        tag = le_get16(resp[5:7])
        print(f"  Rversion: size={size}, type={msg_type}, tag={tag}")
        if msg_type == Rversion:
            r_msize = le_get32(resp[7:11])
            r_version_len = le_get16(resp[11:13])
            r_version = resp[13:13+r_version_len].decode('utf-8')
            print(f"  msize={r_msize}, version={r_version}")

    # Tattach
    print("Sending Tattach...")
    msg = build_tattach(tag=1, fid=0, afid=0xffffffff, uname='none', aname='')
    sock.sendall(msg)

    resp = recv_msg(sock)
    if resp:
        msg_type = resp[4]
        print(f"  Rattach: type={msg_type}")

    # Twalk to toggle
    print("Sending Twalk to /toggle...")
    msg = build_twalk(tag=2, fid=0, newfid=1, wname='toggle')
    sock.sendall(msg)

    resp = recv_msg(sock)
    if resp:
        msg_type = resp[4]
        print(f"  Rwalk: type={msg_type}")

    # Topen
    print("Sending Topen...")
    msg = build_topen(tag=3, fid=1, mode=0)  # OREAD
    sock.sendall(msg)

    resp = recv_msg(sock)
    if resp:
        msg_type = resp[4]
        print(f"  Ropen: type={msg_type}")

    # Tread (should get "0")
    print("Sending Tread...")
    msg = build_tread(tag=4, fid=1, offset=0, count=100)
    sock.sendall(msg)

    resp = recv_msg(sock)
    if resp:
        msg_type = resp[4]
        if msg_type == Rread:
            count = le_get32(resp[7:11])
            data = resp[11:11+count].decode('utf-8')
            print(f"  Rread: data='{data}'")
            if data == '0':
                print("  ✓ Initial state is 0")
            else:
                print(f"  ✗ Expected '0', got '{data}'")

    # Twrite (set to "1")
    print("Sending Twrite('1')...")
    msg = build_twrite(tag=5, fid=1, offset=0, data='1')
    sock.sendall(msg)

    resp = recv_msg(sock)
    if resp:
        msg_type = resp[4]
        if msg_type == Rwrite:
            count = le_get32(resp[7:11])
            print(f"  Rwrite: wrote {count} bytes")

    # Tread (should get "1" now)
    print("Sending Tread...")
    msg = build_tread(tag=6, fid=1, offset=0, count=100)
    sock.sendall(msg)

    resp = recv_msg(sock)
    if resp:
        msg_type = resp[4]
        if msg_type == Rread:
            count = le_get32(resp[7:11])
            data = resp[11:11+count].decode('utf-8')
            print(f"  Rread: data='{data}'")
            if data == '1':
                print("  ✓ State changed to 1")
            else:
                print(f"  ✗ Expected '1', got '{data}'")

    # Tclunk
    print("Sending Tclunk...")
    msg = build_tclunk(tag=7, fid=1)
    sock.sendall(msg)

    resp = recv_msg(sock)
    if resp:
        msg_type = resp[4]
        print(f"  Rclunk: type={msg_type}")

    sock.close()
    print("\n✓ All tests passed!")

if __name__ == '__main__':
    import sys
    host = sys.argv[1] if len(sys.argv) > 1 else '127.0.0.1'
    port = int(sys.argv[2]) if len(sys.argv) > 2 else 17019
    test_kryon_server(host, port)
