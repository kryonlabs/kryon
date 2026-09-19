# Completion evidence and remaining ownership

This ledger preserves implementation/verification evidence while completed
plan tasks are removed. It does not replace the full requirements in
`plan/COMPLETION.md`. An inventory row is not a claim that every branch of its
subsystem has been audited.

Historical JavaScript/web entries in this file are archived evidence only. The
old `k2js` and generated web runtime path is paused, excluded from current
default builds, tests, packaged tools, and public status matrices, and must not
be used as current support evidence. Current web direction lives in
`docs/WEB_JS_ROADMAP.md` and `plan/COMPLETION.md`.

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
`make style-pack-source-test`, `make style-release-table-repro-test`, `make style-release-table-emitter-test`, `make style-release-table-import-emitter-test`, `make style-release-startup-test`, `make go-runtime-test`,
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

The main selector list/chain lexer and atom parser now live in `.kry` (below).
The web host still owns functional-pseudo dispatch, selector object composition,
compound matching/traversal, and CSS export interpretation. Specificity weights,
declaration interpretation, and cascade decisions are generated. This is not
proof of an entirely shared KSS implementation.

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

## Shared declarative selector lexer

`KssSelectorPart` streams lists and combinator chains, while `KssSelectorNext`
streams kinds, IDs, classes, attributes, canonical states, and functional pseudo
arguments. They live in `runtime/kss_parser.kry`. The web builder now maps these
tagged results into its selector object; its two handwritten splitters, regex
atom parser, state alias list, and blanket comment stripping are removed.

The lexer retains quoted commas/brackets/braces/URLs, escaped quote boundaries,
and nested functional arguments. Declarative rule capture now skips comments
and escaped quotes consistently. Nested argument selectors use full chain
matching. Empty selector-list pseudo arguments, malformed separators/groups,
unterminated selector comments, unsupported attribute flags, and unquoted
multiword values are rejected. Nesting is bounded at 64 delimiter levels.
CSS identifier escapes and quoted-value escape decoding remain unsupported;
retaining raw escape bytes is not a claim of browser escape conformance.

`tests/fixtures/kss/selector-grammar.tsv` supplies 32 C/Go/JS cases, checking
validity, progress, counts, specificity, and final atom/span values. Web
integration covers quoted URLs and delimiters, comments, nested `:not(:is(...))`,
compound selectors inside `:is`, and malformed input. This does not close
compound-selector Boolean semantics, functional-pseudo dispatch, CSS export
mapping, fact normalization/presence, traversal, or backend conformance.

The C++ syntax gate exposed a pre-existing linkage conflict: `ui_tk.h` declared
C functions with C++ linkage, conflicting with `Menu` in `ui_tree.h`. The header
now declares its C API with `extern "C"`. The C++ test also compiles the public
headers with the generated KSS header, catching both linkage conflicts and
C++-reserved field names. The new selector atom uses `operation`, not the C++
keyword `operator`; the existing attribute predicate's parameter was renamed
accordingly without changing its call contract.

Verification passes: focused C/JS grammar fixtures and integration, Go runtime
(including the matched grammar fixture), `fast-test`, the declared generated
parity subset, Go and C++ syntax, full JS syntax/runtime plus strict KSS and
Chromium DOM/inspector suites, generated provenance, and all five style guards.
Logs are in `/tmp/kryon-selector-grammar/`. The initial aggregate log retains
its C++ linkage failure; `cpp-final.log` and `final-checks.log` record the corrected
passes, with the latter ending `RESULT 0`. Checks affected by the final comment
and header corrections were rerun. Go uses gofmt; the `.kry` formatter ran on a
review copy and unrelated continuation rewrites were inspected and omitted.
No original plan file is fully closed by this lexer migration.

## Shared functional pseudo argument parsing

Web CSS export and structural matching now read pseudo names and balanced
arguments through generated `KssSelectorNext` and `KssPseudoName`. This removes
six independent regular-expression argument parsers. Nested selectors such as
`:has(> Button:not(.quiet))` now export intact; quoted parentheses in attribute
values remain part of the argument. Prebuilt malformed pseudo strings fail
explicitly. CSS pseudo names retain their spelling after shared normalization,
without converting native CSS `active`/`read-only` to internal state aliases.

