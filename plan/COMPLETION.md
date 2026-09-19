# Kryon remaining-work completion plan

Reviewed 2026-09-18 against master `2efbf174`.

This plan consolidates all 26 documents in `plan/canonical`, `plan/style`, and
`plan/dom`. It orders implementation and verification; the linked documents
retain the detailed requirements. This was a document and source review, not a
fresh execution of the test suites. Existing tests are evidence of coverage
intent until rerun at the revision being delivered.

## Findings that change the task list

| Area | Source evidence | Consequence |
|---|---|---|
| Canonical API | Canonical plan records completed naming and release-call migrations; generated policy modules and surface guards exist. | Audit remaining behavior ownership. Do not repeat public renaming or the 34-call release-consumption migration. |
| Web/JS target | The old `k2js` + `web/kryon-runtime.js` path is paused and removed from default builds, preflight/test gates, packaged tools, and the public website matrix. | Do not continue widget-by-widget JS runtime parity work. Future web work belongs in `docs/WEB_JS_ROADMAP.md`: `.kry -> HTML/DOM + KSS/CSS + small JS` rather than a handwritten widget-emulation runtime. |
| Web input | `createRuntime` in `web/kryon-runtime.js` exposes `SubmitTextComposition`, taps, text, keys, shortcuts, queued mouse move/down/up/wheel, and frame-scoped public key/mouse queries covered by `tests/web_input_driver_test.mjs`. | Reuse the driver primitives to connect lifecycle routing/capture decisions to actual shared fixtures. Native DOM pointer handlers alone do not prove generated-runtime parity. |
| Expression lowering | Historical `k2js` placeholder fixes remain in the tree, but JS/web is paused. Active target lowering must still fail visibly for unsupported executable forms. | Add real lowering for supported expression forms in active targets before claiming condition/activation parity. Do not use paused JS syntax coverage as completion evidence. |
| KSS language | Shared parser, formatter, matched fixtures, and generated modules exist; recent commits add variants, provenance, formatter CLI, and theme switching. | Several parser-removal and formatter tasks are stale. Verify routing and close them instead of rebuilding them. |
| Theme switching | `src/ui/style_pack_source.c` implements `SetStyleTheme`; Go registers source packs in `go/kryon/style_pack.go`, but no Go `SetStyleTheme` definition was found. | Complete source retention and theme re-resolution in Go. |
| DOM | Existing `web/kryon-runtime.js` and k2js syntax-runner work are paused experimental evidence. | Treat DOM documents as future web-native contract audits; API existence does not establish current support. |
| DOM names | The DOM plan lists `ColGroup`/`Col`; current test surface uses `TableColumnGroup`/`TableColumn`. | Reconcile with the canonical names. Do not introduce compatibility aliases from stale plan wording. |
| Style allowances | `scripts/check-style-gates.py` classifies legacy theme calls, fallback background, content metrics, and recorder-generated source; its getter list also includes `ui_node_registry.c`, absent from the older summary. | Reinventory current call sites instead of trusting historical counts. A passing ratchet is not completion. |

The existing `plan/dom/` directory was untracked at review time. Its original
requirements are preserved as the DOM planning baseline.

