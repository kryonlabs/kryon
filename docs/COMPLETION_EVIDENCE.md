# Completion evidence and remaining ownership

This ledger preserves implementation/verification evidence while completed
plan tasks are removed. It does not replace the full requirements in
`plan/COMPLETION.md`. An inventory row is not a claim that every branch of its
subsystem has been audited.

## 2026-09-18 baseline

Baseline started at master `6b2d56d6`. Runtime generation, generated C/Go/JS
parity (its declared subset), Go syntax, JS snapshots, and Go runtime tests
passed. `fast-test` failed because the public API snapshot omitted the recently
introduced `SetStyleTheme` and `ReapplyBuiltInStyleTheme`; regenerated snapshot
records those existing declarations.

The JS syntax/runtime suite failed at
`tests/k2js_syntax_test_runner.mjs:3800`: mounted Ruby expected its authored
`漢` text but received an empty string. Pre-mount assertions pass. The runtime's
`__kryOwnsChildren` text guard is a lead to investigate; it is not yet a proven
root-cause diagnosis. This is DOM content preservation, not an IME failure.
Because this gate stopped before browser execution, it establishes no browser
pass. Baseline logs are in `/tmp/kryon-completion/baseline.log` for this run.

## Ownership and coverage inventory

| Requirement | Maintained owner / host role | Evidence and next action |
|---|---|---|
| KSS environment names | `runtime/kss_parser.kry:KssEnvironmentWithNames`; C/Go/JS supply strings and platform defaults | Removed native theme-name interpretation and web axis/name construction. Shared `tests/fixtures/kss/environments.txt` drives generated C/Go/JS tests, including defaults, all platforms, case matching, and variants. |
| KSS grammar/overlays | `runtime/kss_parser.kry`; `src/ui/kss_parser.c`, `go/kryon/style_parse.go`, web host handle strings/import storage | Matched fixture covers actual parsed/resolved values. Full residual host semantic inventory still open. |
| Go theme switch | Generated KSS owns parse/overlay decisions; `go/kryon/style_pack.go` owns retained source and published sheet storage | `style_theme_test.go` covers repeated themes, active declared variant, built-in retention, fixed palette retention, late registration, typed replacement, failed imports and recovery. |
| Native theme switch | Generated KSS owns parse/overlay decisions; `src/ui/style_pack_source.c` owns retained source storage | Existing theme source test passes, but code audit finds borrowed theme pointer, non-atomic publication, explicit-palette replay without overrides, and retained sources surviving `ClearStylePacks`. Built-in replay also registers absent packs. Reconcile lifecycle with Go and add matched failure/reset tests before closing. |
| Web executable fixtures | KIR/k2js lowering plus generated runtime policies | Baseline parity excludes menus, scroll_content, drag_drop, composed_popup from JS and labels composition partial. Pointer/wheel lifecycle driver and expression placeholder removal remain open. |
| Mounted DOM content | `web/kryon-runtime.js` DOM element/text emission | Existing Ruby regression fails; fix content coexistence with children and verify real-browser update behavior. |
| Widget/text/visual policy | `runtime/*.kry`; C/Go/JS storage, measurement and painting hosts | Detailed branch-level inventory across files listed in P0 remains open. |
| Style backend conformance | Shared parser/resolver and individual renderer services | Matched resolver evidence exists; complete backend/state/degradation matrix remains open. |
| Downstream/platform coverage | Maintained consumer repositories, clean upstream submodule revisions | Full consumer inventory, platform builds, actual OS IME, and live-host checks remain open. |

The Go registry change is one completed implementation slice. No original
plan document is fully closed by that slice alone; none should be deleted on
that basis. All KSS language and policy changes remain owned by `.kry`.

## Verified implementation slice

After the environment/Go registry changes, `make kss-matched-test`,
`make style-pack-source-test`, `make go-runtime-test`,
`make generated-runtime-parity-test`, and the web KSS strict suite pass.
The shared environment fixture executes on C, Go, and JavaScript. The generated
parity report retains its original exclusions; this slice does not close them.

