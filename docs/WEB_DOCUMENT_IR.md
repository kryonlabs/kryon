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
```

`k2js` emits these as `app.styles` so a web host can load built-in and file
style sheets without scanning source text. KSS still owns style resolution;
the Web Document frame supplies the node facts it resolves against.

Rendered DOM elements carry source identity as native attributes:
`data-kry-ref`, `data-kry-path`, `data-kry-parent-path`, `data-kry-name`,
`data-kry-key`, `data-kry-kind`, `data-kry-source`, and `data-kry-line`.
The runtime exposes
`webDOMObject(target, query)` and `webDOMObjects(target)` so JS logic,
inspectors, tests, and hydration code can ask for native DOM objects by `.kry`
path, node name, key, or DOM id without making generated JavaScript the source
of structure. `sourcePath` and `sourceLine` identify the `.kry` source location
that produced each node. Anonymous widget expressions receive source-derived
path components such as `Text@42`; repeated anonymous widgets under the same
parent receive deterministic occurrence suffixes such as `Text@42-2` so every
DOM object remains individually addressable.

Supported metadata fields:

| `.kry` field | Web frame field |
|---|---|
| named block | `nodeName`, `key`, `name`, `path`, `parentPath` |
| source span | `sourcePath`, `sourceLine` |
| `dom`, `dom_tag`, `html_tag`, `tag` | `tag` |
| `dom_id`, `html_id` | `domId` |
| `dom_name`, `html_name`, `name_attr` | `domName` |
| `class`, `classes`, `class_name` | `classes` |
| `title`, `dom_title`, `html_title` | `title` |
| `dom_href`, `html_href` | `href` |
| `dom_target`, `html_target` | `target` |
| `dom_rel`, `html_rel` | `rel` |
| `data_*`, `dom_data_*`, `html_data_*` | `dataAttrs` |
| `attr_*`, `dom_attr_*`, `html_attr_*` | `extraAttrs` |
| `placeholder`, `dom_placeholder` | `placeholder` |
| `input_type`, `dom_type`, `html_type`, `dom_input_type` | `inputType` |
| `dom_action`, `html_action`, `form_action` | `formAction` |
| `dom_method`, `html_method`, `form_method` | `formMethod` |
| `dom_enctype`, `html_enctype`, `form_enctype` | `formEncType` |
| `autocomplete`, `dom_autocomplete`, `html_autocomplete` | `autoComplete` |
| `hidden`, `dom_hidden`, `html_hidden` | `hidden` |
| `draggable`, `dom_draggable`, `html_draggable` | `draggable` |
| `spellcheck`, `spell_check`, `dom_spellcheck`, `html_spellcheck` | `spellCheck` |
| `contenteditable`, `content_editable`, `dom_contenteditable`, `html_contenteditable` | `contentEditable` |
| `autofocus`, `auto_focus`, `dom_autofocus`, `html_autofocus` | `autoFocus` |
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
| `aria_controls` | `ariaControls` |
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
      dataAttrs,
      ariaAttrs,
      extraAttrs,
      inputType,
      formAction,
      formMethod,
      formEncType,
      autoComplete,
      hidden,
      draggable,
      spellCheck,
      contentEditable,
      autoFocus,
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
browser path; later routes use `/<route-id>`.

The initial tag mapping is intentionally conservative:

| Kryon kind | Browser tag |
|---|---|
| `Screen`, `Page` | `main` |
| `Section` | `section` |
| `Heading` | `h1`-`h6` |
| `Paragraph`, `ParagraphText` | `p` |
| `Link` | `a` |
| `Button` | `button` |
| invisible hit-test support | `button` |
| `TextField` | `input type=text` |
| `TextArea` | `textarea` |
| `Image` with `alt_text` | `img` |
| `Checkbox`, `Toggle` | `input type=checkbox` |
| `Radio` | `input type=radio` |

Other widgets remain `div` nodes until they gain a specific web-native
contract.

## KSS Fit

KSS should resolve against each node's `styleFacts`: `kind`, `tag`, `key`,
`name`, `path`, `parentPath`, `sourcePath`, `sourceLine`, `id`, `domName`,
`href`, `target`, `rel`, `inputType`, `formAction`, `formMethod`,
`formEncType`, `autoComplete`, `hidden`, `draggable`, `spellCheck`,
`contentEditable`, `autoFocus`, `download`, `formNoValidate`, `noValidate`,
`popover`, `popoverTarget`, `popoverTargetAction`, `readOnly`, `required`,
`min`, `max`, `step`, `minLength`, `maxLength`, `pattern`, `accept`,
`multiple`, `inputMode`, `classes`, `dataAttrs`, `ariaAttrs`, `extraAttrs`,
`role`, `open`, `scrollLeft`, `scrollTop`, and `state`. The DOM
backend may translate resolved KSS values to CSS variables, classes, or style
attributes, but browser CSS is an output detail rather than the authoring source
of truth.

The JavaScript runtime exposes `parseWebStyleSheet(source)`,
`resolveWebStyle(node, sheets)`, and `setWebStyleSheets(rt, sheets)` for the
same bridge in browser-hosted k2js apps. k2js embeds KSS source text in
`app.styles[].source` when a `#style` import resolves on disk, and
`createRuntime({ app })` installs those embedded sheets automatically. The web
resolver supports kind selectors, `#id`, `.class`, source identity selectors
such as `[source=...]` and `[line=...]`, `[role=...]`, `[state=...]`,
native attribute aliases such as `[name=...]`, `[type=...]`, `[href=...]`,
`[target=...]`, `[rel=...]`, `[action=...]`, `[method=...]`,
`[enctype=...]`, `[autocomplete=...]`, `[hidden=true]`,
`[draggable=true]`, `[spellcheck=...]`, `[contenteditable=...]`,
`[download=...]`, `[readonly=true]`, `[required=true]`, `[popover=...]`,
`[popovertarget=...]`, `[popovertargetaction=...]`, `[min=...]`,
`[max=...]`, `[step=...]`, `[minlength=...]`, `[maxlength=...]`,
`[pattern=...]`, `[accept=...]`, `[multiple=true]`, `[inputmode=...]`,
data/ARIA/extra attribute selectors, state pseudos, layers, colors, spacing,
radius, border width, opacity, font size, and local
`tokens { color { ... } length { ... } material { ... } }` references.

