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
- Browser-backed Web Document smoke coverage now exercises native elements,
  KSS inline and installed CSS styling, selector lookup, event decoration, table
  header relationships, form ownership, numeric input range facts, and source
  ref/range object lookup
  against a real DOM, plus native lifecycle events, DOM observers, and native
  containment style application.
- Expression-backed widget calls in declarations, assignments, returns, and
  control-flow conditions emit source-derived Web Document metadata, so logic
  expressions still produce inspectable DOM nodes.
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
  selectors and selectable options, segmented controls, status output,
  progress, numeric input, separators, tables, title/navigation landmarks,
  line/separator content, menu item buttons, tab buttons, icon/list item content,
  canvas-backed widgets, and common overlay roles.
- Direct `.kry` widget args can override native tag choice with `dom`,
  `dom_tag`, `html_tag`, or `tag`, and can expose stable web refs with
  `dom_ref`, `web_ref`, or `kry_ref`.
- Nodes with `dom_for`/`html_for` infer native `<label>` tags when no explicit
  tag override is present, so label ownership uses browser-native markup.
- Table header `TableCell` nodes infer native `<th>` tags from row/column
  scope metadata or direct `scope` args and surface row/column header roles in
  accessibility facts.
- Mounted table relationship facts classify direct row/column headers and
  row-group/column-group headers for inspectors and browser DOM snapshots.
- Semantic group relationship facts expose `groupOwner` and `groupMembers` for
  controls inside native or ARIA groups.
- Canonical `Image(ImageProps)` nodes expose native `img` source and alt text
  through DOM attributes, KSS selector facts, and mounted DOM snapshots.
- Query APIs for unmounted Web Document nodes and mounted DOM objects:
  exact identity, KSS-style selectors, subtree queries, source lookup, and
  source maps.
- Pre-mount Web Document snapshots expose the same serializable Kry identity,
  relation, event, and style-fact packet shape before a frame is rendered into
  live browser DOM.
- Mounted DOM snapshots serialize the full `WebNodeIdentity` packet and alias
  list so devtools can show every stable lookup key for a `.kry` node without
  keeping live DOM references.
- Mounted roots expose both single-node `krySnapshot(query)` and selector-based
  `krySnapshots(selector)` helpers, mirroring module-level snapshot APIs for
  browser tools that start from the native root element.
- Headless browser smoke coverage verifies real DOM rendering, native
  attributes, source-range annotations, Kry object lookup, KSS application, and
  decorated event dispatch in Chromium when available.
- Dedicated headless browser inspector coverage verifies mounted source maps,
  object maps, selector-filtered snapshots, style traces, accessibility
  snapshots, and element/event snapshot round-trips in Chromium when available.
- Mounted DOM snapshots include effective role and `webNodeStyleFacts(...)`
  data, with direct mounted `webDOMStyleFacts(...)` and root/element/object
  accessors so devtools can inspect Kry identity and KSS selector facts
  together.
- Mounted DOM accessibility snapshots project rendered Kry DOM objects into
  the same serializable accessibility-node packet as pre-mount frame
  snapshots, including current text/value, role, state, and range facts.
- Web KSS style traces expose pre-mount `traceWebStyle(...)` and mounted
  `webDOMStyleTrace(...)` data with matched rules, resolved values, and winning
  declarations for inspectors.
- Web runtime TypeScript declarations expose the native DOM facts used by KSS
  and DOM queries, including global attribute facts such as title and tab index.
- Web runtime TypeScript declarations expose parsed KSS conditional groups for
  `@media`, `@supports`, and `@container`.
- Native relation resolution for `aria_controls`, `aria_owns`,
  `aria_labelledby`, `aria_activedescendant`, `aria_describedby`,
  `aria_details`, `aria_errormessage`, `aria_flowto`, `headers`, `dom_for`,
  `form`, and `popover_target`, while preserving authored Kry refs in Web
  Document facts; mounted relation objects and snapshots expose reverse
  `controlledBy`, `ownedBy`, `describes`, `detailedBy`, `errorFor`,
  `flowFrom`, `activeDescendantOf`, and `popoverInvokers` buckets for
  inspector navigation from relation targets, plus direct serializable
  `webNodeRelations(...)`,
  `webNodeRelationRefs(...)`, and
  `webDOMRelationRefs(...)` lookup.
- First-class ARIA structure facts for menus, lists, trees, and grouped
  controls: orientation, level, position in set, set size, popup type, sort
  order, and multiselect state.
- Accessibility and DOM snapshots expose native value range facts for
  slider/spinbox/progress controls so inspectors can show `min`, `max`, and
  current value without scraping browser attributes.
- Event bridge for click/tap/double-click, form/text/key down/key up,
  focus/blur, scroll, first-class pointer and mouse events, wheel/context menu,
  drag/drop, clipboard, dialog, and popover events.
- Direct `.kry` widget event args such as `on_click`, `on_input`, `on_key_up`,
  pointer/mouse/drag/clipboard handlers, and dialog/popover lifecycle handlers
  populate Web Document event facts and rendered `data-kry-on-*` glue hooks.
- Direct `Scroll(...)` widget expressions now participate in compiler-owned
  Web Document identity and source-span metadata instead of relying on runtime
  fallback paths.
- Web relationship facts include reverse `formControls` links so form-like DOM
  nodes can enumerate controls that reference them through native form
  ownership.
