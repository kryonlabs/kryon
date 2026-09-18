# Events And DOM Commands

Goal: generated JavaScript wires logic to native events, while DOM structure
and identity remain Kry-owned.

## Event Surface

Supported event facts should cover click/tap, double click, input, before
input, change, select, key down, key up, invalid, submit, reset, toggle, close,
cancel, focus, blur, scroll, pointer, mouse, wheel, context menu, drag/drop,
clipboard, dialog, and popover lifecycle events.

## Steps

1. Keep authored `on_*` fields as event refs in Web Document nodes.
2. Render `data-kry-on-*` hooks for inspector visibility.
3. Decorate native events with non-enumerable Kry helpers:
   `kryRef`, `kryObject`, `kryIdentity`, `krySnapshot`, and traversal helpers.
4. Provide direct listener helpers such as `webDOMAddEventListener(...)`,
   delegated listener helpers, and mounted element convenience APIs.
5. Provide native commands for click, focus, blur, submit, reset, dialog,
   popover, dispatch, text, value, scroll, style, attrs, properties, class, and
   state mutation.
6. Synchronize mutations back into Web Document facts before re-render.

## Evidence

- Fake DOM tests prove event refs and command helpers.
- Browser tests prove native dispatch, decorated events, and bubbling lookup.
- Snapshots expose event refs without scraping raw `data-kry-on-*` attributes.
