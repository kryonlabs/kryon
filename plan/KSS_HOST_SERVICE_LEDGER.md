# KSS host service ledger

This ledger classifies active KSS parser, formatter, resolver, and style-pack
call sites. The completion rule for P4-01 is that grammar, cascade, formatter,
and selector semantics stay in maintained `.kry` modules; host code may only
supply storage, import resolution, output buffers, generated registration code,
or renderer/tool entry points.

| Surface | File | Classification | Host responsibility | Semantic owner | Regression gate |
|---|---|---|---|---|---|
| Shared parser grammar | `runtime/kss_parser.kry` | generated owner | None; this is source truth lowered to C and Go. | KSS lexical grammar, selectors, token overlays, imports, variants, diagnostics, origins. | `make kss-parser-test kss-matched-test`; `make go-runtime-test` |
| Shared formatter | `runtime/kss_formatter.kry` | generated owner | None; this is source truth lowered to C and Go. | KSS formatting, declaration reflow, format-result diagnostics. | `make kss-formatter-test`; `sh tests/kssfmt_cli_test.sh build/linux-x86_64/bin/kssfmt`; `make go-runtime-test` |
| Shared cascade/style decisions | `runtime/style_sheet.kry` | generated owner | None; this is source truth lowered to C and Go. | Style facts, specificity, priority, rule application, per-field decision facts. | `make style-sheet-policy-test`; `make kss-matched-test`; `make go-runtime-test` |
| Public C style API | `include/ui_style_sheet.h` | host service boundary | Declare retained C API for style registration, activation, resolving, and diagnostics. | Generated parser/cascade modules plus C source-retaining registry. | `make public-headers-compile-check style-pack-source-test` |
| C parser/formatter wrapper | `src/ui/kss_parser.c` | host service | Allocate parser state, copy diagnostics/source traces, resolve/import source text supplied by host, and expose C wrapper functions. | `runtime/kss_parser.kry`, `runtime/kss_formatter.kry`. | `make kss-parser-test kss-matched-test kss-formatter-test` |
| Private C parser/formatter boundary | `src/ui/kss_parser.h` | host service | Declare wrapper entry points and import callback shape for C internals. | `runtime/kss_parser.kry`, `runtime/kss_formatter.kry`. | `make kss-parser-test kss-matched-test kss-formatter-test` |
| C active style resolver | `src/ui/style_sheet.c` | host service | Hold active style-pack pointer and route render-time queries to generated cascade. | `runtime/style_sheet.kry`. | `make style-sheet-policy-test style-pack-source-test` |
| C source-retaining pack registry | `src/ui/style_pack_source.c` | host service | Retain pack source strings, provide module imports, enumerate variants, and atomically update active packs. | `runtime/kss_parser.kry`, `runtime/style_sheet.kry`. | `make style-pack-source-test style-builtins-test` |
| Native formatter CLI | `cmd/kssfmt/main.c` | tool host service | Read input files/stdin, grow output buffers, write formatted text, report diagnostics. | `runtime/kss_formatter.kry`. | `sh tests/kssfmt_cli_test.sh build/linux-x86_64/bin/kssfmt` |
| Generated C app registration emitter | `cmd/k2c/k2c_project.c` | compiler-emitted host glue | Emit app startup calls that register authored KSS sources. | Runtime registration API and shared parser. | `make k2c-syntax-test` |
| Generated C++ app registration emitter | `cmd/k2cpp/k2cpp_project.c` | compiler-emitted host glue | Emit app startup calls that register authored KSS sources. | Runtime registration API and shared parser. | `make k2cpp-syntax-test` |
| Generated Go app registration emitter | `cmd/k2go/k2go_lower.c` | compiler-emitted host glue | Emit Go init code that registers authored KSS sources. | Go runtime registration API and shared parser. | `make k2go-syntax-test`; `make go-runtime-test` |
| Go parser/variant wrappers | `go/kryon/style_parse.go` | host service | Convert Go strings into generated parser cursors, collect rules/variants, and expose Go errors. | `runtime/kss_parser.kry`. | `make go-runtime-test` |
| Go source-retaining pack registry | `go/kryon/style_pack.go` | host service | Retain source text, reparse on theme/variant changes, and hold active style-pack state. | `runtime/kss_parser.kry`, `runtime/style_sheet.kry`. | `make go-runtime-test` |
| Go built-in pack generator | `scripts/generate-go-style-builtins.py` | generated-data tool | Copy checked-in KSS pack sources into Go registration data and verify staleness. | Authored KSS files plus Go style-pack registry. | `make go-style-builtins-check` |
| Web type declarations | `web/kryon-runtime.d.ts` | future web reference | Preserve API type names while old JS/web remains paused. | Future `.kry` to HTML/CSS/small-JS target, not current release evidence. | Future web roadmap only |

`go/kryon/style_builtins.go` is generated data from
`scripts/generate-go-style-builtins.py`, so the generator is the classified
call site. Test files intentionally exercise the same public APIs and are not
host-service owners.
