#!/bin/sh
set -eu

root=$(cd "${1:-.}" && pwd)
build_arg=${2:-build/linux-x86_64}
case "$build_arg" in
    /*) build=$build_arg ;;
    *) build=$root/$build_arg ;;
esac

k2kir=$build/bin/k2kir
k2c=$build/bin/k2c
k2go=$build/bin/k2go
k2js=$build/bin/k2js
k2b=$build/bin/k2b
case_file=tests/spec/language_contract.kry
work=${TMPDIR:-/tmp}/kryon-spec-test.$$

cleanup()
{
    rm -rf "$work"
}
trap cleanup EXIT INT TERM

mkdir -p "$work/ir" "$work/c" "$work/go" "$work/js" "$work/krb" "$work/go-check"

"$k2kir" --root "$root" -o "$work/ir" "$root/$case_file"
kir=$work/ir/tests/spec/language_contract.kir
grep -Fq 'assert condition (42) == 42 known 1 value 1' "$kir"
grep -Fq 'expr binary text count + 1 name  op +' "$kir"

"$k2c" --root "$root" -o "$work/c" "$root/$case_file"
sh "$root/tests/check_clean_generated_output.sh" "$work/c"
cc -fsyntax-only -I"$root/include" -I"$work/c" -I"$build/generated/src" "$work/c/tests/spec/language_contract.c"

"$k2go" --root "$root" -o "$work/go" "$root/$case_file"
sh "$root/tests/check_clean_generated_output.sh" "$work/go"
go_file=$(find "$work/go" -name "*.go" | head -1)
test -f "$go_file"
cp "$go_file" "$work/go-check/language_contract.go"
sed '/^func main()/,$d' "$work/go-check/language_contract.go" \
    > "$work/go-check/language_contract.go.tmp"
mv "$work/go-check/language_contract.go.tmp" "$work/go-check/language_contract.go"
{
    printf '%s\n' 'module kryon-spec-test'
    printf '\n'
    printf '%s\n' 'go 1.25.0'
    printf '\n'
    printf '%s\n' 'require ('
    printf '%s\n' '	github.com/waozixyz/kryon/go/kryon v0.0.0'
    printf '%s\n' '	golang.org/x/image v0.45.0'
    printf '%s\n' '	golang.org/x/sys v0.47.0'
    printf '%s\n' '	golang.org/x/text v0.41.0'
    printf '%s\n' ')'
    printf '%s\n' "replace github.com/waozixyz/kryon/go/kryon => $root/go/kryon"
} > "$work/go-check/go.mod"
cp "$root/go/kryon/go.sum" "$work/go-check/go.sum"
cat > "$work/go-check/contract_test.go" << 'GOTEST'
package krygen

import "testing"

func TestContractIndexingParity(t *testing.T) {
	st := &LanguageContractState{}
	if got := LanguageContract_ContractSumBytes(st, "AB"); got != 131 {
		t.Fatalf("string byte sum = %d, want 131", got)
	}
	bytes := LanguageContract_ContractFillBytes(st, ContractBytes{}, 10)
	if bytes.Filled != 4 || bytes.Data[0] != 10 || bytes.Data[3] != 13 {
		t.Fatalf("array fill = %+v", bytes)
	}
	if got := LanguageContract_ContractByteAt(st, bytes, 2); got != 12 {
		t.Fatalf("array byte at = %d, want 12", got)
	}
	if !LanguageContract_ContractBytesReady(st, bytes) {
		t.Fatal("indexed comparisons disagreed across fields")
	}
}
GOTEST
(cd "$work/go-check" && GOCACHE=${GOCACHE:-$work/go-cache} go test ./...)

"$k2js" --root "$root" -o "$work/js" "$root/$case_file"
test -f "$work/js/tests/spec/language_contract.js"
cp "$root"/web/*.js "$work/js/"
printf '%s\n' '{"type":"module"}' > "$work/js/package.json"
if command -v node >/dev/null 2>&1; then
    node -e 'import(process.argv[1]).then((m) => { const s = m.frame(); if (!s || !Array.isArray(s.frame)) process.exit(1); })' "$work/js/tests/spec/language_contract.js"
    node --input-type=module - "$work/js/tests/spec/language_contract.js" << 'JSCHECK'
import assert from "node:assert/strict";
import { pathToFileURL } from "node:url";

const [modulePath] = process.argv.slice(2);
const m = await import(pathToFileURL(modulePath).href);
const rt = null;
assert.equal(m.LanguageContract_ContractSumBytes(rt, undefined, undefined, "AB"), 131);
let bytes = { data: [0, 0, 0, 0], count: 0 };
bytes = m.LanguageContract_ContractFillBytes(rt, undefined, undefined, bytes, 10);
assert.equal(bytes.filled, 4);
assert.equal(bytes.data[0], 10);
assert.equal(bytes.data[3], 13);
assert.equal(m.LanguageContract_ContractByteAt(rt, undefined, undefined, bytes, 2), 12);
assert.equal(m.LanguageContract_ContractBytesReady(rt, undefined, undefined, bytes), true);
JSCHECK
fi

"$k2b" --root "$root" -o "$work/krb" "$root/tests/spec/krb_contract.kry"
test -s "$work/krb/tests/spec/krb_contract.krb"
strings "$work/krb/tests/spec/krb_contract.krb" | grep -Fq "KRB Spec Contract"

if "$k2kir" --root "$root" -o "$work/ir" "$root/tests/spec/assert_fail.kry" 2>"$work/assert_fail.err"; then
    echo "spec false #assert did not fail in k2kir" >&2
    exit 1
fi
grep -Fq "spec intentional assertion failure" "$work/assert_fail.err"

if "$k2go" --root "$root" -o "$work/go" "$root/tests/spec/assert_unresolved.kry" 2>"$work/assert_unresolved_go.err"; then
    echo "spec unresolved #assert did not fail in k2go" >&2
    exit 1
fi
grep -Fq "unresolved #assert is not supported by the Go backend" "$work/assert_unresolved_go.err"

if "$k2js" --root "$root" -o "$work/js" "$root/tests/spec/assert_unresolved.kry" 2>"$work/assert_unresolved_js.err"; then
    echo "spec unresolved #assert did not fail in k2js" >&2
    exit 1
fi
grep -Fq "unresolved #assert is not supported by the JS backend" "$work/assert_unresolved_js.err"

if "$k2b" --root "$root" -o "$work/krb" "$root/tests/spec/assert_unresolved.kry" 2>"$work/assert_unresolved_krb.err"; then
    echo "spec unresolved #assert did not fail in k2b" >&2
    exit 1
fi
grep -Fq "unresolved #assert is not supported by KRB" "$work/assert_unresolved_krb.err"

# Over-long source lines and crafted #import targets must fail with located
# errors instead of being silently truncated or injected into includes.
if "$k2c" --root "$root" -o "$work/c" "$root/tests/spec/long_line.kry" 2>"$work/long_line.err"; then
    echo "spec long line did not fail in k2c" >&2
    exit 1
fi
grep -Fq "source line exceeds" "$work/long_line.err"

if "$k2c" --root "$root" -o "$work/c" "$root/tests/spec/bad_import.kry" 2>"$work/bad_import.err"; then
    echo "spec bad import did not fail in k2c" >&2
    exit 1
fi
grep -Fq "#import target contains a character that cannot appear in an include path" "$work/bad_import.err"

echo "spec ok"
