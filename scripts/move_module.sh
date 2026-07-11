#!/bin/sh
set -eu

usage() {
  echo "Usage: $0 <module-name> [--apply]"
  echo "Example: $0 lyra --apply"
  exit 1
}

if [ $# -lt 1 ]; then usage; fi
MODULE=$1
APPLY=0
if [ "${2:-}" = "--apply" ]; then APPLY=1; fi

SRC_DIR=src
TARGET_DIR=$SRC_DIR/$MODULE

# gather candidate files: src/lyra_*.c and src/lyra_*.h
candidates=$(find "$SRC_DIR" -maxdepth 1 -type f \( -name "${MODULE}_*.[ch]" -o -name "${MODULE}*.c" -o -name "${MODULE}*.h" \) | sort)

if [ -z "$candidates" ]; then
  echo "No candidates found for module '$MODULE' in $SRC_DIR"
  exit 0
fi

printf "Planned moves for module '%s':\n" "$MODULE"
for f in $candidates; do
  base=$(basename "$f")
  dest="$TARGET_DIR/$base"
  echo "$f -> $dest"
done

if [ $APPLY -eq 0 ]; then
  echo "\nDry-run only. Re-run with --apply to perform moves.";
  exit 0
fi

# apply moves
mkdir -p "$TARGET_DIR"
for f in $candidates; do
  base=$(basename "$f")
  dest="$TARGET_DIR/$base"
  echo "git mv '$f' '$dest'"
  git mv "$f" "$dest"
done

echo "Moves applied. Review changes and run 'make' to build." 
