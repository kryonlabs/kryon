#!/usr/bin/env bash
set -euo pipefail

# Find duplicate basenames among .c and .h files excluding common build/vendor dirs
find . -type f \( -name '*.c' -o -name '*.h' \) \
  -not -path './vendor/*' -not -path './build/*' -not -path './generated/*' \
  -not -path './.git/*' -print0 \
| while IFS= read -r -d '' f; do
    b=$(basename "$f")
    printf '%s|%s\n' "$f" "$b"
  done \
| awk -F'|' '{files[$2]=files[$2] "\n" $1; count[$2]++} END{for (b in count) if (count[b]>1) {printf "%3d %s:\n%s\n\n", count[b], b, files[b]}}' \
| sort -rn
