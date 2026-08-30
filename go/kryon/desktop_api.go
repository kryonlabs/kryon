package kryon

// Desktop integration API shared by every platform: the types and
// constants here are platform-neutral; the implementations live in the
// platform files (session-bus tray + notifications on Linux, stubs
// elsewhere). Mirrors include/desktop_tray.h and include/notification.h.

// DesktopTrayMenuItemKind mirrors the C enum.
type DesktopTrayMenuItemKind int

const (
	DesktopTrayMenuAction DesktopTrayMenuItemKind = iota
	DesktopTrayMenuSubmenu
	DesktopTrayMenuSeparator
)

// DesktopTrayMenuItem is one menu row; children nest for submenus.
type DesktopTrayMenuItem struct {
	Kind     DesktopTrayMenuItemKind
	Label    string
	Action   int
	Enabled  bool
	Children []DesktopTrayMenuItem
}

// DesktopTraySpec configures the tray icon at startup.
type DesktopTraySpec struct {
	ID             string
	Title          string
	IconName       string
	IconPaths      []string // absolute PNG paths, preferred over IconName
	CloseAction    int      // delivered when the app window is closed (unused here)
	ActivateAction int      // delivered on a plain left click
	MenuItems      []DesktopTrayMenuItem
}

// NotificationPriority mirrors the C enum (NotificationPriority in
// include/notification.h).
const (
	NotificationPriorityDefault = 0
	NotificationPriorityLow     = 1
	NotificationPriorityHigh    = 2
)

// NotificationIDAuto lets the daemon assign an id (C: NOTIFICATION_ID_AUTO).
const NotificationIDAuto = 0
