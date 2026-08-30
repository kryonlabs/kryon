//go:build linux

package kryon

// StatusNotifierItem tray icon (the XDG/KDE spec served by XFCE's sn_tray
// plugin, KDE Plasma, most modern panels) plus just enough of the
// com.canonical.dbusmenu protocol for a nested menu. Pure Go: no GTK, no
// AppIndicator library, nothing linked at build time.

import (
	"fmt"
	"image/png"
	"log"
	"os"
	"sync"
	"time"
)

const (
	sniObjectPath  = "/StatusNotifierItem"
	sniIface       = "org.kde.StatusNotifierItem"
	sniBusPrefix   = "org.kde.StatusNotifierItem"
	menuObjectPath = "/MenuBar"
	menuIface      = "com.canonical.dbusmenu"
)

type trayRuntime struct {
	mu       sync.Mutex
	conn     *dbusConn
	ready    bool
	busName  string
	spec     DesktopTraySpec
	status   string // Title / ToolTip line
	pixmap   []byte // ARGB32 big-endian
	pixW     int
	pixH     int
	menuRev  uint32
	actionCh chan int
}

var trayRT trayRuntime

// InitDesktopTray registers the tray icon; false when no tray host answers.
func InitDesktopTray(spec DesktopTraySpec) bool {
	trayRT.mu.Lock()
	if trayRT.ready {
		trayRT.mu.Unlock()
		return true
	}
	trayRT.mu.Unlock()

	conn, err := dbusSession()
	if err != nil {
		log.Printf("kryon tray: %v", err)
		return false
	}
	name := fmt.Sprintf("%s-%d-1", sniBusPrefix, os.Getpid())
	if _, err := conn.call("org.freedesktop.DBus", "/org/freedesktop/DBus",
		"org.freedesktop.DBus", "RequestName", "su", []any{name, uint32(4)}, 8*time.Second); err != nil {
		conn.close()
		log.Printf("kryon tray: %v", err)
		return false
	}

	trayRT.mu.Lock()
	trayRT.conn = conn
	trayRT.busName = name
	trayRT.spec = spec
	trayRT.status = spec.Title
	trayRT.menuRev = 1
	if trayRT.actionCh == nil {
		trayRT.actionCh = make(chan int, 16)
	}
	trayRT.mu.Unlock()
	trayRT.loadPixmap(spec.IconPaths)

	// The host probes the item the instant registration returns, so the
	// method surfaces must be served before the first Register call.
	trayRT.serveSNI(conn)
	trayRT.serveMenu(conn)

	// Register with the host; tolerate a not-yet-running watcher by watching
	// for it to appear (the panel may start after the app).
	register := func() {
		trayRT.mu.Lock()
		c := trayRT.conn
		trayRT.mu.Unlock()
		if c == nil {
			return
		}
		// The argument is the item's object path, not its bus name: the
		// spec allows both, but XFCE's host only understands paths (steam
		// and blueman register the same way).
		_, err := c.call("org.kde.StatusNotifierWatcher", "/StatusNotifierWatcher",
			"org.kde.StatusNotifierWatcher", "RegisterStatusNotifierItem",
			"s", []any{sniObjectPath}, 8*time.Second)
		if err != nil {
			log.Printf("kryon tray: no watcher yet (%v)", err)
			return
		}
		log.Printf("kryon tray: registered as %s", trayRT.busName)
	}
	register()
	_ = conn.onSignal("type='signal',sender='org.freedesktop.DBus',interface='org.freedesktop.DBus',member='NameOwnerChanged'",
		func(m *dbusMessage) {
			vals, err := dbusDecodeAll("sss", m.body)
			if err != nil || len(vals) != 3 {
				return
			}
			name, _ := vals[0].(string)
			newOwner, _ := vals[2].(string)
			if name == "org.kde.StatusNotifierWatcher" && newOwner != "" {
				register()
			}
		})

	trayRT.mu.Lock()
	trayRT.ready = true
	trayRT.mu.Unlock()
	return true
}