Focused web tests cover matching and CSS serialization, quoted parentheses,
malformed trailing atoms/delimiters, and normalized names. The Chromium DOM
suite installs a nested selector and checks its computed outline style.
Functional dispatch, relative-selector traversal, compound Boolean semantics,
specificity of functional selectors, and remaining CSS mapping still require
migration or conformance work. This closes no original plan document.

Verification: `make k2js-syntax-test` passes, including cross-module checks,
shared KSS fixtures, strict KSS, and Chromium DOM/inspector execution. All five
style guards and `git diff --check` pass. No generated source changes were
needed: these adapters now call the existing generated lexer.

## Independent functional selector groups

Parsed web selectors now retain each `:is`, `:where`, and `:not` occurrence as
its own group. Previously repeated positive groups were flattened, so
`Button:is(.a):is(.b)` incorrectly matched a button with only `.a`. Matching
now requires each group to hold. `KssSelectorGroupMatches` in maintained
`runtime/kss_parser.kry` owns positive/negative alternative reduction, rejecting
empty groups, impossible counts, and unknown group names. Hosts supply match
counts and retain the nested selector objects. Generated C/Go/JS outputs all
execute the same 13 new predicate cases (53 combined predicate cases).

CSS export preserves repeated groups, list boundaries, and `:where` spelling.
Unmatched-class inspection traverses groups and negative alternatives. Web
integration covers repeated positive/negative groups, nested groups, and CSS
serialization; browser coverage ensures repeated groups with an unsatisfied
condition cannot override an installed matching rule. The existing prebuilt
`matches`/`not` fields remain supported; parsed selectors no longer fill them.

This does not close functional specificity, repeated ID/attribute storage,
combinator backtracking, relative-selector traversal, or the complete compound
matching migration. Native typed selector conformance remains separate from
execution of the shared group predicate. No original plan is fully closed.

Verification: matched C/Go/JS predicates, `fast-test`, generated-runtime parity,
Go runtime, C++/Go/JS syntax, strict KSS, both Chromium DOM/inspector suites,
generated provenance, and all five style guards pass. The detached suite at
`/tmp/kryon-selector-groups/verify.log` ends with `RESULT 0`. Go uses gofmt;
the Kry formatter ran on a review copy, with unrelated continuation indentation
rewrites inspected and omitted. Generated outputs were regenerated normally.

## Repeated ID and attribute conditions

Parsed web selectors now retain ordered `ids` and `attributes` arrays. The
previous scalar ID and attribute maps silently overwrote earlier constraints:
`#missing#save` behaved as `#save`, and `[title="wrong"][title]` lost the equality
condition. Matching and CSS export now consume all conditions. Existing
`id`/`attrs`/`attrOps` fields remain last-value summaries; prebuilt objects without
the ordered arrays retain their original behavior. Summary operator entries
are also cleared when a later presence condition replaces their value.

`KssIdentityFacts` and `KssIdentityMatches` in `runtime/kss_parser.kry` own the
case-sensitive ID/name/key alternative comparison, with empty queries rejected.
Eight new shared C/Go/JS fact cases cover each alternative, missing/mismatching
facts, case sensitivity, Unicode, and an empty query (63 total fact cases).
Web integration covers conflicting equalities, prefix/suffix intersections,
presence combined with equality, repeated IDs, distinct identity aliases,
traces, serialization, and the existing prebuilt-map form. Browser coverage
ensures failed earlier conditions cannot override a matching installed rule.

This closes the repeated-condition storage defect, not the entire compound
selector migration. Native/data/ARIA presence normalization, functional
specificity, combinator backtracking, and traversal policy remain open.
No original plan document is fully closed by this change.

Follow-up probe confirms an existing chain-matching defect: for an `.outer`
parent with two nested `.branch` descendants and a `.leaf` beneath them,
`.outer > .branch .leaf` resolves to no style, even though the outer branch
satisfies the full chain. The greedy nearest-ancestor selection does not
backtrack after a later condition fails. This remains an explicit open task;
the repeated-condition fix does not claim complete combinator matching.