The frame is also the right place for inspector data: matched KSS rules,
winning declarations, token origins, state slice, and backend degradation can
attach to nodes without changing app logic.

## Runtime DOM APIs

`renderWebDocument(rt, target)` reconciles the Web Document frame into browser
elements. It also applies document metadata from `SetPageTitle`,
`SetPageDescription`, `SetPageCanonicalURL`, `SetPageThemeColor`, app metadata,
and `Page` nodes.

`findWebNode(rt, query)` returns the normalized Web Document node whose Kry
path, node name, key, or DOM id matches `query`.

`webNodeQuery(rt, selector)` and `webNodeQueryAll(rt, selector)` return
unmounted Web Document nodes by the same KSS-style selector facts used for
style resolution.

`findWebElement(target, query)` returns the mounted DOM element whose Kry path,
node name, key, DOM id, DOM name, or selector fallback matches `query`.

`webDOMQuery(target, selector)` and `webDOMQueryAll(target, selector)` return
native DOM objects by the same KSS-style selector facts used for style
resolution: kind selectors, `#id`, `.class`, `[role=...]`, `[name=...]`,
`[type=...]`, `[href=...]`, `[data-*=...]`, source fields, and state pseudos.

`webDOMObjectFromElement(element)` walks from a native element or event target
to the nearest mounted Kry DOM object, which gives delegated browser handlers
and inspectors a reverse bridge back to `.kry` identity.

`webDOMParent(target, query)`, `webDOMChildren(target, query)`, and
`webDOMClosest(target, query, selector)` expose the mounted `.kry` node tree as
DOM objects. This lets inspectors, tests, and host code walk from a native
element back through Kry parent/child relationships without scraping browser
markup.

`webDOMAddClass(target, query, className)`, `webDOMRemoveClass(...)`,
`webDOMToggleClass(...)`, and `webDOMHasClass(...)` mutate or inspect mounted
native class names by the same query forms. Runtime class mutations are folded
back into node facts, so they survive re-render and remain visible to KSS.

