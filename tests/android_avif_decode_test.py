#!/usr/bin/env python3
"""Build/package the actual shared image boundary for all Android ABIs.
Optionally run the ARMv7 test on the explicitly selected USB Moto, without
launching or touching an application, display or OS image decoder.
"""
import argparse
import os
from pathlib import Path
import subprocess
from toolchain import ROOT, ZIRAN, ZIRAN_ROOT
from image_packages import image_module_args
ABIS=('armeabi-v7a','arm64-v8a','x86','x86_64')
# dav1d assembly symbol suffixes each ABI's libavif.a must contain.
DAV1D_ASM={'armeabi-v7a':('_neon',),'arm64-v8a':('_neon','_neon_dotprod','_neon_i8mm'),
           'x86':('_ssse3','_sse4'),'x86_64':('_ssse3','_sse4','_avx2','_avx512icl')}
CMAKE='''cmake_minimum_required(VERSION 3.22)
project(KryonAndroidAVIF C CXX)
include("${KRYON_DIR}/cmake/KryonAndroid.cmake")
kryon_android_codecs("${WEBP_DIR}" "${AVIF_DIR}" "${AOM_DIR}")
file(GLOB sources "${GENERATED}/*.c")
add_executable(codec-test ${sources})
target_include_directories(codec-test PRIVATE "${GENERATED}" "${ZIRAN_INCLUDE}")
target_compile_options(codec-test PRIVATE -ffunction-sections -fdata-sections -Wno-main)
target_link_options(codec-test PRIVATE -Wl,--gc-sections -Wl,--no-undefined)
target_link_libraries(codec-test PRIVATE kryon_android_codecs android log atomic dl m)
'''
def run(command,env=None):
    result=subprocess.run(list(map(str,command)),cwd=ROOT,env=env,text=True,stdout=subprocess.PIPE,stderr=subprocess.STDOUT)
    if result.returncode:raise RuntimeError(f'{command} exited {result.returncode}\n{result.stdout[-5000:]}')
    return result.stdout

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--ndk',type=Path,required=True)
    parser.add_argument('--serial')
    parser.add_argument('--build-type',choices=('Debug','Release'))
    args=parser.parse_args()
    if args.serial and args.serial!='ZE2223BQZT':parser.error('this test is reserved for the USB Moto ZE2223BQZT')
    env={k:v for k,v in os.environ.items() if k not in ('DISPLAY','WAYLAND_DISPLAY','XAUTHORITY','DBUS_SESSION_BUS_ADDRESS','SESSION_MANAGER')}
    env['YUE_DESKTOP_RECOVERY']='0'
    packages={name:run([ZIRAN,'pkg','path',name]).strip() for name in ('libwebp','libavif','libaom','dav1d')}
    work=ROOT/'build/scratch/android-avif';work.mkdir(parents=True,exist_ok=True)
    (work/'CMakeLists.txt').write_text(CMAKE)
    for abi in ABIS:
        generated=work/abi/'generated'
        define={'armeabi-v7a':'__arm__','arm64-v8a':'__aarch64__','x86':'__i386__','x86_64':'__x86_64__'}[abi]
        run([ZIRAN,'build',*image_module_args(),'--target=c','--define','ANDROID_BUILD','--define',define,'--root',ROOT/'tests',
             '--module-path',ROOT/'src/backend','--module-path',ZIRAN_ROOT/'std','-o',generated,ROOT/'tests/avif_codec_test.zi'],env)
        # Cover the APK's Debug mode and Release with the real large crash input.
        configurations=(args.build_type,) if args.build_type else (('Debug','Release') if abi=='armeabi-v7a' else ('Release',))
        for build_type in configurations:
            run(['cmake','-S',work,'-B',work/abi,f'-DCMAKE_TOOLCHAIN_FILE={args.ndk}/build/cmake/android.toolchain.cmake',
                 f'-DANDROID_ABI={abi}','-DANDROID_PLATFORM=android-21',f'-DCMAKE_BUILD_TYPE={build_type}',
                 f'-DKRYON_DIR={ROOT}',f'-DWEBP_DIR={packages["libwebp"]}',
                 f'-DAVIF_DIR={packages["libavif"]}',f'-DAOM_DIR={packages["libaom"]}',f'-DDAV1D_DIR={packages["dav1d"]}',
                 f'-DGENERATED={generated}',f'-DZIRAN_INCLUDE={ZIRAN_ROOT}/include'],env)
            run(['cmake','--build',work/abi,'--target','codec-test','--parallel','2'],env)
            if abi=='armeabi-v7a':
                config=(work/abi/'aom/config/aom_config.h').read_text()
                assert '#define HAVE_NEON 0' in config,'the ARMv7 libaom oracle keeps alignment-safe portable kernels'
            nm=args.ndk/'toolchains/llvm/prebuilt/linux-x86_64/bin/llvm-nm'
            # The default dav1d engine is merged into libavif.a with its
            # runtime-dispatched assembly for this ABI.
            names=[line.split()[-1] for line in run([nm,'--defined-only',work/abi/'avif/libavif.a'],env).splitlines()
                   if line.startswith(('0','1','2','3','4','5','6','7','8','9','a','b','c','d','e','f')) and 'dav1d_' in line]
            counts={tier:sum(1 for name in names if tier in name) for tier in DAV1D_ASM[abi]}
            assert all(counts.values()),f'{abi}: dav1d assembly missing {counts}'
            tiers=', '.join(f'{tier[1:]} {count}' for tier,count in counts.items())
            undefined=run([nm,'--undefined-only',work/abi/'codec-test'],env)
            assert not any(name in undefined for name in ('avifDecoder','aom_codec','WebPDecode')),undefined
            print(f'{abi} {build_type}: Ziran codec boundary, dav1d default ({tiers}) plus libaom oracle linked, no unresolved codec symbols',flush=True)
            if abi=='armeabi-v7a' and args.serial:
                adb=['adb','-s',args.serial]
                assert run(adb+['get-state']).strip()=='device'
                assert run(adb+['shell','getprop','ro.build.version.sdk']).strip()=='28'
                remote='/data/local/tmp/kryon-avif-codec-test'
                run(adb+['shell','mkdir','-p',remote])
                run(adb+['push',work/abi/'codec-test',remote+'/codec-test'])
                # Push file contents explicitly so reruns cannot create fixtures/avif.
                fixtures=sorted(p for p in (ROOT/'tests/fixtures/avif').iterdir() if p.is_file())
                run(adb+['shell','mkdir','-p',remote+'/fixtures'])
                run(adb+['push',*fixtures,remote+'/fixtures/'])
                result=run(adb+['shell','chmod','700',remote+'/codec-test'])
                print(run(adb+['shell',remote+'/codec-test',remote+'/fixtures']).strip(),flush=True)
                print(f'USB Moto Android9 ARMv7 {build_type}: dav1d NEON/libaom strict parity, large title sync/worker, golden pixels and error fixtures passed',flush=True)
    print('Android AVIF integration passed for all four ABIs')
if __name__=='__main__':main()