Verification: a fresh detached run in `build/selector-conditions/verify.log`
ends with `RESULT 0`, covering generation, matched C/Go/JS fixtures,
`fast-test`, generated-runtime parity, Go runtime, C++/Go/JS syntax, strict KSS,
Chromium DOM/inspector, generated provenance, and all five style guards. The
prior interrupted run's temporary logs disappeared; this run supplies complete
replacement evidence. The Go fixture reader handles the generated `ID` acronym.
Go uses gofmt; the Kry formatter ran on a review copy, with unrelated existing
continuation-indent rewrites inspected and omitted. Generated sources were
regenerated normally, and `git diff --check` passes.

## Shared selector chain traversal and backtracking

The previously recorded `.outer > .branch .leaf` failure is fixed.
`KssSelectorChainBegin` and `KssSelectorChainStep` in `runtime/kss_parser.kry`
own chain traversal decisions. The generated driver distinguishes immediate
child/adjacent-sibling relations from descendant/general-sibling alternatives,
and retries alternatives when a later condition fails. The web host only
retains node references/frame storage, supplies parent/previous-sibling indices,
and evaluates simple selectors. Its greedy combinator loop is removed.

`selector-chains.tsv` supplies 17 shared C/Go/JS graph cases: positive/negative
matches, child/adjacent no-skip rules, descendant/general-sibling skipping,
ancestor and sibling backtracking, mixed relationships, missing nodes, empty
chains, and default/unknown relation bytes. Fixture graphs supply parent,
previous-sibling, and simple-part match masks; the same generated driver
executes on each backend. Web integration repeats the original failure and
the analogous sibling failure, and exercises a 70-part chain with dynamic
frame storage. Browser coverage checks runtime traces and actual exported CSS
on both ancestor and sibling trees.

The driver assumes valid acyclic host relationships. This does not expand the
closed native typed selector surface or finish relative `:has` discovery,
functional specificity/dispatch, native/data/ARIA normalization, or CSS mapping.
Those requirements remain open; no original plan is fully closed here.

A follow-up relative-selector probe remains failing: `Column:has(> .branch
.leaf)` does not match a Column with a direct `.branch` child containing a
`.leaf` grandchild. The current host `:has` driver tests only direct children
as final candidates after stripping `>`, so it cannot evaluate that full
relative chain. This is separate from the ordinary chain backtracking fixed
here and is the next concrete traversal case to migrate.

Verification passes: generation, matched C/Go/JS traversal fixtures,
`fast-test`, generated-runtime parity, Go runtime, C++/Go/JS syntax, strict KSS,
Chromium DOM/inspector, generated provenance, and all five style guards. Logs
live under `build/selector-chains/`. The initial aggregate log retains two
test errors: unqualified Go enum constants and a self-parented browser fixture
root. The fixture now uses generated Go enum names and an empty root parent;
`go-runtime-final.log` and `browser-final.log` record the successful reruns.
The production traversal code did not change during those corrections.
Go uses gofmt; the Kry formatter ran on a review copy, with unrelated existing
continuation-indent changes inspected and omitted. `git diff --check` passes.

## Shared relative selectors and anchored `:has` matching

The recorded `Column:has(> .branch .leaf)` failure is fixed. Relative arguments
now use `KssRelativeSelectorPart`, including leading comments and an implicit
descendant relationship. Ordinary sequence parsing rejects leading combinators.
`KssRelativeSelectorBegin` adds the subject's stable index to chain frames;
`KssSelectorChainStep` accepts only after the first relationship reaches that
anchor. The web host enumerates frame candidates and supplies facts, replacing
its separate child/descendant/following-sibling candidate-selection rules.
This also prevents an unrelated ancestor from satisfying a relative argument.
Node-path identity keeps copied subjects consistent with their frame nodes.

The shared lexer now rejects nested `:has`, including inside other functional
arguments, while skipping quoted values, attributes, and comments. The old
JavaScript substring check incorrectly rejected literal `:has(` text in an
attribute; it is removed along with the CSS-export leading-combinator regex
and the unused descendant-discovery helper.

