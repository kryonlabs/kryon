#ifndef KRYON_H
#define KRYON_H

#include "kryon_version.h"
/* Owned raylib-style graphics/input surface. The concrete backend is
 * selected at link time via KRYON_BACKEND. */
#include "kryon_compat.generated.h"
/* Math3d surface (Vector3/Matrix/Quaternion arithmetic): must come after
 * the compat surface so raymath reuses its type definitions. */
#include "kry_math3d.generated.h"
#include "kryon_frame.h"

#include "ui_color.h"
#include "ui_scaling.h"
#include "ui_dpi.h"
#include "ui_layout.h"
#include "ui_clip.h"
#include "ui_core.h"
#include "ui_controls.h"
#include "ui_tk.h"
#include "ui_draw.h"
#include "ui_icons.h"
#include "ui_modal.h"
#include "ui_nav.h"
#include "ui_overlay.h"
#include "ui_profile.h"
#include "ui_reorder.h"
#include "ui_rows.h"
#include "ui_scroll.h"
#include "ui_text.h"
#include "ui_toast.h"
#include "ui.h"
#include "ui_page.h"
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
#include "ui_node_registry.h"
#include "ui_text_layout.h"
#include "ui_transition.h"
#include "locale.h"
#include "ksync_account.h"
#include "ksync_sync.h"
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
#include "kry_automation.h"
#include "kry_capabilities.h"
#include "kry_settings.h"
#include "kryon_mem.h"
#include "preview_canvas.h"
#include "preview_host.h"
#include "preview_io.h"
#include "preview_layers.h"

/* Kry standard library: platform surfaces (process, filesystem, dynamic
 * libraries) usable directly from .kry apps. */
#include "kry_process.h"
#include "terminal.h"
#include "terminal_pane.h"
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
#define KRYON_ABI_VERSION 7
int KryonAbiVersion(void);

#endif /* KRYON_H */
