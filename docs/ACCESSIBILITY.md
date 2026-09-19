# Native Accessibility

## Available Builds

Linux native Go windows register with AT-SPI using `godbus` (no cgo). Linux C
raylib builds enable the adapter when `pkg-config` finds `gio-2.0` and `pango`.
The root Makefile and standard `mk/common.mk` consumer build include these
dependencies. Custom C builds must compile `src/platform/accessibility_linux.c`
with `KRYON_ACCESSIBILITY_DBUS` and link GIO/Pango. Without that define, the file
provides no-op platform hooks; backend-neutral snapshot APIs still work.

`KRYON_ACCESSIBILITY=0` disables automatic registration at runtime; the Makefile
variable of the same name disables compiling the C adapter. `NO_AT_BRIDGE=1`
also disables registration. `AT_SPI_BUS_ADDRESS` overrides discovery through
the session bus's `org.a11y.Bus` service. An unavailable bus does not prevent
the application from opening. Connection is attempted once at window startup;
reconnection after a bus/registry restart is not yet supported.

## Contract

The exported tree is application -> window -> semantic controls. Unique stable
positive focus IDs preserve object paths across frames. Removed paths are
retired permanently during the connection; duplicate IDs expose no actions.
Keyed groups retain identity as well. Other anonymous controls have positional
identities and are not actionable.
Applications must provide meaningful labels and unique IDs themselves.

The adapters implement Accessible queries, Cache bulk queries/change signals,
Application registration, Component geometry/focus, and Action activation.
Buttons, clickable cards, checkboxes, toggles, and radios support activation.
Radio decoration is exposed as one labeled, checked-state control.
Text and text editors support reading, Unicode character counts, grapheme/word/
sentence ranges, caret queries, and editor selections. Line and paragraph
queries use hard line breaks, not rendered soft wraps. EditableText replacement
is supported in both adapters; native Go also supports insertion/deletion.

Snapshots preserve nested semantic groups through parent indices. Linux object
paths retain identity for unique focus IDs and semantic keys when siblings move
or a control changes parent. Child queries, cache entries and parent-relative
geometry follow the exposed hierarchy.

List boxes expose every option, including off-screen items. Single- and
multi-selection, deselection, clearing, and multi-select SelectAll use the normal
UI-thread action queue. Selection updates application storage and reveals the
affected row when scroll storage is supplied. Optional positive `item_keys`
(`ItemKeys` in Go) preserve option identity and queued intent across reordering;
removed or ambiguous keys cannot select a replacement item. Keys are local to
each list. AT-SPI Selection methods distinguish child indices from indices into
the selected subset and publish selection changes and option visibility.

Object state/name/children changes, text changes, caret/selection changes, and
window activation changes emit AT-SPI events. Unicode scalar offsets on D-Bus
are converted to the runtime's UTF-8 byte offsets. Password fields expose no
content, character count, or selection offsets; OS selection requests for them
are rejected, while full replacement remains possible when editable.

Methods acknowledge queue acceptance, not synchronous application mutation.
Actions execute at the next frame and revalidate the current widget and popup
state. Go transport callbacks use only owned snapshots and a bounded inbox.
C transport callbacks run on the UI thread through a private GLib context
before frame input reset. Shutdown cancels connection work and retires objects.

## Limits

This is an initial Linux integration, not full screen-reader conformance.
Windows UI Automation, macOS accessibility, Android, and iOS adapters are not
implemented. C libdraw/terminal/null backends do not automatically register.
Tree/table child selection, dropdown options, tab selection, range-value interfaces,
relations, rich text attributes, glyph/range geometry, clipboard text actions,
and legacy Text boundary methods remain unsupported. Rendered-line navigation
and exhaustive Orca interaction testing remain work.

## Verification

`make accessibility-dbus-test` builds a C fixture and runs C/Go round trips on a
private D-Bus daemon with Go's race detector. It covers discovery, bulk cache
queries, Unicode offsets, password privacy, queued activation/replacement,
selection, nested children, parent-relative bounds, stable/retired paths, and
shutdown. Requires Linux, Go,
`dbus-daemon`, GIO, and Pango.

`make generated-runtime-parity-test` exercises generated C and Go, including
radio accessibility activation and keyed list selection/clearing through the
ordinary application state, including option hierarchy assertions.

An optional real libatspi smoke test (PyGObject and the Atspi 2.0 typelib) is:

```sh
dbus-run-session -- /usr/bin/python3 tests/accessibility_atspi_test.py \
  build/linux-x86_64/tests/accessibility_dbus_fixture

KRYON_ACCESSIBILITY_TEST_LIST=1 dbus-run-session -- /usr/bin/python3 \
  tests/accessibility_atspi_test.py build/linux-x86_64/tests/accessibility_dbus_fixture
```

Protocol reference: [GNOME AT-SPI architecture and interfaces](https://gnome.pages.gitlab.gnome.org/at-spi2-core/devel-docs/architecture.html).