// ShutdownDesktopTray removes the icon and drops the bus connection.
func ShutdownDesktopTray() {
	trayRT.mu.Lock()
	conn := trayRT.conn
	trayRT.conn = nil
	trayRT.ready = false
	trayRT.mu.Unlock()
	if conn != nil {
		conn.close()
	}
}

// PollDesktopTrayAction returns one queued tray action (0 when none):
// menu picks and the icon's ActivateAction alike.
func PollDesktopTrayAction() int {
	select {
	case a := <-trayRT.actionCh:
		return a
	default:
		return 0
	}
}

// SetDesktopTrayStatus updates the tooltip/title line.
func SetDesktopTrayStatus(text string) {
	trayRT.mu.Lock()
	defer trayRT.mu.Unlock()
	if !trayRT.ready || trayRT.status == text {
		return
	}
	trayRT.status = text
	trayRT.propertiesChanged(sniIface, map[string]any{
		"Title":   dbusVariant{Sig: "s", Val: text},
		"ToolTip": dbusVariant{Sig: "(sx(siiay))", Val: trayTooltip(text)},
	}, nil)
}

// SetDesktopTrayIcon switches the pixmap to the PNG at path ("" restores
// the startup icon).
func SetDesktopTrayIcon(path string) {
	trayRT.mu.Lock()
	spec := trayRT.spec
	trayRT.mu.Unlock()
	if path == "" {
		trayRT.loadPixmap(spec.IconPaths)
	} else {
		trayRT.loadPixmap([]string{path})
	}
	trayRT.mu.Lock()
	defer trayRT.mu.Unlock()
	if !trayRT.ready {
		return
	}
	var val any
	if len(trayRT.pixmap) > 0 {
		val = dbusVariant{Sig: "(iiay)", Val: dbusStruct{
			Sig: "(iiay)",
			Val: []any{int32(trayRT.pixW), int32(trayRT.pixH), trayRT.pixmap},
		}}
	} else {
		val = dbusVariant{Sig: "(iiay)", Val: dbusStruct{
			Sig: "(iiay)", Val: []any{int32(0), int32(0), []byte{}},
		}}
	}
	trayRT.propertiesChanged(sniIface, map[string]any{
		"IconPixmap": val,
	}, nil)
}

// SetDesktopTrayMenu replaces the menu and bumps the layout revision.
func SetDesktopTrayMenu(items []DesktopTrayMenuItem) {
	trayRT.mu.Lock()
	defer trayRT.mu.Unlock()
	if !trayRT.ready {
		return
	}
	trayRT.spec.MenuItems = items
	trayRT.menuRev++
	trayRT.layoutUpdated()
}

// SetDesktopTrayActivateAction changes the action delivered on icon click.
func SetDesktopTrayActivateAction(action int) {
	trayRT.mu.Lock()
	trayRT.spec.ActivateAction = action
	trayRT.mu.Unlock()
}

func (t *trayRuntime) queue(action int) {
	if action == 0 {
		return
	}
	select {
	case t.actionCh <- action:
	default: // overflow: drop the oldest
		select {
		case <-t.actionCh:
		default:
		}
		select {
		case t.actionCh <- action:
		default:
		}
	}
}

// loadPixmap decodes the first usable PNG into ARGB32 big-endian bytes.
func (t *trayRuntime) loadPixmap(paths []string) {
	for _, p := range paths {
		f, err := os.Open(p)
		if err != nil {
			continue
		}
		img, err := png.Decode(f)
		f.Close()
		if err != nil {
			continue
		}
		b := img.Bounds()
		w, h := b.Dx(), b.Dy()
		px := make([]byte, 0, w*h*4)
		for y := b.Min.Y; y < b.Max.Y; y++ {
			for x := b.Min.X; x < b.Max.X; x++ {
				r, g, bb, a := img.At(x, y).RGBA()
				px = append(px, byte(a>>8), byte(r>>8), byte(g>>8), byte(bb>>8))
			}
		}
		t.mu.Lock()
		t.pixmap, t.pixW, t.pixH = px, w, h
		t.mu.Unlock()
		return
	}
}

