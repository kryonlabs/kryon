# Kryon Web Document Frame

Status: active implementation contract

The Web Document frame is the browser-facing structure produced from a Kryon
runtime frame. It is not browser DOM, CSS, or a second widget API. It is the
typed bridge between `.kry` structure, future KSS style resolution, and native
browser presentation.

## Purpose

The existing JavaScript runtime records generated widget calls and can simulate
logic. The Web Document frame gives that stream a stable semantic shape:

- named `.kry` UI blocks as source-level node identities;
- widget kind, key, name, source path, parent path, classes, state, and bounds;
- native element tag selection for common web-capable widgets;
- text, links, image sources, input type, and accessibility-facing state;
- one frame format that can be consumed by DOM reconciliation, tests, and KSS.

JavaScript remains responsible for generated logic, event glue, host calls, and
browser bootstrapping. Widget structure stays in `.kry`; route state is exposed
as logic input; visual styling belongs to KSS.

## Source Metadata

Named UI blocks identify nodes:

```kry
Button save: {
    label = "Save"
    dom = "button"
    dom_id = "save-button"
    class = "primary"
    role = "button"
    aria_label = "Save settings"
    on_click = save_settings
}
```

The block name, `save`, is the stable Kry node name. The web metadata fields
are stored on the KIR statement and are not native widget props, so C and Go
widget APIs do not need browser-only fields.
Named blocks use their source name as the compiler-provided key. Anonymous
widget expressions use their generated Kry path as the compiler-provided key,
so native DOM identity does not have to fall back to frame indexes.

Route blocks identify pages:

```kry
route home {
    title "Home"
    group "Pages"
    page Home
}
```

`k2js` emits these as `app.routes`. For modules with routes and no explicit
`app.frame`, generated `frame()` selects the route page from `GetRoutePath()`;
the first route handles `/` and also acts as the fallback.

Style imports identify KSS inputs:

```kry
#style <kryon.material> as material
#style "brand.kss" as brand
#style <brand> as brand_pack
#style <acme.dark> as acme_dark
```

`k2js` emits these as `app.styles` so a web host can load built-in and file
style sheets without scanning source text. KSS still owns style resolution;
the Web Document frame supplies the node facts it resolves against.
Angle-bracket package imports resolve from built-in Kryon packs first for
`kryon.*`, then from project package files such as `styles/brand.kss` and
dotted package paths such as `styles/acme/dark.kss`.

Rendered DOM elements carry source identity as native attributes:
`data-kry-ref`, `data-kry-index`, `data-kry-path`, `data-kry-parent-path`,
`data-kry-name`, `data-kry-key`, `data-kry-kind`, `data-kry-aliases`,
`data-kry-source`,
`data-kry-line`, and `data-kry-column`. The same source location is also available as
`data-kry-source-ref` (`path:line`) and `data-kry-source-column-ref`
(`path:line:column`) for native DOM queries and devtools inspection. When the
compiler knows the end of the source statement, elements also carry
`data-kry-end-line`, `data-kry-end-column`, and `data-kry-source-range-ref`
(`path:startLine:startColumn-endLine:endColumn`).
The runtime exposes
`webDOMObject(target, query)` and `webDOMObjects(target)` so JS logic,
inspectors, tests, and hydration code can ask for native DOM objects by `.kry`
path, node name, key, or DOM id without making generated JavaScript the source
of structure. Rendered elements also expose non-enumerable `kryRef`, `kryNode`,
and `kryObject` getters, so native event targets and browser inspectors can move
directly from an element back to the source `.kry` object. `sourcePath`,
`sourceLine`, and `sourceColumn` identify the
`.kry` source location that produced each node. Source references support both
`path:line` and `path:line:column` lookup forms. Anonymous widget expressions
receive source-derived path components such as `Text@42`; repeated anonymous
widgets under the same
parent receive deterministic occurrence suffixes such as `Text@42-2` so every
DOM object remains individually addressable.

Supported metadata fields:

