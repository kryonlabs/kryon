# Legacy candidates — open items (2026-11-22)

The rest of the audit is done: the BSD/GNU make shims (root and examples),
go.mod, images_pending, the host/android and host/terminal re-exports, the
unused canvas/DOM HTML shells, system_typeface_linux, typeface_collection,
the tree_label_cost scripts, release-notes.sh, update_version.sh,
subset-fonts.sh, update-showcase.py and the SwipeRow widget are removed.

## Open

| Item | Blocked on | Fix |
| --- | --- | --- |
| ziran.toml export `tree_draw` (second name for `Frame`) | kryonlabs/explorer imports `kryon/tree_draw` (src/taiji_host.zi:15, tests/split_test.zi:12, tests/host_test.zi:5); packages/css tests import it (css_motion_behavior.zi:17, css_integration_behavior.zi:22) | switch those imports to `kryon/Frame`, then delete the `tree_draw` export line |

## Kept after checking downstream users

- `ClockLinux` (src/backend/clock_linux.zi): kryonlabs/browser src/app.zi:26.
- `SystemClipboardLinux`: harness/harmony client/src/native_desktop.zi:82, kryonlabs/terminal src/app/app_clipboard.zi:10; tested by tests/system_clipboard_linux_test.py.
- `FileDialogLinux`: kryonlabs/terminal src/app/app_context_menu.zi:11.
- src/ui/motion_api_example.zi and src/ui/layout/layout_api_example.zi: indexed by motion.zi.examples and layout*.zi.examples.
