#!/bin/sh
# Build, run or check the Android profile of a Kryon application.
#
# The kryon project tool sets KRYON_DIR, ZIRAN_DIR, KRYON_PROJECT_ROOT and
# KRYON_ANDROID_* from ziran.toml and runs this from the project root:
#   sh scripts/android-build.sh build|run|check
set -eu

verb="${1:-build}"
root="${KRYON_PROJECT_ROOT:-$PWD}"
kryon_dir="${KRYON_DIR:?kryon: KRYON_DIR must name the Kryon checkout}"
ziran_dir="${ZIRAN_DIR:?kryon: ZIRAN_DIR must name the pinned Ziran toolchain}"
package="${KRYON_ANDROID_PACKAGE:?kryon: set [tool.kryon.android] package in ziran.toml}"
activity="${KRYON_ANDROID_ACTIVITY:-MainActivity}"
abis="${KRYON_ANDROID_ABIS:-arm64-v8a}"
min_sdk="${KRYON_ANDROID_MIN_SDK:-21}"

cd "$root"
if [ ! -f ziran.toml ]; then
    echo "kryon: no ziran.toml in $root; run this through kryon build --profile android" >&2
    exit 2
fi

ziran_bin="$ziran_dir/build/bin/ziran"
zi2c_bin="$ziran_dir/build/bin/zi2c"
if [ ! -x "$ziran_bin" ] || [ ! -x "$zi2c_bin" ]; then
    echo "kryon: building the pinned Ziran toolchain" >&2
    make -j4 -C "$ziran_dir" build/bin/ziran build/bin/zi2c build/bin/zi2zir
fi

raylib_dir="${KRYON_ANDROID_RAYLIB_DIR:-}"
if [ -z "$raylib_dir" ]; then
    raylib_dir="$("$ziran_bin" pkg path raylib 2>/dev/null || true)"
fi
# The package root may be the repository, whose library sources live in src/.
if [ -n "$raylib_dir" ] && [ ! -f "$raylib_dir/raylib.h" ] &&
    [ -f "$raylib_dir/src/raylib.h" ]; then
    raylib_dir="$raylib_dir/src"
fi
if [ -z "$raylib_dir" ] || [ ! -f "$raylib_dir/raylib.h" ]; then
    echo "kryon: the locked raylib source package is missing; run ziran fetch" >&2
    exit 2
fi

if [ "$verb" = "check" ]; then
    # The generator parses, checks and lowers the whole Zi graph; the arch
    # define matches a phone build without needing the NDK.
    rm -rf "$root/build/android/check"
    python3 "$kryon_dir/scripts/generate_android_sources.py" \
        --ziran "$ziran_bin" --zi2c "$ziran_dir/build/bin/zi2c" \
        --root "$root" --kryon "$kryon_dir" \
        --ir "$root/build/android/check/ir" \
        --output "$root/build/android/check/generated" \
        --manifest "$root/build/android/check/generated-sources.cmake" \
        --define ANDROID_BUILD --define __aarch64__
    echo "kryon: android profile checked"
    exit 0
fi

python3 "$kryon_dir/scripts/android_scaffold.py" \
    --root "$root" --name "${KRYON_PROJECT_NAME:-app}" \
    --package "$package" --activity "$activity" --kryon "$kryon_dir"

if [ -z "${ANDROID_HOME:-}" ] && [ -d "$HOME/Android/Sdk" ]; then
    ANDROID_HOME="$HOME/Android/Sdk"
    export ANDROID_HOME
fi
if ! command -v java >/dev/null 2>&1; then
    echo "kryon: Gradle needs a JDK; install one (for example a headless openjdk)" >&2
    exit 2
fi

run_gradle() {
    if [ -f ./droid/gradlew ]; then
        sh ./droid/gradlew "$@"
    else
        gradle "$@"
    fi
}

run_gradle -p droid assembleDebug \
    -Pkryon.dir="$kryon_dir" -Pziran.dir="$ziran_dir" -Praylib.dir="$raylib_dir" \
    -Papp.minSdk="$min_sdk" -Papp.abis="$abis"

apk="droid/app/build/outputs/apk/debug/app-debug.apk"
if [ ! -f "$apk" ]; then
    echo "kryon: gradle did not produce $apk" >&2
    exit 2
fi

if [ "$verb" = "run" ]; then
    adb="${ANDROID_HOME:-$HOME/Android/Sdk}/platform-tools/adb"
    if ! command -v "$adb" >/dev/null 2>&1; then
        adb="$(command -v adb || true)"
    fi
    if [ -z "$adb" ]; then
        echo "kryon: adb not found; install the APK manually from $apk" >&2
        exit 2
    fi
    # With several devices attached, ANDROID_SERIAL selects the target.
    serial="${ANDROID_SERIAL:-}"
    if [ -n "$serial" ]; then
        set -- -s "$serial"
    else
        set --
    fi
    "$adb" "$@" install -r "$apk"
    "$adb" "$@" shell am start -n "$package/.$activity"
fi
