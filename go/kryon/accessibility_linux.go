//go:build linux

package kryon

import (
	"context"
	"fmt"
	"os"
	"strconv"
	"sync"
	"time"
	"unicode/utf8"

	"github.com/godbus/dbus/v5"
)

const (
	atspiPrefix                 = "org.a11y.atspi."
	atspiRoot   dbus.ObjectPath = "/org/a11y/atspi/accessible/root"
	atspiWindow dbus.ObjectPath = "/org/a11y/atspi/accessible/window"
	atspiNull   dbus.ObjectPath = "/org/a11y/atspi/null"
)

type accessibleReference struct {
	Bus  string
	Path dbus.ObjectPath
}

type accessibleObject struct {
	node  AccessibilityNode
	path  dbus.ObjectPath
	index int32
}

type platformAccessibilityRequest struct {
	accessibilityRequest
	generation uint64
}

// The D-Bus goroutines only access owned snapshots and this bounded inbox.
// Runtime input and application buffers remain confined to the UI thread.
type accessibilityBus struct {
	mu       sync.Mutex
	conn     *dbus.Conn
	name     string
	title    string
	parent   accessibleReference
	objects  map[dbus.ObjectPath]accessibleObject
	keys     map[string]dbus.ObjectPath
	children []accessibleReference
	next     uint64
	pending  []platformAccessibilityRequest
	closed   bool
	appID    int32
	bounds   Rectangle
	active   bool
}

func openAccessibilityBus(address, title string) (*accessibilityBus, error) {
	if address == "" {
		address = os.Getenv("AT_SPI_BUS_ADDRESS")
	}
	if address == "" {
		session, err := dbus.ConnectSessionBus()
		if err != nil {
			return nil, err
		}
		ctx, cancel := context.WithTimeout(context.Background(), time.Second)
		err = session.Object("org.a11y.Bus", "/org/a11y/bus").CallWithContext(ctx,
			"org.a11y.Bus.GetAddress", 0).Store(&address)
		cancel()
		session.Close()
		if err != nil {
			return nil, err
		}
	}
	conn, err := dbus.Connect(address)
	if err != nil {
		return nil, err
	}
	bus := &accessibilityBus{conn: conn, name: conn.Names()[0], title: title,
		objects: make(map[dbus.ObjectPath]accessibleObject), keys: make(map[string]dbus.ObjectPath),
		parent: accessibleReference{Path: atspiNull}}
	if err := bus.export(); err != nil {
		conn.Close()
		return nil, err
	}
	ctx, cancel := context.WithTimeout(context.Background(), time.Second)
	var parent accessibleReference
	err = conn.Object("org.a11y.atspi.Registry", atspiRoot).CallWithContext(ctx,
		atspiPrefix+"Socket.Embed", 0, accessibleReference{bus.name, atspiRoot}).Store(&parent)
	cancel()
	if err != nil {
		conn.Close()
		return nil, err
	}
	bus.mu.Lock()
	bus.parent = parent
	bus.mu.Unlock()
	return bus, nil
}

type accessibilityConnection struct {
	mu     sync.Mutex
	bus    *accessibilityBus
	closed bool
}

func startAccessibilityConnection(title string) *accessibilityConnection {
	if os.Getenv("NO_AT_BRIDGE") == "1" || os.Getenv("KRYON_ACCESSIBILITY") == "0" {
		return nil
	}
	connection := &accessibilityConnection{}
	go func() {
		bus, err := openAccessibilityBus("", title)
		if err != nil {
			if os.Getenv("KRYON_WINDOW_DEBUG") != "" {
				fmt.Fprintf(os.Stderr, "kryon: accessibility bus unavailable: %v\n", err)
			}
			return
		}
		connection.mu.Lock()
		defer connection.mu.Unlock()
		if connection.closed {
			bus.close()
		} else {
			connection.bus = bus
		}
	}()
	return connection
}

func (connection *accessibilityConnection) current() *accessibilityBus {
	if connection == nil {
		return nil
	}
	connection.mu.Lock()
	defer connection.mu.Unlock()
	return connection.bus
}

