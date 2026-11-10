import struct
import zlib

PNG_SIGNATURE = b"\x89PNG\r\n\x1a\n"


def _chunk(kind, payload):
    return struct.pack(">I", len(payload)) + kind + payload + struct.pack(
        ">I", zlib.crc32(kind + payload))


def solid_png(path, width, height, color):
    scan = b"".join(b"\0" + bytes(color) * width for _ in range(height))
    path.write_bytes(
        PNG_SIGNATURE
        + _chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, 6, 0, 0, 0))
        + _chunk(b"IDAT", zlib.compress(scan))
        + _chunk(b"IEND", b""))


def png_pixels(path):
    data = path.read_bytes()
    assert data.startswith(PNG_SIGNATURE)
    offset = len(PNG_SIGNATURE)
    compressed = bytearray()
    while offset < len(data):
        length = struct.unpack_from(">I", data, offset)[0]
        kind = data[offset + 4:offset + 8]
        payload = data[offset + 8:offset + 8 + length]
        offset += 12 + length
        if kind == b"IHDR":
            width, height, depth, color, _, _, interlace = struct.unpack(
                ">IIBBBBB", payload)
            assert depth == 8 and color == 6 and interlace == 0
            channels = 4
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
        row = bytearray(raw[offset + 1:offset + 1 + stride])
        offset += 1 + stride
        assert 0 <= filter_kind <= 4
        for col in range(stride):
            left = row[col - channels] if col >= channels else 0
            above = previous[col]
            upper_left = previous[col - channels] if col >= channels else 0
            if filter_kind == 1:
                row[col] = (row[col] + left) & 255
            elif filter_kind == 2:
                row[col] = (row[col] + above) & 255
            elif filter_kind == 3:
                row[col] = (row[col] + ((left + above) >> 1)) & 255
            elif filter_kind == 4:
                value = left + above - upper_left
                distance = abs(value - left)
                if distance <= abs(value - above) and distance <= abs(value - upper_left):
                    predictor = left
                elif abs(value - above) <= abs(value - upper_left):
                    predictor = above
                else:
                    predictor = upper_left
                row[col] = (row[col] + predictor) & 255
        previous = row
        rows.append(row)
    return width, height, channels, rows


def rgb(png, x, y):
    _, _, channels, rows = png
    return tuple(rows[y][x * channels:x * channels + 3])