| `.kry` field | Web frame field |
|---|---|
| named block | `nodeName`, `key`, `name`, `path`, `parentPath` |
| source span | `sourcePath`, `sourceLine`, `sourceColumn`, `sourceEndLine`, `sourceEndColumn` |
| `dom`, `dom_tag`, `html_tag`, `tag` | `tag` |
| `dom_ref`, `web_ref`, `kry_ref` | `webRef` |
| `dom_id`, `html_id` | `domId` |
| `dom_name`, `html_name`, `name_attr` | `domName` |
| `class`, `classes`, `class_name` | `classes` |
| `title`, `dom_title`, `html_title` | `title` |
| `dom_href`, `html_href` | `href` |
| `dom_target`, `html_target` | `target` |
| `dom_rel`, `html_rel` | `rel` |
| `part`, `dom_part`, `html_part` | `part` |
| `slot`, `dom_slot`, `html_slot` | `slot` |
| `data_*`, `dom_data_*`, `html_data_*` | `dataAttrs` |
| `attr_*`, `dom_attr_*`, `html_attr_*` | `extraAttrs` |
| `placeholder`, `dom_placeholder` | `placeholder` |
| `input_type`, `dom_type`, `html_type`, `dom_input_type` | `inputType` |
| `form`, `dom_form`, `html_form` | `formOwner` |
| `dom_action`, `html_action`, `form_action` | `formAction` |
| `dom_method`, `html_method`, `form_method` | `formMethod` |
| `dom_enctype`, `html_enctype`, `form_enctype` | `formEncType` |
| `autocomplete`, `dom_autocomplete`, `html_autocomplete` | `autoComplete` |
| `hidden`, `dom_hidden`, `html_hidden` | `hidden` |
| `draggable`, `dom_draggable`, `html_draggable` | `draggable` |
| `spellcheck`, `spell_check`, `dom_spellcheck`, `html_spellcheck` | `spellCheck` |
| `contenteditable`, `content_editable`, `dom_contenteditable`, `html_contenteditable` | `contentEditable` |
| `autofocus`, `auto_focus`, `dom_autofocus`, `html_autofocus` | `autoFocus` |
| `inert`, `dom_inert`, `html_inert` | `inert` |
| `autocapitalize`, `auto_capitalize`, `dom_autocapitalize`, `html_autocapitalize` | `autoCapitalize` |
| `enterkeyhint`, `enter_key_hint`, `dom_enterkeyhint`, `html_enterkeyhint` | `enterKeyHint` |
| `download`, `dom_download`, `html_download` | `download` |
| `form_no_validate`, `formnovalidate`, `dom_formnovalidate`, `html_formnovalidate` | `formNoValidate` |
| `no_validate`, `novalidate`, `dom_novalidate`, `html_novalidate` | `noValidate` |
| `popover`, `dom_popover`, `html_popover` | `popover` |
| `popover_target`, `popovertarget`, `dom_popover_target`, `html_popover_target` | `popoverTarget` |
| `popover_target_action`, `popovertargetaction`, `dom_popover_target_action`, `html_popover_target_action` | `popoverTargetAction` |
| `readonly`, `read_only`, `dom_readonly`, `html_readonly` | `readOnly` |
| `required`, `dom_required`, `html_required` | `required` |
| `dom_min`, `html_min`, `form_min` | `min` |
| `dom_max`, `html_max`, `form_max` | `max` |
| `step`, `dom_step`, `html_step` | `step` |
| `min_length`, `minlength`, `dom_minlength`, `html_minlength` | `minLength` |
| `max_length`, `maxlength`, `dom_maxlength`, `html_maxlength` | `maxLength` |
| `pattern`, `dom_pattern`, `html_pattern` | `pattern` |
| `accept`, `dom_accept`, `html_accept` | `accept` |
| `multiple`, `dom_multiple`, `html_multiple` | `multiple` |
| `input_mode`, `inputmode`, `dom_inputmode`, `html_inputmode` | `inputMode` |
| `tab_index`, `tabindex`, `dom_tab_index` | `tabIndex` |
| `role` | `role` |
| `aria_label`, `accessible_label` | `ariaLabel` |
| `aria_description`, `accessible_description` | `ariaDescription` |
| `aria_describedby`, `aria_described_by` | `ariaDescribedBy` |
| `aria_labelledby`, `aria_labelled_by` | `ariaLabelledBy` |
| `aria_activedescendant`, `aria_active_descendant` | `ariaActiveDescendant` |
| `aria_controls` | `ariaControls` |
| `aria_owns`, `aria_own` | `ariaOwns` |
| `aria_live`, `live` | `ariaLive` |
| `aria_*`, `dom_aria_*`, `html_aria_*` | `ariaAttrs` |
| `on_click` | `onClick`, `action` |
| `on_input` | `onInput`, `inputAction(value)` |
| `on_before_input`, `on_beforeinput` | `onBeforeInput`, `beforeInputAction(value)` |
| `on_change` | `onChange`, `changeAction(value)` |
| `on_select` | `onSelect`, `selectAction(value)` |
| `on_key`, `on_key_down` | `onKey`, `keyAction(key)` |
| `on_invalid` | `onInvalid`, `invalidAction(value)` |
| `on_submit` | `onSubmit`, `submitAction(values)` |
| `on_reset` | `onReset`, `resetAction(values)` |
| `on_toggle` | `onToggle`, `toggleAction()` |
| `on_close` | `onClose`, `closeAction()` |
| `on_cancel` | `onCancel`, `cancelAction()` |
| `on_focus` | `onFocus`, `focusAction()` |
| `on_blur` | `onBlur`, `blurAction()` |
| `on_scroll` | `onScroll`, `scrollAction(value)` |
| `on_mouse_enter`, `on_pointer_enter` | `onMouseEnter`, `mouseEnterAction()` |
| `on_mouse_leave`, `on_pointer_leave` | `onMouseLeave`, `mouseLeaveAction()` |
| `on_mouse_move`, `on_pointer_move` | `onMouseMove`, `mouseMoveAction()` |
| `on_mouse_down`, `on_pointer_down` | `onMouseDown`, `mouseDownAction()` |
| `on_mouse_up`, `on_pointer_up` | `onMouseUp`, `mouseUpAction()` |
| `on_wheel` | `onWheel`, `wheelAction(value)` |
| `on_drag_start`, `on_dragstart` | `onDragStart`, `dragStartAction(value)` |
| `on_drag_end`, `on_dragend` | `onDragEnd`, `dragEndAction(value)` |
| `on_drag_over`, `on_dragover` | `onDragOver`, `dragOverAction()` |
| `on_drop` | `onDrop`, `dropAction(value)` |
| `on_copy` | `onCopy`, `copyAction(value)` |
| `on_cut` | `onCut`, `cutAction(value)` |
| `on_paste` | `onPaste`, `pasteAction(value)` |