Shared fixtures now contain 42 grammar cases and 27 traversal cases. Added
cases cover relative prefixes/comments, ordinary-prefix rejection, quoted and
nested `:has`, child/descendant/sibling relative chains, anchor boundaries,
missing anchors, self/previous-node rejection, and empty relative chains.
Web integration checks the original failure, following-sibling descendants,
selector lists, subject copies, literal/comment text, exported selectors, and
negative cases. Chromium coverage compares runtime traces and exported CSS
for child-relative and sibling-relative chains on mounted nodes.

Remaining KSS work includes functional-pseudo dispatch, specificity conformance,
native/data/ARIA fact normalization, and CSS mappings. The typed native selector
surface is not expanded by this generated traversal helper. No original plan
document is fully closed by this change.

Verification: `build/selector-relative/verify.log` ends with `RESULT 0`.
Generation, matched C/Go/JS fixtures, `fast-test`, generated-runtime parity,
Go runtime, C++/Go/JS syntax, strict KSS, Chromium DOM/inspector, generated
provenance, and all five style guards pass. Go uses gofmt; the Kry formatter
ran on a review copy, with unrelated existing continuation-indent rewrites
inspected and omitted. Generated outputs were regenerated from maintained
sources. `git diff --check` passes.

## Shared structural and positional pseudo dispatch

`KssPseudoMatch` in `runtime/kss_parser.kry` now owns the choice between basic
structural predicates and the four nth-position forms. It selects sibling
versus same-type indices and forward versus reverse counting, rejects unknown
or mismatched forms, and returns a distinct request for relative `:has`
traversal. The web adapter supplies observations and acts on that decision;
its positional branch chain and forwarding helper are removed.

Twenty-two new shared C/Go/JS fact cases bring `selector-facts.tsv` to 85 rows.
They check structural/functional form gating, unknown forms, traversal requests,
all four nth forms, positive/negative matches, formulas, missing sibling facts,
and unsupported `of` syntax. Existing frame-node integration covers all four
forms together; extra negative rules prove that missing arguments, functional
basic pseudos, and unknown functional names do not override a valid rule.

This closes the host positional-dispatch duplication. Specificity conformance,
remaining compound/fact semantics, and CSS mappings remain open, alongside the
other completion-plan phases. No original plan document is fully closed.

Verification: `build/selector-dispatch/verify.log` ends with `RESULT 0`.
Generation, matched C/Go/JS fixtures, `fast-test`, generated-runtime parity,
Go runtime, C++/Go/JS syntax, strict KSS, Chromium DOM/inspector, generated
provenance, and all five style guards pass. Go uses gofmt; the Kry formatter
ran on a review copy, with unrelated continuation-indent rewrites inspected
and omitted. Generated outputs were regenerated normally; `git diff --check`
passes.

A follow-up CSS-export probe identifies the next concrete mapping defect:
`Button:nth-child(1.5)` exports as `:nth-child(15)`. Runtime matching correctly
rejects the fractional formula, but the host CSS argument sanitizer removes
the decimal point and changes its meaning. This remains open for shared
formula validation/serialization; the dispatch migration does not fix it.

## Shared positional formula validation and CSS serialization

The previously recorded `:nth-child(1.5)` export defect is fixed. Shared
`KssParseNth` returns validated coefficients and `KssNthText` emits canonical
CSS text. Matching consumes the same parsed formula. The web adapter uses the
shared functional classification and emits `:not(*)` for invalid positional
predicates, preserving runtime rejection inside positive and negated selectors.
It no longer strips punctuation from positional arguments. KSS comments are
normalized away before CSS output, including line comments unsupported by CSS.
Unterminated block comments are rejected rather than accepted as trailing space.

Thirty shared C/Go/JS fixture rows cover coefficients, offsets, odd/even, case,
whitespace, comments, zero/negative integers, overflow, unsupported syntax, and
canonical output. Web integration covers all four nth forms and nested negation.
The Chromium fixture compares runtime traces with actual computed styles on
15 sibling buttons after removing inline styles, including the formerly
misinterpreted fractional input and a valid commented formula.

