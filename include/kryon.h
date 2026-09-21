#ifndef KRYON_H
#define KRYON_H

#include "kryon_version.h"
/* Legacy graphics/input compatibility surface. New Kryon-facing code should
 * use owned widgets and primitives (Button, Card, Router, Box, Circle,
 * Line, Triangle, ...); the concrete backend is selected at link time via
 * KRYON_BACKEND. */
#include "kryon_compat.generated.h"
/* Math3d surface (Vector3/Matrix/Quaternion arithmetic): must come after
 * the compat surface so raymath reuses its type definitions. */
#include "kry_math3d.generated.h"
#include "kryon_frame.h"
#include "kry_input.h"

#include "ui_dpi_props.generated.h"
#include "ui_dpi.h"
#include "ui_layout_props.generated.h"
#include "ui_drawing_props.generated.h"
#include "ui_core.h"
#include "ui_instance.h"
#include "ui_controls.h"
#include "ui_accessibility_node.generated.h"
#include "ui_accelerator.generated.h"
#include "ui_clipboard.generated.h"
#include "ui_canvas_props.generated.h"
#include "ui_collapsible_props.generated.h"
#include "ui_color_picker_props.generated.h"
#include "ui_drag_drop_props.generated.h"
#include "ui_drag_props.generated.h"
#include "ui_dropdown_props.generated.h"
#include "ui_fieldset_props.generated.h"
#include "ui_input_props.generated.h"
#include "ui_list_box_props.generated.h"
#include "ui_menu_props.generated.h"
#include "ui_paned_view_props.generated.h"
#include "ui_plot_props.generated.h"
#include "ui_popup_props.generated.h"
#include "ui_progress_props.generated.h"
#include "ui_radio_props.generated.h"
#include "ui_scroll_props.generated.h"
#include "ui_separator_props.generated.h"
#include "ui_slider_props.generated.h"
#include "ui_spinbox_props.generated.h"
#include "ui_table_view_props.generated.h"
#include "ui_text_props.generated.h"
#include "ui_toggle_props.generated.h"
#include "ui_tree_view_props.generated.h"
#include "ui_drawing_props.generated.h"
#include "ui_inspect_props.generated.h"
#include "ui_icons.h"
#include "kryon_compat.generated.h"
#include "ui_controls.h"
#include "ui_modal_props.generated.h"
#include "ui_title_bar_props.generated.h"
#include "ui_icon_types.h"
#include "ui_navigation_bar_props.generated.h"
#include "ui_paned_view_props.generated.h"
#include "ui_tab_bar_props.generated.h"
#include "ui_toolbar_props.generated.h"
#include "ui_profile_icon_props.generated.h"
#include "ui_reorder_props.generated.h"
#include "ui_swipe_props.generated.h"
#include "ui_text_props.generated.h"
#include "ui_toast_props.generated.h"
#include "ui_tree.h"
#include "ui_page_props.generated.h"
#include "ui_window.h"
#include "spritesheet.h"
#include "kry_math.h"
#include "scene_tree.h"
#include "node2d_props.h"
#include "scene_property.h"
#include "kry_signal.h"
#include "kry_animation.h"
#include "kryon_node.h"
#include "kryon_property.h"
#include "kryon_edit_host.h"
#include "ui_node_registry_props.generated.h"
#include "ui_text_layout.h"
#include "ui_transition_props.generated.h"
#include "ui_style_sheet.h"
#include "locale.h"
#if defined(KRYON_WITH_SYNC) && KRYON_WITH_SYNC
#include "sync/account.h"
#include "sync.h"
#include "sync_nodes.h"
#endif
#include "theme.h"
#include "theme_meta.h"
#include "web.h"
#include "runtime_assets.h"
#include "embedded_assets.h"
#include "desktop.h"
#include "desktop_tray.h"
#include "android_surface.h"
#include "markdown.h"
#include "app_host.h"
#include "app_runtime.h"
#include "app_shell.h"
#include "app_storage.h"
#include "app_instance.h"
#include "automation.h"
#include "kry_capabilities.h"
#include "kryon_mem.h"

/* Kry standard library: platform surfaces (process, filesystem, dynamic
 * libraries) usable directly from .kry apps. */
#include "kry_process.h"
#include "termi.h"
#include "kry_filesystem.h"
#include "kry_dylib.h"
#include "kry_backend.h"
#include "krb.h"
#include "notification.h"
#include "kry_uri.h"

/* ABI guard for prebuilt-library consumers (the Go bindings link static
 * archives that are NOT rebuilt automatically when this tree moves).
 * Bump KRYON_ABI_VERSION whenever a struct declared in include/ changes
 * layout or a public function changes signature; KryonAbiVersion() is
 * compiled into libkryon.a, while the macro is read from the current
 * headers — a mismatch means the archive is stale and must be rebuilt. */
#define KRYON_ABI_VERSION 10
int KryonAbiVersion(void);

#include "kry_bounds.h"

#endif /* KRYON_H */
