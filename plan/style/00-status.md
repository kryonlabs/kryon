# Style: remaining work

Updated 2026-09-15; ownership correction against Kryon `1e6b28ac`.
This folder contains unfinished implementation and verification work only.
The design reference is [STYLE_SEPARATION_PROPOSALS.md](../../docs/STYLE_SEPARATION_PROPOSALS.md);
shipped behavior is documented in [API.md](../../docs/API.md).
An audit item means coverage has not been established, not that the feature is absent.

The first task is architectural correction: KSS parsing and semantics belong
only in maintained `.kry` source. C, Go, JavaScript, and KRB consume generated
implementations/data. Handwritten host code is limited to platform services,
resource I/O, and rendering integration; it must not interpret KSS.
The handwritten color-variant additions in `16510c75` are migration debt,
not the architecture to extend. Passing parity tests does not make duplicated
implementations acceptable.

| Order | Remaining area | Task document |
|---|---|---|
| 1 | Consolidate KSS into `.kry` and delete handwritten implementations; then finish language features | [KSS language](02-kss-language.md) |
| 2 | Residual widget metrics and paint policy | [Widget migration](04-widget-migration.md) |
| 3 | Renderer fallbacks and full backend style parity | [Runtime and backends](05-runtime-and-backends.md) |
| 4 | Inspector, formatter, hot reload, compiled release styles | [Tooling](06-tooling-and-workflow.md) |
| 5 | Uncovered style/state/backend combinations and stricter gates | [Testing gaps](07-testing-and-gates.md) |
| 6 | Remaining examples/apps and unverified platform builds | [Downstream rollout](08-downstream-rollout.md) |
| 7 | Theme bridges, deprecated visual fields, stale documentation | [Legacy removal](09-legacy-removal.md) |

Remove each task when its source change and relevant verification are complete.
Delete its task document when no open items remain. Do not recreate completed
milestones as tasks or infer completion from a passing ratchet with allowlists.
