# Native language and runtime completion

Requested 2026-09-19. JavaScript remains a future target. Shared behavior belongs
in `.kry`; resource storage, Unicode/font services, rasterization, and OS protocol
adapters remain native. Work is performed on upstream master.

## Acceptance checklist

- [ ] Compiler: validate portable aggregate, array, slice, and callable types;
  replace silent Go `any`/comment fallbacks with source diagnostics; exercise
  supported cases and rejected cases across native targets.
- [ ] Styling: retire obsolete theme bridges and widget appearance defaults
  after migrating maintained callers; keep explicit content metrics and genuine
  platform fallbacks documented in the bridge ledger.
- [x] Editable text: benchmark actual row measurement and painting, bound layout
  reuse, preserve Unicode/selection/wrapping behavior, and record measurements.
- [x] Native Go: render asset images with canonical ImageProps; verify existing
  OS IME integration; support useful text-range selection with shared policy.
- [x] Host organization: split the large Go host into focused modules without
  duplicating policy or changing generated API ownership.
- [ ] Validation: native generated-output guards, C/Go/C++ parity, appropriate
  native tests on a virtual display, and current API/architecture/boundary docs.

## Existing functionality to preserve

KSS parsing/cascade already comes from `.kry`. Native Go already implements an
IBus bridge and bounded read-only text and surface raster caches. Reuse these;
do not introduce alternate parsers, image APIs, or compatibility aliases.

Website and accessibility action work is underway independently in this shared
checkout and is outside this change's ownership.

## Completed batches

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

## Active goal and scope

Updated 2026-09-19 after `6e4c2a6f` (named array bounds). Complete the remaining
native language and shared-policy migration so maintained applications can use
portable arrays, slices and callables, obtain styling from KSS, and build against
one upstream implementation with reproducible native verification.

This document defines the current goal. `COMPLETION.md` and the ownership/style
ledgers retain the broader requirement inventory. Reconcile their older rows
against implementation evidence before treating them as new work. An old open
checkbox is not proof that implementation is missing.

The last compiler batch is complete; this goal is not. Completed image, text,
IME, host organization, local array and named-bound work must be preserved.
Do not repeat those migrations or count their passing tests as proof of features
that have not been implemented.

## Milestone 1 — Reconcile the remaining native requirements

- [ ] Review remaining native rows in `COMPLETION.md`, `OWNERSHIP_LEDGER.md`,
  the canonical/style plans, and `DOWNSTREAM_CONSUMERS.md` against current code.
- [ ] Classify each row as verified, implemented but unverified, partial,
  missing, future, or an intentional host service. Record source owner, affected
  target, test/evidence, revision and concrete remaining action.
- [ ] Identify concurrent accessibility/style work by its eventual commits.
  Review its integration once committed; do not absorb unrelated working-tree
  edits or report unfinished concurrent work as delivered.
- [ ] Record required native platform checks separately from unavailable-device
  checks and future web work. Preserve unsupported combinations explicitly.

Done when every active requirement maps to a milestone below or a specific
native follow-up with an acceptance test. This is reconciliation, not an excuse
to restart completed parser, editor, image or naming work.

## Milestone 2 — Direct fixed-array parameters and returns

Implementation contract and source audit:
[`ARRAY_CALL_ABI.md`](ARRAY_CALL_ABI.md). Six strict compiler probes on
2026-09-19 established the original rejection on C, C++ and Go. Ordinary
function array calls are now implemented. Array/record execution and C/C++
syntax checks pass; concurrent slider source/generated-host mismatches currently
block final runtime generation and Go integration (see the ABI evidence).

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

## Milestone 3 — Slice semantics and safe storage lifetimes

- [ ] Write the language contract first: element type, length, valid range,
  empty slices, view versus copy, mutability and borrowed storage lifetime.
- [ ] Define ownership/escape checks for views of locals, parameters, returned
  values and captured values. Choose a bounded supported contract; do not imply
  a general allocator or garbage collector merely by introducing slices.
