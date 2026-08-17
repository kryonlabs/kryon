/* Compiles kryon's desktop tray into the app binary.
 *
 * The prebuilt libkryon.a ships the no-tray stubs (its generic build has no
 * tray backend), so app compiles the real thing here: the GTK status-icon
 * backend with lazy GTK (KRYON_TRAY_GTK_DL) — no link-time GTK dependency,
 * libgtk-3 is dlopen'd only when the tray starts. Same setup inbe uses.
 */
#define KRYON_DESKTOP_TRAY_ENABLED 1
#define KRYON_DESKTOP_TRAY_GTK_STATUS_ICON 1
#define KRYON_TRAY_GTK_DL 1

#include "../../src/platform/desktop_tray/desktop_tray.c"
#include "../../src/platform/desktop_tray/gtk_dl.c"
