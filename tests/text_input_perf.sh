#!/bin/sh
set -eu
root=$(cd "${1:-.}" && pwd)
build_arg=${2:-build/linux-x86_64}
case "$build_arg" in /*) build=$build_arg ;; *) build=$root/$build_arg ;; esac
work=${TMPDIR:-/tmp}/kryon-text-input-perf.$$
trap 'rm -rf "$work"' EXIT INT TERM
mkdir -p "$work/ir" "$work/c" "$work/go" "$work/krb" "$work/go-check"
source_file=tests/perf/text_input.kry
"$build/bin/k2ir" --root "$root" -o "$work/ir" "$source_file"
"$build/bin/k2c" --root "$root" -o "$work/c" "$source_file"
"$build/bin/k2g" --root "$root" -o "$work/go" "$source_file"
"$build/bin/k2b" --root "$root" -o "$work/krb" "$source_file"
sh "$root/tests/check_clean_generated_output.sh" "$work/c"
sh "$root/tests/check_clean_generated_output.sh" "$work/go"
grep -F 'widget TextField' "$work/ir/tests/perf/text_input.kir" >/dev/null
grep -F 'TextField((TextFieldProps)' "$work/c/tests/perf/text_input.c" >/dev/null
cc -fsyntax-only -I"$root/include" -I"$work/c" "$work/c/tests/perf/text_input.c"
grep -F 'kryon.TextField(kryon.TextFieldProps' "$work/go/text_input.go" >/dev/null
test -s "$work/krb/tests/perf/text_input.krb"
test "$(od -An -tu4 -j8 -N4 "$work/krb/tests/perf/text_input.krb" | tr -d ' ')" -ge 1
cp "$work/go/text_input.go" "$work/go-check/text_input.go"
sed -i '/^func main()/,$d' "$work/go-check/text_input.go"
printf '%s\n' 'module kryon-text-input-perf' '' 'go 1.25.0' '' 'require github.com/waozixyz/kryon/go/kryon v0.0.0' "replace github.com/waozixyz/kryon/go/kryon => $root/go/kryon" > "$work/go-check/go.mod"
(cd "$work/go-check" && GOCACHE=${GOCACHE:-$work/go-cache} go test ./... >/dev/null)
printf '%s\n' '{"generated_lowerings":["kir","c","go","krb"],"generated_contract":"validated"}'
"$build/tests/text_input_perf_test"