The completion plan now explicitly assigns all KSS language decisions,
including CSS conversion and text serialization, to maintained `.kry`. Its
remaining migration sequence names current host functions and exit evidence.
Generic functional argument sanitization, attribute/fact normalization, CSS
state/property mappings, compound matching, specificity conformance, and
remaining tooling policy are still open. No original plan is fully closed.

Verification: `build/nth-export-final/verify.log` ends with `RESULT 0`.
Generation, matched C/Go/JS fixtures, `fast-test`, generated-runtime parity,
Go runtime, C++/Go/JS syntax, strict KSS, Chromium DOM/inspector, generated
provenance, and all five style guards pass. The earlier `build/nth-export/`
run was stopped when canonical comment serialization was added; its incomplete
log is not passing evidence. Go uses gofmt. The Kry formatter ran on a review
copy; unrelated continuation-indentation rewrites were inspected and omitted.
`git diff --check` passes. All 26 original plans remain linked and present.

## Shared CSS property vocabulary, units, and border classification

`KssCSSPropertyName` reuses the shared declaration vocabulary and a small alias
resolver, replacing the browser's duplicate 424-entry map. It rejects unknown
and noncanonical ordinary names, preserves full custom property names, and
leaves composite-only fields to their existing emitters. `KssCSSNeedsPixels`
replaces the host unitless-property branch chain for both inline styles and CSS
export. `KssCSSBorderShorthand` replaces the host trim/whitespace regular
expression, including its Unicode whitespace behavior.

The shared `css-properties.tsv` fixture has 512 rows executed by C, Go, and JS:
every existing map entry, all prior unitless names, DOM camel-case spellings,
unknown/composite names, a long custom property, and whitespace boundaries.
Integration coverage checks ordinary rules and keyframes. The browser fixture
checks aliases, border shorthand, dimensional and unitless values, and explicit
zero in computed styles, then removes inline styles to verify exported rules.

This retains the existing adapter's numeric-unit and border interpretation
contract. It does not establish complete CSS value conformance. Composite
expansion, effect generation, state/attribute mappings, remaining functional
sanitization, and other plan phases stay open. No original plan is deleted.

A follow-up probe identifies a concrete remaining composite-emission defect:
`Button { padding-x: 3; }` exports both left and right padding, while
`@keyframes probe { from { padding-x: 3; } }` exports only left padding.
The ordinary-rule host has special axis expansion that the keyframe host lacks.
Shared declaration expansion is the next action; the vocabulary migration does
not claim to fix this mismatch.

Verification: `build/css-properties/verify.log` ends with `RESULT 0`.
Generation, matched C/Go/JS fixtures, `fast-test`, generated-runtime parity,
Go runtime, C++/Go/JS syntax, strict KSS, Chromium DOM/inspector, generated
provenance, and all five style guards pass. Browser verification checks inline
values before installing the stylesheet, then checks exported CSS after removing
inline styles. Go uses gofmt; the Kry formatter ran on a review copy, and its
unrelated continuation-indentation rewrites were inspected and omitted.
`git diff --check` passes.

## Shared declaration expansion for rules and keyframes

The recorded `padding-x` keyframe mismatch is fixed. `KssCSSExpandDeclaration`
now decides paired padding/margin expansion, content offsets, icon-size and
background-end storage, border shorthand, and deferred offset/transform output.
`KssCSSEffectAt` owns gradient construction, offset variables, and composed
transform recipes. Outputs borrow their value fragments rather than imposing
an output-buffer length limit. `webStyleDeclarationLines` supplies values and
emits those decisions for both ordinary rules and keyframes; the separate
host axis/effect branch chain and single-property keyframe emitter are removed.

Twenty-six expansion and twelve effect fixture rows execute on C, Go, and JS.
They cover both sides of all axes, explicit zero, absent values, unsupported
properties, aliases, deferred transforms, missing gradient endpoints, one-axis
offsets, composed transforms, and invalid effect indices. Integration compares
complete ordinary-rule and keyframe declarations, including content offsets
and icon size. Chromium drives a paused animation to both endpoints and checks
all padding/margin sides, offsets with scale, and gradient presence.

