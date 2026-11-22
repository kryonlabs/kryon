# Hosted widgets

An application runs another `.zx` inside its own window with
[`hosted_app`](../src/backend/hosted_app.zi): `HostedAppOpen` takes the
bundle's bytes, `HostedAppStart` asks the host about every capability the
bundle requests and binds them to one `HostCall`, and `HostedAppRun` runs it
under a step limit. `HostedAppRestart` keeps the working instance until the
new one passes the host's handshake. It draws that
bundle's widgets with [`widget_host`](../src/backend/widget_host.zi). The
bundle calls each widget's `HostedHost` function; the host binds
`WidgetHostCall` for the functions `WidgetHostSupports` lists, and wraps each
bundle frame in `WidgetHostBegin(session, asset_allowed)` and
`WidgetHostEnd()`. The callback decides which asset paths the bundle may
draw; texture handles never cross. Records match fields by name, so a bundle
and its host may be built against different Kryon layouts.
Hosted `image_raster.ImageWidth(path)` and `ImageHeight(path)` return typed
dimensions for assets approved by the same callback. Paths resolve through
the active bundle owner; denied paths and calls outside a frame are rejected.
An unresolved approved image returns `-1`, and an empty path returns `0`.
Host replies copy returned strings into the bundle's memory, including text
inside records and collections. A host may reuse its reply buffers on its
next call; the bundle can retain those strings across calls and instance runs.
[`scripts/generate-widget-codecs.py`](../scripts/generate-widget-codecs.py)
generates the codecs for Kryon's widget records and, with `--uses`, for an
application's own records.

A bundle measures text with its host's fonts through
[`text_measure`](../src/ui/text/text_measure.zi): `TextWidth(session, props)` is
the width a line takes and `TextHeight(session, props)` the height Text
takes to draw a paragraph wrapped in `props.bounds.width`.

A vector icon that is not a button, such as a symbol beside a name or a coin
beside an amount, is `SvgIcon(session, SvgIconProps.{bounds, shape, color})`
from [`svg_icon_widget`](../src/ui/svgicon/svgicon.zi); a bundle submits it
to its host like the other widgets. An icon is rasterized once at the
screen's pixels and kept, with the 255 others painted most recently, as the
rectangles it paints, so a screen with many icons does not rasterize them
again every frame. A host that draws textures registers
`SvgIconUseTextures(make, free)` and then paints each icon as one image of
the same pixels.

A bundle draws a row of choices, such as a week or calendar view, with
`SegmentedControl.Draw(session, props, options)` from
[`segmented_control_widget`](../src/ui/segmented/segmented.zi); its host
draws it as it draws its own, with up to 32 options. It frames a form's
sections with `Fieldset.Draw(session, props)` from
[`fieldset_widget`](../src/ui/fieldset/fieldset.zi), and places a toggle at a
row's end with the size `ToggleSize(session, props)` from
[`toggle_measure`](../src/ui/toggle/toggle_measure.zi) answers, which stays apart from
the toggle widget as `text_measure` stays apart from Text. Lines, circles,
rings and triangles from [`shape_widget`](../src/ui/shape/shape.zi) are
hosted too, so a bundle can draw a chart's lines and dots; their host fills
them with the renderer's `RasterTriangles`. `Slider` and `DiscreteSlider`
from [`slider_widget`](../src/ui/slider/slider.zi) and `Progress` from
[`progress_widget`](../src/ui/progress/progress.zi) are hosted as well: the
host drags the slider with its own pointer and answers the value, and paints
the progress bar.

[`open_uri`](../src/backend/open_uri.zi) opens a link with the platform's
handler: `OpenUri(uri)` passes it unchanged to the desktop opener, Windows'
shell, an Android `ACTION_VIEW` intent or a new browser tab,
and `CanOpenUri(uri)` says whether that platform can open it. Android code
reaches the activity through [`android_jni`](../src/backend/android_jni.zi).