Implementation constraint: keep KSS language and behavior in maintained `.kry`
modules, including CSS-only declaration semantics. Hosts provide storage, I/O, DOM access, and output sinks. KSS serialization
decisions also belong in `.kry`, along with parsing, matching, cascade,
diagnostics, formatting, and CSS conversion. Most maintained runtime behavior
should be `.kry`; generated C/Go volume does not count as hand-written policy. The paused JS path also does not count as maintained policy. Declaration interpretation, cascade comparisons,
specificity weights, attribute operators, nth-position formulas, state predicates,
and basic structural rules now use shared `.kry` policy. Main selector lexing
and atom parsing are also shared, including functional argument parsing in web
matching and CSS export. Selector-list groups retain their boundaries and use
shared positive/negative reduction. Ordered ID and attribute conditions preserve
repeated constraints, with shared identity matching. Chain traversal and
backtracking now use a shared `.kry` driver with host-owned frame storage.
Relative `:has` chains also use shared anchored traversal and prefix parsing.
Structural/positional pseudo dispatch also uses shared policy. Positional
formula validation and canonical CSS text now share `.kry` implementations.
CSS property vocabulary/aliases, numeric units, and border shorthand detection
also use shared code; the duplicate 424-entry browser property map is removed.
Ordinary-rule and keyframe declaration expansion and effect recipes are shared.
Inline composite application now consumes the same declaration stream; its
property table and border/effect decisions are removed. Exact cross-rule
one-property aliases now cascade together in the web resolver (`foreground` /
`color`, color-border aliases, `focus` / `outline-color`, `radius` /
`border-radius`, and `typeface` / `font-family`) with focused web KSS
coverage. Historical JS lowering fixes remain reference material only while the
JS/web target is paused; do not use them as current completion evidence. Broader
computed capacities remain open for active targets where applicable.
Broader shorthand/longhand side precedence remains open. Remaining compound matching,
specificity conformance, and CSS export migration stay open (see the evidence
ledger). Generated C and Go are outputs, not policy owners; paused JavaScript output is reference material only.

## Execution order

1. Establish the requirement, ownership, and coverage inventory (P0).
2. Pause the old JavaScript/web runtime target and record the future web roadmap (P1).
3. Close shared widget, text, and visual policy gaps (P2).
4. Finish DOM compiler metadata and browser contracts (P3).
5. Establish style backend conformance (P4).
6. Finish style authoring and release tooling (P5).
7. Migrate and verify downstream consumers (P6).
8. Remove obsolete bridges and close the plans with evidence (P7).

Dependencies: P1 depends on P0 and stops the old JS parity track. Future DOM compiler work in P3 must coordinate through the web roadmap before reactivating any KIR-to-web compiler path. P2 proves interaction parity on active native/Go surfaces.
P4 consumes P2's visual-policy inventory and P3's DOM facts. P5 inspector
degradation reporting depends on P4; its formatter and Go theme work can start
after P0. P6 follows the relevant upstream slices, not necessarily the entire
tooling backlog. P7 follows verified migration of every affected caller.

## P0 — Establish an actionable baseline

Working ledger: [`plan/OWNERSHIP_LEDGER.md`](OWNERSHIP_LEDGER.md). It is the
current requirement/ownership inventory and remains incomplete until every row
below has authoritative evidence at the delivered revision.

- [ ] Reconcile each linked requirement with current code and tests. Label it
  implemented/unverified, partial, missing, or verified at a named revision.
  Preserve documented unsupported features as unsupported; do not silently
  expand this project to every idea in a design proposal.
- [ ] Create one behavior inventory shared by canonical and style work. Each
  row records requirement, source symbol, `.kry` owner or specific host-service
  justification, affected backends, test, result, and remaining action.
- [ ] Cover `src/ui/ui.c`, `ui_tk.c`, `ui_text.c`, `ui_text_layout.c`,
  `ui_tree.c`, `ui_page.c`, `ui_node_registry.c`, Go runtime/render/style hosts,
  and the web runtime. Inventory branches and defaults, not just public APIs.
- [ ] Record generated-fixture coverage separately for generation, execution,
  state comparison, and rendered-output verification.
- [ ] Run the baseline gates below. Record exact failures and prerequisites;
  unavailable browsers, toolchains, or devices remain unverified.
- [ ] Update stale task statements identified above once their evidence is
  confirmed. Preserve completion evidence before removing any task document.

Exit: every original requirement has an owner and a next action or closure
record. No percentage-complete claim based only on file counts or scanners.

## P1 — Pause the old JavaScript/web runtime target

- [x] Remove `k2js` and generated JS runtime parity from default `all`, `tools`,
  `test`, and `preflight` gates.
- [x] Remove `k2js` and the generated web runtime from the tools package.
- [x] Remove public green JS status from the website conformance matrix.
- [x] Document the future web target as `.kry -> HTML/DOM + KSS/CSS + small JS`
  in `docs/WEB_JS_ROADMAP.md`.