func (t *trayRuntime) tooltip() dbusStruct {
	t.mu.Lock()
	title := t.status
	name := t.spec.IconName
	t.mu.Unlock()
	icon := dbusStruct{Sig: "(siiay)", Val: []any{name, int32(0), int32(0), []byte{}}}
	return dbusStruct{Sig: "(sx(siiay))", Val: []any{title, int64(0), icon}}
}

func trayTooltip(title string) dbusStruct {
	icon := dbusStruct{Sig: "(siiay)", Val: []any{"", int32(0), int32(0), []byte{}}}
	return dbusStruct{Sig: "(sx(siiay))", Val: []any{title, int64(0), icon}}
}

// serveSNI answers the StatusNotifierItem surface.
func (t *trayRuntime) serveSNI(conn *dbusConn) {
	props := map[string]func() dbusVariant{
		"Category": func() dbusVariant { return dbusVariant{Sig: "s", Val: "ApplicationStatus"} },
		"Id":       func() dbusVariant { t.mu.Lock(); defer t.mu.Unlock(); return dbusVariant{Sig: "s", Val: t.spec.ID} },
		"Title":    func() dbusVariant { t.mu.Lock(); defer t.mu.Unlock(); return dbusVariant{Sig: "s", Val: t.status} },
		"Status":   func() dbusVariant { return dbusVariant{Sig: "s", Val: "Active"} },
		"IconName": func() dbusVariant {
			t.mu.Lock()
			defer t.mu.Unlock()
			return dbusVariant{Sig: "s", Val: t.spec.IconName}
		},
		"IconPixmap": func() dbusVariant {
			t.mu.Lock()
			defer t.mu.Unlock()
			return dbusVariant{Sig: "(iiay)", Val: dbusStruct{
				Sig: "(iiay)",
				Val: []any{int32(t.pixW), int32(t.pixH), append([]byte(nil), t.pixmap...)},
			}}
		},
		"AttentionIconName": func() dbusVariant { return dbusVariant{Sig: "s", Val: ""} },
		"AttentionIconPixmap": func() dbusVariant {
			return dbusVariant{Sig: "(iiay)", Val: dbusStruct{
				Sig: "(iiay)", Val: []any{int32(0), int32(0), []byte{}}}}
		},
		"ToolTip":       func() dbusVariant { return dbusVariant{Sig: "(sx(siiay))", Val: t.tooltip()} },
		"Menu":          func() dbusVariant { return dbusVariant{Sig: "o", Val: menuObjectPath} },
		"ItemIsMenu":    func() dbusVariant { return dbusVariant{Sig: "b", Val: false} },
		"WindowId":      func() dbusVariant { return dbusVariant{Sig: "i", Val: int32(0)} },
		"IconThemePath": func() dbusVariant { return dbusVariant{Sig: "s", Val: ""} },
	}

	conn.serve(sniObjectPath, "org.freedesktop.DBus.Properties", "Get", func(m *dbusMessage) (string, []any, *dbusError) {
		vals, err := dbusDecodeAll("ss", m.body)
		if err != nil {
			return "", nil, &dbusError{"org.freedesktop.DBus.Error.InvalidArgs", err.Error()}
		}
		prop, _ := vals[1].(string)
		fn := props[prop]
		if fn == nil {
			return "", nil, &dbusError{"org.freedesktop.DBus.Error.InvalidArgs", "unknown property " + prop}
		}
		return "v", []any{fn()}, nil
	})
	conn.serve(sniObjectPath, "org.freedesktop.DBus.Properties", "GetAll", func(m *dbusMessage) (string, []any, *dbusError) {
		out := map[string]any{}
		for name, fn := range props {
			out[name] = fn()
		}
		return "a{sv}", []any{out}, nil
	})
	conn.serve(sniObjectPath, "org.freedesktop.DBus.Properties", "Set", func(m *dbusMessage) (string, []any, *dbusError) {
		return "", nil, &dbusError{"org.freedesktop.DBus.Error.PropertyReadOnly", "read-only"}
	})

	activate := func(m *dbusMessage) (string, []any, *dbusError) {
		t.mu.Lock()
		action := t.spec.ActivateAction
		t.mu.Unlock()
		t.queue(action)
		return "", nil, nil
	}
	conn.serve(sniObjectPath, sniIface, "Activate", activate)
	conn.serve(sniObjectPath, sniIface, "SecondaryActivate", activate)
	conn.serve(sniObjectPath, sniIface, "Scroll", func(m *dbusMessage) (string, []any, *dbusError) {
		return "", nil, nil
	})
}

