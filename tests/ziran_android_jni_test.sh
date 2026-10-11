#!/bin/sh
# android_jni's JNI tables, NativeActivity and android_app layouts match the
# NDK headers for every Android ABI. Skipped without an NDK.
set -eu
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
. "$repo/tests/toolchain.sh"
ndk_base=${ANDROID_HOME:-$HOME/Android/Sdk}/ndk
toolchain=$(ls -d "$ndk_base"/*/toolchains/llvm/prebuilt/linux-x86_64 2>/dev/null | sort | tail -n 1 || true)
if [ -z "$toolchain" ]; then
    echo 'Android NDK unavailable; android_jni ABI checks skipped'
    exit 0
fi
ndk=$(CDPATH= cd -- "$toolchain/../../../.." && pwd)
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM
module=$repo/src/backend/android_jni.zi
{
    printf '%s\n' '#include <stddef.h>' '#include <pthread.h>' '#include <jni.h>' \
        '#include <android/native_activity.h>' '#include <android_native_app_glue.h>' \
        '#include "android_jni.h"' \
        '_Static_assert(sizeof(JniValue) == sizeof(jvalue), "JNI value size");' \
        '_Static_assert(_Alignof(JniValue) == _Alignof(jvalue), "JNI value alignment");'
    for pair in callbacks:callbacks vm:vm env:env activity:clazz internal_data_path:internalDataPath \
        external_data_path:externalDataPath sdk_version:sdkVersion instance:instance asset_manager:assetManager; do
        printf '_Static_assert(offsetof(NativeActivity, %s) == offsetof(ANativeActivity, %s), "activity %s");\n' \
            "${pair%%:*}" "${pair#*:}" "${pair%%:*}"
    done
    for pair in userdata:userData command_callback:onAppCmd input_callback:onInputEvent activity:activity \
        configuration:config saved_state:savedState saved_state_size:savedStateSize looper:looper \
        input_queue:inputQueue window:window content_rect:contentRect activity_state:activityState \
        destroy_requested:destroyRequested mutex_storage:mutex condition_storage:cond \
        command_read:msgread command_write:msgwrite; do
        printf '_Static_assert(offsetof(NativeAndroidApp, %s) == offsetof(struct android_app, %s), "app %s");\n' \
            "${pair%%:*}" "${pair#*:}" "${pair%%:*}"
    done
    for name in $(sed -n 's/^    \([A-Za-z]*\): Jni[A-Za-z]*Fn$/\1/p' "$module"); do
        case $name in
            AttachCurrentThread|DetachCurrentThread|GetEnv) table=JniVmFunctions native=JNIInvokeInterface ;;
            *) table=JniFunctions native=JNINativeInterface ;;
        esac
        printf '_Static_assert(offsetof(%s, %s) == offsetof(struct %s, %s), "%s");\n' \
            "$table" "$name" "$native" "$name" "$name"
    done
} > "$work/abi.c"
for target in aarch64-linux-android24:__aarch64__ armv7a-linux-androideabi24:__arm__ \
    i686-linux-android24:__i386__ x86_64-linux-android24:__x86_64__; do
    arch=${target#*:}
    "$ziran_root/build/bin/zi2c" --no-main --define ANDROID_BUILD --define "$arch" \
        --root "$repo/src/backend" --module-path "$ziran_root/std" -o "$work/$arch" "$module"
    includes="-I$ziran_root/include"
    for header in $(find "$work/$arch" -name '*.h' | xargs -n1 dirname | sort -u); do
        includes="$includes -I$header"
    done
    # shellcheck disable=SC2086
    "$toolchain/bin/clang" --target="${target%%:*}" -std=c11 -fsyntax-only $includes \
        -I"$ndk/sources/android/native_app_glue" -x c "$work/abi.c"
done
echo 'android_jni matches the NDK JNI, NativeActivity and android_app layouts on every Android ABI'
