"""Compare the native dark dropdown panel with the user's approved render."""
from pathlib import Path
from PIL import Image, ImageChops

root = Path(__file__).resolve().parents[1]
expected = Image.open(root / "design/dropdown/approved.png").convert("RGB").crop((0, 64, 768, 1024))
actual = Image.open(root / "build/linux-x86_64/dropdown-captures/dark.png").convert("RGB")
if actual.size != expected.size:
    raise SystemExit(f"Dropdown capture size changed: {actual.size} != {expected.size}")
if ImageChops.difference(expected, actual).getbbox() is not None:
    raise SystemExit("Dark dropdown appearance differs from the approved render")
print("Dark dropdown appearance matches the approved render")
