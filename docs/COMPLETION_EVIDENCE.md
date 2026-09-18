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

The web host still owns selector parsing in `splitSelectorSequence`,
`splitSelectorList`, `parseSimpleSelector`, and `parseSelector`, and selector
matching/traversal in its DOM-fact helpers. Specificity weights, declaration
interpretation, and cascade winner decisions have moved into `.kry` (below).
This is not proof of an entirely shared KSS implementation.

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

## Shared cascade priority and specificity

`runtime/style_sheet.kry` now owns the selector weights used by both native and
web paths, plus layer/specificity/order winner comparisons. Web resolution and
inspector traces use that generated policy for parsed and prebuilt rules.
Native field cascades retain `StylePriority` tuples instead of scalar scores.
The obsolete host layer-name table and scalar winner comparisons were removed.

The scalar priority encoding had a confirmed collision: a base-layer selector
with 51 repeated matching classes beat a components-layer rule. Direct tuple
comparison fixes that case and source-order spill into specificity. Equal
priorities still choose the later visited rule. `score` remains diagnostic web
metadata and cannot override a rule's priority fields.

The C/Go/JS fixture `tests/fixtures/kss/cascade.txt` covers weights, priority
tiers, ties, absence, and spill cases. Native resolution and web resolution/trace
integration tests verify actual per-field winners. Web tests also cover prebuilt
rules without scalar scores and equal priorities across sheets. Remaining web
selector parsing and matching are not closed by this change.

Focused native policy and web integration tests pass. The broader run has
passed `kss-matched-test`, `fast-test`, the declared generated-runtime parity
subset, `go-runtime-test`, and `k2go-syntax-test`. All five style guards pass.
Logs are under `/tmp/kryon-cascade/`. The `.kry` formatter ran on a review copy;
its unrelated continuation-indentation changes were inspected and omitted.
Generated Go and the new Go test are gofmt-formatted.

Remaining web selector ownership is more than its parser: `selectorKindMatches`,
`selectorMatchesFacts`, functional pseudo dispatch, and combinator traversal rules
still execute in JavaScript. DOM/frame relationship lookup, attribute retrieval,
route reading, and property-map storage are host services; predicate/operator
semantics and traversal decisions need shared `.kry` owners. Preserve support for
attribute operators, state aliases, selector lists, combinators, `:not`/`:is`/
`:where`, and structural pseudos while migrating. Add matched fixtures for these
rather than treating the current browser smoke tests as full language coverage.

The complete verification run finished with `RESULT 0`: `k2js-syntax-test`
includes the KSS suites and both Chromium DOM/inspector suites, and generated
provenance checks pass against the maintained `.kry` sources. No original plan
file is fully closed by this cascade slice.

## Shared selector predicates

Attribute operator matching and `nth-*` formulas now live in
`runtime/kss_parser.kry`. JavaScript supplies strings and sibling indices/counts;
its operator switch, numeric coercion, and `an+b` regex have been removed.
Reverse indexing and absent-sibling rejection also use generated decisions.
The existing selector grammar/pseudo dispatch and sibling retrieval remain in
the web host; this does not claim completion of selector migration.

`tests/fixtures/kss/selector-predicates.tsv` drives 40 identical C/Go/JS cases:
attribute words/prefixes/suffixes/substrings/dash matches, Unicode, empty operands,
missing values, invalid operators, integer and positive/negative/zero-step formulas, invalid
number spellings, oversized input, and arithmetic beyond signed 32-bit deltas.
Web integration tests exercise forward/reverse and same-type sibling matching
through the actual adapters and verify per-property winners.

Two host-specific behaviors are deliberately corrected: empty substring-style
attribute operands no longer match every string, and JavaScript numeric forms
such as `0x1`, `1e0`, and `1.0` are no longer accepted as nth positions. The shared
formula parser bounds coefficient/offset magnitudes and uses i64 intermediates.

The adapter also preserves missing values for shared rejection: an absent
`data-*` attribute no longer matches an empty equality operand. Native attribute
fact normalization still needs its separate presence audit.

Verification passes: focused C/Go/JS predicates and web integration;
`fast-test`; the declared generated-runtime parity subset; Go runtime;
`k2js-syntax-test` including strict KSS and Chromium DOM/inspector suites;
generated provenance; and all five style guards. Checks affected by the later
presence-parameter correction were rerun successfully, including the final
missing-value fixture. Logs are in `/tmp/kryon-selectors/`; both verification
scripts finish with `RESULT 0`. Go was formatted with gofmt. The `.kry` formatter
ran on a review copy; unrelated continuation rewrites were inspected and omitted.
No original plan file is fully closed by this predicate migration.

## Shared state and structural predicates

`KssStateFacts` and `KssStructuralFacts` in `runtime/kss_parser.kry` now drive
state aliases, form-control classification, normal/enabled/required/read-only,
validity and placeholder decisions, plus root/scope/child/type/content/focus/
target structural matching. The browser adapter supplies observations and looks
up custom state flags using the shared name normalizer. It collects structural
facts once per selector/node match. Its former state switch, form-control list,
and basic structural branches have been removed.

`tests/fixtures/kss/selector-facts.tsv` drives 55 matched C/Go/JS cases. Actual
web frame-node integration covers uppercase state queries, custom flags,
read-only/required/valid/placeholder behavior, first/last and same-type siblings,
empty content, scope, and focus-within. State names follow shared ASCII folding
and a 64-byte bound; `ANY` now follows the same normalization as other names.

Remaining host-owned work includes selector grammar and parser alias handling,
compound-selector combination, native/data/ARIA fact normalization and presence,
`:has`/functional-pseudo dispatch, combinator traversal, and focus/route discovery.
This slice does not close the entire KSS language or any original plan document.

These fixtures execute the generated predicates directly; they do not establish
complete conformance between typed native selectors and the declarative web
selector surface. That broader backend comparison remains in the plan.

Verification: focused fixtures/integration, `kss-matched-test`, `fast-test`, the
declared generated-runtime parity subset, Go runtime, full `k2js-syntax-test`
(including strict KSS and both Chromium DOM/inspector suites), generated
provenance, and all five style guards pass. `/tmp/kryon-selector-state/verify.log`
ends with `RESULT 0`. The first focused Go attempt hit a sandbox read-only cache;
the permitted rerun and full suite passed. Go uses gofmt; the `.kry` formatter
ran on a review copy and unrelated continuation rewrites were inspected and
omitted. No original plan file is fully closed by this migration.
