"""Verify actual rendered PDF pixels, page bounds and searchable Unicode text."""
from pathlib import Path
import sys
import xml.etree.ElementTree as ET

from PIL import Image

root, form = Path(sys.argv[1]), sys.argv[2]
image = Image.open(root / f"{form}-page-1.png").convert("RGB")
assert image.size == (300, 300), image.size
for offset in (0, 100):
    for point, expected in (((20 + offset, 60), (255, 0, 0)),
                            ((80 + offset, 60), (127, 127, 255)),
                            ((40 + offset, 25), (255, 255, 255))):
        actual = image.getpixel(point)
        assert all(abs(a - b) <= 3 for a, b in zip(actual, expected)), (point, actual, expected)

tree = ET.parse(root / f"{form}-text.html")
pages = tree.findall(".//{*}page")
assert len(pages) == 2, len(pages)
assert not pages[0].findall(".//{*}word"), "Measurement must not draw text"
words = pages[1].findall(".//{*}word")
text = " ".join(word.text or "" for word in words)
compact = "".join(text.split())
for expected in ("Café", "Mowgli", "forêt", "你好", "日本語", "한국어", "END-OF-BOOK"):
    assert expected in compact, (expected, text)
for word in words:
    assert 19 <= float(word.attrib["xMin"]) < float(word.attrib["xMax"]) <= 151, word.attrib
    assert 19 <= float(word.attrib["yMin"]) < float(word.attrib["yMax"]) <= 281, word.attrib
print(f"{form}: rendered alpha, aspect ratio, page count and Unicode text local tests passed")