func (connection *accessibilityConnection) close() {
	if connection == nil {
		return
	}
	connection.mu.Lock()
	defer connection.mu.Unlock()
	connection.closed = true
	connection.bus.close()
	connection.bus = nil
}

func (b *accessibilityBus) close() {
	if b == nil {
		return
	}
	b.mu.Lock()
	b.closed = true
	for _, request := range b.pending {
		clear(request.value)
	}
	b.pending = nil
	b.objects = nil
	b.children = nil
	b.keys = nil
	b.mu.Unlock()
	b.conn.Close()
}

func (b *accessibilityBus) drain(runtime Runtime) {
	if b == nil {
		return
	}
	b.mu.Lock()
	requests := b.pending
	b.pending = nil
	b.mu.Unlock()
	for _, request := range requests {
		switch request.action {
		case AccessibilityActionSetValue:
			runtime.QueueAccessibilityValue(request.id, request.generation, string(request.value))
		case AccessibilityActionSetSelection:
			runtime.QueueAccessibilitySelection(request.id, request.generation, request.anchor, request.cursor)
		default:
			runtime.QueueAccessibilityAction(request.id, request.generation, request.action)
		}
		clear(request.value)
	}
}

func (b *accessibilityBus) event(path dbus.ObjectPath, event, detail string, first, second int32, value any) {
	b.conn.Emit(path, atspiPrefix+"Event.Object."+event, detail, first, second,
		dbus.MakeVariant(value), map[string]dbus.Variant{})
}

func (b *accessibilityBus) windowActive(active bool) {
	b.mu.Lock()
	defer b.mu.Unlock()
	if b.closed || b.active == active {
		return
	}
	b.active = active
	name := "Deactivate"
	if active {
		name = "Activate"
	}
	b.conn.Emit(atspiWindow, atspiPrefix+"Event.Window."+name, "", int32(0), int32(0),
		dbus.MakeVariant(b.title), map[string]dbus.Variant{})
}

func (b *accessibilityBus) publish(nodes []AccessibilityNode, bounds Rectangle) {
	if b == nil {
		return
	}
	b.mu.Lock()
	defer b.mu.Unlock()
	if b.closed {
		return
	}
	b.bounds = bounds
	objects := make(map[dbus.ObjectPath]accessibleObject, len(nodes))
	keys := make(map[string]dbus.ObjectPath, len(nodes))
	children := make([]accessibleReference, 0, len(nodes))
	ids := make(map[int32]int)
	for _, node := range nodes {
		if node.FocusID > 0 {
			ids[node.FocusID]++
		}
	}
	for index, node := range nodes {
		if node.Role == "main" {
			continue
		}
		key := fmt.Sprintf("position:%d:%s", index, node.Role)
		if node.FocusID > 0 && ids[node.FocusID] == 1 {
			key = fmt.Sprintf("focus:%d:%s", node.FocusID, node.Role)
		} else {
			node.Actions = 0
		}
		if node.Secure {
			node.Value = ""
			node.SelectionAnchor, node.SelectionCursor = 0, 0
		}
		path := b.keys[key]
		if path == "" {
			b.next++
			path = dbus.ObjectPath(fmt.Sprintf("/org/a11y/atspi/accessible/node_%d", b.next))
		}
		keys[key] = path
		objects[path] = accessibleObject{node: node, path: path, index: int32(len(children))}
		children = append(children, accessibleReference{b.name, path})
	}
	oldObjects := b.objects
	b.objects, b.keys, b.children = objects, keys, children
	if len(oldObjects) != len(objects) {
		b.cacheUpdate(b.toplevel(atspiWindow))
	}
	for path, old := range oldObjects {
		if _, exists := objects[path]; !exists {
			b.conn.Emit(atspiCache, atspiPrefix+"Cache.RemoveAccessible", accessibleReference{b.name, path})
			b.event(path, "StateChanged", "defunct", 1, 0, int32(0))
			b.event(atspiWindow, "ChildrenChanged", "remove", old.index, 0, accessibleReference{b.name, path})
		}
	}
	for _, ref := range children {
		current := objects[ref.Path]
		old, exists := oldObjects[ref.Path]
		if !exists || old.index != current.index || old.node.Label != current.node.Label ||
			old.node.Focused != current.node.Focused || old.node.Checked != current.node.Checked ||
			old.node.Disabled != current.node.Disabled || old.node.ReadOnly != current.node.ReadOnly ||
			old.node.Actions != current.node.Actions || old.node.Secure != current.node.Secure {
			b.cacheUpdate(current)
		}
		if !exists {
			b.event(atspiWindow, "ChildrenChanged", "add", current.index, 0, ref)
		}
		for _, state := range []struct {
			name     string
			old, now bool
		}{
			{"focused", old.node.Focused, current.node.Focused},
			{"checked", old.node.Checked, current.node.Checked},
			{"enabled", !old.node.Disabled, !current.node.Disabled},
		} {
			if state.old != state.now {
				enabled := int32(0)
				if state.now {
					enabled = 1
				}
				b.event(ref.Path, "StateChanged", state.name, enabled, 0, int32(0))
			}
		}
		if exists && old.node.Label != current.node.Label {
			b.event(ref.Path, "PropertyChange", "accessible-name", 0, 0, current.node.Label)
		}
		if exists && !current.node.Secure && old.node.Value != current.node.Value {
			b.event(ref.Path, "TextChanged", "delete", 0, int32(utf8.RuneCountInString(old.node.Value)), old.node.Value)
			b.event(ref.Path, "TextChanged", "insert", 0, int32(utf8.RuneCountInString(current.node.Value)), current.node.Value)
		}
		if !current.node.Secure && (old.node.SelectionAnchor != current.node.SelectionAnchor || old.node.SelectionCursor != current.node.SelectionCursor) {
			b.event(ref.Path, "TextCaretMoved", "", scalarOffset(current.node.Value, current.node.SelectionCursor), 0, int32(0))
			b.event(ref.Path, "TextSelectionChanged", "", 0, 0, int32(0))
		}
	}
}

