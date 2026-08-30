//go:build !linux

package kryon

// Desktop tray and notification stubs for platforms without the session
// bus implementation: everything reports unsupported, sends return false.

func SetNotificationAppName(string)                              {}
func NotificationSupported() bool                                { return false }
func SendNotification(string, string) bool                       { return false }
func SendNotificationEx(string, string, string, int32, int) bool { return false }
func SendNotificationAction(string, string, string, int32, int32, string, string) bool {
	return false
}
func PollNotificationAction() (int32, string) { return 0, "" }

func InitDesktopTray(DesktopTraySpec) bool     { return false }
func ShutdownDesktopTray()                     {}
func PollDesktopTrayAction() int               { return 0 }
func SetDesktopTrayStatus(string)              {}
func SetDesktopTrayMenu([]DesktopTrayMenuItem) {}
func SetDesktopTrayActivateAction(int)         {}
func SetDesktopTrayIcon(string)                {}
