# Native widget catalog audit

The catalog is implemented in [`26_widget_catalog.kry`](26_widget_catalog.kry).
It has ten sidebar categories, independently scrollable previews, persistent
control values, and standard Kryon theme styling. The preview scale fits smaller
windows while keeping both columns and every category reachable.

Build and run from the repository root:

```sh
make -C examples 26_widget_catalog
./build/examples/bin/26_widget_catalog
```

Image previews use the checked-in `icons/tiles.png` asset. Run from the repository
root or `examples/`. Close the window to quit; Escape is available to controls.

Verified on 2026-09-05: **122/122 native audit checks passed**, along with
the native widget regressions, real-renderer regressions, Go runtime tests and
C/Go/JS generated-runtime parity suite. These counts include multiple checks
per widget and are not a count of distinct widget families.

## Screens and interactions

| Category | Preview and verification |
| --- | --- |
| Display & media | Text, wrapped text, labels, values, geometry, icons and four image variants. Select-and-copy text works across frames; ImageButton reports clicks. |
| Actions & input | Buttons, arrow and invisible buttons, checkbox/flags, toggle, radio, selectable, slider, spinbox, dropdown, combobox, progress, color picker, TextField and TextArea. Disabled buttons ignore clicks. Tooltip hover is visible. The audit intercepts external link dispatch and verifies its URL. |
| Layout & containers | Column, Row, Stack, GridLayout, Notebook, PanedView and Collapsible. Both row and grid buttons respond at their displayed positions. The divider follows a drag outside its original handle. |
| Collections & editors | ListBox, TreeView, TableView, SourceView, MultiSelectList and drag/drop. Ctrl adds selections; Shift selects a range. Drag/drop copies the actual payload. CascadingTreeView expands folders and selects children. TreeView itself displays the caller-supplied flat tree; use CascadingTreeView for interactive expansion. |
| Navigation | TopNav, Toolbar, menubar, context menu, TabBar, ClosableTabBar, SubtabBar, PaneTabs and TabItemButton. Menu actions update the displayed command. New tab adds a real tab; closing removes it and updates selection. An empty tab bar can be reopened. Add is disabled at eight tabs. Tabs stay within their bounds. |
| Feedback & dialogs | Toast, theme switcher/picker, MessageDialog, ConfirmDialog, PromptDialog, ActionModal, PickerDialog, ModalFrame and TextPopover. Prompt/popover submissions update text; picker results are shown. Theme choices apply to the actual rendered theme. Modal backdrops cover the underlying catalog. |
| Canvas | Clipped grid and circle, wheel zoom, pointer pan, displayed transform values and Reset. |
| Numbers & plots | Float/int drags and sliders, float/int ranges, angle slider, float/int/double inputs, vertical sliders, line plot and histogram. The audit exercises all three vector components and keyboard entry, as well as scalar changes. |
| Colors | RGB/RGBA editors and pickers, alpha adjustment, and a clickable color button. |
| Pages & settings | Page, Section, Heading, ParagraphText and ThemeSettings. A full-window preview demonstrates TitleBar and BottomNav with three working routes and a return button. Settings menus apply mode and style changes. |

## Corrections made

- Background and image painting no longer cover immediate controls.
- Small, arrow, invisible and color buttons are not repainted as generic buttons.
- ButtonProps styling supplies the correct button colors, including sidebar
  selection and secondary actions.
- Menu arrays persist across frames, avoiding expired-pointer crashes.
- Toolbar dropdowns respect the toolbar's horizontal origin.
- Grid layout resolves button hit positions before immediate input handling.
- Drag targets accept their source's dragged release while preserving capture
  and disabled checks.
- PanedView retains its drag outside the original handle in C and Go.
- Tabs add/remove real state, remain bounded, and support empty/full states.
- Text selection uses stable text content and position rather than allocation
  addresses, so copying works when the generated tree reallocates text.
- A popover does not dismiss itself on its trigger's opening click.
- Pending tree content paints before an immediate modal; later declarations can
  still paint over that modal. This covers standard dialogs and custom frames.
- Preview data, tab heights, no-selection results, image sources and desktop
  scaling were corrected. Themes, canvas gestures and dialog results are wired
  to persistent example state.

## Reproduce the audit

Linux dependencies are `xdotool`, `xvfb-run`, ImageMagick's `import`, Python 3 and
Kryon's normal native build dependencies. The test observer is generated under
`build/`; application UI and behavior remain in the `.kry` source.

```sh
xvfb-run -a -s '-screen 0 1920x1080x24' python3 tests/widget_catalog_audit.py
```

The audit writes per-group JSON results, application logs and screenshots under
`build/widget-catalog-audit/`, and exits nonzero when a check fails. To repeat
one group against an existing build:

```sh
xvfb-run -a -s '-screen 0 1920x1080x24' \
  python3 tests/widget_catalog_audit.py --no-build --group tab-limits
```

Groups cover screens, actions, buttons, dialogs, numbers, vectors, navigation,
new dialogs, submission/clipboard behavior, settings, tab limits and window
sizes. Screenshot inspection complements state assertions; neither establishes
an exhaustive test of every possible input sequence.

Durable runtime regressions:

```sh
make build/linux-x86_64/tests/ui_tree_api_test build/linux-x86_64/tests/ui_tk_test
build/linux-x86_64/tests/ui_tree_api_test
build/linux-x86_64/tests/ui_tk_test
make overlay-paint-test
make generated-runtime-parity-test
(cd go/kryon && go test .)
```

`overlay_paint_test` uses the real renderer to check six modal backdrops, content
painted after a modal, tab overflow, popover dismissal and clipboard selection.
The other native tests cover grid hit positions and dragged-release behavior.

## Validation scope

This is a native Linux desktop catalog. Screens are inspected at 1200 × 800;
additional resize checks exercise 800 × 600, 1120 × 640 and 1600 × 1000. The C,
Go and JS parity suite checks its shared fixtures. It does not establish visual
identity of the entire catalog on other renderers, operating systems or mobile
hardware. This document covers the catalog's widget examples, not every public
runtime helper or every possible composition.