func scalarOffset(text string, offset int32) int32 {
	return int32(utf8.RuneCountInString(text[:min(max(int(offset), 0), len(text))]))
}

func byteOffset(text string, offset int32) (int32, bool) {
	if offset < 0 {
		return 0, false
	}
	count := int32(0)
	for index := range text {
		if count == offset {
			return int32(index), true
		}
		count++
	}
	return int32(len(text)), count == offset
}

func (b *accessibilityBus) object(message dbus.Message) (accessibleObject, *dbus.Error) {
	path, _ := message.Headers[dbus.FieldPath].Value().(dbus.ObjectPath)
	if b.closed {
		return accessibleObject{}, dbus.NewError("org.freedesktop.DBus.Error.UnknownObject", []any{"closed application"})
	}
	if path == atspiRoot || path == atspiWindow {
		return b.toplevel(path), nil
	}
	object, ok := b.objects[path]
	if !ok {
		return accessibleObject{}, dbus.NewError("org.freedesktop.DBus.Error.UnknownObject", []any{"retired accessible object"})
	}
	return object, nil
}

func (b *accessibilityBus) queue(object accessibleObject, action AccessibilityAction, value string, anchor, cursor int32) bool {
	node := object.node
	if node.FocusID <= 0 || node.Actions&uint32(action) == 0 || len(value) > int(AccessibilityPolicy_AccessibilityValueByteLimit()) || !utf8.ValidString(value) {
		return false
	}
	if action == AccessibilityActionSetValue {
		for _, codepoint := range value {
			if !AccessibilityPolicy_AccessibilityValueCodepointAllowed(codepoint, node.Multiline) {
				return false
			}
		}
	}
	for index, request := range b.pending {
		if request.id == node.FocusID && request.action == action {
			clear(request.value)
			b.pending = append(b.pending[:index], b.pending[index+1:]...)
			break
		}
	}
	if len(b.pending) >= 32 {
		return false
	}
	b.pending = append(b.pending, platformAccessibilityRequest{
		accessibilityRequest: accessibilityRequest{id: node.FocusID, action: action, value: []byte(value), anchor: anchor, cursor: cursor},
		generation:           node.Generation,
	})
	return true
}

