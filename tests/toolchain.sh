# Shared by the shell test harnesses. Use the same package source as the
# Makefile, including local overrides; callers may supply a built toolchain.
if [ -n "${ZIRAN_ROOT:-}" ]; then
    ziran_root=$ZIRAN_ROOT
else
    ziran_root=$("${ZIRAN_BIN:-ziran}" pkg path ziran)
fi
ziran=${ZIRAN_BIN:-"$ziran_root/build/bin/ziran"}

kryon_test_root=${repo:-${root:-$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)}}
# Add test providers as this package's own modules; dependencies retain their exports.
kryon_project_args="--project --module-path $kryon_test_root/tests"

# Standalone compiler fixtures use ordinary module roots. Widget tests using
# public exports or provider bindings use --project so aliases and module
# identities come from the manifest, rather than these filename search paths.
kryon_ui_args=$(find "$kryon_test_root/src/ui" -type d | LC_ALL=C sort | while IFS= read -r directory; do
    printf -- "--module-path %s --module-path kryon=%s " "$directory" "$directory"
done)
