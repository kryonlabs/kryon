"""Small exact RGBA fixture; test tooling writes binary PNG directly."""
import struct,zlib
from pathlib import Path
out=Path(__file__).resolve().parents[1]/'build/scratch/image-pixels'
out.mkdir(parents=True,exist_ok=True)
def chunk(tag,data):
    return struct.pack('>I',len(data))+tag+data+struct.pack('>I',zlib.crc32(tag+data))
data=b'\x00'+bytes([173,61,219,97,19,203,41,255])
png=b'\x89PNG\r\n\x1a\n'+chunk(b'IHDR',struct.pack('>IIBBBBB',2,1,8,6,0,0,0))+chunk(b'IDAT',zlib.compress(data))+chunk(b'IEND',b'')
(out/'rgba.png').write_bytes(png)
(out/'invalid.png').write_bytes(b'invalid image')