## Runtime Contract

`webDocumentFrame(rt)` returns:

```js
{
  app: rt.app,
  metadata: {
    title,
    description,
    canonicalURL,
    themeColor
  },
  nodes: [
    {
      index,
      kind,
      tag,
      key,
      name,
      path,
      parentPath,
      domId,
      domName,
      classes,
      text,
      value,
      level,
      href,
      target,
      rel,
      part,
      slot,
      dataAttrs,
      ariaAttrs,
      extraAttrs,
      inputType,
      formOwner,
      formAction,
      formMethod,
      formEncType,
      autoComplete,
      hidden,
      draggable,
      spellCheck,
      contentEditable,
      autoFocus,
      inert,
      autoCapitalize,
      enterKeyHint,
      download,
      formNoValidate,
      noValidate,
      popover,
      popoverTarget,
      popoverTargetAction,
      alt,
      asset,
      role,
      ariaLabel,
      onClick,
      onInput,
      onBeforeInput,
      onChange,
      onSelect,
      action,
      inputAction,
      beforeInputAction,
      changeAction,
      selectAction,
      pageTitle,
      pageDescription,
      pageCanonicalURL,
      pageThemeColor,
      sourcePath,
      sourceLine,
      sourceColumn,
      bounds,
      hasBounds,
      state,
      styleFacts
    }
  ]
}
```

`app.styles` contains `{ kind, target, alias }` records. `app.routes` contains
`{ id, title, group, page, path }` records. The first route uses `/` as its
browser path by default; later routes use `/<route-id>` unless the route block
declares `path "..."`.

The initial tag mapping is intentionally conservative:

| Kryon kind | Browser tag |
|---|---|
| `Screen`, `Page` | `main` |
| `Section` | `section` |
| `NavigationBar` | `nav` |
| `Fieldset` | `fieldset` |
| `Collapsible` | `details` |
| `Modal` | `dialog` |
| `Text` | `span` |
| `Heading` | `h1`-`h6` |
| `Paragraph`, `ParagraphText` | `p` |
| `Link` | `a` |
| `Button` | `button` |
| invisible hit-test support | `button` |
| `TextField` | `input type=text` |
| `TextArea` | `textarea` |
| `Slider` | `input type=range` |
| `Spinbox` | `input type=number` |
| `Dropdown` | `select` |
| `ListBox` | `select` |
| `Image` with `alt_text` | `img` |
| `Checkbox`, `Toggle` | `input type=checkbox` |
| `Radio` | `input type=radio` |
| `Progress` | `progress` |
| `Separator` | `hr` |
| `TableView` | `table` |
| `CanvasGrid` | `canvas` |

