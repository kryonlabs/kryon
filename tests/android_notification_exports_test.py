#!/usr/bin/env python3
"""Check real generated JNI exports through the canonical Android linker rules."""
import argparse
from pathlib import Path
import subprocess
from toolchain import ROOT, ZIRAN, ZIRAN_ROOT

WORK = ROOT / 'work/android-notification-exports'
COMMON = {
    'Java_org_kryonlabs_notifications_NotificationReceiver_receive',
    'Java_org_kryonlabs_notifications_NotificationReceiver_requestPermission',
    *('Java_org_kryonlabs_notifications_push_PushCallbacks_' + name
      for name in ('start', 'cancel', 'endpoint', 'failed', 'accepts', 'snapshot')),
}
GPLAY = {
    'Java_org_kryonlabs_notifications_push_TokenCallback_complete',
    *('Java_org_kryonlabs_notifications_push_SharedFcmService_' + name
      for name in ('refresh', 'ingress', 'acceptsMessage')),
}


def run(args, **kwargs):
    return subprocess.check_output(list(map(str, args)), text=True, **kwargs)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--ndk', type=Path)
    args = parser.parse_args()
    run(['git', '-C', ROOT, 'check-ignore', WORK])
    WORK.mkdir(parents=True, exist_ok=True)
    # Minimal Android entry stubs; every notification callback comes from the
    # production modules. The map must still hide all other library functions.
    host = WORK / 'android_glue.zi'
    host.write_text('''#import "android_jni"
#import "android_notifications"
#import "notification_push"
#program_export
GetAndroidApp :: () -> *NativeAndroidApp { return null }
#program_export
ANativeActivity_finish :: (activity: *void) { unused activity }
#program_export
ANativeActivity_onCreate :: () {}
#program_export
JNI_OnLoad :: () -> s32 { return 65542 }
#program_export
CORE :: () {}
#program_export
NotificationPrivateProbe :: () {}
''')
    abis = [('arm64-v8a', '__aarch64__'), ('armeabi-v7a', '__arm__')] if args.ndk else [('host', '__aarch64__')]
    for abi, architecture in abis:
        for edition in ('direct', 'fdroid', 'gplay'):
            project = WORK / abi / edition
            generated = project / 'c'
            definitions = ['--define', 'ANDROID_BUILD', '--define', architecture]
            if edition == 'gplay': definitions += ['--define', 'GOOGLE_PLAY']
            if edition == 'fdroid': definitions += ['--define', 'FDROID']
            run([ZIRAN, 'build', '--target=c', '--no-main', *definitions,
                 '--root', WORK, '--module-path', ROOT / 'src/backend',
                 '--module-path', ROOT / 'src/ui', '--module-path', ZIRAN_ROOT / 'std',
                 '-o', generated, host])
            # Exercise the identity-prefixed filenames produced by package mode.
            for module in ('android_notifications', 'notification_push'):
                (generated / (module + '.c')).rename(generated / ('pkg_' + module + '.c'))
            (project / 'CMakeLists.txt').write_text(f'''cmake_minimum_required(VERSION 3.22)
project(NotificationExports C)
include("{ROOT}/cmake/KryonAndroid.cmake")
file(GLOB sources "${{CMAKE_SOURCE_DIR}}/c/*.c")
add_library(main SHARED ${{sources}})
target_include_directories(main PRIVATE "{ZIRAN_ROOT}/include")
target_compile_options(main PRIVATE -fvisibility=hidden -ffunction-sections -fdata-sections)
target_link_options(main PRIVATE -Wl,--gc-sections -Wl,--no-undefined)
target_link_libraries(main PRIVATE m)
kryon_android_host(main ${{sources}})
''')
            build = project / 'build'
            command = ['cmake', '-S', project, '-B', build]
            if args.ndk:
                command += [f'-DCMAKE_TOOLCHAIN_FILE={args.ndk}/build/cmake/android.toolchain.cmake',
                            f'-DANDROID_ABI={abi}', '-DANDROID_PLATFORM=android-21']
            run(command)
            run(['cmake', '--build', build, '--parallel', '2'])
            symbols = run(['nm', '-D', '--defined-only', build / 'libmain.so'])
            exported = {line.split()[-1] for line in symbols.splitlines() if line.split()}
            assert COMMON <= exported, (abi, edition, COMMON - exported)
            assert 'JNI_OnLoad' in exported and 'ANativeActivity_onCreate' in exported
            assert 'NotificationPrivateProbe' not in exported, 'private symbols leaked'
            assert (GPLAY <= exported) if edition == 'gplay' else not (GPLAY & exported), (abi, edition)
            print(f'{abi} {edition}: canonical ELF JNI exports and private-symbol isolation passed', flush=True)


if __name__ == '__main__':
    main()
