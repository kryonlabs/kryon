# Kryon portable components

The TSX migration also introduces native metadata for mounted application
components. This is separate from the portable bundle registry described below.
Neither foundation establishes complete TSX authoring coverage yet.

## Mounted component implementation

`component_runtime` supplies the internal typed begin/end scope used by component
bindings, plus `Current(session)` for native model initialization inside that
scope. Hosted calls obtain the same identity from the retained host. Identity
includes the parent instance, retained parent generation,
component declaration, authored location and explicit key or occurrence.
Identical component types have separate instances; keyed movement keeps the
instance. Duplicate keys fail the frame transaction. The metadata owns no second
layout, paint, input, focus or accessibility tree.

The ordinary typed `Compose(ComposeProps)` boundary runs a borrowed `Scoped`
body synchronously between begin and end. Its authored TSX fixture mounts
typed counter cells and submits real `Box` nodes. Authored components take
`origin: Source_Code_Location` after their props parameter and forward it into
`Compose`: native Ziran bindings default `location := #caller_location`, and
TSX views use the standard-syntax intrinsic
`location: Source_Code_Location = nativeCallerLocation()`. The native scope
uses the caller's file, line and column, so two ordinary `<Counter/>` calls on
one line have separate sites.
The compiler supplies ordinary parameter defaults and source locations; the
component scope remains library code.

The fixture checks same-line calls, keyed movement, nested parent identity,
conditional removal and remount, duplicate-key rollback, cleanup and stale
writes through source and saved IR in C, Go and ZX using the published compiler
pin, including byte-identical source/saved ZX. Both C forms pass ASan, UBSan
and leak detection. Raster effects use the existing test host; this is a retained
node and state check without a pixel-rendering claim.

Initialization follows the returned `fresh` flag. Successful frames retain seen
instances; removal disposes children before parents, in reverse resource
registration order. A cancelled frame disposes its new instances and resources.
Session epochs and monotonic instance/resource generations reject stale handles.
Cleanup and timer actions cannot begin, commit or cancel a nested frame or close
their active session.

Native cleanup, callback and timer actions currently take a typed stable
`component_types.Instance`. They run on the UI thread. Desktop and native Android
waits respect the next timer deadline; pending timers do not request continuous
painting. Portable `App.Run` samples the host clock before composition for its
shared input snapshot, retained motion and component timers, and sends absolute
deadlines to the desktop player, native executable host and embedded
Canvas/Android players. Timer callers use the same clock for absolute due times.
Low-level lifecycle callers can instead supply time through `Tick`.
Plain widget frames import component procedure slots without the component
resource/timer store. Normal exit and failed frames close the guest session
before host resources or bundle code are released. Hosted component slots retire
guest resources on close or successful replacement. Replacement retains native
value identities so matching widget nodes keep their focus and queued input.

Render-observed events use `component_lifecycle.Queue(token)`. Queuing does not
invoke an action while composition reads its snapshots. A successful frame
accepts its events; cancellation discards that frame's events. Before the next
composition, `Tick` or `FrameStart` pumps an accepted batch in insertion order,
checking each token again at delivery. Removal or cancellation makes a queued
token harmless. Events queued by an action wait for the next batch. The pump
blocks nested frame operations and session close while allowing model writes.
Accepted live events report an immediate wake deadline; they do not request
continuous painting. Closing a session discards all pending events.
`component_state` supplies typed native `Store(T)`, `Handle(T)` and `Snapshot(T)`
records. A mounted instance can own several independent cells of the same type;
the cell key identifies each one within its store and instance. Repeating a
mount returns the existing handle without replacing its value. Handles contain
generation-checked resource tokens, with no pointer to the stored value. Writes
are rejected during composition and disposal. Reads use a native typed clone
procedure to give each snapshot an independently owned value and revision.
Retirement invalidates the token before releasing its cell, including a new cell
created on an existing instance during a cancelled frame. Snapshots can outlive
their mounted instance.

Focused source and saved-IR checks exercise scalar and owned text cells through C, Go and ZX,
including independent instances, multiple cells, keyed reordering, cancelled
initialization, independent snapshots, retirement and stale writes. The same
matrix exercises actual authored TSX actions through the optional
`component_actions` module with captured owned text, resource-table growth,
repeated delivery, cancellation during invocation, cleanup and stale tokens.
The same probe passes ASan, UBSan and leak detection. Internal reference scopes
close before invoking user actions, which may grow or free the resource table.
The native host test also passes source/saved bundles with captured guest
callbacks, timer deadlines, failed-frame cleanup, explicit disposal/remount,
replacement and sibling continuity. Its private native presentation store owns
text and vector fields, verifies independent snapshots after replacement, and
releases cells after token invalidation. Full authored component and platform
coverage remains migration work; these checks do not establish device coverage.