// serveMenu answers the flat/nested com.canonical.dbusmenu surface.
func (t *trayRuntime) serveMenu(conn *dbusConn) {
	// dbusmenu property surface: hosts probe Version/Status on /MenuBar
	// through org.freedesktop.DBus.Properties and refuse the whole item
	// when the answer is not the menu's.
	conn.serve(menuObjectPath, "org.freedesktop.DBus.Properties", "Get", func(m *dbusMessage) (string, []any, *dbusError) {
		vals, err := dbusDecodeAll("ss", m.body)
		if err != nil || len(vals) != 2 {
			return "", nil, &dbusError{"org.freedesktop.DBus.Error.InvalidArgs", "bad menu property read"}
		}
		switch vals[1].(string) {
		case "Version":
			return "v", []any{dbusVariant{Sig: "u", Val: uint32(3)}}, nil
		case "Status":
			return "v", []any{dbusVariant{Sig: "s", Val: "normal"}}, nil
		}
		return "", nil, &dbusError{"org.freedesktop.DBus.Error.InvalidArgs", "unknown menu property"}
	})
	conn.serve(menuObjectPath, "org.freedesktop.DBus.Properties", "GetAll", func(m *dbusMessage) (string, []any, *dbusError) {
		return "a{sv}", []any{map[string]any{
			"Version": dbusVariant{Sig: "u", Val: uint32(3)},
			"Status":  dbusVariant{Sig: "s", Val: "normal"},
		}}, nil
	})

	conn.serve(menuObjectPath, menuIface, "GetLayout", func(m *dbusMessage) (string, []any, *dbusError) {
		vals, err := dbusDecodeAll("iias", m.body)
		if err != nil {
			return "", nil, &dbusError{"org.freedesktop.DBus.Error.InvalidArgs", err.Error()}
		}
		parent, _ := vals[0].(int32)
		if parent != 0 {
			return "u(ia{sv}av)", []any{uint32(0), dbusStruct{Sig: "(ia{sv}av)", Val: []any{int32(0), map[string]any{}, []any{}}}}, nil
		}
		t.mu.Lock()
		items := t.spec.MenuItems
		rev := t.menuRev
		t.mu.Unlock()
		layout := trayBuildLayout(items, new(int32))
		return "u(ia{sv}av)", []any{rev, layout}, nil
	})
	conn.serve(menuObjectPath, menuIface, "GetGroupProperties", func(m *dbusMessage) (string, []any, *dbusError) {
		return "a(ia{sv})", []any{[]dbusStruct{}}, nil
	})
	conn.serve(menuObjectPath, menuIface, "AboutToShow", func(m *dbusMessage) (string, []any, *dbusError) {
		return "b", []any{true}, nil
	})
	conn.serve(menuObjectPath, menuIface, "Event", func(m *dbusMessage) (string, []any, *dbusError) {
		vals, err := dbusDecodeAll("isvu", m.body)
		if err != nil {
			return "", nil, nil // events are fire-and-forget
		}
		id, _ := vals[0].(int32)
		eventID, _ := vals[1].(string)
		if eventID != "clicked" {
			return "", nil, nil
		}
		t.mu.Lock()
		items := t.spec.MenuItems
		t.mu.Unlock()
		flat := map[int32]DesktopTrayMenuItem{}
		var walk func(list []DesktopTrayMenuItem, next *int32)
		walk = func(list []DesktopTrayMenuItem, next *int32) {
			for _, it := range list {
				*next++
				flat[int32(*next)] = it
				if it.Kind == DesktopTrayMenuSubmenu {
					walk(it.Children, next)
				}
			}
		}
		n := int32(0)
		walk(items, &n)
		if it, ok := flat[id]; ok && it.Kind == DesktopTrayMenuAction && it.Enabled {
			t.queue(it.Action)
		}
		return "", nil, nil
	})
}

