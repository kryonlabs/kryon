# 08 Go And Web Transpilation Parity

## Goal

Make `.kry` the source of truth so native C, generated Go, and generated web
runtime behavior stay identical.

## Current State

Generated Go files in `go/kryon` are produced from `runtime/*.kry`. The public
Go API should stay short and idiomatic, for example `kr.Slider` and
`kr.Link`, not old or prefixed names.

## Tasks

1. Never hand-edit generated Go policy as the source of truth.
2. Make policy changes in `runtime/*.kry`.
3. Run `make generate-runtime` after `.kry` changes.
4. Add Go tests when generated runtime behavior matters.
5. Add web/runtime parity tests for lowered blocks and widget behavior.
6. Keep public Go names canonical and prefix-free.
7. Keep public web widget registry names canonical and prefix-free.

## Proof

```sh
make generate-runtime
go test ./... ./go/kryon/...
sh tests/generated_runtime_parity_test.sh
sh tests/k2go_syntax_test.sh
sh tests/k2js_syntax_test.sh
sh tests/k2js_runtime_snapshot_test.sh
sh tests/public_api_names_test.sh
```

## Done When

- A widget policy change in `.kry` updates native and Go behavior together.
- Web generated output has the same canonical names and behavior.
- No public compatibility names exist in generated outputs.