The retained `ziran_component_lifecycle_test` and `ziran_frame_reuse_test` exercise
source and saved IR through C, Go and portable ZX. The lifecycle check includes
owned presentation cells and captured authored TSX actions.
Focused private-display runs also verify timer delivery, cancellation, failed
frames, nested unmount cleanup and stale-token rejection through the desktop
player and native executable host. A native component-host run verifies slot
replacement and close while a sibling stays mounted. The Canvas2D/Wasm fixture
also passes source/saved bundle parity, live Chromium interaction, replacement
rollback and clipboard isolation. Android wake paths pass source checks;
the component fixture builds an ARMv7 APK with its current guest assets and
Android loader and notification JNI exports. Separately, the default native
Android fixture compiles a mixed TSX/native `App.Run` graph through saved IR
with the ARM64 NDK and links its loader and notification JNI exports. Device
integration remains unverified.

## Portable bundle registry

A Kryon component is a separately bundled Ziran entry plus its declared host
capabilities. `ComponentHost` owns stable namespace/name identities and up to
sixteen live components. All components share the host's retained tree, input
session, style state, paint queue and renderer. Their owned widget state stays
inside each ZX instance.

The ordinary SDK entry is `component_guest:Run`. Bundle it with the authored
module and bind its typed `Compose` capability to that module's `Frame`
procedure, for example `--bind component_guest:Compose=component:Frame`.
Use the package-qualified SDK module identity when building a dependency.
The SDK dispatches composition, commit, cancellation, tick and disposal phases
through the same typed entry. Instance identities and deadlines cross as
values; owned action environments and presentation stores stay inside the
guest. Widgets are available during composition. Lifecycle control calls do
not compose the application or submit widgets.

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
`ComponentHostClose` returns success only after guest disposal and native
session cleanup. A failed disposal keeps the owning VM valid for a retry.

Replacement opens the candidate outside the live registry, checks its typed
ABI, and migrates its owned state. Verification composes the migrated candidate
on a private session and commits its tree/paint queue without flushing any
raster effects. Staged text editors return clipboard intents without reading or
writing the system clipboard, and staging does not bind audio services. The
active tree, focus, input and framebuffer stay untouched.
The validation frame's guest resources are disposed while its VM is valid,
then its portable state changes are discarded before Ziran atomically
commits replacement. Each component has a stable retained-tree scope, so local
widget keys may repeat in different components. All sixteen slots may be live
while a replacement is staged. A failed ABI, initialization, schema, resource or
verification candidate is closed and the old component remains runnable.
Unrelated slots keep their state and handles. A successful replacement
invalidates old handles. The old live guest is disposed only after all candidate
validation and restoration succeed, immediately before its VM is released.

`COMPONENT_CAPABILITY_HOST`, `COMPONENT_CAPABILITY_INPUT`,
`COMPONENT_CAPABILITY_WIDGET` and `COMPONENT_CAPABILITY_TREE` are independent
grants. A component that imports only text therefore does not carry button or
renderer code merely because another slot uses them.

State migration matches public portable global values by module and name.
File-private and static globals remain instance-local resources and are absent
from the state interface; public nonportable values remain errors. Scalars
move directly; the recursive matcher also supports matching owned strings,
fixed arrays and nested records by field name and type. Borrowed pointers and
slices are rejected. The replacement's unique public `component_schema_version`
is retained and must meet the slot's minimum version. Other matching globals
migrate. New globals keep their initialized values.
The counter test fixtures preserve a nested record, an owned string and a fixed
array as well as scalar state; the replacement keeps its new initialized
suffix field.

`ComponentHostLiveBytes` returns Ziran's owned portable-value payload bytes.
It is not immutable code bytes, bundle bytes, renderer memory or process RSS.

## Local integration test

Replacement versions, status guests, and deliberate failures live under
`tests/fixtures/component_*`. They are test inputs, separate from the public
component usage examples.

Run `env -u DISPLAY -u WAYLAND_DISPLAY -u XAUTHORITY -u DBUS_SESSION_BUS_ADDRESS sh tests/ziran_component_host_test.sh`.
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

Run `env -u DISPLAY -u WAYLAND_DISPLAY -u XAUTHORITY -u DBUS_SESSION_BUS_ADDRESS YUE_DESKTOP_RECOVERY=0 PYTHONDONTWRITEBYTECODE=1 python3 tests/component_browser_test.py`.
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
per tag.