Remaining work includes inline composite application and precedence, selector
and state/attribute CSS mappings, specificity conformance, and the other plan
phases. Inline currently resets a missing offset axis to zero while exported
CSS omits that custom declaration and uses the transform's variable fallback;
reconcile this deliberately when sharing inline emission. Inline also applies
an explicit `background-image` after constructing its gradient, whereas CSS
export appends the gradient after ordinary declarations. Resolve and test that
precedence divergence during inline migration. The DOM style handoff now makes
clear that browser-only availability still uses shared `.kry` implementation.
No original plan is fully closed by this change.

Verification: `build/css-expansion/verify.log` ends with `RESULT 0`.
Generation, matched C/Go/JS fixtures, `fast-test`, generated-runtime parity,
Go runtime, C++/Go/JS syntax, strict KSS, Chromium DOM/inspector, generated
provenance, and all five style guards pass. The browser animation is paused
and explicitly sampled at 0 and 10000 ms, verifying both authored endpoints.
Go uses gofmt; the Kry formatter ran on a review copy, with unrelated
continuation-indentation rewrites inspected and omitted. `git diff --check`
passes. Original plan documents remain because required work is still open.

## Shared inline and exported declaration application

`applyResolvedWebStyle` now consumes the declaration stream used by ordinary
rules and keyframes. Its roughly 470-line property/precedence/effect mapping is
removed. The remaining DOM adapter maps names to DOM access, writes custom
properties with `setProperty`, clears tracked styles, and reapplies explicit
mounted inline overrides. Plain-object test hosts retain ordinary JavaScript
property spelling. An initial focused test exposed the fake DOM's lack of
CSS-name/camel-case reflection; the sink now uses standard ordinary DOM property
access while reserving custom-property methods for `--` names.

Shared declaration expansion also owns the former inline-only WebKit fallbacks
for line-clamp and box-decoration-break. `KssCSSBorderDefault` emits `solid`
before authored declarations when a nonzero border width or border paint
requests it, allowing explicit whole-border and per-side styles to override.
Zero alone does not request that default. Explicit background-image suppresses
the generated gradient in shared effect policy. Missing offset axes no longer
receive an inline-only zero reset: custom values can cascade, with the shared
transform's zero fallback used when absent. Inline ordinary declarations now
follow the resolved object's order, matching export for the same declaration set.

Shared fixtures cover 28 expansions, 14 effects, and 8 border-default decisions.
The browser test compares independent inline and exported snapshots for reversed
alias/padding order, border defaults and explicit side styles, gradient override,
custom offset axes, and zero values. It reuses the same DOM element across cases
to verify removal of stale gradients, borders, offsets, and transforms.

This does not establish equivalence for aliases/shorthands competing across
multiple rules: the resolver's per-property winners and object insertion order
still need a cascade audit. Selector/fact normalization, specificity, remaining
CSS serialization, other completion phases, and original plans stay open.

A concrete cross-rule alias reproducer remains: `.probe {color:#111111;}`,
then `.probe {foreground:#222222;}`, then `.probe {color:#333333;}`. Resolution
returns `{color:#333333, foreground:#222222}` in first-insertion order, so its
merged declaration stream ends with `#222222`; exported separate rules end with
`#333333`. This is a resolver/cascade-order defect, beyond aligning emission
of the same input object. Carry priorities/declaration order into alias and
shorthand conflict handling rather than restoring a host property table.

Verification: generation, matched C/Go/JS fixtures, `fast-test`, generated-runtime
parity, C++/Go/JS syntax, strict KSS, Chromium DOM/inspector, generated provenance,
and all five style guards pass in `build/css-inline/`. The aggregate log retains
the initial Go fixture compile error (calling a generated runtime method as a
package function); `go-runtime-final.log` records the corrected suite passing.
No production code changed for that correction. Browser checks confirm retained
node cleanup and independent inline/export snapshots. Go uses gofmt; the Kry
formatter ran on a review copy, with unrelated indentation rewrites inspected
and omitted. `git diff --check` passes. The full plan remains incomplete.