The generated Android activity also exposes `public static synchronized String
takeLaunchUri()` (JNI static signature `()Ljava/lang/String;`). It consumes the
latest nonempty intent data string of at most 4,096 UTF-16 units and returns
`""` when none is pending. Capture occurs before native startup and on
`onNewIntent`; recreation with saved state does not replay the original launch.
The single slot is process memory only; a newer bounded value replaces it.
Null, empty, oversized or unreadable data leaves the slot unchanged. Values
are preserved without decoding or truncation. The application owns URL
allowlisting, UTF-8 copy bounds, user confirmation and any resulting action.
The bridge does not open URLs or validate their origin. Applications own
their manifest intent filters and custom-activity integration.

`kryon/play_billing` exposes `PlayBillingBuy(id, accountID, output)` for
Google Play editions. Pass the signed-in account's stable ID. The bridge sends
`buy:product|account` and sets Play's obfuscated account ID to the lowercase
64-character SHA-256 digest of the account ID's UTF-8 bytes. Empty IDs,
embedded NULs, delimiters and oversized requests are rejected without opening
a purchase sheet. Each queued sheet retains the account from its request.
The application server must verify the paid purchase and its account binding
before crediting it; this client binding alone grants no balance or referral
reward. Other editions/platforms return an empty answer.

Other platform services an application may import:

| Module | Surface |
| --- | --- |
| [`file_export`](../src/backend/file_export.zi) | `ExportBytes` and `ExportFile`: hand a finished file to the user as a browser download or through Android's share sheet |
| [`pdf_export`](../src/backend/pdf_export.zi) | `PdfOpen`, `PdfImage`, `PdfImagePixels`, `PdfText`, `PdfTextHeight`, `PdfPage` and `PdfClose`: compose Linux-native PDF pages from full-resolution images and searchable UTF-8 text |
| [`file_picker`](../src/backend/file_picker.zi) | `PickFile(FilePickerProps, output)`: zenity on Linux and BSD, Windows' common dialog, and the browser's chooser, which copies the file into the page's file system |
| [`window_close`](../src/backend/window_close.zi) | `WindowCloseWatch(window)` and `WindowCloseTake()`: close requests for the application's own SDL or Win32 window, so it can ask first or hide to a tray |
| [`idle_time`](../src/backend/idle_time.zi) | `IdleMilliseconds()` from the session's idle service over D-Bus, and `IsWaylandSession()` |
| [`notification_desktop`](../src/backend/notification_desktop.zi) | The `notification` host for Linux and BSD desktops, through notify-send |
| [`android_picker`](../src/backend/android_picker.zi) | `AndroidPickerOpen`, `AndroidScannerOpen` and `AndroidPickerPoll`: asynchronous document and QR selection. Import accepts one MIME type or a comma-separated allowlist; custom activities use the scaffold's `org.kryonlabs.platform.DocumentTypes.apply` to set Android's primary type and `EXTRA_MIME_TYPES`. Android filters by MIME type rather than filename extension. `AndroidSavePickerOpen` returns a private staging path through the same poll; write it, then `AndroidSavePickerCommit` confirms the selected document was written and closed. The activity implements `openImportPicker`, `openQrScanner`, `openSavePicker` and `savePickedFile`. |
| [`android_text_editor`](../src/backend/android_text_editor.zi) | Input method editing on Android: the activity's input connection sends snapshots of the focused field, so composition, autocorrect and suggestions work; android_device's text input uses it when the activity implements `setTextInputState` and `nativeTextInputState` |
| [`file_download`](../src/backend/file_download.zi) | `FileDownloadStart`, `FileDownloadPoll` and `FileDownloadRelease`: a large file downloaded to a path, appearing only when complete, through curl, the browser's streaming `fetch` into the page's file system, or the Android activity |
| [`http_transfer`](../src/backend/http_transfer.zi) | `HttpTransferStart`, `HttpTransferPoll` and `HttpTransferCancel`: HTTP requests that run while the application draws, through curl on a worker thread, the Android activity's transport, or the browser's `fetch` |
| [`page_socket`](../src/backend/page_socket.zi) | `PageSocketOpen`, `PageSocketPoll` and `PageSocketClose`: a browser WebSocket for text events, polled each frame, with bounded messages and a token carried as a subprotocol |
| [`browser_viewport`](../src/backend/browser_viewport.zi) | `BrowserViewportMeasure` and `BrowserResizeSettled`: the room a canvas application has in the browser, and when a new size has settled enough to resize to, for applications that size their own canvas |