`webDOMSetAttribute(target, query, name, value)`,
`webDOMRemoveAttribute(...)`, `webDOMGetAttribute(...)`, and
`webDOMHasAttribute(...)` expose native attributes without making generated JS
own the document shape. `data-*`, `aria-*`, global boolean attributes, form
attributes, and arbitrary extra attributes are reflected into node facts.

`webDOMSetStyle(target, query, name, value)`, `webDOMRemoveStyle(...)`, and
`webDOMGetStyle(...)` apply imperative style overrides after resolved KSS.
These overrides are intended for browser-measured or runtime-only state; KSS
remains the authoring surface for visual design.

`webDOMGetText(target, query)`, `webDOMSetText(...)`, `webDOMGetValue(...)`,
and `webDOMSetValue(...)` read and write current mounted text and form values.
Mutations synchronize back to the Web Document node before the next render.

`webDOMSetState(target, query, name, value)`, `webDOMToggleState(...)`, and
`webDOMGetState(...)` expose node state facts for logic-owned state that should
also participate in KSS selectors.

`webDOMClick(target, query)`, `webDOMFocus(...)`, `webDOMBlur(...)`,
`webDOMSubmit(...)`, and `webDOMReset(...)` issue native commands when the
mounted element supports them and fall back to dispatching the corresponding
event.

`webDOMShowModal(target, query)`, `webDOMClose(...)`,
`webDOMShowPopover(...)`, `webDOMHidePopover(...)`, and
`webDOMTogglePopover(...)` expose native dialog and popover behavior from the
same Kry node identity surface.

`webDOMDispatchEvent(target, query, type, init)` dispatches arbitrary browser
events against a resolved DOM object. Prefer named helpers for stable app
logic; this exists for host integrations and tests.

`webDOMRect(target, query)`, `webDOMGetScroll(...)`, `webDOMSetScroll(...)`,
and `webDOMScrollIntoView(...)` expose measured geometry and scroll state.
Scroll mutations are reflected into node facts as `scrollLeft` and `scrollTop`
so KSS can react to native browser position when needed.

The DOM renderer maintains native interaction facts for KSS state selectors:
`mouseenter`/`mouseleave` update `hover`, `mousedown`/`mouseup` update
`pressed`, `focus`/`blur` update `focus`, and native dialog/popover lifecycle
events update `open`. These update the Web Document node state and reapply
resolved KSS without requiring app logic to mirror browser pseudo-state.

`webFormValue(target, query)` and `webFormValues(target)` expose current mounted
native form values by Kry path, node name, key, DOM id, and DOM name. Values are
refreshed on render and after native `input`/`change` events.

`webAccessibilitySnapshot(rtOrFrame)` returns a compact accessibility-facing
projection of the Web Document frame: document title/description plus each
node's path, name, DOM id, classes, role, label, text, value, href, input type,
heading level, and state. The DOM renderer also writes accessibility state such
as `aria-checked`, `aria-disabled`, `aria-busy`, and selected-link
`aria-current` when those facts are present.

`GetRoutePath()`, `GetRouteHash()`, and `GetRouteVersion()` expose browser route
state to generated logic. `PushRoute(path)` and `ReplaceRoute(path)` update
native browser history when available and use the same in-memory route state in
non-browser tests.

## Current Limits

- `renderWebDocument()` reuses DOM nodes by tag and Kry path, and nests nodes
  beneath their Kry parent when the parent is present in the frame.
- Route helpers expose path/hash changes, and k2js can dispatch route pages
  declared in `.kry`; nested route parameters are not parsed yet.
- Event handling covers click-to-`QueueTap`, text/form events, key events,
  submit/reset, focus/blur, scroll, mouse and pointer enter/leave/move/down/up,
  wheel, drag/drop, clipboard actions, and native dialog/popover lifecycle
  events. Native hover/pressed/focus/open facts are maintained for KSS state
  selectors.
- KSS parsing exists in C for style-rule tables and in the JS runtime for web
  DOM style application; k2js embeds resolvable style imports, but package
  discovery beyond `styles/kryon/<pack>.kss` and every style property are still
  incremental.
- Query-based DOM lookup, mutation, commands, event dispatch, geometry, scroll,
  form value lookup, and accessibility snapshots are available. Deeper
  per-widget ARIA relationships, such as controlled regions and described-by
  chains, are still incremental.
