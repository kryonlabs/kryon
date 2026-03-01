#!/usr/bin/env bash
# Kryon CLI — unified launcher for Marrow + WM + viewer
# Usage: kryon <command> [options]
set -euo pipefail

# ---------------------------------------------------------------------------
# Locate binaries
# ---------------------------------------------------------------------------
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# Script lives either at project root (kryon.sh) or in bin/ (installed kryon)
if [[ "$(basename "$SCRIPT_DIR")" == "bin" ]]; then
    KRYON_ROOT="$(dirname "$SCRIPT_DIR")"
else
    KRYON_ROOT="$SCRIPT_DIR"
fi

KRYON_WM="$KRYON_ROOT/bin/kryon-wm"
KRYON_VIEW="$KRYON_ROOT/bin/kryon-view"
MARROW_BIN="$KRYON_ROOT/../marrow/bin/marrow"
MARROW_PORT=17010

# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------

# Poll TCP port until open (100ms intervals)
# Usage: wait_for_port <port> <max_attempts>
wait_for_port() {
    local port="$1" attempts="$2" i
    for i in $(seq 1 "$attempts"); do
        if (echo >/dev/tcp/127.0.0.1/"$port") 2>/dev/null; then
            return 0
        fi
        sleep 0.1
    done
    return 1
}

marrow_running()    { pgrep -x marrow     >/dev/null 2>&1; }
wm_running()        { pgrep -x kryon-wm   >/dev/null 2>&1; }
view_running()      { pgrep -x kryon-view >/dev/null 2>&1; }

die() { echo "[kryon] Error: $*" >&2; exit 1; }

# ---------------------------------------------------------------------------
# kryon run FILE.kry [--watch] [--stay-open]
# ---------------------------------------------------------------------------
cmd_run() {
    local kry_file="" watch=0 stay_open=0

    while [[ $# -gt 0 ]]; do
        case "$1" in
            --watch)     watch=1;     shift ;;
            --stay-open) stay_open=1; shift ;;
            --help|-h)   cmd_help;    return 0 ;;
            -*)          die "Unknown option: $1" ;;
            *)           kry_file="$1"; shift ;;
        esac
    done

    [[ -z "$kry_file" ]] && die "no .kry file specified\nUsage: kryon run FILE.kry [--watch]"

    # Resolve relative to cwd (not bin/)
    if [[ ! -f "$kry_file" ]]; then
        # Try relative to kryon root as a fallback
        if [[ -f "$KRYON_ROOT/$kry_file" ]]; then
            kry_file="$KRYON_ROOT/$kry_file"
        else
            die "file not found: $kry_file"
        fi
    fi
    kry_file="$(cd "$(dirname "$kry_file")" && pwd)/$(basename "$kry_file")"

    # Check required binaries exist
    [[ -x "$KRYON_WM" ]]   || die "kryon-wm not found at $KRYON_WM (run: make)"
    [[ -x "$KRYON_VIEW" ]] || die "kryon-view not found at $KRYON_VIEW (run: make)"
    [[ -x "$MARROW_BIN" ]] || die "marrow not found at $MARROW_BIN"

    local wm_pid="" view_pid=""

    # ── Step 1: Marrow ──────────────────────────────────────────────────────
    if marrow_running; then
        echo "[kryon] Marrow already running"
    else
        echo "[kryon] Starting Marrow..."
        "$MARROW_BIN" >/tmp/marrow.log 2>&1 &
        if ! wait_for_port "$MARROW_PORT" 50; then
            die "Marrow failed to start after 5s (check /tmp/marrow.log)"
        fi
        echo "[kryon] Marrow ready on port $MARROW_PORT"
    fi

    # ── Step 2: WM ──────────────────────────────────────────────────────────
    if wm_running; then
        echo "[kryon] Stopping existing kryon-wm..."
        pkill -TERM kryon-wm 2>/dev/null || true
        sleep 0.3
    fi

    local wm_extra=""
    [[ $watch -eq 1 ]] && wm_extra="--watch"

    # Remove stale screen-size file so we don't start the viewer with old dims
    rm -f /tmp/.kryon-screensize

    echo "[kryon] Starting kryon-wm: $kry_file"
    "$KRYON_WM" --run "$kry_file" $wm_extra >/tmp/kryon-wm.log 2>&1 &
    wm_pid=$!

    # ── Step 3: Wait for WM ready, then launch viewer ───────────────────────
    # The WM writes /tmp/.kryon-screensize once it has rendered the first frame
    # and mounted /mnt/wm. We wait for that file before starting the viewer so
    # the viewer gets the correct dimensions immediately (no 9P namespace race).
    echo "[kryon] Waiting for WM to be ready..."
    local _attempts=0
    while [[ ! -f /tmp/.kryon-screensize ]] && [[ $_attempts -lt 50 ]]; do
        # Bail early if the WM already died
        if ! kill -0 "$wm_pid" 2>/dev/null; then
            echo "[kryon] Error: kryon-wm exited before becoming ready"
            echo "[kryon] Check /tmp/kryon-wm.log for details"
            exit 1
        fi
        sleep 0.1
        _attempts=$((_attempts + 1))
    done

    if [[ ! -f /tmp/.kryon-screensize ]]; then
        echo "[kryon] Error: WM did not become ready after 5s (check /tmp/kryon-wm.log)"
        kill "$wm_pid" 2>/dev/null || true
        exit 1
    fi

    if view_running; then
        echo "[kryon] kryon-view already running"
    else
        local view_extra=""
        [[ $stay_open -eq 1 ]] && view_extra="--stay-open"

        echo "[kryon] Starting kryon-view..."
        "$KRYON_VIEW" $view_extra >/tmp/kryon-view.log 2>&1 &
        view_pid=$!
    fi

    # ── Step 4: block ───────────────────────────────────────────────────────
    echo ""
    echo "[kryon] Running. Press Ctrl-C to stop."
    [[ $watch -eq 1 ]] && echo "[kryon] Watch mode active — edit $kry_file to hot-reload."
    echo "[kryon] Logs: /tmp/kryon-wm.log  /tmp/marrow.log"
    echo ""

    trap '
        trap "" INT TERM
        echo ""
        echo "[kryon] Stopping..."
        [[ -n "$wm_pid" ]]   && kill "$wm_pid"   2>/dev/null || true
        [[ -n "$view_pid" ]] && kill "$view_pid"  2>/dev/null || true
        pkill -TERM kryon-wm   2>/dev/null || true
        pkill -TERM kryon-view 2>/dev/null || true
        wait 2>/dev/null || true
        echo "[kryon] Stopped. Marrow still running (kryon stop --all to kill it)."
        exit 0
    ' INT TERM

    # Wait for WM; when it exits, kill viewer too
    wait "$wm_pid" 2>/dev/null || true
    [[ -n "$view_pid" ]] && kill "$view_pid" 2>/dev/null || true
    pkill -TERM kryon-view 2>/dev/null || true

    echo "[kryon] Done."
}

