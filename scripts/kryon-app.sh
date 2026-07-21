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