## Generated JavaScript is build output

The nine generated browser modules are no longer tracked. `.gitignore` names
those artifacts explicitly; maintained `.kry` sources remain authoritative.
`make generate-web-runtime` supplies them, and public compiler/tool, web test,
provenance, and distribution targets depend on generation. The tools archive
checks every generated module. The reproducibility gate also rejects tracked
generated browser modules, including instance policy.

A fresh source tree materialized from the index had no generated browser files
or compiler binary. `make -j4 generate-web-runtime web-generated-check` rebuilt
the compiler and all nine artifacts; Node then imported the complete browser
runtime successfully. Root provenance/reproducibility checks and the tools
archive check pass. Logs: `build/css-inline/clean-web-build.log`,
`untracked-web-check.log`, and `tools-package.log`. This removes about 39,500
lines of generated JavaScript from Git, not the necessary browser build outputs.

## Website redesign completion (2026-09-19)

Recorded by commits `667a36f0` (implementation) and `d6108fd9` (verification).
The completed website plan was removed on 2026-09-19. The checks below are
preserved historical results, not a new production audit.

- Rebuilt all ten public content/tool pages, with the older examples route
  forwarding its example and artifact parameters to the Playground.
- Generated the approved workshop artwork and a matching social card; exact
  prompts and font licenses are in `docs/site/assets/editorial/`. The original
  generated images are retained with the approved design artifacts.
- Homepage app metadata comes from the existing showcase registry. The example
  image is a real Kryon-owned KRB capture of the downloadable `hello.kry`.
- Verified the exact sample with native C generation, KRB compilation, and the
  SDL native host capture. The quickstart explicitly starts with the KRB subset.
- Built the site in an isolated checkout, including all five JavaScript/WASM
  tool pairs. The first baseline build lacked the raylib header submodule;
  initializing the pinned dependency resolved it without source changes.
- Checked all ten content/tool pages at 390, 768, and 1440 pixels in both themes:
  60 combinations, no document overflow, one primary heading, valid skip link.
- Checked Docs keyboard tabs, showcase filters and missing data, API contents
  filtering and anchors, matrix filtering, mobile menu/Escape, reduced motion,
  theme persistence, compiler outputs, error diagnostics, draft recovery, failed
  compiler downloads, and the older example/output-tab links.
- Corrected the Playground's theme-color decoding (signed bit comparison made
  text invisible), obsolete default sample, incomplete output count, and draft
  loss when switching examples. These are website-tool changes, not runtime APIs.
- Checked generated API links under the Markdown renderer, including stable
  heading IDs, older style-section links, and links to repository documents.
- Validated local assets, internal links/fragments, metadata, scripts, and
  unique IDs. Text palette contrast on the page background is at least 5.05:1
  in light mode and 7.73:1 in dark mode.
- Hero WebP: 210,322 bytes. Both required fonts together: 82,104 bytes.
  The homepage does not load compiler modules; app images load lazily.
- Benchmark data, coverage evidence, paused JS-target status, and KRB subset
  limits remain explicit. No fresh benchmark numbers or broader target support
  are claimed by this redesign.

Local design review artifacts: `waozi-design-proposals/2026-09-19/kryonlabs/`.
Publishing uses the existing `Cloudflare Pages` workflow and project `kryon`.

### Production verification

