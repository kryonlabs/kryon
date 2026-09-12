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

Supported metadata fields in this first slice:

| `.kry` field | Web frame field |
|---|---|
| named block | `nodeName`, `key`, `name`, `path`, `parentPath` |
| `dom`, `dom_tag`, `html_tag`, `tag` | `tag` |
| `dom_id`, `html_id` | `domId` |
| `class`, `classes`, `class_name` | `classes` |
| `role` | `role` |
| `aria_label`, `accessible_label` | `ariaLabel` |
| `on_click` | `onClick`, `action` |
| `on_input` | `onInput`, `inputAction(value)` |
| `on_change` | `onChange`, `changeAction(value)` |

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
      classes,
      text,
      href,
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
      state
    }
  ]
}
```

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

KSS should resolve against the facts in this frame: `kind`, `name`, `classes`,
semantic attributes, and state. The DOM backend may translate resolved KSS
values to CSS variables, classes, or style attributes, but browser CSS is an
output detail rather than the authoring source of truth.

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

`findWebElement(target, query)` returns the mounted DOM element whose Kry path,
node name, key, or DOM id matches `query`.

`GetRoutePath()`, `GetRouteHash()`, and `GetRouteVersion()` expose browser route
state to generated logic. `PushRoute(path)` and `ReplaceRoute(path)` update
native browser history when available and use the same in-memory route state in
non-browser tests.

## Current Limits

- `renderWebDocument()` reuses DOM nodes by tag and Kry path, and nests nodes
  beneath their Kry parent when the parent is present in the frame.
- Route helpers expose path/hash changes, but there is not yet a declarative
  `.kry` route-to-node mapping.
- Event handling covers click-to-`QueueTap`, `on_click`, `on_input(value)`,
  and `on_change(value)` actions in this slice.
- KSS parsing and compiled style tables are not implemented yet.
- Full form value synchronization and ARIA snapshots remain pending.
