# Kry Web Native DOM Project Plan

## Goal

Make `.kry` the authoring surface for web applications while emitting native
browser DOM, CSS, and events. Generated JavaScript should primarily hold app
logic and runtime glue. Structure comes from Kry nodes; styling comes from KSS;
the browser receives normal elements, attributes, CSS, and events.

## Architecture

- `.kry` widget calls emit Web Document nodes with stable identity:
  `ref`, `path`, `parentPath`, `name`, `key`, DOM id/name, source path/line/column,
  classes, state, relationships, accessibility facts, and style facts.
- The web runtime renders Web Document nodes to native DOM elements using
  conservative semantic tag mapping.
- DOM objects expose the bridge between Kry identity and native elements:
  `WebDOMObject`, `element.kryObject`, `root.kryObject(...)`, selectors,
  source lookups, relation lookups, snapshots, and source maps.
- KSS resolves against Web Document style facts and can be exported/installed as
  browser CSS targeting Kry's native DOM annotations.
- Generated JS remains responsible for logic actions, state providers, routing,
  and runtime calls, not hand-building application DOM.

## Implemented Foundation

- Stable node identity for named and anonymous `.kry` widgets, including
  compiler source spans.
- Native DOM annotations: `data-kry-ref`, path/name/key/kind/tag/source fields,
  source refs, aliases, state, classes, data attrs, ARIA attrs, and native attrs.
- Query APIs for unmounted Web Document nodes and mounted DOM objects:
  exact identity, KSS-style selectors, subtree queries, source lookup, and
  source maps.
- Native relation resolution for `aria_controls`, `aria_describedby`, `dom_for`,
  and `popover_target`, while preserving authored Kry refs in Web Document facts.
- Event bridge for click/tap, form/text/key, focus/blur, scroll, pointer/mouse,
  wheel, drag/drop, clipboard, dialog, and popover events.
- KSS web runtime parsing, style resolution, CSS export, style installation,
  app style loading, and `data-kry-state` mirroring.
- k2js route dispatch for explicit route paths with nested `:param` segments,
  plus runtime access to captured route params.

## Remaining Work

- Finish all-node compiler metadata so every `.kry` syntactic node that should
  have a DOM surface emits complete identity without runtime fallback synthesis.
- Add source end spans once parser/KIR spans expose them, then support
  cursor-range lookup for editors and devtools.
- Expand native tag contracts for widgets that still render as `div`, choosing
  browser semantics only when the widget behavior maps cleanly.
- Grow KSS property coverage for web CSS export in lockstep with KSS language
  support, with tests for each property and state selector.
- Extend KSS package discovery beyond built-in `styles/kryon/<pack>.kss`.
- Add more semantic relationship facts where widgets need them, such as grouped
  controls, owned regions, labelled-by chains, menu/list relationships, and
  table/grid headers.
- Build browser-backed integration tests once a real DOM harness is available,
  keeping the fake DOM tests as fast contract tests.

## Coordination Contract

- KSS work should target `webNodeStyleFacts(node)` and the `data-kry-*` DOM
  annotations; do not require generated JS to hand-assign style classes.
- All-node metadata work should populate source/path/name/key/ref facts before
  runtime fallback. The runtime fallback remains for compatibility and generated
  anonymous nodes.
- Widget semantic work should update the Web Document node facts, renderer,
  TypeScript declarations, docs, and `tests/k2js_syntax_test_runner.mjs`
  together.
- Downstream apps should consume this through the real Kryon repository first,
  then bump `vendor/kryon`; do not edit vendored Kryon copies.
