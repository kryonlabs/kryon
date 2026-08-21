# Kryon Repository Rules

Kryon is the canonical runtime. Keep it small, direct, and free of new legacy
surfaces.

## Clean API Rule

Do not introduce new public generated-runtime APIs with legacy prefixes or
compatibility names. New `.kry` generated code must target the clean runtime
surface:

- `BeginFrame`
- `EndFrame`
- `Text`
- `Button`
- `TextField`
- `TextArea`
- `Row`
- `Column`

Do not add `kryc`. The supported transpilers are `k2g` for native Go and `k2c`
for native C.

Generated Go must be native Go and must not use cgo, `go/kryui`, `kryruntime`,
or an injected `rt` runtime object. Generated C must not call legacy prefixed UI
symbols such as `DrawUI*`, `UIText*`, `TextInputControl`, or `UIRender*`.

Existing compatibility code may only remain while actively migrating callers.
Do not expand it, duplicate it under another alias, or use it from generated
output. Remove compatibility code once no maintained app or fixture depends on
it.

## Test Rule

Any change to k2g, k2c, the Go runtime, or the C runtime surface must keep the
generated-output scanners and runtime parity tests passing. If a new widget or
semantic is added, add it to both generated runtimes and to parity coverage in
the same change.
