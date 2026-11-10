# Kryon portable components

A Kryon component is a separately bundled Ziran entry plus its declared host
capabilities. `ComponentHost` owns stable namespace/name identities and up to
sixteen live components. All components share the host's retained tree, input
session, style state, paint queue and renderer. Their owned widget state stays
inside each ZX instance.

## Lifecycle

Open a component with `ComponentHostOpen`, run slots together with
`ComponentHostFrame`, and replace one with:

```ziran
ComponentHostReplace :: (host: *ComponentHost, id: u64, zx: []u8) -> bool
```

`ComponentHostAcquire`, `ComponentHostHandleValid`,
`ComponentHostCloseHandle`, `ComponentHostLiveBytes` and `ComponentHostClose`
complete the public lifecycle. Ziran's bundle-component ABI checks code and
capability identity. Kryon additionally records each admitted capability's
typed ABI descriptor per slot.

Replacement opens the candidate outside the live registry, checks its typed
ABI, and migrates its owned state. Verification composes the migrated candidate
on a private session and commits its tree/paint queue without flushing any
raster effects. Staged text editors return clipboard intents without reading or
writing the system clipboard, and staging does not bind audio services. The
active tree, focus, input and framebuffer stay untouched.
The validation frame's state changes are discarded before Ziran atomically
commits replacement. Each component has a stable retained-tree scope, so local
widget keys may repeat in different components. All sixteen slots may be live
while a replacement is staged. A failed ABI, initialization, schema, resource or
verification candidate is closed and the old component remains runnable.
Unrelated slots keep their state and handles. A successful replacement
invalidates old handles.

`COMPONENT_CAPABILITY_HOST`, `COMPONENT_CAPABILITY_INPUT`,
`COMPONENT_CAPABILITY_WIDGET` and `COMPONENT_CAPABILITY_TREE` are independent
grants. A component that imports only text therefore does not carry button or
renderer code merely because another slot uses them.

State migration matches portable global values by module and name. Scalars
move directly; the recursive matcher also supports matching owned strings,
fixed arrays and nested records by field name and type. Borrowed pointers and
slices are rejected. The replacement's `component_schema_version` in its entry module is retained
and must meet the slot's minimum version. Other matching globals migrate. New globals keep their initialized values.
The counter test fixtures preserve a nested record, an owned string and a fixed
array as well as scalar state; the replacement keeps its new initialized
suffix field.

`ComponentHostLiveBytes` returns Ziran's owned portable-value payload bytes.
It is not immutable code bytes, bundle bytes, renderer memory or process RSS.

## Local integration test

Replacement versions, status guests, and deliberate failures live under
`tests/fixtures/component_*`. They are test inputs, separate from the public
component usage examples.

Run `build-queue -- env -u DISPLAY -u WAYLAND_DISPLAY -u XAUTHORITY -u DBUS_SESSION_BUS_ADDRESS sh tests/ziran_component_host_test.sh`.
The test builds each selective guest from both source and saved IR and compares
ZX bytes. A native Ziran host runs two live components into the retained tree
and CPU pixmap renderer. It checks interaction, nested-state migration,
ABI/init/schema/resource and migrated-frame rejection, unchanged pixels/focus/
queued input on rollback, staged clipboard isolation with a live-copy positive
control, independent local keys, stale handles and replacement
with every registry slot occupied. It reports bundle/gzip size, swap/frame time,
owned-value bytes and process peak RSS. These are local native/portable-VM
results; they do not establish Android device integration.

## Browser integration test

Run `build-queue -- env -u DISPLAY -u WAYLAND_DISPLAY -u XAUTHORITY -u DBUS_SESSION_BUS_ADDRESS YUE_DESKTOP_RECOVERY=0 PYTHONDONTWRITEBYTECODE=1 python3 tests/component_browser_test.py`.
The dedicated Zi/Wasm host uses the production `ComponentHost`, `WidgetHost`,
Canvas2D renderer and canonical Ziran browser runtime. A disposable headless
Chromium profile receives actual pointer and key events. WebGL is disabled and
all WebGL context requests are counted. Source and saved-IR guest bundles must
match byte for byte.

The test proves migrated state, old-handle invalidation, independent B text
input after swapping A, focus and queued-input continuity, and unchanged live
pixel hashes through ABI/schema/resource/initialization/truncation and migrated
render rejection. The same clipboard-copy candidate is accepted with initial
state and rejected with changed state, proving the render verifier executed.
Staging performs zero clipboard calls; a normal editor copy reaches the real
browser clipboard API as a positive control. It reports measured browser swap
time and owned-value bytes. No visible desktop or Browser MCP is used.

## Caller-owned widget state

TagInput, OneTimeCode, InlineEdit, ScrubField, MoneyField and ShortcutRecorder,
and Tooltip, Sheet, CommandPalette, HoverCard, ConfirmPopover and GuidedTour
share their canonical native composition with hosted guests. The boundary
passes typed props and owned state values and returns both updated state and
the widget result. No caller pointer crosses the boundary. Command, tour and
initial-tag slices are decoded into bounded host storage. SheetBegin/SheetEnd
also preserve the host scope stack for caller-composed content.

The component tests edit all six input states, exercise all six overlays, then
reject a clipboard-producing candidate and migrate those records to a second
ZX version. The native CPU renderer and actual Canvas2D browser host both use
this fixture. State storage bounds are validated before composition; hosted
TagInput currently permits 32 tags, matching its 2048-byte storage at 64 bytes
per tag. Android device coverage for this fixture remains pending.