`PdfImagePixels(document, pixels, width, height, bounds)` draws tightly packed
straight RGBA bytes, contained and centered in `PdfBounds` without changing their
aspect ratio. It preserves alpha, does not modify or retain the caller's bytes,
and returns false for invalid dimensions, byte count or a closed document.
`PdfImage` loads a file and uses the same renderer.

`PdfTextHeight(document, text, width, size, bold=false)` returns the full wrapped
height in PDF points, using the same Noto Sans font, fallback fonts and word wrap
as `PdfText`, without drawing or limiting the number of lines. It returns zero
for empty or invalid UTF-8, nonpositive width/size, dimensions exceeding Pango's
fixed-point range, or a closed document. Measure each paragraph before placing
it; add a page when its measured height exceeds the remaining space.

After a completed HTTP poll, `transfer.failure` distinguishes a response that
exceeded the caller's buffer (`HTTP_TRANSFER_FAILURE_CAPACITY`) from a transport
failure. Failed responses never publish partial bytes. A new start or cancel
clears the failure. Large logical transfers can use this result to switch to
the service's paged API and preserve progress without enlarging frame memory.

[`src/ui/modules.txt`](../src/ui/modules.txt) is the core build inventory.
`make test` checks source, saved `.zir`, `.zx`, and generated C/Go behavior.
See [FEATURE_MATRIX.md](FEATURE_MATRIX.md) for package and host verification.

Portable apps can import `kryon/input_protocol` for pointer observations,
key codes, modifier bits and `PointInBounds` without loading the native tree
implementation. Existing `tree_input` imports retain that surface.
`kryon/file_picker` exposes the native file chooser; `kryon/app_theme` reads
optional host theme colours from a portable app.

An app embedded by a supervising host, such as Harmony's app host, draws in
the host's theme by default on Linux. The host names its process and the
window it embeds the app in (`HARMONY_APP_HOST_PID`, `HARMONY_APP_CONTAINER`)
and keeps its palette on that window (`_HARMONY_APP_THEME`).
`SystemThemeLinux.SystemPaletteRead()` returns that palette after checking
that the host is the app's parent and owns the window; started on its own, an
app gets the desktop's GTK palette as before. Each call reads the host's
current palette, so an app that reads it again follows a theme change.
`ParentThemeLinux.ParentThemeActive()` says whether a host supplies the theme,
and `ParentPaletteRead(*palette)` reads only the host's palette. libX11 is
loaded at run time and only when a host is named.
`SystemThemeHostLinux.SystemPaletteFollow()`, called once per frame, applies
that palette (the desktop's when not embedded) the first time and again
whenever the host changes it, and returns true when it applied, so an app
follows its host's theme change with one call; portable apps call
`SystemThemeHost.SystemPaletteFollow()`. Applying installs the palette's style
rules and sets the active theme's colours (`SystemPaletteApply`), so code
that draws from `CurrentPalette()` follows too.
`SystemTheme.SystemPaletteColors(palette)` expands a system palette into a full
theme `Palette` (surfaces, text levels, borders, accent, controls) for an app
that draws from one.

`Theme.SweetDarkPalette()` provides the dark Sweet surfaces, text and accent
colors. `Theme.CompactDarkPanel()` returns a `CompactPanelStyle` with a near-black
palette, 32-pixel height and item width, 20-pixel icons, workspace and clock widths,
and compact corner, padding and font sizes. Apply these tokens within the panel's
palette scope; application compositions retain their own saved layout and state.
