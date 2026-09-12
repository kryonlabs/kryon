# Kryon Web Document Frame

Status: initial implementation contract

The Web Document frame is the browser-facing structure produced from a Kryon
runtime frame. It is not browser DOM, CSS, or a second widget API. It is the
typed bridge between `.kry` structure, future KSS style resolution, and native
browser presentation.

## Purpose

The existing JavaScript runtime records generated widget calls and can simulate
logic. The Web Document frame gives that stream a stable semantic shape:

- widget kind, key, name, classes, state, and bounds;
- native element tag selection for common web-capable widgets;
- text, links, image sources, input type, and accessibility-facing state;
- one frame format that can be consumed by DOM reconciliation, tests, and KSS.

JavaScript remains responsible for generated logic, event glue, host calls, and
browser bootstrapping. Widget structure stays in `.kry`; visual styling belongs
to KSS.

## Runtime Contract

`webDocumentFrame(rt)` returns:

```js
{
  app: rt.app,
  nodes: [
    {
      index,
      kind,
      tag,
      key,
      name,
      classes,
      text,
      href,
      inputType,
      alt,
      asset,
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

## Current Limits

- `mount()` currently rebuilds the DOM from the frame; keyed reconciliation is
  the next step.
- Event handling is click-to-`QueueTap` only in this first slice.
- KSS parsing and compiled style tables are not implemented yet.
- Full form value synchronization and ARIA snapshots remain pending.
