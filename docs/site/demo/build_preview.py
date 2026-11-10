#!/usr/bin/env python3
"""Build the canonical Ziran browser compiler for the Ziran gallery editor.

Recovered from this project's Canvas preview pipeline. Compiler implementation
remains upstream; this file supplies the gallery's source/module mounts only.
"""
import os
from pathlib import Path
import shutil
import subprocess
import sys
ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT / 'scripts'))
from gallery import compiler_embeds, PROJECT

def main():
    env = os.environ.copy()
    # Source mounts plus the compiler stack exceed Emscripten's 16 MiB default.
    env.setdefault('EMCC_CFLAGS', '-sINITIAL_MEMORY=33554432')
    ziran = Path(subprocess.check_output(['ziran', 'pkg', 'path', 'ziran'], cwd=PROJECT, text=True).strip())
    emcc = env.get('EMCC') or shutil.which('emcc') or str(Path.home() / 'emsdk/upstream/emscripten/emcc')
    output = ROOT / 'docs/site/runtime'
    output.mkdir(parents=True, exist_ok=True)
    env.setdefault('EM_CACHE', str(ziran / 'build/emscripten-cache'))
    command = [str(ziran / 'build/bin/ziran'), 'run', '--target=c', str(ziran / 'scripts/build_playground.zi'), '--emcc', emcc, '--output-dir', str(output)]
    embeds = [str(ROOT / 'src') + '@/kryon']
    for name in ('preview_entry', 'preview_style', 'preview_protocol'):
        embeds.append(str(Path(__file__).parent / ('src/' + name + '.zi')) + '@/' + name + '.zi')
    embeds.extend(compiler_embeds(core=ROOT))
    for mount in embeds: command += ['--embed-file', mount]
    subprocess.run(command, cwd=ziran, env=env, check=True)
    print('Built canonical browser compiler with gallery source mounts')

if __name__ == '__main__': main()