Other widgets remain `div` nodes until they gain a specific web-native
contract.

Div-backed widgets still expose conservative native ARIA roles when the widget
semantics are clear: `Toolbar` uses `toolbar`, `TabBar` uses `tablist`,
`TreeView` uses `tree`, `Menu` uses `menu`, `Toast` uses `status`, and
`Plot`/`CanvasGrid` use `img`.

## KSS Fit

KSS should resolve against each node's `styleFacts`: `index`, `kind`, `tag`, `key`,
`name`, `path`, `parentPath`, `ref`, `webRef`, `sourcePath`, `sourceLine`, `sourceColumn`,
`sourceRef`, `sourceColumnRef`, `id`, `domName`, `href`, `target`, `rel`, `part`, `slot`,
`inputType`, `formOwner`, `formAction`, `formMethod`,
`formEncType`, `autoComplete`, `hidden`, `draggable`, `spellCheck`,
`contentEditable`, `autoFocus`, `inert`, `autoCapitalize`, `enterKeyHint`,
`download`, `formNoValidate`, `noValidate`,
`popover`, `popoverTarget`, `popoverTargetAction`, `readOnly`, `required`,
`min`, `max`, `step`, `minLength`, `maxLength`, `pattern`, `accept`,
`multiple`, `inputMode`, `classes`, `dataAttrs`, `ariaAttrs`, `extraAttrs`,
`role`, `open`, `scrollLeft`, `scrollTop`, and `state`. The DOM
backend may translate resolved KSS values to CSS variables, classes, or style
attributes, but browser CSS is an output detail rather than the authoring source
of truth.

The JavaScript runtime exposes `parseWebStyleSheet(source)`,
`resolveWebStyle(node, sheets)`, `webStyleSheetToCSS(sheet)`,
`installWebStyleSheet(sheet, target?, id?)`, `loadAppWebStyleSheets(app)`,
`installAppWebStyleSheets(app, target?, id?)`, and `setWebStyleSheets(rt,
sheets)` for the same bridge in browser-hosted k2js apps. k2js embeds KSS
source text in `app.styles[].source` when a `#style` import resolves on disk.
File imports resolve relative to the source module and then the project root;
package imports resolve built-in `kryon.*` packs and project packages under
`styles/`, including dotted package names as nested paths. Projects can also
map package names with `styles/packages.kssmap`, one `package.name =
relative/file.kss` entry per line. `createRuntime({ app })` installs those
embedded sheets automatically. CSS export and
installation target Kry's native DOM annotations,
including `data-kry-*`, data/ARIA/native attributes, classes, and
`data-kry-state` for KSS pseudo-state selectors. The web
resolver supports kind selectors, `[index=...]`, `#id`, `.class`, `[ref=...]`,
`[webRef=...]`, source identity selectors
such as `[source=...]`, `[line=...]`, `[column=...]`, `[sourceRef=...]`,
and `[sourceColumnRef=...]`, `[role=...]`, `[state=...]`,
native attribute aliases such as `[name=...]`, `[type=...]`, `[href=...]`,
`[target=...]`, `[rel=...]`, `[part=...]`, `[slot=...]`, `[action=...]`, `[method=...]`,
`[enctype=...]`, `[autocomplete=...]`, `[hidden=true]`,
`[draggable=true]`, `[spellcheck=...]`, `[contenteditable=...]`,
`[inert=true]`, `[autocapitalize=...]`, `[enterkeyhint=...]`,
`[download=...]`, `[readonly=true]`, `[required=true]`, `[popover=...]`,
`[popovertarget=...]`, `[popovertargetaction=...]`, `[min=...]`,
`[max=...]`, `[step=...]`, `[minlength=...]`, `[maxlength=...]`,
`[pattern=...]`, `[accept=...]`, `[multiple=true]`, `[inputmode=...]`,
data/ARIA/extra attribute selectors, state pseudos, layers, colors, spacing,
radius, border width, opacity, font size, offsets, icon size, and local
`tokens { color { ... } length { ... } material { ... } }` references.

