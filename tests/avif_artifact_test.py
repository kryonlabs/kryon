#!/usr/bin/env python3
"""Verify the actual provider's CPU codec outputs and native consumer links."""
import importlib.util
import json
import os
from pathlib import Path
import shutil
import subprocess
from toolchain import ROOT, ZIRAN, ZIRAN_ROOT, pinned, kryon_only
from image_packages import image_module_args
spec=importlib.util.spec_from_file_location('renderer_artifact',ROOT/'scripts/renderer_artifact.py')
renderer=importlib.util.module_from_spec(spec);spec.loader.exec_module(renderer)
WORK=ROOT/'build/scratch/avif-artifact'
ENV={k:v for k,v in os.environ.items() if k not in ('DISPLAY','WAYLAND_DISPLAY','XAUTHORITY','DBUS_SESSION_BUS_ADDRESS','SESSION_MANAGER')}
ENV.update(YUE_DESKTOP_RECOVERY='0',CMAKE_BUILD_PARALLEL_LEVEL='2')

def run(command,cwd=None,extra=None):
    result=subprocess.run(list(map(str,command)),cwd=cwd or WORK,env={**ENV,**(extra or {})},text=True,stdout=subprocess.PIPE,stderr=subprocess.STDOUT)
    if result.returncode:raise RuntimeError(result.stdout[-5000:])
    return result.stdout.strip()

def main():
    WORK.mkdir(parents=True,exist_ok=True)
    (WORK/'ziran.toml').write_text(pinned('''[package]
name = "avif_artifact_consumer"
[toolchain]
git = "https://github.com/ziranlang/ziran.git"
ref = "master"
[dependencies.kryon]
git = "https://github.com/kryonlabs/kryon.git"
ref = "master"
'''))
    packages=[('kryon',ROOT),('ziran',ZIRAN_ROOT)]
    packages.extend((name,run([ZIRAN,'pkg','path',name],cwd=ROOT))
                    for name in ('avif','webp','image_codec'))
    (WORK/'ziran.local.toml').write_text(kryon_only('[overrides]\n'+''.join(f'{name} = {json.dumps(str(path))}\n' for name,path in packages)))
    run([ZIRAN,'lock','--offline'])
    helper=ZIRAN_ROOT/'scripts/ziran_artifacts.py'
    spec=importlib.util.spec_from_file_location('ziran_artifacts',helper)
    artifacts=importlib.util.module_from_spec(spec);spec.loader.exec_module(artifacts)
    for name in ('renderer-desktop',):
        artifact=artifacts.request('kryon:'+name,'linux-x86_64-gnu',project=WORK,ziran=ZIRAN,env=ENV)
        meta=renderer.renderer_metadata(artifact)
        print(name+': descriptor/output integrity verified',flush=True)
        if name=='renderer-desktop':
            assert len(meta['shared_libraries'])==1
            codec=Path(meta['shared_libraries'][0])
            needed=run(['readelf','-d',codec])
            assert 'libavif.so' not in needed and 'libaom.so' not in needed,needed
            assert 'avifDecoderCreate' in run(['nm','-D',codec])
            generated=ROOT/'build/scratch/avif-decode/source'
            program=WORK/'installed/usr/bin/test';program.parent.mkdir(parents=True,exist_ok=True)
            lib=WORK/'installed/usr/lib';lib.mkdir(parents=True,exist_ok=True)
            shutil.copyfile(codec,lib/'libkryonavif.so')
            # A fresh packaged layout, with no absolute artifact/build rpath.
            run(['cc','-O1','-ffunction-sections','-fdata-sections','-Wl,--gc-sections',
                '-I'+str(ZIRAN_ROOT/'include'),'-I'+str(generated),*generated.glob('*.c'),
                ROOT/'build/webp-ziran/libkryonwebp.a',
                ROOT/'build/avif-ziran/avif/libavif.a',ROOT/'build/avif-ziran/aom/libaom.a',
                *run(['pkg-config','--libs','sdl2']).split(),'-ldl','-lpthread','-lm',
                '-Wl,--no-as-needed',lib/'libkryonavif.so','-Wl,--as-needed','-Wl,-rpath,$ORIGIN/../lib','-o',program])
            assert 'libkryonavif.so' in run(['readelf','-d',program])
            print(run([program,ROOT/'tests/fixtures/avif'],cwd='/'),flush=True)
            print('Installed AppDir codec discovered from cwd/ via ORIGIN and retained DT_NEEDED',flush=True)
        else:
            assert all(Path(path).is_file() for path in meta['archives'])
            assert any(Path(path).name=='libavif.a' for path in meta['archives'])

if __name__=='__main__':main()
