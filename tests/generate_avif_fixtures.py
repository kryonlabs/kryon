#!/usr/bin/env python3
"""Regenerate small AVIF assets with official avifenc, preserving binary files.
Golden RGBA bytes come from the input pixels and Pillow geometry, not Kryon.
Usage: AVIFENC=/path/to/avifenc python3 tests/generate_avif_fixtures.py
"""
import os
from pathlib import Path
import subprocess
import tempfile
from PIL import Image, ImageOps
ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / 'tests/fixtures/avif'

def main():
    OUT.mkdir(exist_ok=True)
    scratch = ROOT / 'build/scratch'
    scratch.mkdir(exist_ok=True)
    with tempfile.TemporaryDirectory(dir=scratch) as temporary:
        work = Path(temporary)
        base = Image.new('RGBA', (8, 6))
        base.putdata([(x * 28, y * 40, (x * 13 + y * 17) % 256, (0,64,128,255)[x % 4])
                      for y in range(6) for x in range(8)])
        opaque = base.copy(); opaque.putalpha(255)
        gray = Image.new('L',(8,6)); gray.putdata([i * 5 for i in range(48)])
        cases = []
        for depth in (8,10,12):
            cases += [(f'rgba{depth}',base,depth,[],base,1 if depth != 8 else 0),
                      (f'gray{depth}',gray,depth,['--yuv','400','--cicp','1/13/6'],gray.convert('RGBA'),1)]
        for angle in range(4):
            for mirror in range(2):
                expected=base.crop((2,1,6,5)).rotate(90*angle,expand=True)
                expected=ImageOps.flip(expected) if mirror == 0 else ImageOps.mirror(expected)
                cases.append((f'transform{angle}{mirror}',base,8,
                              ['--crop','2,1,4,4','--irot',str(angle),'--imir',str(mirror)],expected,0))
        cases += [('premultiplied',base,8,['--premultiply'],base,4),
                  ('opaque',opaque,8,[],opaque,0)]
        for name, source, depth, options, expected, tolerance in cases:
            path=work/'input.png'; source.save(path)
            command=[os.environ.get('AVIFENC','avifenc'),'--jobs','1','--speed','10',*(['--qcolor','100'] if source.mode == 'L' else ['--lossless']),
                     '--yuv','444','--cicp','1/13/0','--depth',str(depth),*options,str(path),str(OUT/(name+'.avif'))]
            result=subprocess.run(command,stdout=subprocess.PIPE,stderr=subprocess.STDOUT)
            if result.returncode: raise SystemExit(result.stdout.decode()[-2000:])
            # Unpremultiplication cannot recover RGB under alpha=0; zero it.
            if name == 'premultiplied':
                expected=expected.copy()
                expected.putdata([p if p[3] else (0,0,0,0) for p in expected.getdata()])
            (OUT/(name+'.rgba')).write_bytes(expected.tobytes())
            (OUT/(name+'.txt')).write_text(f'{expected.width} {expected.height} {tolerance}\n')
        # A sequence must expose only its first frame in Image's still API.
        first=work/'first.png'; second=work/'second.png'
        base.save(first); opaque.save(second)
        result=subprocess.run([os.environ.get('AVIFENC','avifenc'),'--jobs','1','--speed','10','--lossless',
            '--yuv','444','--cicp','1/13/0',str(first),str(second),str(OUT/'animated.avif')],
            stdout=subprocess.PIPE,stderr=subprocess.STDOUT)
        if result.returncode: raise SystemExit(result.stdout.decode()[-2000:])
        (OUT/'animated.rgba').write_bytes(base.tobytes());(OUT/'animated.txt').write_text('8 6 0\n')
        original=(OUT/'rgba8.avif').read_bytes()
        # ispe dimensions are parsed before AV1 pixel allocation.
        for name,w,h in [('oversized',32769,6),('pixel-limit',16384,16384)]:
            corrupt=bytearray(original); offset=0
            while True:
                offset=corrupt.find(b'ispe',offset)
                if offset<0:break
                corrupt[offset+8:offset+16]=w.to_bytes(4,'big')+h.to_bytes(4,'big'); offset+=16
            (OUT/(name+'.avif')).write_bytes(corrupt)
        crop=bytearray((OUT/'transform00.avif').read_bytes()); pos=crop.index(b'clap')
        crop[pos+8:pos+12]=bytes(4) # zero width denominator
        (OUT/'bad-crop.avif').write_bytes(crop)
        (OUT/'truncated.avif').write_bytes(original[:-1])
        (OUT/'malformed.avif').write_bytes(original[:32])
    print('Generated AVIF binary fixtures and independent RGBA goldens')
if __name__ == '__main__':main()
