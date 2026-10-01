# Shared by the shell test harnesses. Use the same package source as the
# Makefile, including local overrides; callers may supply a built toolchain.
if [ -n "${ZIRAN_ROOT:-}" ]; then
    ziran_root=$ZIRAN_ROOT
else
    ziran_root=$("${ZIRAN_BIN:-ziran}" pkg path ziran)
fi
ziran=${ZIRAN_BIN:-"$ziran_root/build/bin/ziran"}