The frame is also the right place for inspector data: matched KSS rules,
winning declarations, token origins, state slice, and backend degradation can
attach to nodes without changing app logic.

## Runtime DOM APIs

`renderWebDocument(rt, target)` reconciles the Web Document frame into browser
elements. It also applies document metadata from `SetPageTitle`,
`SetPageDescription`, `SetPageCanonicalURL`, `SetPageThemeColor`, app metadata,
and `Page` nodes. After reconciliation, the mount root dispatches a bubbling
`kry-render` event whose `detail` contains the Web Document frame, mount root,
and current Kry DOM objects.
During reconciliation, individual elements dispatch bubbling `kry-mount`,
`kry-update`, and `kry-unmount` events. Their `detail` contains `{ frame, root,
object, node, element }`, making each `.kry` source node observable as a live
native DOM object through ordinary browser event listeners.

`webDOMRoot(target)` returns the native Kry mount root for a mounted target or
the root itself. `webDOMFrame(target)` returns the last rendered Web Document
frame. The mount root also exposes non-enumerable `kryRuntime`, `kryFrame`, and
`kryObjects` getters for browser inspectors and host integrations.
`webDOMObjectMap(target)` and `root.kryObjectMap` return a `Map` from every
stable alias, including `web_ref`, Kry path/name/key, DOM id/name, and source
refs, to the current live Kry DOM object.
`webDOMObserve(target, selector, handler, options?)` and
`root.kryObserve(selector, handler, options?)` subscribe to `kry-render` and
pass the current matching Kry DOM objects to browser logic. Observers run once
immediately unless `options.immediate` is `false`.
`webDOMBind(target, selector, handlers, options?)` and
`root.kryBind(selector, handlers, options?)` provide per-object lifecycle
binding for JS behavior: a mount function attaches native listeners or state to
each matching Kry DOM object, optional update/unmount callbacks follow future
renders, and any cleanup function returned from mount is called when the object
leaves the selector or the binding is unsubscribed.
`webDOMSync(target, query?)`, `root.krySync(query?)`, `element.krySync()`, and
`object.sync()` fold direct native DOM mutations back into Kry node facts and
refresh the mount root indexes, so browser-authored class, attribute, state,
text/value, and scroll changes become visible to Kry queries and KSS selectors.
Mount roots also provide non-enumerable `kryElement(query)`,
`kryObject(query)`, `kryQuery(selector)`, `kryQueryAll(selector)`, and
`kryAtSource(sourcePath, sourceLine, sourceColumn?)` methods so native browser
code can resolve `.kry` nodes without importing the module-level helpers.
They also expose query-based `kryAddClass(...)`, `krySetAttr(...)`,
`krySetStyle(...)`, `krySetState(...)`, `kryText(...)`, `kryValue(...)`,
`kryDispatch(...)`, geometry, scroll, dialog, popover, and native command
methods that mirror the module-level `webDOM*` helpers from the mount root.

`findWebNode(rt, query)` returns the normalized Web Document node whose Kry
path, node name, key, or DOM id matches `query`.
`webSourceRef(sourcePath, sourceLine, sourceColumn?)` builds the stable source
identity string for a `.kry` node. `webNodeAtSource(...)` and
`webNodesAtSource(...)` resolve those source locations back to unmounted Web
Document nodes without requiring callers to hand-format selector strings.
`webNodeAtSourceRange(...)` and `webNodesAtSourceRange(...)` resolve a cursor
position within compiler source spans.
`webSourceMap(rt)` returns the source-backed node identities in the current
frame.

`webNodeIdentity(node)` returns a plain identity projection for a Web Document
node: canonical ref, all stable aliases, kind/tag, Kry path/name/key, DOM id,
DOM name, source refs, and source range refs.
If a generated widget item does not yet carry an explicit Kry path, the web
runtime synthesizes one from the nearest known parent, widget kind, source line,
and frame index before exposing node identity. This keeps every Web Document
node addressable as a DOM object while the compiler-side all-node metadata work
continues.

`webNodeQuery(rt, selector)` and `webNodeQueryAll(rt, selector)` return
unmounted Web Document nodes by the same KSS-style selector facts used for
style resolution.

`webNodeMatches(rt, query, selector)` tests an unmounted node against the same
selector facts without requiring callers to repeat query/filter logic.

