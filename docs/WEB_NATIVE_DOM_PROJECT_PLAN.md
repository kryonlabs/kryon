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
- Direct expression-statement scope calls for `BeginCanvas`, `BeginTableCell`,
  and `BeginPopup` lower through the same metadata path instead of relying on
  runtime fallback synthesis.
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
- Parenthesized single-widget expressions use the same compiler-owned Web
  Document metadata path as bare widget expressions instead of falling back to
  generic JavaScript expression lowering.
- Guard expression widgets emit source-derived Web Document metadata and k2js
  lowers blockless guards to a closed early-return branch, so generated JS does
  not lose the DOM node or produce malformed control flow.
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
- Label-bearing widgets derive native accessible labels from ordinary Kry
  `label`/`title` props when no explicit ARIA label metadata is supplied.
- Direct `.kry` widget args feed native global attributes such as DOM id/name,
  title, tab index, hidden, draggable, content-editable, part, slot, popover,
  and table cell relation attributes.
- Native tags and fallback ARIA roles for widgets with clear browser
  equivalents or accessibility semantics, with coverage for form controls,
  selectors and selectable options, segmented controls, status output,
  progress, numeric input, separators, tables, title/navigation landmarks,
  line/separator content, menu item buttons, menu/tab/tree selectable actions,
  tab buttons, modal popup dialogs, tooltip/context popup roles, icon/list item
  content, canvas-backed widgets, and common overlay roles.
- `Toast` status output nodes default to polite native live regions while still
  allowing explicit `aria_live`/`live` metadata to override the default.
- Direct `.kry` widget args can override native tag choice with `dom`,
  `dom_tag`, `html_tag`, or `tag`, and can expose stable web refs with
  `dom_ref`, `web_ref`, or `kry_ref`.
- Nodes with `dom_for`/`html_for` infer native `<label>` tags when no explicit
  tag override is present, so label ownership uses browser-native markup.
- `Fieldset` titles render as native `<legend>` children while preserving Kry
  label facts for queries, snapshots, KSS, and accessibility inspection.
- Table header `TableCell` nodes infer native `<th>` tags from row/column
  scope metadata or direct `scope` args and surface row/column header roles in
  accessibility facts.
- Mounted table relationship facts classify direct row/column headers and
  row-group/column-group headers for inspectors and browser DOM snapshots.
- Semantic group relationship facts expose `groupOwner` and `groupMembers` for
  controls inside native or ARIA groups.
- Disabled scope relationship facts expose `disabledOwner` and
  `disabledMembers` for nodes inside disabled `Fieldset` and `Disabled`
  scopes.
- Semantic landmark relationship facts expose `landmarkOwner` and
  `landmarkMembers` for nodes inside native or ARIA page landmarks.
- Semantic collection relationship facts expose `collectionOwner` and
  `collectionItems` for menu, tablist, tree, listbox, and list-style
  owner/member roles.
- Semantic collection relationship facts expose `selectedCollectionOwner` and
  `selectedCollectionItems` for selected members inside those collections.
- Semantic collection relationship facts expose `activeCollectionOwner` and
  `activeCollectionItems` for composite widgets with an active descendant.
- Canonical `Image(ImageProps)` nodes expose native `img` source and alt text
  through DOM attributes, KSS selector facts, pre-mount and mounted DOM
  snapshots, and accessibility snapshots.
- Query APIs for unmounted Web Document nodes and mounted DOM objects:
  exact identity, KSS-style selectors, subtree queries, source lookup, and
  source maps.
- Source lookup APIs include cursor containment and source-range overlap
  helpers for editors and devtools mapping `.kry` selections to Kry DOM
  objects.
- k2js syntax coverage locks direct `BeginDisabled` scope calls to emitted
  Web Document path, parent, and source-range metadata so authored Kry scopes
  remain addressable before browser mount.
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
  inspector navigation from relation targets; semantic landmark ownership
  exposes nodes inside native or ARIA page landmarks, plus direct serializable
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
- Parenthesized `BeginScroll(...)`, `BeginCanvas(...)`,
  `BeginTableCell(...)`, `BeginDisabled(...)`, and `BeginPopup(...)`
  scope-producing expressions emit compiler-owned `Scroll`, `Canvas`,
  `TableCell`, `Disabled`, and `Popup` Web Document identity, including popup
  open results stored in locals.
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
  previous/next sibling-list variants, `webDOMPreviousSibling(...)`,
  `webDOMNextSibling(...)`, mounted root helpers, element getters, and
  `WebDOMObject` getters.
- Ancestor traversal APIs expose full parent chains before and after mount for
  breadcrumb-style inspectors and scoped DOM tooling.
- Decorated native browser events expose the same Kry DOM traversal helpers as
  mounted elements and objects, so logic handlers can inspect structure without
  generated JS rebuilding DOM shape.
