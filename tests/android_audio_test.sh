#!/bin/sh
# Source and saved-IR UI audio on the API-21 armv7 Moto, via std/audio_sink.
set -eu
cd "$(dirname "$0")/.."
ziran=${ZIRAN_BIN:-ziran}
serial=${ANDROID_SERIAL:-ZE2223BQZT}
ndk=${ANDROID_NDK:-${ANDROID_NDK_HOME:-$HOME/Android/Sdk/ndk/28.2.13676358}}
repo=$(pwd)
out=$repo/build/test/android-audio
mkdir -p "$out/consumer/src"
root=$("$ziran" pkg path ziran --offline)
ziran=$root/build/bin/ziran
audio=$("$ziran" pkg path audio --offline)
opus=$("$ziran" pkg path libopus --offline)
samplerate=$("$ziran" pkg path libsamplerate --offline)
cmake -S "$audio/native" -B "$out/native" -DCMAKE_TOOLCHAIN_FILE="$ndk/build/cmake/android.toolchain.cmake" \
    -DANDROID_ABI=armeabi-v7a -DANDROID_PLATFORM=android-21 -DCMAKE_BUILD_TYPE=Release \
    -DOPUS_DIR="$opus" -DSAMPLERATE_DIR="$samplerate" >"$out/configure.log"
cmake --build "$out/native" --target opus --parallel 2 >"$out/build.log"
cc=$ndk/toolchains/llvm/prebuilt/linux-x86_64/bin/armv7a-linux-androideabi21-clang
# A manifest-only consumer uses the real package exports, without desktop links.
python3 - "$ziran" "$repo" "$out/consumer" <<'PYTHON'
import json, pathlib, subprocess, sys, tomllib
ziran, repo, output = sys.argv[1:]
root = pathlib.Path(repo)
manifest = tomllib.loads((root/'ziran.toml').read_text())
project = pathlib.Path(output)
(project/'ziran.toml').write_text('[package]\nname = "android_audio_consumer"\nmodule_roots = ["src"]\n'
    '[toolchain]\ngit = "https://github.com/ziranlang/ziran.git"\nref = '
    + json.dumps(manifest['toolchain']['ref']) + '\n'
    '[dependencies.kryon]\ngit = "https://github.com/kryonlabs/kryon.git"\nref = "master"\n')
overrides = {'kryon': repo}
for name in ('ziran', 'audio', 'avif', 'webp', 'image_codec'):
    overrides[name] = subprocess.check_output([ziran, 'pkg', 'path', name, '--offline'],
        cwd=repo, text=True).strip()
(project/'ziran.local.toml').write_text('[overrides]\n' + ''.join(
    name + ' = ' + json.dumps(path) + '\n' for name, path in overrides.items()))
PYTHON
cp "$repo/tests/android_audio_test.zi" "$out/consumer/src/android_audio_test.zi"
cd "$out/consumer"
"$ziran" lock --offline
"$ziran" check --project --target-os android --target-cpu none --define ANDROID_BUILD --define __arm__ \
    --root "$out/consumer/src" "$out/consumer/src/android_audio_test.zi"
"$ziran" ir --project --target-os android --target-cpu none --define ANDROID_BUILD --define __arm__ \
    --root "$out/consumer/src" -o "$out/consumer/ir" "$out/consumer/src/android_audio_test.zi"
remote=/data/local/tmp/kryon-android-audio
cleanup() { adb -s "$serial" shell "rm -rf $remote" >/dev/null; }
trap cleanup EXIT
adb -s "$serial" shell "mkdir -p $remote" >/dev/null
for form in source saved; do
    if [ "$form" = source ]; then
        set -- --project --root "$out/consumer/src" "$out/consumer/src/android_audio_test.zi"
    else
        set -- --module-path "$out/consumer/ir" --root "$out/consumer/ir" "$out/consumer/ir/android_audio_test.zir"
    fi
    CC="$cc" CFLAGS="-O2 -Wno-parentheses-equality" \
        LDLIBS="$out/native/libopus/libopus.a -lm -ldl" \
        "$ziran" build --target=c --exe --entry android_audio_test:main --target-os android \
        --target-cpu none --define ANDROID_BUILD --define __arm__ -o "$out/bin" "$@"
    adb -s "$serial" push "$out/bin/android_audio_test" "$remote/test" >/dev/null
    adb -s "$serial" shell "chmod 755 $remote/test && $remote/test"
    echo "Android UI audio $form on $serial: passed"
done