`webNodeParent(rt, query)`, `webNodeChildren(rt, query)`, and
`webNodeClosest(rt, query, selector)` expose the same `.kry` tree relationships
before a frame has been mounted into browser DOM.
`webNodeDescendants(rt, query)`, `webNodeQueryWithin(rt, query, selector)`, and
`webNodeQueryAllWithin(rt, query, selector)` provide the pre-mount version of
scoped subtree selection for compiler tests, static inspectors, and hydration
planning.

`findWebElement(target, query)` returns the mounted DOM element whose Kry path,
node name, key, DOM id, DOM name, or selector fallback matches `query`.

`webDOMQuery(target, selector)` and `webDOMQueryAll(target, selector)` return
native DOM objects by the same KSS-style selector facts used for style
resolution: kind selectors, `#id`, `.class`, `[role=...]`, `[name=...]`,
`[type=...]`, `[href=...]`, `[data-*=...]`, source fields, and state pseudos.
`webDOMObjectAtSource(...)` and `webDOMObjectsAtSource(...)` provide the same
bridge after mount, returning Kry DOM objects whose `element` is the native
browser object and whose `node` retains the `.kry` source identity.
`webDOMSourceMap(target)` and `root.krySourceMap` list all mounted
source-backed DOM objects.

`webDOMObjectFromElement(element)` walks from a native element or event target
to the nearest mounted Kry DOM object, which gives delegated browser handlers
and inspectors a reverse bridge back to `.kry` identity.

Kry DOM objects keep the plain enumerable shape `{ ref, node, element }` for
serialization and compatibility, and add non-enumerable helpers for live JS
logic. `object.root`, `object.identity`, `object.snapshot`, `object.parent`,
and `object.children` expose the mount root, identity, serializable snapshot,
and Kry tree links. Object methods such as `matches(...)`, `closest(...)`,
`listen(...)`, `addClass(...)`, `setAttr(...)`, `setStyle(...)`,
`setState(...)`, `text(...)`, `value(...)`, `dispatch(...)`, `rect(...)`,
`scroll(...)`, and native commands mirror the module-level `webDOM*` helpers
without requiring generated JS to own browser DOM shape.

`webDOMObjectFromEvent(eventOrTarget)` and `webDOMIdentityFromEvent(...)` accept
a native browser event, event target, or element and resolve the nearest Kry DOM
object/identity through the same event-target walk.
`webDOMDecorateEvent(eventOrTarget)` performs the same lookup and, for native
event objects, adds non-enumerable `kryRef`, `kryObject`, `kryIdentity`, and
`krySnapshot` getters, scalar path/kind/tag/index/source identity, plus
`kryRoot` for the mount root, so ordinary browser handlers can inspect `.kry`
identity without generated JS owning DOM structure.

`webDOMIdentity(target, query)` returns the same plain identity projection for
a mounted Kry DOM object. Rendered elements expose the native bridge directly
as non-enumerable `element.kryRef`, `element.kryNode`, `element.kryObject`,
scalar path/kind/tag/name/key/index/source identity getters, plus
`element.kryRoot`, `element.kryIdentity`, `element.krySnapshot`,
`element.kryParent`, and
`element.kryChildren` getters, plus `element.kryMatches(selector)` and
`element.kryClosest(selector)` methods for KSS-style selector checks.
Relationship fields such as `aria_controls`, `aria_owns`,
`aria_labelledby`, `aria_activedescendant`, `aria_describedby`, `dom_for`,
`form`, and `popover_target` may name another Kry DOM object by ref, path, name, key, or
native id. The Web Document facts keep the authored Kry value for KSS and
queries, while the DOM renderer resolves the native attribute to a real element
id during mount. `webDOMRelations(target, query)`, `element.kryRelations`, and
`object.relations` expose the resolved Kry DOM objects, including `owns`,
direct `labelledBy` links from `aria_labelledby`, active descendant links from
`aria_activedescendant`, form owner links from `form`, and reverse `labelledBy`
links for controls targeted by `dom_for`; snapshots include serializable
`relationRefs`.

`webDOMSnapshot(target, query)` and `webDOMSnapshots(target, selector)` return
plain, serializable views of mounted Kry DOM objects: identity, source
location, parent/child refs, attributes, dataset, style, text/value, state,
geometry, and scroll without live DOM references.
`webDOMSnapshotFromElement(element)` and `webDOMSnapshotFromEvent(...)` provide
the same serializable projection from native DOM elements and events.