- [ ] When web work resumes, design the web-native compiler path before adding
  more widget-specific JavaScript runtime behavior.

Exit: JS/web is clearly paused as a current target, and future work is tracked
as a roadmap redesign rather than a remaining parity checklist.

## P2 — Complete shared behavior and drawing ownership

- [ ] Audit capture, focus, navigation, owner-reset gates, and retained tree
  target selection around the already-migrated release calls.
- [ ] Move remaining widget decisions and decorative metrics into the owning
  `runtime/*.kry` modules. Regenerate active C/Go outputs and remove duplicated host rules
  only after their callers use the generated implementation.
- [ ] Classify text reflow, line breaking, selection, and retained placement.
  Keep buffer storage, UTF-8 traversal, font measurement, glyph/atlas work,
  image decoding/upload/cache, and OS input as justified host services.
- [x] Add matched cases for disabled controls, empty data, simultaneous keys,
  release without press, drag cancellation, popup capture, focus loss, and
  nested ownership restoration. Active generated C/Go coverage lives in
  `interaction-policy-matrix-test` and `TestInteractionPolicyMatrix*`; the
  old JS/web leg remains paused.
- [ ] Match text cases for wrap boundaries, empty lines, alignment, selection
  across lines, and text/image content in clipped scopes. Reuse the existing
  shared editor rather than introducing a second editor.
- [ ] Extend web buffer-capacity lowering for computed capacity expressions
  where maintained callers require it; fixed module state/global/local arrays
  are supported and recorded.
- [ ] Verify native OS IME candidate windows and browser/platform combinations
  beyond Chromium. Distinguish synthetic composition from actual OS behavior.
- [ ] Audit widget subparts, emitted role/state facts, pack declarations, and
  no-style rendering together. Preserve explicit zero and canonical
  `Text(TextProps)` / `Image(ImageProps)` content properties.

Exit: every retained behavior has a generated owner or a concrete host-service
reason, with affected-backend evidence. Game2D physics/audio/scene behavior and
Kapsule's terminal product behavior stay outside this migration.

## P3 — Complete the DOM contract from compiler to browser

- [ ] Inventory every compiler DOM-producing expression and runtime `widget`
  call. Classify compiler-owned metadata versus justified runtime fallback.
- [ ] Cover named/anonymous blocks, declarations, assignments, returns,
  conditions, guards, parenthesized widgets, native aliases, lexical scopes,
  and component calls. Emit deterministic path, parentPath, key, name, source
  start/end; remap component children into each call-site tree.
- [ ] Verify identity across repeated component instances and updates; avoid
  index-only fallback wherever the compiler has sufficient information.
- [ ] Capture full multiline spans, normalize missing ends, and verify deepest
  cursor lookup, containment, overlap, and source aliases before/after mount.
- [ ] Inventory widgets rendering as `div`; classify native-safe, ARIA-only,
  app-specific, or intentionally conservative. Align parser whitelist, tag
  mapping, TypeScript declarations, feature matrix, docs, and actual tag tests.
- [ ] Complete native attribute families from DOM document 05: globals, forms,
  links, media, and document/embed elements. Normalize boolean attributes,
  preserve extra attrs, and keep compiler metadata authoritative.
- [ ] Make mounted attribute/property/value/class/state mutations update Web
  Document facts. Check selector results and subsequent re-render consistency.
- [ ] Audit every forward/reverse relationship in DOM document 07: labels,
  forms/datalists, tables, landmarks, collections, disclosure, fieldsets,
  media/maps, ARIA references, and structural traversal. Test supported aliases
  and equal semantic snapshots before/after mount.
- [ ] Cover all event families and commands in DOM document 08, including
  decorated events, delegation, native submit/reset, dialog/popover lifecycle,
  and mutation synchronization. Use real-browser tests where native semantics
  matter, not just fake DOM method calls.
- [ ] Align `webNodeStyleFacts`, mounted facts, runtime resolution, traces, and
  CSS export. Test native/data/ARIA attributes, structural/state selectors,
  keyframes and conditional groups supported by the web layer.

