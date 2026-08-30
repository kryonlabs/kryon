//go:build linux

package kryon

// org.freedesktop.Notifications client on the session bus (xfce4-notifyd,
// GNOME Shell, plasma-workspace, dunst, …). Mirrors the C notification
// backend's semantics: one action button whose clicks arrive through
// PollNotificationAction together with the URL captured at send time.

import (
	"log"
	"sync"
	"time"
)

// The single action key this client uses for popup buttons (C backends use
// the same constant, so behavior matches across runtimes).
const dbusNotificationActionKey = "open"

const dbusNotifyMethod = "org.freedesktop.Notifications.Notify"

type notifyRuntime struct {
	mu      sync.Mutex
	conn    *dbusConn
	appName string
	slots   [32]struct {
		daemonID uint32
		action   int32
		url      string
	}
	pending struct {
		action int32
		url    string
	}
}

var notifyRT notifyRuntime

// SetNotificationAppName sets the daemon-side name used for icon lookup and
// notification grouping. Safe before any notification is sent.
func SetNotificationAppName(name string) {
	notifyRT.mu.Lock()
	notifyRT.appName = name
	notifyRT.mu.Unlock()
}

// NotificationSupported reports whether a session bus and a notification
// daemon are reachable. It must not be called before the window opens on
// GUI runtimes — it dials the bus on first use.
func NotificationSupported() bool {
	notifyRT.mu.Lock()
	defer notifyRT.mu.Unlock()
	if notifyRT.conn == nil {
		notifyRT.dial()
	}
	return notifyRT.conn != nil
}

// dial connects and wires the action signals. Caller holds mu.
func (n *notifyRuntime) dial() {
	conn, err := dbusSession()
	if err != nil {
		log.Printf("kryon notify: %v", err)
		return
	}
	notifyRT.conn = conn
	conn.onSignal("type='signal',interface='org.freedesktop.Notifications',member='ActionInvoked'",
		n.onActionInvoked)
	conn.onSignal("type='signal',interface='org.freedesktop.Notifications',member='NotificationClosed'",
		n.onClosed)
}

func (n *notifyRuntime) onActionInvoked(m *dbusMessage) {
	vals, err := dbusDecodeAll("us", m.body)
	if err != nil || len(vals) != 2 {
		return
	}
	id, _ := vals[0].(uint32)
	key, _ := vals[1].(string)
	if key != dbusNotificationActionKey {
		return
	}
	n.mu.Lock()
	defer n.mu.Unlock()
	for i := range n.slots {
		if n.slots[i].daemonID == id && n.slots[i].daemonID != 0 {
			n.pending.action = n.slots[i].action
			n.pending.url = n.slots[i].url
			n.slots[i] = struct {
				daemonID uint32
				action   int32
				url      string
			}{}
			return
		}
	}
}

func (n *notifyRuntime) onClosed(m *dbusMessage) {
	vals, err := dbusDecodeAll("uu", m.body)
	if err != nil || len(vals) != 2 {
		return
	}
	id, _ := vals[0].(uint32)
	n.mu.Lock()
	defer n.mu.Unlock()
	for i := range n.slots {
		if n.slots[i].daemonID == id {
			n.slots[i] = struct {
				daemonID uint32
				action   int32
				url      string
			}{}
			return
		}
	}
}

// notify submits one notification. actions may be nil for a plain popup.
// It returns the daemon id or 0 on failure.
func (n *notifyRuntime) notify(title, body, icon string, replacesID uint32,
	actions []string, urgency byte, expireMS int32) uint32 {
	n.mu.Lock()
	if n.conn == nil {
		n.dial()
	}
	conn := n.conn
	app := n.appName
	if app == "" {
		app = "kryon"
	}
	n.mu.Unlock()
	if conn == nil {
		return 0
	}
	if actions == nil {
		actions = []string{}
	}
	hints := map[string]any{"urgency": dbusVariant{Sig: "y", Val: urgency}}
	args := []any{app, replacesID, icon, title, body, actions, hints, expireMS}
	reply, err := conn.call("org.freedesktop.Notifications", "/org/freedesktop/Notifications",
		"org.freedesktop.Notifications", "Notify",
		"susssasa{sv}i", args, 8*time.Second)
	if err != nil {
		log.Printf("kryon notify: %v", err)
		n.mu.Lock()
		conn.close() // a dead bus reconnects on the next send
		n.conn = nil
		n.mu.Unlock()
		return 0
	}
	vals, err := dbusDecodeAll("u", reply.body)
	if err != nil || len(vals) == 0 {
		return 0
	}
	id, _ := vals[0].(uint32)
	return id
}

// SendNotification fires a plain notification; true when the daemon took it.
func SendNotification(title, body string) bool {
	return notifyRT.notify(title, body, "", 0, nil, NotificationPriorityDefault, 0) != 0
}

// SendNotificationEx sends with an explicit replace id and priority.
func SendNotificationEx(title, body, tag string, id int32, priority int) bool {
	urgency := byte(0)
	if priority == NotificationPriorityLow {
		urgency = 1
	} else if priority == NotificationPriorityHigh {
		urgency = 2
	}
	_ = tag // grouping is a caller concern on desktop daemons
	return notifyRT.notify(title, body, "", uint32(id), nil, urgency, 0) != 0
}

// SendNotificationAction sends a popup carrying one button labeled
// actionLabel. A click is delivered once by PollNotificationAction with
// actionURL attached. Returns false when the backend cannot do actions —
// callers should fall back to a plain notification.
func SendNotificationAction(title, body, icon string, expireMS, action int32,
	actionLabel, actionURL string) bool {
	if actionLabel == "" {
		actionLabel = "Open"
	}
	id := notifyRT.notify(title, body, icon, 0,
		[]string{dbusNotificationActionKey, actionLabel}, 0, expireMS)
	if id == 0 {
		return false
	}
	notifyRT.mu.Lock()
	defer notifyRT.mu.Unlock()
	slot := -1
	for i := range notifyRT.slots {
		if notifyRT.slots[i].daemonID == 0 {
			slot = i
			break
		}
	}
	if slot < 0 {
		slot = int(id) % len(notifyRT.slots)
	}
	notifyRT.slots[slot] = struct {
		daemonID uint32
		action   int32
		url      string
	}{id, action, actionURL}
	return true
}

// PollNotificationAction returns the pending popup action (0 when none)
// together with the URL captured when the notification was sent. One slot:
// a click arriving before the previous one was polled replaces it.
func PollNotificationAction() (int32, string) {
	notifyRT.mu.Lock()
	defer notifyRT.mu.Unlock()
	action := notifyRT.pending.action
	url := notifyRT.pending.url
	notifyRT.pending.action = 0
	notifyRT.pending.url = ""
	return action, url
}
