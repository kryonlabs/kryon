#!/bin/sh
set -eu

usage()
{
    cat <<'USAGE'
usage: kryon-app [--project DIR] COMMAND [TARGET]

Commands:
  preview                 build the app live-preview module
  run [native]            run the native app
  build TARGET            build native, web, android-debug, android-release,
                          android-bundle, windows, or dist
  package TARGET          build appimage, deb, rpm, flatpak, snap, click,
                          freebsd, linux, windows, or web package output
  test                    run the app test target
  clean [TARGET]          clean all or a target-specific build tree
  dev-backend             run a local Lyra sync server for this project
                          (locates the server at $LYRA_DIR or ../lyra; prints
                          the sync URL to point your app at)
USAGE
}

die()
{
    printf '%s\n' "$*" >&2
    exit 1
}

project=.
if [ "${1:-}" = "--project" ]; then
    [ $# -ge 3 ] || { usage >&2; exit 2; }
    project=$2
    shift 2
fi

[ $# -ge 1 ] || { usage >&2; exit 2; }
command=$1
target=${2:-}

cd "$project"
project_id=$(pwd | cksum | awk '{print $1}')
lock_dir="${TMPDIR:-/tmp}/kryon-app-${project_id}.lock"
while ! mkdir "$lock_dir" 2>/dev/null; do
    if [ ! -f "$lock_dir/pid" ]; then
        rm -rf "$lock_dir"
        continue
    fi
    old_pid=$(cat "$lock_dir/pid" 2>/dev/null || true)
    if [ -z "$old_pid" ] || ! kill -0 "$old_pid" 2>/dev/null; then
        rm -rf "$lock_dir"
        continue
    fi
    sleep 1
done
printf '%s\n' "$$" > "$lock_dir/pid"
trap 'rm -rf "$lock_dir"' 0 1 2 3 15

if [ "$(uname -s 2>/dev/null || true)" = "FreeBSD" ] && command -v gmake >/dev/null 2>&1; then
    make_cmd=${MAKE:-gmake}
else
    make_cmd=${MAKE:-make}
fi

[ -f Makefile ] || die "No Makefile found in $(pwd)"

run_make()
{
    exec "$make_cmd" "$@"
}

case "$command" in
preview)
    run_make kryon-live
    ;;
run)
    case "${target:-native}" in
    native|"")
        run_make run
        ;;
    *)
        run_make "$target"
        ;;
    esac
    ;;
build)
    [ -n "$target" ] || die "build target is required"
    case "$target" in
    native|web|android-debug|android-release|android-bundle|windows|dist)
        run_make "$target"
        ;;
    *)
        die "unknown build target: $target"
        ;;
    esac
    ;;
package)
    [ -n "$target" ] || die "package target is required"
    case "$target" in
    appimage|deb|package-deb|rpm|package-rpm|flatpak|package-flatpak|snap|package-snap|click|package-freebsd|freebsd|linux|windows|web)
        if [ "$target" = "freebsd" ]; then
            run_make package-freebsd
        elif [ "$target" = "linux" ]; then
            run_make dist-linux
        elif [ "$target" = "windows" ]; then
            run_make dist-windows
        elif [ "$target" = "web" ]; then
            run_make dist-web
        else
            run_make "$target"
        fi
        ;;
    *)
        die "unknown package target: $target"
        ;;
    esac
    ;;
test)
    run_make test
    ;;
dev-backend)
    # Run a local Lyra sync server for development. Locates the server source
    # at $LYRA_DIR (if set) or as a sibling checkout (../lyra relative to this
    # script), isolates its data in <project>/.kryon/, and runs it in the
    # foreground. The server regenerates its token secret each start, so this
    # is for local development only — do not use it for shared deployments.
    lyra_dir=${LYRA_DIR:-}
    if [ -z "$lyra_dir" ]; then
        # Try a sibling checkout: walk up from the current directory looking
        # for a ../lyra that contains the server, then fall back to a sibling
        # of the script's own location (for the in-tree kryon build).
        search_dir=$(pwd)
        while [ "$search_dir" != "/" ]; do
            candidate=$search_dir/../lyra
            if [ -f "$candidate/main.go" ] || [ -x "$candidate/lyra" ]; then
                lyra_dir=$(cd "$candidate" 2>/dev/null && pwd)
                break
            fi
            search_dir=$(dirname "$search_dir")
        done
    fi
    if [ -z "$lyra_dir" ]; then
        script_dir=$(cd "$(dirname "$0" 2>/dev/null || printf '.')" 2>/dev/null && pwd)
        candidate=$script_dir/../lyra
        if [ -f "$candidate/main.go" ] || [ -x "$candidate/lyra" ]; then
            lyra_dir=$(cd "$candidate" 2>/dev/null && pwd)
        fi
    fi
    if [ -z "$lyra_dir" ] || { [ ! -f "$lyra_dir/main.go" ] && [ ! -x "$lyra_dir/lyra" ]; }; then
        die "could not find the Lyra server source.
Set LYRA_DIR to its checkout path (e.g. export LYRA_DIR=/home/wao/src/lyra),
or place it as a sibling of a parent directory (../lyra)."
    fi
    # Per-project data dir. SQLite needs a native filesystem: shared/virtual
    # mounts (9p, etc.) fail its WAL/locking with "disk I/O error". Default to
    # a runtime cache dir on tmpfs, keyed by project so concurrent projects
    # don't collide; let LYRA_DB override for users who want it elsewhere.
    project_tag=$(pwd | cksum | awk '{print $1}')
    dev_root=${XDG_CACHE_HOME:-${TMPDIR:-/tmp}}/kryon-dev-backend
    mkdir -p "$dev_root"
    dev_db=${LYRA_DB:-$dev_root/lyra-$project_tag.db}
    dev_addr=${LYRA_ADDR:-127.0.0.1:8080}
    printf '== kryon dev-backend ==\n'
    printf 'server:  %s\n' "$lyra_dir"
    printf 'data:    %s\n' "$dev_db"
    printf 'listen:  http://%s\n' "$dev_addr"
    printf '\nPoint your app at this URL, e.g. by setting the sync base URL\n'
    printf 'to http://%s before calling RunLyraSync/RequestLyraSyncBearer.\n' "$dev_addr"
    printf '(Tokens are ephemeral; they reset on each restart.)\n\n'
    export LYRA_ADDR=$dev_addr
    export LYRA_BASE_URL=http://$dev_addr
    export LYRA_DB=$dev_db
    export LYRA_ALLOW_EPHEMERAL_TOKEN_SECRET=1
    if [ -x "$lyra_dir/lyra" ]; then
        exec "$lyra_dir/lyra"
    fi
    if ! command -v go >/dev/null 2>&1; then
        die "found Lyra source at $lyra_dir but 'go' is not on PATH.
Install Go, or build the server once with 'make build' in $lyra_dir."
    fi
    printf '(building + running via "go run ."; first run compiles liboqs)\n\n'
    make_cmd=${MAKE:-make}
    "$make_cmd" -C "$lyra_dir" run
    ;;
clean)
    case "$target" in
    ""|all)
        run_make clean
        ;;
    native|linux|windows|web)
        run_make "clean-$target"
        ;;
    vendor)
        run_make clean-vendor-builds
        ;;
    *)
        die "unknown clean target: $target"
        ;;
    esac
    ;;
-h|--help|help)
    usage
    ;;
*)
    usage >&2
    exit 2
    ;;
esac