Exit: each required DOM family has compiler/runtime/fake-DOM/browser evidence
as applicable, with matching identity, facts, relationships, and styles.
Browser-only CSS features remain explicitly distinct from native StyleData.

## P4 — Finish style backend conformance

- [x] Verify all active hosts/tools delegate grammar and shared semantic decisions to
  maintained `.kry` sources. Include all KSS formatting, diagnostics, selector
  serialization, and active target mapping decisions; output sinks remain host
  services. Remove any residual independent implementations discovered by the
  audit, not the thin I/O/generated-code shims.
- [ ] Reconcile the declared value/selector contract and diagnostics across
  typed and web surfaces. Retain intentional web CSS extensions; resolve or
  explicitly document divergences such as numeric `px` handling. Do not assume
  unsupported nested token groups, computed colors, or transitions are promised.
- [ ] Build a matrix with active language targets C/Go, KRB execution, and rendering
  paths native/retained/DOM/canvas/libdraw/termi/null. Mark valid combinations,
  unsupported combinations, and unverified combinations separately.
- [ ] Cover pack loading, imports, themes/env/variants, classes/roles/states,
  provenance, explicit zero, transparency, no-style content, and missing-value
  behavior. Extend the shared matched fixture rather than duplicating it.
- [ ] Audit Go frame operations and all paint paths for sufficient presence
  bits/provenance. Resolve remaining unstyled debug decoration against the
  documented no-style contract; don't silently change content visibility.
- [ ] Define and test constrained-backend material/effect degradation. Keep
  resolved colors stable, report unsupported effects, and avoid silent pack
  substitution. Audit browser and terminal default-style leakage.
- [ ] Add missing family/state captures: popup/modal, navigation, tables,
  menus/lists/trees, text/layout, light/dark, contrast, and degraded effects.
- [ ] Add ownership/provenance guards and required CI checks for the matrix.
  Keep structural/content allowances explicit while shrinking migrated debt.

Exit: declared support matches measured behavior; resolution equality and
rendering correctness are recorded separately for every supported path.

### KSS migration sequence within P4/P5

These inspected host functions are concrete remaining migration targets, not
an exhaustive completion inventory. Keep browser-specific semantics in shared
`.kry` even when only the browser currently consumes them.

| Order | Current host policy | Shared implementation and completion evidence |
|---|---|---|
| 1 | `webStylePseudoToCSS` generic functional argument sanitizer | Replace remaining argument rewriting with explicit shared validation/serialization; test unknown functions, malformed arguments, and nested negation. Positional formulas already use `KssNthText`. |
| 2 | `selectorKindMatches`, `selectorNativeAttrValue`, `selectorDataAttrValue`, `selectorAriaAttrValue`, presence helpers | Separate raw DOM observations from shared naming, coercion, presence, and matching rules. Match absent/empty/false/native/data/ARIA cases across generated targets and mounted DOM. |
| 3 | `webStyleSelectorAttrToCSS`, `webStyleStateSelectorToCSS` | Move alias and state mappings plus escaping/selector emission into shared KSS code. Compare runtime resolution with browser computed styles. |
| 4 | `webStyleCSSValue`, `webStyleDeclarationLines`, `applyResolvedWebStyle` | Property vocabulary/aliases, numeric units, and border shorthand classification now use shared code. Ordinary rules and keyframes now share axis/composite expansion and effect recipes. Inline application now consumes the same stream, including shared border defaults and vendor fallbacks. Exact one-property alias precedence is covered in the web resolver; finish broader shorthand/longhand side precedence and remaining CSS text decisions; retain only the output sink in hosts. Cover explicit zero, custom properties, and constrained-backend behavior. |
| 5 | `selectorMatchesFacts` orchestration and existing shared specificity weights | Finish compound decisions and reconcile functional specificity with the declared contract. Preserve ordered repeated constraints and test conflicting rules against actual CSS results. |
| 6 | `webKssDiagnostic`, source-pack adapters, inspector/formatter/release paths | Move remaining diagnostic and language decisions into `.kry`; retain file access, source ownership, publication, UI presentation, and allocation services in hosts. Verify atomic theme switching, source locations, formatting round trips, and compiled/dynamic equality. |

