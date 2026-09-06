#!/bin/sh
set -eu

usage()
{
    cat <<'USAGE'
usage: kryon [--project DIR] COMMAND [TARGET]

Commands:
  host                    build the generated Kryon app host
  run [native]            run the native app
  build TARGET            build native, web, android-debug, android-release,
                          android-bundle, windows, or dist
  package TARGET          build linux-desktop, appimage, deb, rpm, flatpak,
                          snap, click, freebsd, linux, windows, or web package
                          output
  test                    run the app test target
  clean [TARGET]          clean all or a target-specific build tree
  fmt [--check] FILE...   format .kry source files
  locale-check SRC... -- LOCALE...
                          check t("key") source references against locales
  dev-backend             run a local Daochi sync node for this project
                          (locates it at $DAOCHI_DIR or ../daochi; prints
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
lock_dir="${TMPDIR:-/tmp}/kryon-${project_id}.lock"
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
host)
    run_make kryon-host
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
    linux-desktop|package-linux-desktop|appimage|deb|package-deb|rpm|package-rpm|flatpak|package-flatpak|snap|package-snap|click|package-freebsd|freebsd|linux|windows|web)
        if [ "$target" = "freebsd" ]; then
            run_make package-freebsd
        elif [ "$target" = "linux-desktop" ]; then
            run_make package-linux-desktop
        elif [ "$target" = "package-linux-desktop" ]; then
            run_make package-linux-desktop
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
fmt)
    script_dir=$(cd "$(dirname "$0" 2>/dev/null || printf '.')" 2>/dev/null && pwd)
    shift
    "$script_dir/kry-fmt.sh" "$@"
    exit $?
    ;;
locale-check)
    script_dir=$(cd "$(dirname "$0" 2>/dev/null || printf '.')" 2>/dev/null && pwd)
    shift
    "$script_dir/kry-locale-check.sh" "$@"
    exit $?
    ;;
dev-backend)
    # Run a local Daochi node for development.
    daochi_dir=${DAOCHI_DIR:-}
    if [ -z "$daochi_dir" ]; then
        search_dir=$(pwd)
        while [ "$search_dir" != "/" ]; do
            candidate=$search_dir/../daochi
            if [ -f "$candidate/main.go" ] || [ -x "$candidate/daochi" ]; then
                daochi_dir=$(cd "$candidate" 2>/dev/null && pwd)
                break
            fi
            search_dir=$(dirname "$search_dir")
        done
    fi
    if [ -z "$daochi_dir" ]; then
        script_dir=$(cd "$(dirname "$0" 2>/dev/null || printf '.')" 2>/dev/null && pwd)
        candidate=$script_dir/../daochi
        if [ -f "$candidate/main.go" ] || [ -x "$candidate/daochi" ]; then
            daochi_dir=$(cd "$candidate" 2>/dev/null && pwd)
        fi
    fi
    if [ -z "$daochi_dir" ] ||
       { [ ! -f "$daochi_dir/main.go" ] && [ ! -x "$daochi_dir/daochi" ]; }; then
        die "could not find the Daochi server source.
Set DAOCHI_DIR to its checkout path, or place Daochi at ../daochi."
    fi
    # Per-project data dir. SQLite needs a native filesystem: shared/virtual
    # mounts (9p, etc.) fail its WAL/locking with "disk I/O error". Default to
    # a runtime cache dir on tmpfs, keyed by project so concurrent projects
    # don't collide; let DAOCHI_DB override for users who want it elsewhere.
    project_tag=$(pwd | cksum | awk '{print $1}')
    dev_root=${XDG_CACHE_HOME:-${TMPDIR:-/tmp}}/kryon-dev-backend
    mkdir -p "$dev_root"
    dev_db=${DAOCHI_DB:-$dev_root/daochi-$project_tag.db}
    dev_addr=${DAOCHI_ADDR:-127.0.0.1:8080}
    printf '== kryon dev-backend ==\n'
    printf 'server:  %s\n' "$daochi_dir"
    printf 'data:    %s\n' "$dev_db"
    printf 'listen:  http://%s\n' "$dev_addr"
    printf '\nPoint your app at this URL, e.g. by setting the sync base URL\n'
    printf 'to http://%s before calling RunSync/RequestSyncBearer.\n' "$dev_addr"
    printf '(Tokens are ephemeral; they reset on each restart.)\n\n'
    export DAOCHI_ADDR=$dev_addr
    export DAOCHI_BASE_URL=http://$dev_addr
    export DAOCHI_DB=$dev_db
    export DAOCHI_ALLOW_EPHEMERAL_TOKEN_SECRET=1
    if [ -x "$daochi_dir/daochi" ]; then
        exec "$daochi_dir/daochi"
    fi
    if ! command -v go >/dev/null 2>&1; then
        die "found Daochi source at $daochi_dir but 'go' is not on PATH.
Install Go, or build the server once with 'make build' in $daochi_dir."
    fi
    printf '(building + running via "go run ."; first run compiles liboqs)\n\n'
    make_cmd=${MAKE:-make}
    "$make_cmd" -C "$daochi_dir" run
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
