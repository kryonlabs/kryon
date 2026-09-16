# Style: remaining work

Updated 2026-09-15 against Kryon master.
This folder contains unfinished implementation and verification work only.
The design reference is [STYLE_SEPARATION_PROPOSALS.md](../../docs/STYLE_SEPARATION_PROPOSALS.md);
shipped behavior is documented in [API.md](../../docs/API.md).
An audit item means coverage has not been established, not that the feature is absent.

Shipped since the last audit: the KSS grammar now exists once in
`runtime/kss_parser.kry` and is lowered to C, Go, and JavaScript. That module
implements `@theme` overlays, typed `@env` axes, `@import` with cycle and
missing-source diagnostics, ordered `@layer` declarations, `@version`, and
file/line/column provenance. Strict `.kry` gained fixed-capacity array
indexing and borrowed string byte access to make that implementation
possible; C and Go hosts consume thin shims, and the hand-written per-backend
parsers are gone.

| Order | Remaining area | Task document |
|---|---|---|
| 1 | Value-set reconciliation, pack options (fuzz fixtures, web delegation, hot-cursor split: done) | [KSS language](02-kss-language.md) |
| 2 | Residual widget metrics and paint policy | [Widget migration](04-widget-migration.md) |
| 3 | Renderer fallbacks and full backend style parity | [Runtime and backends](05-runtime-and-backends.md) |
| 4 | Inspector, formatter, hot reload, compiled release styles | [Tooling](06-tooling-and-workflow.md) |
| 5 | Uncovered style/state/backend combinations and stricter gates | [Testing gaps](07-testing-and-gates.md) |
| 6 | Remaining examples/apps and unverified platform builds | [Downstream rollout](08-downstream-rollout.md) |
| 7 | Theme bridges, deprecated visual fields, stale documentation | [Legacy removal](09-legacy-removal.md) |

Remove each task when its source change and relevant verification are complete.
Delete its task document when no open items remain. Do not recreate completed
milestones as tasks or infer completion from a passing ratchet with allowlists.
