#!/bin/sh
set -eu

case $1 in
/*) kssfmt=$1 ;;
*) kssfmt=$(pwd)/$1 ;;
esac
tmp=${TMPDIR:-/tmp}/kryon-kssfmt-test-$$

cleanup()
{
    rm -rf "$tmp"
}
trap cleanup EXIT INT TERM

mkdir -p "$tmp"
cat >"$tmp/ugly.kss" <<'EOF'
@version 1;
@pack cli.demo;
   // keep me
tokens {
      color {
        accent: #112233;
      }
}



@theme dark {
  accent: #ffcc00;
}
Button {
    background: accent;
      radius: 0;
}
Button:pressed {
            background: #304050;
}
EOF
cp "$tmp/ugly.kss" "$tmp/original.kss"

"$kssfmt" "$tmp/ugly.kss"
grep -q '^// keep me' "$tmp/ugly.kss"
grep -q '^        accent: #112233;' "$tmp/ugly.kss"
grep -q '^Button {$' "$tmp/ugly.kss"

# Idempotent: a second run changes nothing and --check is clean.
cp "$tmp/ugly.kss" "$tmp/formatted.kss"
"$kssfmt" "$tmp/ugly.kss"
cmp -s "$tmp/formatted.kss" "$tmp/ugly.kss"
"$kssfmt" --check "$tmp/ugly.kss"

# --check rejects the unformatted original.
if "$kssfmt" --check "$tmp/original.kss" 2>/dev/null; then
    echo "kssfmt cli test: --check accepted an unformatted file" >&2
    exit 1
fi

# Invalid input is diagnosed, not silently passed through.
printf '@pack broken;
Button {
' >"$tmp/broken.kss"
if "$kssfmt" --check "$tmp/broken.kss" 2>/dev/null; then
    echo "kssfmt cli test: --check accepted an invalid file" >&2
    exit 1
fi

echo "kssfmt cli ok"
