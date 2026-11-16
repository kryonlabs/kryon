"""PNG fixtures, pixel assertions and command assertions for host tests."""
import struct
import subprocess
import zlib

PNG_SIGNATURE = b"\x89PNG\r\n\x1a\n"


def chunk(kind, data):
    body = kind + data
    return struct.pack(">I", len(data)) + body + struct.pack(">I", zlib.crc32(body))


def solid_png(path, width, height, color):
    scan = b"".join(b"\x00" + bytes(color) * width for _ in range(height))
    path.write_bytes(
        PNG_SIGNATURE
        + chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, 6, 0, 0, 0))
        + chunk(b"IDAT", zlib.compress(scan))
        + chunk(b"IEND", b"")
    )


def quadrant_png(path):
    scan = bytearray()
    colors = ((30, 200, 40, 255), (230, 30, 40, 255),
              (30, 40, 220, 255), (230, 200, 40, 255))
    for y in range(64):
        scan.append(0)
        for x in range(64):
            scan.extend(colors[(y >= 32) * 2 + (x >= 32)])
    path.write_bytes(
        PNG_SIGNATURE
        + chunk(b"IHDR", struct.pack(">IIBBBBB", 64, 64, 8, 6, 0, 0, 0))
        + chunk(b"IDAT", zlib.compress(scan))
        + chunk(b"IEND", b"")
    )


def png_pixels(path):
    data = path.read_bytes()
    assert data.startswith(PNG_SIGNATURE)
    offset = len(PNG_SIGNATURE)
    compressed = bytearray()
    while offset < len(data):
        length = struct.unpack_from(">I", data, offset)[0]
        kind = data[offset + 4 : offset + 8]
        payload = data[offset + 8 : offset + 8 + length]
        offset += 12 + length
        if kind == b"IHDR":
            width, height, depth, color, _, _, interlace = struct.unpack(
                ">IIBBBBB", payload
            )
            assert depth == 8 and color in (2, 6) and interlace == 0
            channels = 3 if color == 2 else 4
        elif kind == b"IDAT":
            compressed.extend(payload)
        elif kind == b"IEND":
            break
    raw = zlib.decompress(compressed)
    stride = width * channels
    rows = []
    previous = bytearray(stride)
    offset = 0
    for _ in range(height):
        filter_kind = raw[offset]
        row = bytearray(raw[offset + 1 : offset + 1 + stride])
        offset += 1 + stride
        assert filter_kind in range(5)
        for col in range(stride):
            left = row[col - channels] if col >= channels else 0
            above = previous[col]
            upper_left = previous[col - channels] if col >= channels else 0
            if filter_kind == 1:
                row[col] = (row[col] + left) & 255
            elif filter_kind == 2:
                row[col] = (row[col] + above) & 255
            elif filter_kind == 3:
                row[col] = (row[col] + (left + above) // 2) & 255
            elif filter_kind == 4:
                estimate = left + above - upper_left
                distances = [abs(estimate - n) for n in (left, above, upper_left)]
                predictor = (left, above, upper_left)[distances.index(min(distances))]
                row[col] = (row[col] + predictor) & 255
        rows.append(row)
        previous = row
    return width, height, channels, rows


def rgb(png, x, y):
    _, _, channels, rows = png
    return tuple(rows[y][x * channels : x * channels + 3])


def run(command, cwd, env=None):
    result = subprocess.run(command, cwd=cwd, env=env, text=True, capture_output=True)
    if result.returncode:
        raise AssertionError(
            f"{command!r} exited {result.returncode}\n"
            f"stdout:\n{result.stdout}\nstderr:\n{result.stderr}"
        )
