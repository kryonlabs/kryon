#!/bin/sh
# termi backend test - builds and runs a Kryon widget app in a pseudo-terminal.
set -eu

root=$(cd "$(dirname "$0")/.." && pwd)
build=${BUILD_DIR:-build/linux-x86_64-termi}
bin="$root/$build/tests/termi_smoke_test"
work=${TMPDIR:-/tmp}/kryon-termi-test.$$
out="$work/typescript"

cleanup() { rm -rf "$work"; }
trap cleanup EXIT INT TERM

if ! command -v python3 >/dev/null 2>&1; then
    echo "termi test: python3 not found - skipping runtime" >&2
    exit 0
fi

mkdir -p "$work"
make -C "$root" BUILD_DIR="$build" KRYON_BACKEND=termi "$build/tests/termi_smoke_test"

python3 - "$bin" "$out" <<'PY'
import errno
import os
import pty
import select
import signal
import subprocess
import sys
import time

bin_path, out_path = sys.argv[1], sys.argv[2]
master, slave = pty.openpty()
env = os.environ.copy()
env["TERMI_COLS"] = "80"
env["TERMI_ROWS"] = "24"
env["TERMI_SIXEL"] = "1"
proc = subprocess.Popen([bin_path], stdin=slave, stdout=slave, stderr=slave,
                        close_fds=True, env=env)
os.close(slave)
data = bytearray()
sent_click_down = False
sent_click_up = False
sent_ctrl_c = False
start = time.time()
deadline = time.time() + 5.0

try:
    while time.time() < deadline:
        elapsed = time.time() - start
        if not sent_click_down and elapsed > 0.30:
            os.write(master, b"\x1b[<0;6;7M")
            sent_click_down = True
        if not sent_click_up and elapsed > 0.55:
            os.write(master, b"\x1b[<0;6;7m")
            sent_click_up = True
        if not sent_ctrl_c and elapsed > 1.25:
            os.write(master, b"\x03")
            sent_ctrl_c = True
        ready, _, _ = select.select([master], [], [], 0.05)
        if master in ready:
            try:
                chunk = os.read(master, 4096)
            except OSError as exc:
                if exc.errno != errno.EIO:
                    raise
                break
            if not chunk:
                break
            data.extend(chunk)
        if proc.poll() is not None:
            while True:
                try:
                    chunk = os.read(master, 4096)
                except OSError:
                    break
                if not chunk:
                    break
                data.extend(chunk)
            break
finally:
    os.close(master)
    if proc.poll() is None:
        proc.terminate()
        try:
            proc.wait(timeout=1.0)
        except subprocess.TimeoutExpired:
            proc.kill()
            proc.wait()

with open(out_path, "wb") as f:
    f.write(data)
sys.exit(proc.returncode if proc.returncode is not None else 1)
PY

test -s "$out"
grep "$(printf '\033')\\[?1049h" "$out" >/dev/null
grep "$(printf '\033')Pq" "$out" >/dev/null
grep "Termi backend" "$out" >/dev/null
grep "Button" "$out" >/dev/null
grep "Clicked" "$out" >/dev/null
grep "$(printf '\033')\\[?1049l" "$out" >/dev/null

echo "termi backend smoke ok"