- Native element list lookup mirrors DOM object queries through
  `findWebElements(...)` and `root.kryElements(...)`, letting host integrations
  work with browser elements while Kry remains the structural source.
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
- KSS web media/background styling accepts native object fit/position/view-box,
  image rendering/orientation/resolution, and `background-image` alongside
  size, position, position-axis longhands, repeat, repeat-axis longhands,
  origin, clip, attachment, and blend controls.
- KSS web type styling accepts native `font` shorthand, Kryon's `typeface`
  alias, the native `font-family` property name, font size adjustment, and font
  synthesis controls, plus native font variant alternates and other longhands,
  language override, and font palette selection.
- KSS web text decoration styling covers native line, color, style, thickness,
  underline offset, and skip-ink controls; native text alignment, rendering,
  emphasis, ruby, combine-upright, orientation, size-adjustment, text wrapping,
  justification, line breaking, hanging punctuation, and vertical alignment are
  accepted for browser CSS export and mounted DOM styling.
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
  transform-style, perspective, perspective-origin, backface visibility, and
  motion-path offset fields.
- KSS web containment styling supports native containment, content visibility,
  intrinsic containment sizing, and container query shorthand/name/type
  properties.
- KSS web overlay and transition styling supports native anchor positioning
  fields and `view-transition-name`.
- KSS web motion, sizing, accessibility, and paint styling supports native
  scroll/view timelines, animation ranges, `field-sizing`, `interpolate-size`,
  `overlay`, forced/print color adjustment, SVG paint-order/interpolation, and
  CSS shape/text box controls for browser-native layouts.
- KSS web overflow styling supports physical `overflow-x`/`overflow-y` and
  logical `overflow-inline`/`overflow-block` for writing-mode-aware scroll
  containers.
- KSS web mask styling supports native mask image, size, position, repeat,
  origin, clip, composite, and mode longhands for browser paint effects.
- KSS web multicolumn styling supports `columns`, column count/width/fill/span,
  column-rule shorthand and color/style/width longhands, and break controls.
- KSS web fragmentation styling supports native `orphans`, `widows`, and
  `box-decoration-break` for paged, multicolumn, and wrapped inline content.
- KSS web table styling supports native `border-collapse`, `border-spacing`,
  `table-layout`, `caption-side`, and `empty-cells` for real browser tables.
- KSS web list styling supports native list-style, counters, quotes, and
  marker positioning fields for document-style content.
- KSS state selectors support explicit `[state=...]`, accumulated pseudo
  states such as `:hover:pressed`, and `:normal` export against native
  `data-kry-state` annotations, plus real browser pseudo/attribute selectors
  for hover, focus, pressed, disabled, checked, invalid, valid, and open where
  the DOM has matching native state.
- KSS state selector CSS export also targets native ARIA/state attributes for
  pressed, disabled, loading, checked, selected, invalid, valid, and expanded
  controls.
- KSS browser-native aliases support `:active` for Kry `pressed` and
  `:focus-visible` for Kry `focus`, so exported CSS and runtime selector
  matching share the same DOM-facing state contract.
- KSS selected-state CSS export also targets native `aria-current` links, so
  current navigation items can use the same `:selected` Kry selector.
- KSS state selectors support `:readonly`/`:read-only` and `:required` against
  Kry form-control facts and native browser pseudo/attribute selectors.
- KSS state selectors support `:enabled` and `:optional` against Kry state and
  form-control facts with native browser pseudo selector export.
- KSS form-control state selectors support browser-native
  `:placeholder-shown`, `:indeterminate`, `:default`, and `:autofill` CSS
  export plus Kry state/fact matching for pre-mount and mounted DOM queries.
- KSS structural selectors support browser-native `:empty` in CSS export,
  pre-mount Web Document queries, and mounted DOM object queries.
- KSS structural selectors support browser-native `:nth-last-child(...)`
  alongside `:nth-child(...)` for CSS export and Kry DOM queries.
- KSS structural selectors support browser-native of-type selectors against
  Kry kind siblings: `:first-of-type`, `:last-of-type`, `:only-of-type`,
  `:nth-of-type(...)`, and `:nth-last-of-type(...)`.
- KSS `nth-*` structural selectors support CSS `an+b` formulas such as
  `2n+1`, `n+2`, and `-n+4` in Web Document and mounted DOM queries.
- KSS structural selectors support `:root` and scoped `:scope` matching in Web
  Document and mounted DOM queries, while CSS export preserves the native
  pseudo selectors.
- KSS structural selectors support browser-native `:focus-within` in CSS
  export, pre-mount style resolution, and mounted DOM queries by walking Kry
  descendant focus state.
- KSS structural selectors support browser-native `:target` in CSS export,
  pre-mount style resolution, and mounted DOM queries by matching the route
  hash against Kry DOM identity aliases and generated element ids.
- KSS structural selectors support bounded browser-native `:has(...)` CSS
  export, pre-mount style resolution, and mounted DOM queries for descendant,
  direct-child, next-sibling, and following-sibling Kry DOM relationships.
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