# ---------------------------------------------------------------------------
# kryon stop [--all]
# ---------------------------------------------------------------------------
cmd_stop() {
    local kill_marrow=0
    [[ "${1:-}" == "--all" ]] && kill_marrow=1

    if wm_running; then
        pkill -TERM kryon-wm 2>/dev/null || true
        echo "[kryon] kryon-wm stopped"
    else
        echo "[kryon] kryon-wm not running"
    fi

    if view_running; then
        pkill -TERM kryon-view 2>/dev/null || true
        echo "[kryon] kryon-view stopped"
    else
        echo "[kryon] kryon-view not running"
    fi

    if [[ $kill_marrow -eq 1 ]]; then
        if marrow_running; then
            pkill -TERM marrow 2>/dev/null || true
            echo "[kryon] Marrow stopped"
        else
            echo "[kryon] Marrow not running"
        fi
    else
        echo "[kryon] Marrow left running  (use: kryon stop --all)"
    fi
}

# ---------------------------------------------------------------------------
# kryon status
# ---------------------------------------------------------------------------
cmd_status() {
    local pid
    echo "=== Kryon Status ==="

    if marrow_running; then
        pid=$(pgrep -x marrow | head -1)
        echo "  Marrow:     running  (PID $pid, port $MARROW_PORT)"
    else
        echo "  Marrow:     stopped"
    fi

    if wm_running; then
        pid=$(pgrep -x kryon-wm | head -1)
        echo "  kryon-wm:   running  (PID $pid)"
    else
        echo "  kryon-wm:   stopped"
    fi

    if view_running; then
        pid=$(pgrep -x kryon-view | head -1)
        echo "  kryon-view: running  (PID $pid)"
    else
        echo "  kryon-view: stopped"
    fi
}

# ---------------------------------------------------------------------------
# kryon list
# ---------------------------------------------------------------------------
cmd_list() {
    [[ -x "$KRYON_WM" ]] || die "kryon-wm not found (run: make)"
    "$KRYON_WM" --list-examples
}

# ---------------------------------------------------------------------------
# kryon help
# ---------------------------------------------------------------------------
cmd_help() {
    cat <<EOF
Usage: kryon <command> [options]

Commands:
  run FILE.kry [--watch] [--stay-open]
      Run a .kry file. Starts Marrow and kryon-view automatically if needed.
      --watch       Hot-reload when the file changes (Ctrl-C to stop)
      --stay-open   Keep the SDL window open after the WM exits

  stop [--all]
      Stop kryon-wm and kryon-view. Add --all to also stop Marrow.

  status
      Show which services are running (with PIDs).

  list
      List available .kry example files.

  help
      Show this message.

Examples:
  kryon run examples/minimal.kry
  kryon run examples/widgets.kry --watch
  kryon run myapp.kry --stay-open
  kryon stop
  kryon stop --all
  kryon status
  kryon list
EOF
}

# ---------------------------------------------------------------------------
# Dispatch
# ---------------------------------------------------------------------------
case "${1:-help}" in
    run)    shift; cmd_run    "$@" ;;
    stop)   shift; cmd_stop   "$@" ;;
    status)        cmd_status      ;;
    list)          cmd_list        ;;
    help|-h|--help) cmd_help       ;;
    *)
        echo "[kryon] Unknown command: $1"
        echo "Run 'kryon help' for usage."
        exit 1
        ;;
esac
