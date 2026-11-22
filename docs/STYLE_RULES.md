# Style rules

Installable style rules plus the image, bitmap, clipboard and
Linux drag/clipboard integrations that stylesheets and widgets draw with.
Read this when styling widgets, showing pictures or moving data through
system clipboards and drags.

`InstallStyleRules` copies typeface text into Kryon-owned storage and returns
`false` without replacing the active rules when the rule count is outside
`0..320` or the combined typeface text exceeds 65,536 bytes.
`InstallParsedStyleRules` reports the same storage failure.

Use `Image.Draw(Image.Props)` for UI images. Its asset path, bounds, fit, and alt
text describe the image at the widget boundary. Backend texture calls are
renderer primitives, not alternate widget APIs.
Asset paths may name PNG, JPEG, WebP or AVIF files on image-loading hosts. WebP is
recognized by its content: lossy, lossless and transparent pictures load as
RGBA, and an animated WebP shows its first frame (Kryon images are still).
See [WebP images](BACKENDS.md#webp-images) and [AVIF images](BACKENDS.md#avif-images) for each host's decoder.
`Image.Props.radius` gives an image rounded corners in native and hosted UI;
a positive value overrides its style radius.
`kryon/image` brings file pictures, so a program that imports it links a host
that measures them. Code that only needs the image placement rules, such as
`ImageFitBounds` and `ImagePreviewSource`, imports `kryon/image_behavior`,
which needs no host or picture.

Import `kryon/bitmap_image` to create a runtime RGBA image for the same
`Image.Draw(session, Image.Props.{.texture = image, .smooth = true, ...})`.
Bytes are tightly packed, row-major straight RGBA8 and copied/consumed
synchronously; reuse the input buffer after the call.

| Call | Behavior |
| --- | --- |
| `BitmapImageCreate(session, width: s32, height: s32, rgba: []u8) -> Texture2D` | Failure returns a zero texture |
| `BitmapImageUpdate(session, image: Texture2D, rgba: []u8) -> bool` | True for accepted unchanged data; false keeps the current pixels. Only successful
changed data invalidates retained paint. Smooth sampling and alpha use the
ordinary Image renderer on Desktop/Cairo, pixmap and Canvas. Explicit `Image.Props.tint`, including white for original colors,
applies to straight RGBA pixels. GPU reset and native window close/reopen
preserve the session's CPU image; textures are recreated when next painted.
Completed native worker frames own deduplicated copies of the revisions they
draw, so an update or session close cannot change pending pixels. Each frame
retains at most 64 revisions and 32 MiB of straight pixels; pressure fails the
frame explicitly. The native texture cache has the same 64-image/32 MiB bound.

Managed handles belong to the complete Session (slot and epoch) and current
provider. Dimensions must be positive, at most 4096 each and 4,194,304 pixels;
byte count must equal width × height × 4. There are at most 64 images, with a
64 MiB total and 32 MiB per-session budget for straight and premultiplied CPU
copies. Provider copies have the same bounded dimensions/count. Bad, modified,
stale and cross-session handles are rejected; releasing them is harmless.
Closing a Session releases its images. Closing/replacing the Canvas provider
also releases its images. Call on the session/provider's frame thread, outside
an active raster emission. Native renderer texture handles are unchanged.
Standard hosted widget policy rejects all guest runtime texture IDs and does
not advertise bitmap lifecycle capabilities; an embedding must provide a
separate authenticated ownership policy to grant those effects.
In a zx bundle the player owns these pictures: `kryon build` defines
`KRYON_ZX` for zx profiles, and the same three calls become the host calls
`BitmapImageCreateHost`, `BitmapImageUpdateHost` and `BitmapImageReleaseHost`
(pixels cross once as a byte view), so the player's raster draws them. The
desktop zx player provides them for the session it runs; a player without
them refuses the bundle.

### System text clipboard and PRIMARY

Import `kryon/system_clipboard` for the ordinary UTF-8 text clipboard:

| Call | Purpose |
| --- | --- |
| `SystemClipboardText(buffer) -> string` | Whole-UTF-8 prefix borrowed from `buffer` |
| `SystemClipboardAvailable() -> bool` | Whether the clipboard holds text |
| `SystemClipboardHasText() -> bool` | Same question, no buffer |
| `SystemClipboardSet(text) -> bool` | Publish text to CLIPBOARD |

On hosts with a distinct selection clipboard:

| Call | Purpose |
| --- | --- |
| `SystemPrimaryText(buffer) -> string` | Caller-owned whole-UTF-8 prefix |
| `SystemPrimaryAvailable() -> bool` | Distinguishes an empty selection from an unsupported host (an empty `SystemPrimaryText` result can mean either) |
| `SystemPrimarySet(text) -> bool` | Publish text to PRIMARY |

`SystemPrimaryText` returns a borrowed prefix borrowed from `buffer`; an empty result can mean either an
empty selection or an unsupported host, so use `SystemPrimaryAvailable` when
that distinction matters. Linux SDL desktop hosts connect these
calls to the platform PRIMARY selection. Android, Canvas, terminal, pixmap,
bundle and unsupported desktop hosts return unavailable rather than falling
back to CLIPBOARD.

Custom hosts implement the snapshot functions `SystemPrimaryOpenHost() -> s32`
and `SystemPrimaryByteHost(index) -> s32`, plus
`SystemPrimaryWriteHost(text) -> bool`. Open returns the byte count, including
zero for an available empty selection, or -1 when unavailable. The snapshot
remains valid until the next open call; write copies the text during the call.

### Optional native MIME clipboard on Linux

Import `kryon/MimeClipboardLinux` in an X11/XWayland native app and link
`-lxcb` (for example in `LDLIBS`). The text and PNG modules keep their
interfaces. This module owns one dedicated connection and unmapped selection
window, without toolkit callbacks or changes to the toolkit's connection.
It does not implement a Wayland-only data source or clipboard persistence.

Call `MimeClipboardOpen() -> bool` on the application's GTK/X11 owning thread,
then keep every call serialized on that same thread until
`MimeClipboardClose()`. Calls from another thread fail; close there does nothing.
Close is idempotent and destroys only the transport's own connection/windows,
freeing its copied publications and outgoing transfer snapshots. No callbacks
or caller buffers remain registered after any call returns.

| Call | Behavior |
| --- | --- |
| `MimeClipboardPublish(payloads: []MimeClipboardPayload) -> MimeClipboardPublishResult` | Copies every `{mime: string, bytes: []u8}` and publishes the complete target set in one selection ownership change. Reuse/free input storage after return; result fields are `status` and `owner: u64`. Duplicate/empty/NUL names, reserved selection protocol targets and invalid counts fail without replacing the prior publication. Empty byte payloads are supported |
| `MimeClipboardRead(mime: string, buffer: []u8, timeout_ms: s32) -> MimeClipboardReadResult` | Requests exactly that target, including incoming INCR. Returns `status`, `bytes: []u8`, `size: s64`, `owner: u64`. Only `Ok` exposes bytes, borrowed from the supplied buffer; partial buffer data on failure must be ignored. Empty success differs from `Absent`. `size` is the bytes observed, the direct property's exact size, or an INCR lower bound/hint when rejected as too large; INCR hints are untrusted |
| `MimeClipboardPump(max_events: s32) -> s32` | Serves at most 256 events per call (zero for nonpositive budgets). Call regularly from the app frame loop while publishing. Reads also serve queued requests; self-reads copy directly without waiting for a toolkit loop. Retires abandoned/expired transfers |
| `MimeClipboardOwner() -> u64` | Zero for no owner/unavailable transport. Local ownership returns the local generation token; foreign ownership returns an observational XID that cannot identify content changes within that window |
| `MimeClipboardRelease(owner: u64) -> bool` | Clears only the still-current publication created by this transport, comparing server ownership atomically. Every successful local publish has a fresh window and generation token; stale tokens, including across close/reopen, cannot clear newer local data. Foreign tokens always fail, even if a read returned that token: a foreign application may republish different data from the same window |

`MimeClipboardStatus` is `Ok`, `Absent`, `Error`, `TooLarge`, or `Timeout`.
`Absent` means no owner or a refused target. Invalid arguments, malformed
conversion properties, connection errors and owner replacement during a read
return `Error`. Read timeouts are total monotonic deadlines (1–5000 ms),
including trickling INCR chunks; ordinary X-server request round trips assume
a responsive local X server. Only the UI thread may open; concurrent first
calls and reentrant calls are unsupported.

Limits: 32 targets; names 1–255 bytes; 16 MiB aggregate copied publication;
16 MiB per read even with a larger caller buffer; 16 outgoing INCR transfers
with 16 MiB aggregate snapshot bytes and a five-second total serving lifetime.
Transfers use 32 KiB chunks and survive local republish/owner loss using their
own copied snapshots. No MIME conversion, URI validation or file operation
policy occurs here. Clipboard data and remote ownership are observations, not
an authorization or a trustworthy content generation identifier.

Run `make mime-clipboard-linux-test` for source and saved-IR native tests against
independent Xlib fixtures and installed `xclip`, under private HOME/XDG/D-Bus/Xvfb.

### Optional native drag source on Linux

Import `kryon/DragSourceLinux` in an X11/XWayland native app and link `-lxcb`
to drag data out of the app with XDND version 5, for example files onto a web
browser's upload area, a file manager, the desktop or an editor. The module
owns its own connection, an unmapped source window and an optional icon
window; it never grabs the pointer or changes the toolkit's connection.

Call `DragSourceBegin(payloads: []DragSourcePayload, actions: u32, icon: DragSourceIcon) -> bool`
while the primary button is still held, typically once the pointer has moved
past the platform drag threshold from the press. Each payload is a
`{mime: string, bytes: []u8}` pair copied during the call (at most 32 targets
and 4 MiB in total). `actions` combines `DRAG_ACTION_COPY`, `DRAG_ACTION_MOVE`,
`DRAG_ACTION_LINK` and `DRAG_ACTION_ASK`. `icon` is straight RGBA up to
256×256 with a hot spot; pixels at least half opaque are shown in a shaped
override-redirect window that follows the pointer. A zero-sized icon shows none.

While `DragSourceActive()` is true, call `DragSourceUpdate() -> DragSourceState`
once per frame on the same thread and keep frames coming (on the Desktop host,
`DesktopRequestFrame()`). Update follows the pointer, finds the top-level window
under it (honouring `XdndProxy`), sends Enter/Position/Leave/Drop, answers
`XdndStatus`, and serves the `XdndSelection` conversions, including `TARGETS`.
The suggested action follows GTK: Control+Shift links, Control copies, Shift
moves, Alt asks, otherwise the first offered of copy, move and link. The state
reports `status`, the root pointer position, `modifiers` (1 Shift, 2 Control,
4 Alt), the current `action`, whether a foreign target `accepted` it, and
`local` when the pointer is over a window of this process (identified by
`_NET_WM_PID`).

| `DragSourceState.status` | Meaning |
| --- | --- |
| `Dragging` | Button still held |
| `Local` | Release over this process; the module sends no XDND messages over its own windows, so the app performs internal drops itself |
| `Dropped` | Drop sent to an accepting target; lasts until the target's `XdndFinished` (or a 15 second timeout) |
| `Finished` | `performed` and `success` report the target's answer |
| `Cancelled` | Release anywhere else |

`DragSourceCancel()` abandons a drag, for example on Escape. After reading a
final status, call `DragSourceEnd()` to release the selection, icon and copied
data. The application performs no file operation for a foreign drop: the
target does, as with any XDND source.

Run `make drag-source-linux-test` for source and saved-IR tests that drop onto
an independent python-xlib XDND target on a private Xvfb.
