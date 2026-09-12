# Kryon Web Document Frame

Status: initial implementation contract

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

Supported metadata fields in this first slice:

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
| `placeholder`, `dom_placeholder` | `placeholder` |
| `input_type`, `dom_type`, `html_type`, `dom_input_type` | `inputType` |
| `tab_index`, `tabindex`, `dom_tab_index` | `tabIndex` |
| `role` | `role` |
| `aria_label`, `accessible_label` | `ariaLabel` |
| `aria_description`, `accessible_description` | `ariaDescription` |
| `aria_describedby`, `aria_described_by` | `ariaDescribedBy` |
| `aria_controls` | `ariaControls` |
| `aria_live`, `live` | `ariaLive` |
| `on_click` | `onClick`, `action` |
| `on_input` | `onInput`, `inputAction(value)` |
| `on_change` | `onChange`, `changeAction(value)` |
| `on_key`, `on_key_down` | `onKey`, `keyAction(key)` |
| `on_submit` | `onSubmit`, `submitAction(values)` |
| `on_focus` | `onFocus`, `focusAction()` |
| `on_blur` | `onBlur`, `blurAction()` |
| `on_mouse_enter`, `on_pointer_enter` | `onMouseEnter`, `mouseEnterAction()` |
| `on_mouse_leave`, `on_pointer_leave` | `onMouseLeave`, `mouseLeaveAction()` |
| `on_mouse_down`, `on_pointer_down` | `onMouseDown`, `mouseDownAction()` |
| `on_mouse_up`, `on_pointer_up` | `onMouseUp`, `mouseUpAction()` |

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
      inputType,
      alt,
      asset,
      role,
      ariaLabel,
      onClick,
      onInput,
      onChange,
      action,
      inputAction,
      changeAction,
      pageTitle,
      pageDescription,
      pageCanonicalURL,
      pageThemeColor,
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
| `Button`, `InvisibleButton` | `button` |
| `TextField` | `input type=text` |
| `TextArea` | `textarea` |
| `Image`, `PageImage` | `img` |
| `Checkbox`, `Toggle` | `input type=checkbox` |
| `Radio` | `input type=radio` |

Other widgets remain `div` nodes until they gain a specific web-native
contract.

## KSS Fit

KSS should resolve against each node's `styleFacts`: `kind`, `tag`, `key`,
`name`, `path`, `parentPath`, `id`, `domName`, `href`, `target`, `rel`,
`inputType`, `classes`, `dataAttrs`, `role`, and `state`. The DOM
backend may translate resolved KSS values to CSS variables, classes, or style
attributes, but browser CSS is an output detail rather than the authoring source
of truth.

The JavaScript runtime exposes `parseWebStyleSheet(source)`,
`resolveWebStyle(node, sheets)`, and `setWebStyleSheets(rt, sheets)` for the
same bridge in browser-hosted k2js apps. k2js embeds KSS source text in
`app.styles[].source` when a `#style` import resolves on disk, and
`createRuntime({ app })` installs those embedded sheets automatically. This
first web resolver supports the initial KSS grammar slice: kind selectors,
`#id`, `.class`, `[role=...]`, `[state=...]`, state pseudos, layers, colors,
spacing, radius, border width, opacity, font size, and local
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
node name, key, DOM id, or DOM name matches `query`.

`webDOMQuery(target, selector)` and `webDOMQueryAll(target, selector)` return
native DOM objects by the same KSS-style selector facts used for style
resolution: kind selectors, `#id`, `.class`, `[role=...]`, `[name=...]`,
`[type=...]`, `[href=...]`, `[data-*=...]`, source fields, and state pseudos.

The DOM renderer maintains native interaction facts for KSS state selectors:
`mouseenter`/`mouseleave` update `hover`, `mousedown`/`mouseup` update
`pressed`, and `focus`/`blur` update `focus`. These update the Web Document node
state and reapply resolved KSS without requiring app logic to mirror browser
pseudo-state.

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
- Event handling covers click-to-`QueueTap`, `on_click`, `on_input(value)`,
  `on_change(value)`, `on_key(key)`, `on_submit(values)`, `on_focus()`,
  `on_blur()`, mouse/pointer enter/leave/down/up actions, and native
  hover/pressed/focus facts for KSS state selectors in this slice.
- KSS parsing exists in C for style-rule tables and in the JS runtime for web
  DOM style application; k2js embeds resolvable style imports, but package
  discovery beyond `styles/kryon/<pack>.kss` and every style property are still
  incremental.
- Form value lookup and accessibility snapshots are available; deeper
  per-widget ARIA relationships, such as controlled regions and described-by
  chains, are still incremental.
