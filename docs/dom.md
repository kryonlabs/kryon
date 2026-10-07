# Browser document host

Select `backend = "dom"` in a Kryon profile. The host builds Ziran through
Emscripten and reconciles successful committed trees into browser elements.
Retained generation IDs preserve element identity. Unchanged text, attributes,
styles and order stay untouched; reordering uses `moveBefore` where available
and restores focus/selection when falling back to insertion. Nested absolute
positions are relative to their parents. Container labels use ARIA instead
of replacing their children with text.

Native button activation, editor focus, input, selection and browser IME
return to Kryon's shared session/widget logic. Applications still own values,
apply ordinary returned edits, and retain cursor/anchor state. Browser UTF-16
selection offsets convert to grapheme-aligned UTF-8 byte offsets. Multiple
inputs to one editor coalesce before a frame; another editor waits for the
session's current request. Read-only and disabled controls use native
properties. Password values travel through a private presentation callback,
never the semantic tree or capture snapshots; password copy/cut is blocked.
Native edit requests are bounded to 64 KiB.

The default remains an absolutely positioned, transparent semantic overlay
on Canvas2D. For document widgets, call this before the first frame:

```ziran
Dom :: #import "kryon/Dom";
// css is an application-owned CSS string, optionally exported by KSS.
Dom.UseDocumentLayout(css)
```

This hides the owned canvas and presents native elements in normal browser
flow. CSS supplies responsive widths, layout and appearance. Supported kinds
are Screen, Text, Paragraph, Page, Section, Link, Image, Button, TextField,
TextArea, Group, Column, Row, Stack and Grid. Other kinds reject the frame so
canvas-only content cannot disappear silently. Browser layout geometry is
owned by CSS; the core retained geometry remains the application's submitted
layout. Prefer native identity activation over canvas hit tests in this mode.

DOM elements carry `data-kryon-widget` names and hashed `data-kryon-class`
values for supported styled text and controls. The separate KSS package
exports `ExportCSS(source, path, environment, output)` and `StyleClass(name)`.
Its exporter shares the existing parser and token/environment rules, scopes
selectors to `#kryon-dom-root`, maps widget/class/state selectors and expands
portable declarations. Media conditions are preserved. Check its `ok` result
before attaching output. Imports, layers, nested selector functions and
unsupported native materials fail explicitly; consult the KSS package for
its current export subset.

The DOM root and private presentation handles are removed at close. A
text/semantics-only test snapshot omits secure values. Verification runs a
new headless Chromium process with desktop variables removed:

```sh
python3 tests/dom_project_test.py
KRYON_DOM_INTERACTION=1 python3 tests/dom_project_test.py
```

The second check uses KSS and verifies no DOM mutations during unchanged
frames, Unicode editing, identity/focus/caret after reorder, button routing,
responsive sizing, password redaction and a native editor in the browser's
accessibility tree. Neither test uses the owner's desktop or browser profile.