// trayBuildLayout renders items as a dbusmenu layout tree. The counter is
// shared so ids match the Event dispatcher's depth-first numbering.
func trayBuildLayout(items []DesktopTrayMenuItem, counter *int32) dbusStruct {
	var children []any
	for _, it := range items {
		*counter++
		id := *counter
		props := map[string]any{}
		switch it.Kind {
		case DesktopTrayMenuSeparator:
			props["type"] = dbusVariant{Sig: "s", Val: "separator"}
		case DesktopTrayMenuSubmenu:
			props["type"] = dbusVariant{Sig: "s", Val: "submenu"}
			props["label"] = dbusVariant{Sig: "s", Val: it.Label}
			props["children-display"] = dbusVariant{Sig: "s", Val: "submenu"}
		default:
			props["label"] = dbusVariant{Sig: "s", Val: it.Label}
		}
		props["enabled"] = dbusVariant{Sig: "b", Val: it.Enabled}
		props["visible"] = dbusVariant{Sig: "b", Val: true}
		var kids []any
		if it.Kind == DesktopTrayMenuSubmenu && len(it.Children) > 0 {
			kids = append(kids, dbusVariant{
				Sig: "(ia{sv}av)", Val: trayBuildLayout(it.Children, counter),
			})
		}
		children = append(children, dbusVariant{
			Sig: "(ia{sv}av)",
			Val: dbusStruct{Sig: "(ia{sv}av)", Val: []any{id, props, kids}},
		})
	}
	return dbusStruct{Sig: "(ia{sv}av)", Val: []any{int32(0), map[string]any{}, children}}
}

// propertiesChanged broadcasts org.freedesktop.DBus.Properties.PropertiesChanged.
// Caller holds the tray mutex.
func (t *trayRuntime) propertiesChanged(iface string, changed map[string]any, invalidated []string) {
	if t.conn == nil {
		return
	}
	if invalidated == nil {
		invalidated = []string{}
	}
	body, err := t.conn.renderBody("sa{sv}as", []any{iface, changed, invalidated})
	if err != nil {
		return
	}
	_ = t.conn.send(&dbusMessage{
		typ:    dbusMsgSignal,
		path:   sniObjectPath,
		iface:  "org.freedesktop.DBus.Properties",
		member: "PropertiesChanged",
		sig:    "sa{sv}as",
		body:   body,
	})
}

// layoutUpdated tells clients to refetch the menu tree. Caller holds mu.
func (t *trayRuntime) layoutUpdated() {
	body, err := t.conn.renderBody("ui", []any{t.menuRev, int32(0)})
	if err != nil {
		return
	}
	_ = t.conn.send(&dbusMessage{
		typ:    dbusMsgSignal,
		path:   menuObjectPath,
		iface:  menuIface,
		member: "LayoutUpdated",
		sig:    "ui",
		body:   body,
	})
}
