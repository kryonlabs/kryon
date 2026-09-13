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
  multiline compiler source spans for UI blocks.
- Lexical UI scopes that lower through host begin/end support now emit Web
  Document nodes at their `.kry` block boundary: `Scroll`, `Canvas`,
  `TableCell`, `Popup`, and `Disabled`.
- Direct runtime/web widget calls are covered by a compiler contract test that
  requires source-derived `path`/`key` metadata before runtime fallback.
- Expression-backed widget calls in declarations and assignments emit
  source-derived Web Document metadata, so logic expressions still produce
  inspectable DOM nodes.
- Declared `.kry` widget blocks emit a call-site Web Document node and remap
  child nodes from the component definition path into that call-site subtree, so
  KSS selectors and native DOM nesting see composed widgets as real structure.
- Native DOM annotations: `data-kry-ref`, path/name/key/kind/tag/source fields,
  source refs, aliases, state, classes, data attrs, ARIA attrs, and native attrs.
- Direct `.kry` widget args feed native input/form attributes such as
  `input_type`, `placeholder`, `required`, `autocomplete`, length/pattern
  constraints, input mode, and enter key hints, with compiler metadata still
  taking precedence.
- Direct `.kry` widget args also feed native `data-*` and extra attributes via
  `data_*`, `dom_data_*`, `html_data_*`, `attr_*`, `dom_attr_*`, and
  `html_attr_*`, with compiler metadata overlays preserving the stronger source
  of truth.
- Direct `.kry` widget args feed native role and ARIA attributes for canonical
  accessibility fields plus generic `aria_*` passthrough, again with compiler
  metadata taking precedence.
- Direct `.kry` widget args feed native global attributes such as DOM id/name,
  title, tab index, hidden, draggable, content-editable, part, slot, popover,
  and table cell relation attributes.
- Native tags and fallback ARIA roles for widgets with clear browser
  equivalents or accessibility semantics, with coverage for form controls,
  selectors, segmented controls, progress, separators, tables, title/navigation
  landmarks, canvas-backed widgets, and common overlay roles.
- Direct `.kry` widget args can override native tag choice with `dom`,
  `dom_tag`, `html_tag`, or `tag`, and can expose stable web refs with
  `dom_ref`, `web_ref`, or `kry_ref`.
- Table header `TableCell` nodes infer native `<th>` tags from row/column
  scope metadata or direct `scope` args and surface row/column header roles in
  accessibility facts.
- Canonical `Image(ImageProps)` nodes expose native `img` source and alt text
  through DOM attributes, KSS selector facts, and mounted DOM snapshots.
- Query APIs for unmounted Web Document nodes and mounted DOM objects:
  exact identity, KSS-style selectors, subtree queries, source lookup, and
  source maps.
- Mounted DOM snapshots include effective role and `webNodeStyleFacts(...)`
  data, so devtools can inspect Kry identity and KSS selector facts together.
- Web runtime TypeScript declarations expose the native DOM facts used by KSS
  and DOM queries, including global attribute facts such as title and tab index.
- Native relation resolution for `aria_controls`, `aria_owns`,
  `aria_labelledby`, `aria_activedescendant`, `aria_describedby`, `headers`,
  `dom_for`, `form`, and `popover_target`, while preserving authored Kry refs
  in Web Document facts.
- First-class ARIA structure facts for menus, lists, trees, and grouped
  controls: orientation, level, position in set, set size, popup type, sort
  order, and multiselect state.
- Event bridge for click/tap/double-click, form/text/key down/key up,
  focus/blur, scroll, first-class pointer and mouse events, wheel/context menu,
  drag/drop, clipboard, dialog, and popover events.
- Direct `.kry` widget event args such as `on_click`, `on_input`, `on_key_up`,
  pointer/mouse/drag/clipboard handlers, and dialog/popover lifecycle handlers
  populate Web Document event facts and rendered `data-kry-on-*` glue hooks.
- KSS web runtime parsing, style resolution, CSS export, style installation,
  app style loading, project package maps, list/logical scroll styling, and
  `data-kry-state` mirroring.
- KSS web CSS export and mounted DOM styling cover physical and logical border
  colors, widths, styles, and corner radii, including per-side inline/block
  properties.
- KSS web sizing supports logical `inline-size`/`block-size` and min/max
  variants for writing-mode-aware layouts.
- KSS web outline styling supports the native shorthand plus width, offset,
  style, and color fields.
- KSS web table styling supports native `border-collapse`, `border-spacing`,
  `table-layout`, `caption-side`, and `empty-cells` for real browser tables.
- KSS state selectors support explicit `[state=...]`, accumulated pseudo
  states such as `:hover:pressed`, and `:normal` export against native
  `data-kry-state` annotations.
- k2js route dispatch for explicit route paths with nested `:param` segments,
  plus runtime access to captured route params.

## Remaining Work

- Finish all-node compiler metadata for remaining expression-backed nodes that
  should have a DOM surface without runtime fallback synthesis.
- Extend compiler source ranges beyond UI blocks to full multiline AST spans
  for every expression-backed DOM node editors and devtools need.
- Expand native tag contracts for remaining widgets that still render as `div`,
  choosing browser semantics only when the widget behavior maps cleanly.
- Grow KSS property coverage for web CSS export in lockstep with KSS language
  support, with tests for each property and state selector.
- Add more semantic relationship facts where widgets need them, such as richer
  grid headers and row/column grouping.
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
