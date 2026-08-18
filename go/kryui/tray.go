package kryui

// Go bindings for kryon's desktop tray (include/desktop_tray.h). The GTK
// status-icon backend is compiled in via desktop_tray_host.c; on XFCE the
// icon appears in the notification area. Action IDs are app-defined ints;
// PollDesktopTrayAction() returns one at a time from the GUI main loop.

/*
#cgo CFLAGS: -I${SRCDIR}/../../vendor/kryon/include
#cgo pkg-config: gtk+-3.0 sdl2
#include <kryon.h>
#include <stdlib.h>
*/
import "C"

import (
	"unsafe"
)

// FlagWindowHidden hides the window without destroying the GL context
// (raylib window state; pair with ClearWindowState + RestoreWindow to show).
const FlagWindowHidden = uint32(C.FLAG_WINDOW_HIDDEN)

func SetWindowState(flags uint32)   { C.SetWindowState(C.uint(flags)) }
func ClearWindowState(flags uint32) { C.ClearWindowState(C.uint(flags)) }
func IsWindowHidden() bool          { return bool(C.IsWindowHidden()) }
func MinimizeWindow()               { C.MinimizeWindow() }
func RestoreWindow()                { C.RestoreWindow() }

// Trace-log threshold: the SDL backend warns about unimplemented calls
// (e.g. GetWindowScaleDPI) from deep inside the frame loop, so app raises
// the level to errors-only once startup is done.
const LogError = int32(C.LOG_ERROR)

func SetTraceLogLevel(level int32) { C.SetTraceLogLevel(C.int(level)) }

// DesktopTrayItem is one tray menu entry: an action or a separator.
type DesktopTrayItem struct {
	Label     string
	Action    int
	Enabled   bool
	Separator bool
}

// InitDesktopTray registers the tray icon. closeAction is delivered instead
// of a real window-close (close-to-tray); activateAction on left-click.
// Returns false when no tray could be created (no GTK, headless...).
func InitDesktopTray(id, title, iconName string, iconPaths []string,
	closeAction, activateAction int, items []DesktopTrayItem) bool {

	cid := C.CString(id)
	ctitle := C.CString(title)
	cicon := C.CString(iconName)

	// The spec must reference no Go memory (cgo pointer rules), and the
	// strings/arrays live for the process lifetime — the tray thread reads
	// them — so everything is C-allocated and deliberately never freed.
	cpaths := (**C.char)(C.calloc(C.size_t(len(iconPaths)+1), C.size_t(unsafe.Sizeof((*C.char)(nil)))))
	for i, p := range iconPaths {
		slot := (**C.char)(unsafe.Pointer(uintptr(unsafe.Pointer(cpaths)) + uintptr(i)*unsafe.Sizeof((*C.char)(nil))))
		*slot = C.CString(p)
	}

	citems := (*C.DesktopTrayMenuItem)(C.calloc(C.size_t(len(items)), C.size_t(unsafe.Sizeof(C.DesktopTrayMenuItem{}))))
	for i, it := range items {
		mi := (*C.DesktopTrayMenuItem)(unsafe.Pointer(uintptr(unsafe.Pointer(citems)) + uintptr(i)*unsafe.Sizeof(*citems)))
		if it.Separator {
			mi.kind = C.DESKTOP_TRAY_MENU_ITEM_SEPARATOR
			continue
		}
		mi.kind = C.DESKTOP_TRAY_MENU_ITEM_ACTION
		mi.label = C.CString(it.Label)
		mi.action = C.int(it.Action)
		if it.Enabled {
			mi.enabled = 1
		} else {
			mi.enabled = 0
		}
	}

	spec := C.DesktopTraySpec{
		id:              cid,
		title:           ctitle,
		icon_name:       cicon,
		icon_paths:      cpaths,
		close_action:    C.int(closeAction),
		activate_action: C.int(activateAction),
		menu_items:      citems,
		menu_item_count: C.int(len(items)),
	}
	return C.InitDesktopTray(&spec) != 0
}

// PollDesktopTrayAction returns the next user action from the tray (0 if
// none pending). Call once per frame from the main loop.
func PollDesktopTrayAction() int {
	return int(C.PollDesktopTrayAction())
}

// SetDesktopTrayStatus updates the tray tooltip / status line.
func SetDesktopTrayStatus(status string) {
	cs := C.CString(status)
	C.SetDesktopTrayStatus(cs)
	C.free(unsafe.Pointer(cs))
}

// SetDesktopTrayIcon swaps the tray icon at runtime — the emblem/attention
// state (e.g. a badge variant while unseen notifications wait). An empty
// path restores the icon InitDesktopTray resolved.
func SetDesktopTrayIcon(iconPath string) {
	cs := C.CString(iconPath)
	C.SetDesktopTrayIcon(cs)
	C.free(unsafe.Pointer(cs))
}

func ShutdownDesktopTray() { C.ShutdownDesktopTray() }