Published website commit: `667a36f0490c941e1d38bbba04ca6465b98b4704`.
[Successful build and production deployment](https://github.com/kryonlabs/kryon/actions/runs/35451762905).

Verified all ten production routes and their updated metadata, workshop image,
social card, example capture, downloadable source, and compiler binaries.
The public homepage loads the three selected registry projects. The live
Playground compiles the exact homepage example into KIR, C, Go, and KRB, and
its greeting is visible in the preview.

Final desktop and Playground production captures are retained beside the
approved proposal under `kryonlabs/implementation/`.

## Native language plan cleanup (2026-09-19)

The active native plan now lists only unfinished work. This cleanup inspected
master `9094c18f`, current compiler sources/tests and existing execution logs;
it did not rerun the full platform suite. Completed tasks removed from plans
remain recorded here and in the fixed-array ABI contract. Slice implementation
is uncommitted and is not part of the completed milestones below.

- Compiler diagnostics fail closed in native Go. Strict aggregate checks now
  cover malformed shapes, borrowed-slot storage, integer-only indices and
  debug bounds checking on array writes. General slice/callable ownership
  remains part of the language work, not completed by diagnostics.
- Removed obsolete style branches, recorder theme snippets and unused palette
  setters/globals. KSS presence reaches native rasterization without debug
  widget chrome; shared `.kry` policy owns text content fallback.
- Native input, image resources, measured row reuse and host organization are
  committed. Linux input verification runs on a private Xvfb/IBus session.
- Theme role derivation now comes from `.kry`, with shared C/C++/Go fixtures.
  Removed the duplicate C/Go tone and contrast implementations and overwritten
  metric defaults. Live theme catalog callers still require migration.
- Local fixed arrays now have native value semantics: zero initialization,
  positional literals, copies, conditional selection and borrowed callback
  captures. Execution fixtures cover ordering, alias isolation, nested record
  contents and bounds traps on C, C++ and Go.
- Named array bounds normalize across literals, copies and imported record
  fields. The checker rejects invalid sizes and arithmetic overflow before
  target emission. Go constant references and bound names use their emitted
  names consistently.


### Direct fixed-array parameters and returns

Implementation contract and source audit:
[ARRAY_CALL_ABI.md](ARRAY_CALL_ABI.md). Six strict compiler probes on
2026-09-19 established the original rejection on C, C++ and Go. Ordinary
function array calls are now implemented. Array/record execution, all native
syntax suites, 17 generated parity fixtures, runtime/provenance guards and Go
runtime tests pass. See the ABI evidence for revisions and shared-checkout scope.

- [x] Specify value-copy behavior for direct array arguments and results,
  mutation isolation, evaluation order, type identity and imported signatures.
- [x] Define the C/C++ representation and calling convention in KIR/shared
  lowering, then implement matching native Go behavior. Internal representation
  must not leak into app-facing compatibility APIs.
- [x] Support declaration, call, return, assignment of results and forwarding
  through another function. Preserve numeric/named bound equivalence and arrays
  whose elements are supported records, strings or scalar types.
- [x] Diagnose incompatible shapes, unsupported element storage and invalid
  returns at source locations before generating output.
- [x] Execute the same fixtures on C, C++ and Go: caller/callee copy isolation,
  returned local storage, evaluation order with side effects, imported functions,
  boundary indexing and rejected mismatches.

Done when direct arrays can cross function boundaries safely on all three
native targets. Wrapping arrays manually in records or rejecting parameters is
not completion. Local construction/copying and checked bounds already work.


### Previously closed checklist rows removed from the overall plan

These retain the earlier plan's recorded completion status, not new execution
claims from this documentation edit.

- [x] Remove `k2js` and generated JS runtime parity from default `all`, `tools`,
  `test`, and `preflight` gates.

- [x] Remove `k2js` and the generated web runtime from the tools package.

- [x] Remove public green JS status from the website conformance matrix.

- [x] Document the future web target as `.kry -> HTML/DOM + KSS/CSS + small JS`
  in `docs/WEB_JS_ROADMAP.md`.

- [x] Add matched cases for disabled controls, empty data, simultaneous keys,
  release without press, drag cancellation, popup capture, focus loss, and
  nested ownership restoration. Active generated C/Go coverage lives in
  `interaction-policy-matrix-test` and `TestInteractionPolicyMatrix*`; the
  old JS/web leg remains paused.

- [x] Verify all active hosts/tools delegate grammar and shared semantic decisions to
  maintained `.kry` sources. Include all KSS formatting, diagnostics, selector
  serialization, and active target mapping decisions; output sinks remain host
  services. Remove any residual independent implementations discovered by the
  audit, not the thin I/O/generated-code shims.

- [x] Add Go source-retaining theme switching, including registered sources,
  built-ins, variants, repeated switching, and atomic failure recovery.
  C registry lifecycle differences remain tracked in the evidence ledger.