For each row: add shared fixtures, implement in maintained `.kry`, regenerate,
route callers through generated code, remove the superseded host policy, and
run affected parity/browser gates. A forwarding wrapper alone does not close a
row while it still contains language decisions. Record exact source owners and
host-service exceptions in P0's ledger before declaring all KSS migrated.

## P5 — Complete style tooling

- [x] Add Go source-retaining theme switching, including registered sources,
  built-ins, variants, repeated switching, and atomic failure recovery.
  C registry lifecycle differences remain tracked in the evidence ledger.
- [ ] Audit inspector parity across existing tooling. Finish per-field losing
  rules and backend degradation reporting; expose active overlays/variants,
  winner values, token origins, source locations, specificity, and layer.
- [ ] Add copy-selector and jump-to-source UI actions. Reuse existing dead-rule
  and unmatched-class diagnostics rather than implementing them again.
- [ ] Finish consistent file/line/column, offending-token, and expected-syntax
  diagnostics, including imported files.
- [ ] Extend the existing formatter/`kssfmt` with declaration reflow. Preserve
  comments, idempotence, and semantic round trips across generated backends.
  Remove redundant opacity declarations only with cascade/state-reset proof.
- [ ] Verify live KSS reload in an actual host, including invalid-source
  recovery, theme overlays, and pack options. Record which downstream hosts
  support reload and test them during P6.
- [ ] Decide whether app-authored KSS registration emitted by `k2c`/`k2cpp`
  and package flows should also switch from source registration to generated
  typed tables. Built-in release startup uses generated C tables by default;
  `make style-release-table-repro-test`, `make style-release-table-emitter-test`,
  `make style-release-table-import-emitter-test`, and
  `make style-release-startup-test` cover dynamic parity, imports/overlays, and
  zero-parser startup without tracking generated tables as source.

Exit: tools use canonical parsing/semantics, authors can inspect the painted
result and its origin, and release styles match dynamic loading.

## P6 — Migrate consumers and verify platforms

- [ ] Inventory all actual Kryon submodule consumers from `.gitmodules`, not
  only named apps. Record repository, upstream revision, caller paths, bundled
  assets, style selection/persistence, and supported platform matrix.
- [ ] Migrate maintained examples/templates to explicit style attachment and
  bundled assets; compile their generated output and inspect representative UI.
- [ ] Finish Inbe's theme-catalog/getter and `app_style.kry` palette bridge
  migration to KSS overlays. Verify selection persists and assets are present.
- [ ] Check other maintained apps, including Kapsule, for remaining API/style
  callers and packaging gaps; keep application-specific behavior in each app.
- [ ] Build and interactively verify each supported native/web/mobile target.
  Explicitly include Inbe Android and Plan 9, real quick taps, clipping,
  style switching, and reload where supported. Linux success cannot close
  another platform's row.
- [ ] Commit upstream changes on Kryon master first, then update clean vendor
  pointers in downstream repositories. Record exact revisions and evidence.

Exit: every maintained consumer has verified caller migration, packaging, and
platform results. Missing devices/toolchains remain named blockers, not passes.

## P7 — Remove debt and close the plans

- [ ] Record every surviving bridge's file/symbol, caller, replacement,
  deletion condition, and regression test.
- [ ] After P6 migrations, delete unused theme catalogs/getters/import-export
  bridges and deprecated decorative fields. Preserve actual platform preference
  services and canonical content/structural props.
- [ ] Remove obsolete renderer defaults and scanner allowances; do not replace
  removed APIs with aliases, forwarding adapters, or a second styling surface.
- [ ] Refresh API, boundaries, architecture, canonical surface, feature matrix,
  examples, and theme/style guidance. Label proposals and retain negative tests
  for intentionally rejected syntax.
- [ ] Run final gates at the delivered revision, review generated diffs, and
  attach fixture/backend/platform evidence to each requirement.