`webDOMMatches(target, query, selector)` and
`webDOMElementMatches(element, selector)` test mounted DOM objects against the
same selector facts used by KSS and query helpers.

`webDOMAddEventListener(target, query, type, handler)` attaches a native event
listener to a resolved Kry DOM object and passes both the browser event and the
nearest Kry DOM object to the handler. `webDOMAddDelegatedEventListener(target,
selector, type, handler)` listens from the Kry mount root and resolves bubbling
event targets through the same selector facts. Both return an unsubscribe
function when the listener is installed.
Mount roots expose the same behavior as `root.kryListen(query, type, handler)`
and `root.kryDelegate(selector, type, handler)`. Mounted elements expose
`element.kryListen(type, handler)` for local Kry-aware native handlers.

`webDOMParent(target, query)`, `webDOMChildren(target, query)`, and
`webDOMClosest(target, query, selector)` expose the mounted `.kry` node tree as
DOM objects. This lets inspectors, tests, and host code walk from a native
element back through Kry parent/child relationships without scraping browser
markup.
`webDOMDescendants(target, query)`, `webDOMQueryWithin(target, query, selector)`,
and `webDOMQueryAllWithin(target, query, selector)` scope the same KSS-style
selector facts to one `.kry` subtree. Mount roots expose these as
`root.kryDescendants(query)`, `root.kryQueryWithin(query, selector)`, and
`root.kryQueryAllWithin(query, selector)`. Mounted elements expose
`element.kryDescendants()`, `element.kryQuery(selector)`, and
`element.kryQueryAll(selector)`, while `WebDOMObject` wrappers expose
`object.descendants`, `object.query(selector)`, and `object.queryAll(selector)`.

`webDOMAddClass(target, query, className)`, `webDOMRemoveClass(...)`,
`webDOMToggleClass(...)`, and `webDOMHasClass(...)` mutate or inspect mounted
native class names by the same query forms. Runtime class mutations are folded
back into node facts, so they survive re-render and remain visible to KSS.
Mounted elements expose the same local bridge as `element.kryAddClass(...)`,
`element.kryRemoveClass(...)`, `element.kryToggleClass(...)`, and
`element.kryHasClass(...)`.

`webDOMSetAttribute(target, query, name, value)`,
`webDOMRemoveAttribute(...)`, `webDOMGetAttribute(...)`, and
`webDOMHasAttribute(...)` expose native attributes without making generated JS
own the document shape. `data-*`, `aria-*`, global boolean attributes, form
attributes, and arbitrary extra attributes are reflected into node facts.
Mounted elements expose the same local bridge as `element.krySetAttr(...)`,
`element.kryGetAttr(...)`, `element.kryRemoveAttr(...)`, and
`element.kryHasAttr(...)`.

`webDOMSetProperty(target, query, name, value)` and
`webDOMGetProperty(...)` expose native DOM element properties. Known state,
form value, text, and scroll properties synchronize back into Web Document
facts; unknown properties remain native host state.
Mounted elements expose the same local bridge as `element.krySetProp(...)` and
`element.kryGetProp(...)`.

`webDOMSetStyle(target, query, name, value)`, `webDOMRemoveStyle(...)`, and
`webDOMGetStyle(...)` apply imperative style overrides after resolved KSS.
These overrides are intended for browser-measured or runtime-only state; KSS
remains the authoring surface for visual design.
Mounted elements expose local `element.krySetStyle(...)`,
`element.kryGetStyle(...)`, and `element.kryRemoveStyle(...)` methods.

`webDOMComputedStyle(target, query, name)` reads the browser-computed style for
a Kry DOM object when `getComputedStyle` is available, and falls back to the
element style object in non-browser hosts.
Mounted elements expose `element.kryComputedStyle(name)` for the same lookup.

`webDOMGetText(target, query)`, `webDOMSetText(...)`, `webDOMGetValue(...)`,
and `webDOMSetValue(...)` read and write current mounted text and form values.
Mutations synchronize back to the Web Document node before the next render.
Mounted elements expose `element.kryText()` / `element.kryText(value)` and
`element.kryValue()` / `element.kryValue(value)` for the same operations.

`webDOMSetState(target, query, name, value)`, `webDOMToggleState(...)`, and
`webDOMGetState(...)` expose node state facts for logic-owned state that should
also participate in KSS selectors.
Mounted elements expose `element.krySetState(...)` and
`element.kryGetState(...)` for local state fact changes.

