package kryui

// Go bindings for kryon's cross-platform notifications
// (include/notification.h). On Linux these are org.freedesktop.Notifications
// calls over the session bus, in-process — no notify-send, no libnotify, no
// extra threads. Action clicks are polled from the GUI main loop, like tray
// actions.

/*
#cgo CFLAGS: -I${SRCDIR}/../../include
#cgo pkg-config: gtk+-3.0 sdl2
#include <kryon.h>
#include <stdlib.h>
*/
import "C"

import (
	"unsafe"
)

// SetNotificationAppName names the app for the notification daemon. On the
// desktop the daemon resolves the icon from the desktop entry matching this
// name, so pass the same name as the installed .desktop / hicolor icon.
func SetNotificationAppName(name string) {
	cs := C.CString(name)
	C.SetNotificationAppName(cs)
	C.free(unsafe.Pointer(cs))
}

// NotificationSupported reports whether a notification backend is compiled
// in and reachable.
func NotificationSupported() bool {
	return C.IsNotificationSupported() != 0
}

// SendNotificationAction posts one notification with a single action button
// labeled actionLabel. icon may be an absolute path or an icon name ("" uses
// the desktop-entry icon from SetNotificationAppName); expireMs <= 0 keeps
// the daemon default. Clicking the button is delivered by
// PollNotificationAction together with openURL. Never blocks. Returns false
// when no backend accepted it — callers should fall back.
func SendNotificationAction(title, body, icon string, expireMs int, action int,
	actionLabel, openURL string) bool {

	ctitle := C.CString(title)
	cbody := C.CString(body)
	cicon := C.CString(icon)
	clabel := C.CString(actionLabel)
	curl := C.CString(openURL)
	ok := C.SendNotificationAction(ctitle, cbody, cicon, C.int(expireMs),
		C.int(action), clabel, curl)
	C.free(unsafe.Pointer(ctitle))
	C.free(unsafe.Pointer(cbody))
	C.free(unsafe.Pointer(cicon))
	C.free(unsafe.Pointer(clabel))
	C.free(unsafe.Pointer(curl))
	return ok != 0
}

// PollNotificationAction returns the pending notification action (0 if none)
// and the URL the notification was posted with. Call once per frame.
func PollNotificationAction() (int, string) {
	buf := (*C.char)(C.malloc(512))
	defer C.free(unsafe.Pointer(buf))
	action := C.PollNotificationAction(buf, 512)
	if action == 0 {
		return 0, ""
	}
	return int(action), C.GoString(buf)
}