A subsequent `fast-test` run exposed an existing isolated keyboard fixture
missing the runtime's generated KSS dependency. Its build now generates both
KSS modules from `.kry`, and `make keyboard-policy-test` and `make fast-test`
pass. The formatter was run on a review copy of `kss_parser.kry`; it proposes
existing continuation-indentation/comment churn, so that unrelated rewrite was
not applied. Go changes were formatted with gofmt; `git diff --check` passes.

## Remaining web KSS semantics identified by source audit

The web host still owns selector parsing/specificity in `splitSelectorSequence`,
`splitSelectorList`, `parseSimpleSelector`, and `parseSelector`; and rule priority/winner
comparisons in `parseWebStyleSheet`, `resolveWebStyle`, and `traceWebStyle`.
These are concrete migration candidates, not proof of an entirely shared KSS
implementation. Native `runtime/style_sheet.kry` already owns typed
`StyleRuleScore` and `StyleWins`; the web path duplicates their arithmetic.

Move those common decisions into shared `.kry`, extending its representation
for supported web selectors/declarations as needed. Preserve CSS-only features
through the web export contract; classify actual DOM traversal, native CSS
installation, and string/property storage as host services. Do not close style
plans merely because the shared grammar and matched fixture pass.

## DOM content and web gate repairs

The mounted-parent text bug was confirmed: `applyWebNode` skipped authored text
whenever a node owned children. It now stores that text in a dedicated DOM text
node and preserves child elements through changes, clearing/restoring text, and
transitions between leaf and parent. The fake DOM now distinguishes text nodes
from element children and computes browser-like `textContent`.

Both Chromium DOM/inspector suites pass, including a new Ruby update regression
that checks text order and retained child identity. The full generated runtime
runner passed during the traced syntax run; that run then exposed two stale
shell assertions requiring `copyValue` around native alias calls. Inspected
compiler output uses direct widget calls, so those assertions now match the
current compiler contract. The remaining syntax cases passed in a focused run.
The final uninterrupted `make k2js-syntax-test` run passes, including both web
KSS suites and both Chromium DOM suites. Final Go runtime tests pass outside
the sandbox (its D-Bus fixtures need local Unix sockets); final generated
runtime parity passes its stated subset. No claim of complete DOM or backend parity follows from these repairs.

The provenance gate now regenerates web modules directly from maintained `.kry`
into a temporary directory instead of comparing against potentially stale build
outputs. An isolated negative check modified a copy of `web/kss_formatter.js`;
the gate rejected it. `make generated-provenance-check` passes on the real tree.
All five style ownership/allowlist gates pass; their classified legacy counts
remain debt rather than evidence of completed migration.

## Shared CSS declaration interpretation

CSS property classification, numeric parsing, token lookup, and explicit color
substitution now live in `runtime/kss_parser.kry`. The JS adapter serializes
`KssCSSValue` results and retains per-property token provenance. It resolves
values when the parser yields each rule, so later overlays cannot retroactively
change earlier rules or keyframes. Keyframes use the shared declaration stream;
the host regex declaration parser and property vocabulary have been removed.

`tests/fixtures/kss/css-values.kss` is consumed in C, Go, and JavaScript. It covers
source-order overlays, token origins, quoted delimiters, Unicode, exponent/px
numbers, keyframes, and color substitution. Numeric unknown properties are
rejected. The fixture exposed UTF-16 indexing in structured JS generation;
string indexing and length now use UTF-8 bytes, matching C/Go and StringSlice.

This does not close selector, cascade, native registry, backend, or downstream
requirements. All original task documents remain until their full requirements
have evidence. Verification results for this slice are recorded below.

Focused C parser/matched/formatter tests, the shared web strict fixture, Go
runtime tests, `fast-test`, the existing generated-runtime parity subset,
generated provenance checks, and all five style guards pass. Logs for this
slice are under `/tmp/kryon-kss-declarations/`. The first Go run failed on
incorrect generated enum names in the new test; the corrected rerun passes.
The `.kry` formatter was run on a review copy; its unrelated continuation-indent
rewrites were inspected and omitted. Generated Go and the Go test use gofmt.

The full `k2js-syntax-test` also passes, including the web KSS suite and both
Chromium DOM/inspector suites; `k2js-runtime-snapshot-test` passes. The aggregate
`verify.log` retains its nonzero result from the first Go compile failure;
`go-runtime-retry.log` records the successful corrected run. No original plan
file is fully closed by this slice.
