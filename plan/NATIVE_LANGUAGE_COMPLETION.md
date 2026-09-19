# Native language and runtime remaining work

Updated 2026-09-19 against master `9094c18f` and the slice implementation saved in this checkpoint. This is the remaining-task list, not a list of all historical
work. Completed array, text/IME, image, host-organization and shared-policy
batches are recorded in [completion evidence](../docs/COMPLETION_EVIDENCE.md#native-language-plan-cleanup-2026-09-19).
The fixed-array contract and verification live in [ARRAY_CALL_ABI.md](../docs/ARRAY_CALL_ABI.md).

Slices now execute in C, C++ and Go in the working tree. Their focused tests
pass; broader integration, final contract review and public language documentation
are still outstanding. Callable extensions and downstream theme migration remain
open. No final completion claim applies to the whole goal.

Shared behavior belongs in `.kry`; resource storage, Unicode/font services,
rasterization and OS adapters remain native. Work goes directly to upstream
master before pristine downstream vendor pointers are updated. Preserve Inbe's
accepted UI. JS/web remains a future target.

Milestone numbers are retained for existing references; completed milestone 2
has been removed. Other plan ledgers contain older inventories: verify an open
row before scheduling it, and preserve independently committed work.

The broader proof/law program is planned separately in [law/README.md](law/README.md).
It reuses this work and does not restart completed language or widget migrations.

## Milestone 1 — Reconcile the remaining native requirements

- [ ] Review remaining native rows in `COMPLETION.md`, `OWNERSHIP_LEDGER.md`,
  the canonical/style plans, and `DOWNSTREAM_CONSUMERS.md` against current code.
- [ ] Classify each row as verified, implemented but unverified, partial,
  missing, future, or an intentional host service. Record source owner, affected
  target, test/evidence, revision and concrete remaining action.
- [ ] Verify integration with the independently committed control/slider work
  (`4c709497`) and text work (`9094c18f`), plus later relevant revisions.
  Preserve unrelated working-tree changes.
- [ ] Record required native platform checks separately from unavailable-device
  checks and future web work. Preserve unsupported combinations explicitly.

Done when every active requirement maps to a milestone below or a specific
native follow-up with an acceptance test. This is reconciliation, not an excuse
to restart completed parser, editor, image or naming work.

## Milestone 3 — Finish slice verification and delivery

The implementation and remaining evidence gaps are tracked in
[SLICE_VALUES.md](SLICE_VALUES.md). Range parsing, descriptors, indexing,
mutation, length, rebinding, arguments, returned views and lifetime summaries
are implemented locally; do not schedule them again as missing features.

- [ ] Review lifetime propagation through branches, loops and captures; add
  explicit adversarial fixtures where current coverage is incomplete.
- [ ] Complete the bounds/type rejection matrix, including debug C/C++ runs
  alongside the passing release-mode checks and C-header fallback probes.
- [ ] Rerun affected native syntax, generation, parity and provenance gates on
  the final code. The latest combined run stopped at a stale Go diagnostic
  expectation; that expectation is updated, but the combined rerun is pending.
- [ ] Update the language version/spec, implementation status and relevant
  architecture/boundary documentation with the actual supported contract.
  Document borrowed-only storage and captured-descriptor rebinding limits.
- [ ] Record the final validated delivery revision and results before retiring
  this plan; the current checkpoint is not final slice acceptance.

Done when the documented slice contract has complete native evidence at the
committed revision; the current focused pass alone does not close integration.

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