- Web relationship facts include native implicit label links for authored
  `label` tags that wrap form controls.
- Table header relationship facts include reverse `headerFor` links so header
  nodes can enumerate cells that reference them.
- Web relationship facts include structural `previousSibling`/`nextSibling`
  links for adjacent Kry DOM objects in both pre-mount and mounted snapshots.
- Sibling traversal APIs expose those structural links directly through
  `webNodePreviousSibling(...)`, `webNodeNextSibling(...)`,
  `webDOMPreviousSibling(...)`, `webDOMNextSibling(...)`, mounted root helpers,
  element getters, and `WebDOMObject` getters.
- Browser-backed inspector coverage verifies mounted tree traversal bridges for
  children, descendants, closest ancestor lookup, and scoped descendant query.
- Browser-backed KSS coverage verifies mounted application of native container,
  containment, compositing, multicolumn, will-change, and view-transition
  properties.
- Mounted DOM snapshots serialize event refs for generated logic hooks so
  inspectors can show which Kry logic action is attached to each native DOM
  object without scraping `data-kry-on-*` attributes; direct
  `webNodeEventRefs(...)` and `webDOMEventRefs(...)` APIs plus mounted
  root/element/object convenience accessors expose the same facts.
- KSS web runtime parsing, style resolution, CSS export, style installation,
  app style loading, project package maps, list/logical scroll styling, and
  `data-kry-state` mirroring. App style installation preserves embedded
  `@keyframes` and conditional groups as native browser CSS.
- KSS web CSS export and mounted DOM styling cover physical and logical border
  colors, widths, styles, and corner radii, including per-side inline/block
  properties and side shorthands, with both Kryon's `radius` alias and native
  `border-radius`; `border` keeps its single-color alias behavior and also
  accepts native multi-part border shorthand values.
- KSS web color styling accepts both Kryon aliases (`foreground`,
  `background`) and native CSS names (`color`, `background-color`) for browser
  CSS export and mounted DOM style application.
- KSS web background styling accepts native `background-image` alongside size,
  position, position-axis longhands, repeat, origin, clip, attachment, and
  blend controls.
- KSS web type styling accepts native `font` shorthand, Kryon's `typeface`
  alias, the native `font-family` property name, font size adjustment, and font
  synthesis controls.
- KSS web text decoration styling covers native line, color, style, thickness,
  underline offset, and skip-ink controls; native text alignment, rendering,
  orientation, size-adjustment, and vertical alignment are accepted for browser
  CSS export and mounted DOM styling.
- KSS web border styling accepts native border-image shorthand and longhands in
  addition to the physical and logical border controls.
- KSS web sizing supports logical `inline-size`/`block-size` and min/max
  variants for writing-mode-aware layouts.
- KSS web grid styling supports named template areas and per-node grid-area
  placement in addition to tracks and line placement.
- KSS web outline styling supports the native shorthand plus width, offset,
  style, and color fields.
- KSS web transform styling supports native transform longhands and 3D
  transform fields such as translate, rotate, scale, transform-box,
  transform-style, perspective, perspective-origin, and backface visibility.
- KSS web containment styling supports native containment, content visibility,
  intrinsic containment sizing, and container query naming/type properties.
- KSS web overlay and transition styling supports native anchor positioning
  fields and `view-transition-name`.
- KSS web motion, sizing, accessibility, and paint styling supports native
  scroll/view timelines, animation ranges, `field-sizing`, `interpolate-size`,
  `overlay`, forced/print color adjustment, SVG paint-order/interpolation, and
  CSS shape/text box controls for browser-native layouts.
- KSS web table styling supports native `border-collapse`, `border-spacing`,
  `table-layout`, `caption-side`, and `empty-cells` for real browser tables.
- KSS state selectors support explicit `[state=...]`, accumulated pseudo
  states such as `:hover:pressed`, and `:normal` export against native
  `data-kry-state` annotations, plus real browser pseudo/attribute selectors
  for hover, focus, pressed, disabled, checked, invalid, and open where the DOM
  has matching native state.
- KSS state selector CSS export also targets native ARIA/state attributes for
  pressed, disabled, loading, checked, selected, invalid, and expanded controls.
- KSS selected-state CSS export also targets native `aria-current` links, so
  current navigation items can use the same `:selected` Kry selector.
- KSS state selectors support `:readonly`/`:read-only` and `:required` against
  Kry form-control facts and native browser pseudo/attribute selectors.
- KSS structural selectors support browser-native `:empty` in CSS export,
  pre-mount Web Document queries, and mounted DOM object queries.
- KSS structural selectors support browser-native `:nth-last-child(...)`
  alongside `:nth-child(...)` for CSS export and Kry DOM queries.
- KSS structural selectors support browser-native of-type selectors against
  Kry kind siblings: `:first-of-type`, `:last-of-type`, `:only-of-type`,
  `:nth-of-type(...)`, and `:nth-last-of-type(...)`.
- KSS `nth-*` structural selectors support CSS `an+b` formulas such as
  `2n+1`, `n+2`, and `-n+4` in Web Document and mounted DOM queries.
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
- Add more semantic relationship facts where widgets need them beyond current
  table header, row/column group, and semantic group coverage.
- Expand browser-backed integration tests beyond the current smoke harness,
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