- [ ] Implement slice construction, indexing, length and the parameter/return
  cases permitted by that contract across KIR, C, C++ and Go.
- [ ] Keep unsupported owning/resizing operations explicitly diagnosed until
  their storage model exists; document these limits in the language spec.
- [ ] Add matching execution and rejection fixtures for empty ranges, first/
  last elements, negative/out-of-range access, mutation aliasing, expired local
  storage and escaping borrowed values.

Done when the documented slice subset is usable across native targets without
silently relying on Go lifetimes that C/C++ cannot provide. Diagnostics alone
do not implement slices.

## Milestone 4 — Callable values beyond synchronous borrowed slots

- [ ] Specify typed arguments/results, capture mode, permitted storage and
  invocation lifetime. Preserve the existing synchronous borrowed `#slot` API.
- [ ] Define how captured storage is retained/released for any permitted escape;
  borrowed references must not outlive their source. Document unsupported forms.
- [ ] Implement callable signatures, invocation and the documented capture/
  storage/return cases with equivalent C/C++/Go lowering.
- [ ] Execute fixtures for argument/result typing, independent captures,
  mutation visibility, nested calls, lifetimes and cleanup. Add negative cases
  for escaping borrows and incompatible signatures.

Done when the specified extension supports real callable values and safely
rejects invalid lifetimes. Do not close this milestone by only improving errors
for every form beyond today's borrowed slots.

## Milestone 5 — Finish KSS ownership and migrate live theme callers

- [ ] Refresh the real caller inventory for `ApplyCurrentTheme`, global palette
  getters and style bridges, beginning with Kryon initialization and maintained
  Uku, Krait and Rill callers. Use actual source and `.gitmodules` evidence.
- [ ] Complete reusable KSS overlay/theme operations upstream where migration
  requires them. Keep parser, formatter, cascade and reusable appearance
  decisions in maintained `.kry`; these already have shared implementations.
- [ ] Move remaining duplicated native appearance decisions identified by the
  audit to shared `.kry`/authored KSS. Preserve explicit zero, transparency,
  content visibility, theme switching and structural text metrics.
- [ ] Migrate affected application callers and assets, checking theme choice
  persistence and pack availability. Inbe needs only integration regression
  checks if affected; its accepted UI does not need another redesign.
- [ ] Commit upstream first, then update clean downstream submodule pointers.
  Never edit `vendor/*`; keep Kapsule product behavior in Kapsule.
- [ ] Delete the obsolete catalog/getter implementations and scanner allowances
  after every maintained caller and Kryon initialization have migrated.
- [ ] Update the style bridge and KSS host-service ledgers with actual removals
  and justified surviving host services.

Done when no maintained caller needs the removed theme contract, style changes
work on affected native paths, and no duplicate appearance policy replaces it.
OS integration, storage, font measurement and rasterization remain native.

### Confirmed downstream migration size (2026-09-19)

A fresh scan of `src/` C/header/`.kry` sources found the following matching lines
for `ApplyCurrentTheme` and retained or historical palette getters. These are
source matches, including generated-source snippets and evaluator dispatch;
they are not a count of distinct APIs or proof of a successful app build.

| Consumer revision | Matching lines | Files | Main migration surfaces |
|---|---:|---:|---|
| Uku `0f7f0d64396c` | 219 | 3 | `src/main.c`, `src/dashboard_empty.kry`, `src/app_chrome.kry` |
| Krait `16a80cc3a4dc` | 134 | 11 | Native engine/level/live UI, live evaluator and scaffold templates |
| Rill `5a1494d29f66` | 69 | 2 | `src/main.c`, `src/rill_x11.c` |

This disproves treating theme removal as deleting a few startup calls. Krait's
emitted scaffolds and evaluator must migrate along with its visible UI. Some
apps still mention already-removed getters such as hover/icon/link; updating a
submodule pointer alone cannot establish compatibility. Scan other maintained
consumers before closing the full inventory. No downstream source was modified
by this audit.