func (b *accessibilityBus) interfaces(object accessibleObject) []string {
	interfaces := []string{atspiPrefix + "Accessible", atspiPrefix + "Component"}
	if object.path == atspiRoot {
		return append(interfaces, atspiPrefix+"Application")
	}
	if object.node.Actions&uint32(AccessibilityActionActivate) != 0 {
		interfaces = append(interfaces, atspiPrefix+"Action")
	}
	if object.node.Role == "textbox" || object.node.Role == "text" {
		interfaces = append(interfaces, atspiPrefix+"Text")
		if object.node.Actions&uint32(AccessibilityActionSetValue) != 0 {
			interfaces = append(interfaces, atspiPrefix+"EditableText")
		}
	}
	return interfaces
}

func (b *accessibilityBus) properties(object accessibleObject, iface string) map[string]dbus.Variant {
	values := map[string]any{"version": uint32(1)}
	node := object.node
	switch iface {
	case atspiPrefix + "Accessible":
		parent := accessibleReference{b.name, atspiWindow}
		count := int32(0)
		if object.path == atspiRoot {
			parent, count = b.parent, 1
		} else if object.path == atspiWindow {
			parent, count = accessibleReference{b.name, atspiRoot}, int32(len(b.children))
		}
		values["Name"], values["Description"], values["HelpText"] = node.Label, "", ""
		values["Parent"], values["ChildCount"] = parent, count
		values["Locale"], values["AccessibleId"] = os.Getenv("LANG"), strconv.Itoa(int(node.FocusID))
	case atspiPrefix + "Application":
		values["ToolkitName"], values["Version"], values["ToolkitVersion"] = "Kryon", "", ""
		values["AtspiVersion"], values["InterfaceVersion"], values["Id"] = "2.1", uint32(1), b.appID
	case atspiPrefix + "Action":
		values["NActions"] = int32(1)
	case atspiPrefix + "Text":
		values["CharacterCount"] = int32(utf8.RuneCountInString(accessibleText(node)))
		values["CaretOffset"] = scalarOffset(node.Value, node.SelectionCursor)
	}
	result := make(map[string]dbus.Variant, len(values))
	for key, value := range values {
		result[key] = dbus.MakeVariant(value)
	}
	return result
}

func accessibleText(node AccessibilityNode) string {
	if node.Secure {
		return ""
	}
	if node.Role == "text" {
		return node.Label
	}
	return node.Value
}

func accessibleRole(node AccessibilityNode) uint32 {
	if node.Secure {
		return 40
	}
	roles := map[string]uint32{"application": 75, "frame": 23, "button": 43, "checkbox": 7,
		"radio": 44, "textbox": 61, "text": 61, "group": 39, "img": 27, "combobox": 11,
		"slider": 51, "progressbar": 42, "menu": 33, "tablist": 38, "toolbar": 63,
		"listbox": 98, "tree": 65, "table": 55, "dialog": 16}
	if role, ok := roles[node.Role]; ok {
		return role
	}
	return 67
}

func accessibleStates(node AccessibilityNode) []uint32 {
	bits := uint64(1)<<25 | uint64(1)<<30
	if !node.Disabled {
		bits |= uint64(1)<<8 | uint64(1)<<24
	}
	if node.Actions&uint32(AccessibilityActionFocus) != 0 {
		bits |= uint64(1) << 11
	}
	if node.Focused {
		bits |= uint64(1) << 12
	}
	if node.Checked {
		bits |= uint64(1) << 4
	}
	if node.Role == "checkbox" || node.Role == "radio" {
		bits |= uint64(1) << 41
	}
	if node.ReadOnly {
		bits |= uint64(1) << 43
	}
	if node.Role == "textbox" {
		if !node.ReadOnly && !node.Disabled {
			bits |= uint64(1) << 7
		}
		if node.Multiline {
			bits |= uint64(1) << 17
		} else {
			bits |= uint64(1) << 26
		}
		if !node.Secure {
			bits |= uint64(1) << 38
		}
	}
	return []uint32{uint32(bits), uint32(bits >> 32)}
}