- [ ] Remove completed task documents only after their evidence is preserved
  in maintained docs/tests. Update this index as each area closes.

Exit: no unexplained host policy, no deprecated maintained callers, no falsely
advertised parity, and no outstanding required platform verification.

## Validation sequence

Run focused gates after each relevant slice, then the complete affected suite.
These are existing entry points, not claims of successful execution in this
review. Use detached logging for long builds per the repository environment.

```sh
make generate-runtime
make fast-test
make keyboard-policy-test input-policy-test focus-policy-test
make reorder-policy-test paragraph-policy-test text-policy-test image-policy-test icon-policy-test
make k2go-syntax-test
make go-runtime-test
python3 tests/canonical_widget_surface_doc_test.py
sh tests/public_api_names_test.sh
sh tests/canonical_surface_test.sh
make kss-parser-test kss-matched-test kss-formatter-test
make style-policy-test style-sheet-policy-test style-pack-registry-test
make style-pack-source-test style-release-table-repro-test style-release-table-emitter-test style-release-table-import-emitter-test style-release-startup-test style-assets-test style-builtins-test style-picker-test
make style-widget-policy-test app-background-style-test
make web-text-input-browser-test
make build/linux-x86_64/tests/ui_tk_test
xvfb-run -a build/linux-x86_64/tests/ui_tk_test
make sdl-pointer-test
make test
git diff --check
```

The old JS/web gates are paused; do not use them as completion evidence for current work. `make test` includes formatter CLI coverage. Run all
five named `scripts/check-style-gates.py` gates when changing style ownership
or allowances. Run `scripts/check-clean-text-api.py` on changed maintained
source roots when changing text rendering. Use `make go-runtime-test` rather
than a root Go command crossing module boundaries. Platform and actual OS IME
checks require additional recorded execution beyond this command list.

## Coverage of the original plan documents

| Original documents | Consolidated work |
|---|---|
| [canonical/README](canonical/README.md) | P0, validation, P7 |
| [canonical/02](canonical/02-widget-policy-migration.md), [03](canonical/03-input-release-lifecycles.md), [07](canonical/07-host-service-boundary.md) | P0–P2 ownership and lifecycle |
| [canonical/04](canonical/04-text-editing.md), [05](canonical/05-rich-text-and-drawing.md) | P1–P2 editing, layout, platform IME |
| [canonical/06](canonical/06-lowered-block-backends.md), [08](canonical/08-go-web-transpilation-parity.md) | P1 executable lowering and parity |
| [style/00](style/00-status.md), [02](style/02-kss-language.md) | P0 reconciliation, P4 declared support |
| [style/04](style/04-widget-migration.md), [05](style/05-runtime-and-backends.md) | P2 visual ownership, P4 conformance |
| [style/06](style/06-tooling-and-workflow.md) | P5 tooling, P4 comparison captures |
| [style/07](style/07-testing-and-gates.md) | P0 matrix, P4 gates, P7 closure |
| [style/08](style/08-downstream-rollout.md), [09](style/09-legacy-removal.md) | P6 consumers, P7 removal |
| [dom/00](dom/00-index.md), [09](dom/09-tests-rollout-and-coordination.md) | P0 inventory, P3 gates, P6 rollout |
| [dom/01](dom/01-dom-node-identity.md), [02](dom/02-compiler-metadata.md), [03](dom/03-source-ranges.md) | P3 identity, compiler metadata, spans |
| [dom/04](dom/04-native-element-surface.md), [05](dom/05-attributes-state-and-data.md) | P3 native tags and synchronized attrs |
| [dom/06](dom/06-kss-selector-and-style-contract.md) | P3 facts/CSS contract, P4 style parity |
| [dom/07](dom/07-relationships-and-accessibility.md), [08](dom/08-events-and-dom-commands.md) | P3 semantic relationships and native commands |

First implementation slice: complete P0's fixture/ownership ledger, then P1's
missing pointer/wheel driver and placeholder rejection. Those unlock honest
cross-backend verification for much of the remaining policy migration.