## Milestone 6 — Close native tooling and integration gaps found by the audit

- [ ] For still-open native style rows, establish evidence for pack imports,
  variants/themes, provenance, no-style behavior and constrained-renderer
  degradation. Separate style resolution from rendered correctness.
- [ ] Verify existing formatter, diagnostics, inspector/source-navigation,
  release-table generation and hot reload against their declared native
  contracts. Implement only demonstrated missing requirements from the ledger;
  do not rebuild existing features because an older task was left open.
- [ ] Audit generated artifacts and source ownership. Keep reproducible build
  output out of tracked application sources; document any required checked-in
  generated distribution artifacts and validate them from their generators.
  Never hand-edit generated files to repair the source behavior.
- [ ] Build affected examples/apps and verify assets, startup, input, clipping,
  style switching and supported reload. Record upstream/downstream revisions.
- [ ] Verify supported affected platforms with their actual toolchains/runtime
  where available. A Linux pass cannot close Android, Plan 9 or another platform;
  record unavailable prerequisites precisely and leave those rows unverified.

Done when each required native integration/tooling row has evidence or an
explicit unresolved prerequisite. An unresolved required row prevents claiming
full completion; unrelated future features do not expand this goal.

## Milestone 7 — Final verification and documentation closure

- [ ] Regenerate active native runtime output and review the resulting changes.
- [ ] Run compiler behavior/rejection fixtures and C/C++/Go syntax suites for
  the completed language contracts, plus generated-runtime parity and native
  Go runtime tests.
- [ ] Run public API, ownership, provenance, boundary, style and capability
  guards. Review remaining allowances rather than equating a pass with no debt.
- [ ] Run relevant native interaction/render/IME checks on a private virtual
  display. Preserve the user's real desktop session.
- [ ] Update language spec/implementation status, API, architecture, boundaries,
  feature matrix and plan ledgers to match the delivered behavior and limits.
- [ ] Attach commit IDs and exact check results to each milestone. Reconcile
  older completed tasks and retain their evidence before removing stale plans.
- [ ] Commit/push only owned upstream changes on master and record any required
  downstream commits. Leave vendor trees pristine.

Done when every required milestone and acceptance row is closed with evidence
at the delivered revisions, with no silent fallbacks, unexplained duplicate
policy, deprecated maintained callers or hidden required verification gaps.

## Verification entry points

Use focused tests after each semantic change. The final integration pass should
include these existing entry points, plus feature-specific fixtures added for
arrays, slices and callables. This list is a plan, not a new test-run claim.

```sh
make generate-native-runtime
make k2c-syntax-test k2cpp-syntax-test k2go-syntax-test
sh tests/record_values_test.sh build/linux-x86_64/bin
make generated-runtime-parity-test runtime-parity-check
make go-runtime-test
make preflight
make kss-parser-test kss-matched-test kss-formatter-test
make style-sheet-policy-test style-pack-source-test
make style-release-table-repro-test style-release-startup-test
make test
git diff --check
```

Run GUI-dependent checks under Xvfb/private input services, with detached logs
for long builds. Record unavailable prerequisites as unverified. Broaden tests
when new changes or failures justify it; do not rerun the whole suite after
unrelated documentation edits.

## Explicit future work and completion boundary

JavaScript/web remains paused. HTML/DOM + KSS/CSS + minimal JS transpilation,
DOM/browser contract completion and browser-only CSS export work belong to
`docs/WEB_JS_ROADMAP.md`. Do not restore web support claims or include paused
JS parity gates in this goal.

Full general-purpose compile-time evaluation, unrestricted owning/resizable
containers, arbitrary asynchronous closures and unrelated application features
are not implied by the bounded language milestones above. Any further extension
needs its own explicit contract and acceptance criteria. The goal is to finish
this native migration honestly, not to claim that Kryon will have no future work.