`webDOMClick(target, query)`, `webDOMFocus(...)`, `webDOMBlur(...)`,
`webDOMSubmit(...)`, and `webDOMReset(...)` issue native commands when the
mounted element supports them and fall back to dispatching the corresponding
event.
Mounted elements expose `element.kryClick()`, `element.kryFocus()`,
`element.kryBlur()`, `element.krySubmit()`, and `element.kryReset()`.

`webDOMShowModal(target, query)`, `webDOMClose(...)`,
`webDOMShowPopover(...)`, `webDOMHidePopover(...)`, and
`webDOMTogglePopover(...)` expose native dialog and popover behavior from the
same Kry node identity surface.
Mounted elements expose `element.kryShowModal()`, `element.kryClose(...)`,
`element.kryShowPopover()`, `element.kryHidePopover()`, and
`element.kryTogglePopover(...)`.

`webDOMDispatchEvent(target, query, type, init)` dispatches arbitrary browser
events against a resolved DOM object and decorates the event with the same Kry
event bridge exposed to native handlers. Prefer named helpers for stable app
logic; this exists for host integrations and tests.
Mounted elements expose `element.kryDispatch(type, init)` for the same
Kry-aware event dispatch.

`webDOMRect(target, query)`, `webDOMGetScroll(...)`, `webDOMSetScroll(...)`,
and `webDOMScrollIntoView(...)` expose measured geometry and scroll state.
Scroll mutations are reflected into node facts as `scrollLeft` and `scrollTop`
so KSS can react to native browser position when needed.
Mounted elements expose `element.kryRect()`, `element.kryScroll()` /
`element.kryScroll(left, top)`, and `element.kryScrollIntoView(...)`.

The DOM renderer maintains native interaction facts for KSS state selectors:
`mouseenter`/`mouseleave` update `hover`, `mousedown`/`mouseup` update
`pressed`, `focus`/`blur` update `focus`, and native dialog/popover lifecycle
events update `open`. These update the Web Document node state and reapply
resolved KSS without requiring app logic to mirror browser pseudo-state.

`webFormValue(target, query)` and `webFormValues(target, query?)` expose current
mounted native form values by Kry ref, path, node name, key, DOM id, and DOM
name. Values are refreshed on render and after native `input`/`change` events.
Passing a form query to `webFormValues` returns values for descendant controls
and controls whose `form` relation points at that Kry form object.

`webAccessibilitySnapshot(rtOrFrame)` returns a compact accessibility-facing
projection of the Web Document frame: document title/description plus each
node's path, name, DOM id, classes, role, label, text, value, href, input type,
heading level, and state. The DOM renderer also writes accessibility state such
as `aria-checked`, `aria-disabled`, `aria-busy`, and selected-link
`aria-current` when those facts are present.

`GetRoutePath()`, `GetRouteHash()`, and `GetRouteVersion()` expose browser route
state to generated logic. `MatchRoute(pattern, path?)` matches paths with
`:param` segments, while `GetRouteParams()` and `GetRouteParam(name)` expose
the params captured by generated route dispatch. `PushRoute(path)` and
`ReplaceRoute(path)` update native browser history when available and use the
same in-memory route state in non-browser tests.

## Current Limits

- `renderWebDocument()` reuses DOM nodes by tag and Kry path, and nests nodes
  beneath their Kry parent when the parent is present in the frame.
- Route helpers expose path/hash changes, and k2js can dispatch route pages
  declared in `.kry`, including explicit paths with nested `:param` segments.
- Event handling covers click-to-`QueueTap`, text/form events, key events,
  submit/reset, focus/blur, scroll, mouse and pointer enter/leave/move/down/up,
  wheel, drag/drop, clipboard actions, and native dialog/popover lifecycle
  events. Native hover/pressed/focus/open facts are maintained for KSS state
  selectors.
- KSS parsing exists in C for style-rule tables and in the JS runtime for web
  DOM style application; k2js embeds resolvable style imports from local files,
  built-in Kryon packs, and project-local style packages. Package
  registry/config discovery and every style property are still incremental.
- Query-based DOM lookup, mutation, commands, event dispatch, geometry, scroll,
  form value lookup, relation lookup, and accessibility snapshots are
  available. Additional per-widget semantic relationships beyond controls,
  described-by, labels, and popovers are still incremental.
