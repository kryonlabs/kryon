#!/bin/sh
# Run the same independent pixel oracle with native Plan 9 8c/8l.
set -eu
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
. "$repo/tests/toolchain.sh"
taiji=$(CDPATH= cd -- "${TAIJI_DIR:?Set TAIJI_DIR to the canonical Taiji checkout}" && pwd)
unset DISPLAY WAYLAND_DISPLAY XAUTHORITY DBUS_SESSION_BUS_ADDRESS ENV BASH_ENV
export YUE_DESKTOP_RECOVERY=0
case "${1:-both}" in
    both) forms='source saved' ;;
    source|saved) forms=$1 ;;
    *) echo 'usage: pixmap_surface_plan9_test.sh [both|source|saved]' >&2; exit 2 ;;
esac
fixture=${PIXMAP_PLAN9_FIXTURE:-pixmap_surface_test}
case "$fixture" in
    pixmap_surface_test|pixmap_measure_test) ;;
    *) echo 'Unsupported native pixmap fixture' >&2; exit 2 ;;
esac
parent=$repo/build/scratch/pixmap-surface-plan9
mkdir -p "$parent" "$taiji/usr/glenda/tmp"
exec 9>"$parent/native.lock"
flock -n 9 || { echo 'A native pixmap gate is already running' >&2; exit 1; }
work=$(mktemp -d "$parent/run.XXXXXX")
stage=$(mktemp -d "$taiji/usr/glenda/tmp/kryon-pixmap.XXXXXX")
guest_stage=/usr/glenda/tmp/${stage##*/}
vm_pid=
cleanup() {
    if test -n "$vm_pid"; then
        /bin/kill -TERM -- "-$vm_pid" 2>/dev/null || /bin/kill -TERM "$vm_pid" 2>/dev/null || true
        wait "$vm_pid" 2>/dev/null || true
    fi
    rm -rf "$stage"
}
trap cleanup EXIT HUP INT TERM
source=$repo/tests/$fixture.zi
"$ziran" ir --project --root "$repo/tests" -o "$work/ir" "$source"
for form in $forms; do
    if test "$form" = source; then
        "$ziran" build --project --target=plan9-c --root "$repo/tests" \
            --entry "$fixture:main" -o "$stage/source" "$source"
    else
        "$ziran" build --target=plan9-c --root "$work/ir" --module-path "$work/ir" \
            --entry "$fixture:main" -o "$stage/saved" "$work/ir/$fixture.zir"
    fi
done
cat > "$work/qemu" <<'QEMU'
#!/bin/sh
exec "$KRYON_PLAN9_QEMU" -accel tcg,tb-size=32 "$@"
QEMU
chmod 700 "$work/qemu"
command="
failed=0
for(form in $forms) {
    if(~ \$failed 0) {
        cd $guest_stage/\$form
        for(source in *.c) {
            if(~ \$failed 0) {
                if(! 8c -FTVw \$source) { echo kryon-pixmap-failed compile \$source; failed=1 }
            }
        }
        if(~ \$failed 0) {
            if(8l -o run *.8) {
                if(./run) echo kryon-pixmap-ok \$form
                if not { echo kryon-pixmap-failed run \$status; failed=1 }
            }
            if not { echo kryon-pixmap-failed link; failed=1 }
        }
    }
}
if(~ \$failed 0) echo kryon-pixmap-all-ok
fshalt
"
limit=${KRYON_PLAN9_TIMEOUT:-900}
# The emoji translation unit needs more than the smaller compiler fixtures;
# this is guest build memory, not a runtime memory measurement.
setsid --wait env Q9_BOOT_TIMEOUT="$limit" Q9_TMPDIR="$work" \
    Q9_MEM="${KRYON_PLAN9_MEMORY:-512M}" Q9_SMP=1 Q9_CHECKPOINT=0 Q9_BUILD_DESKTOP=0 \
    KRYON_PLAN9_QEMU="${QEMU:-qemu-system-x86_64}" QEMU="$work/qemu" \
    "$taiji/q9" --raw tty-run "$command" >"$work/output.log" 2>&1 &
vm_pid=$!
start=$(date +%s)
while test "$(( $(date +%s) - start ))" -lt "$limit"; do
    if rg -q '^kryon-pixmap-all-ok' "$work/output.log"; then
        echo "Pixmap oracle $fixture passed native Plan 9 ($forms)"
        exit 0
    fi
    if rg -q '^kryon-pixmap-failed|cannot init 9P|Operation not permitted' "$work/output.log"; then
        tail -35 "$work/output.log" >&2
        exit 1
    fi
    if ! kill -0 "$vm_pid" 2>/dev/null; then tail -35 "$work/output.log" >&2; exit 1; fi
    sleep 2
done
echo "Native Plan 9 pixel fixture timed out; output: $work/output.log" >&2
tail -35 "$work/output.log" >&2
exit 1
