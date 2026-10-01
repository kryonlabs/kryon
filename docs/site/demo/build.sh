#!/bin/sh
set -eu
cd "$(dirname "$0")"
if [ -f ziran.local.toml ]; then
    env -u DISPLAY -u WAYLAND_DISPLAY ziran tool kryon build --profile web
else
    env -u DISPLAY -u WAYLAND_DISPLAY ziran tool kryon build --profile web --locked
fi
python3 - <<'PY_BUILD'
from pathlib import Path
content = Path('build/widgets-web.html').read_text()
content = content.replace('</head>', '<style>#result{display:none}</style></head>', 1)
content = content.replace('</body>', '<script src="../demo-theme.js"></script></body>', 1)
Path('../assets/widgets.html').write_text(content)
PY_BUILD
