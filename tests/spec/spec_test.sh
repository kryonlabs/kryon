#!/bin/sh
set -eu

root=$(cd "${1:-.}" && pwd)
build_arg=${2:-build/linux-x86_64}
case "$build_arg" in
    /*) build=$build_arg ;;
    *) build=$root/$build_arg ;;
esac

k2ir=$build/bin/k2ir
k2c=$build/bin/k2c
k2g=$build/bin/k2g
k2b=$build/bin/k2b
case_file=tests/spec/language_contract.kry
work=${TMPDIR:-/tmp}/kryon-spec-test.$$

cleanup()
{
    rm -rf "$work"
}
trap cleanup EXIT INT TERM

mkdir -p "$work/ir" "$work/c" "$work/go" "$work/krb" "$work/go-check"

"$k2ir" --root "$root" -o "$work/ir" "$root/$case_file"
kir=$work/ir/tests/spec/language_contract.kir
grep -Fq 'assert condition (42) == 42 known 1 value 1' "$kir"
grep -Fq 'expr binary text count + 1 name  op +' "$kir"

"$k2c" --root "$root" -o "$work/c" "$root/$case_file"
cc -fsyntax-only -I"$root/include" -I"$work/c" "$work/c/tests/spec/language_contract.c"

"$k2g" --root "$root" -o "$work/go" "$root/$case_file"
go_file=$(find "$work/go" -name "*.go" | head -1)
test -f "$go_file"
cp "$go_file" "$work/go-check/language_contract.go"
sed -i '/^func main()/,$d' "$work/go-check/language_contract.go"
{
    printf '%s\n' 'module kryon-spec-test'
    printf '\n'
    printf '%s\n' 'go 1.25.0'
    printf '\n'
    printf '%s\n' 'require github.com/waozixyz/kryon/go/kryui v0.0.0'
    printf '%s\n' "replace github.com/waozixyz/kryon/go/kryui => $root/go/kryui"
} > "$work/go-check/go.mod"
(cd "$work/go-check" && GOCACHE=${GOCACHE:-$work/go-cache} go test ./...)

"$k2b" --root "$root" -o "$work/krb" "$root/tests/spec/krb_contract.kry"
test -s "$work/krb/tests/spec/krb_contract.krb"
strings "$work/krb/tests/spec/krb_contract.krb" | grep -Fq "KRB Spec Contract"

if "$k2ir" --root "$root" -o "$work/ir" "$root/tests/spec/assert_fail.kry" 2>"$work/assert_fail.err"; then
    echo "spec false #assert did not fail in k2ir" >&2
    exit 1
fi
grep -Fq "spec intentional assertion failure" "$work/assert_fail.err"

if "$k2g" --root "$root" -o "$work/go" "$root/tests/spec/assert_unresolved.kry" 2>"$work/assert_unresolved_go.err"; then
    echo "spec unresolved #assert did not fail in k2g" >&2
    exit 1
fi
grep -Fq "unresolved #assert is not supported by the Go backend" "$work/assert_unresolved_go.err"

if "$k2b" --root "$root" -o "$work/krb" "$root/tests/spec/assert_unresolved.kry" 2>"$work/assert_unresolved_krb.err"; then
    echo "spec unresolved #assert did not fail in k2b" >&2
    exit 1
fi
grep -Fq "unresolved #assert is not supported by KRB" "$work/assert_unresolved_krb.err"

echo "spec ok"
