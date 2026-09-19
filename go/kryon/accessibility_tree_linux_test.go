//go:build linux

package kryon

import (
	"testing"

	"github.com/godbus/dbus/v5"
)

func TestAccessibilityDBusTreeIdentity(t *testing.T) {
	address, client := accessibilityTestBus(t)
	bus, err := openAccessibilityBus(address, "Tree")
	if err != nil {
		t.Fatal(err)
	}
	defer bus.close()
	nodes := []AccessibilityNode{
		{Role: "group", Key: 501, Bounds: NewRectangle(10, 20, 150, 100)},
		{Role: "button", FocusID: 502, Label: "Child", Parent: 1, Bounds: NewRectangle(15, 25, 40, 20)},
		{Role: "group", Key: 503, Bounds: NewRectangle(100, 0, 150, 100)},
	}
	bus.publish(nodes, NewRectangle(30, 40, 300, 200))
	var roots []accessibleReference
	accessibilityCall(t, client, bus, atspiWindow, atspiPrefix+"Accessible.GetChildren").Store(&roots)
	if len(roots) != 2 {
		t.Fatalf("roots = %+v", roots)
	}
	var children []accessibleReference
	accessibilityCall(t, client, bus, roots[0].Path, atspiPrefix+"Accessible.GetChildren").Store(&children)
	if len(children) != 1 {
		t.Fatalf("children = %+v", children)
	}
	child := children[0]
	var rect accessibleRectangle
	accessibilityCall(t, client, bus, child.Path, atspiPrefix+"Component.GetExtents", uint32(2)).Store(&rect)
	if rect.X != 5 || rect.Y != 5 {
		t.Fatalf("parent-relative extents = %+v", rect)
	}
	var hit accessibleReference
	accessibilityCall(t, client, bus, roots[0].Path, atspiPrefix+"Component.GetAccessibleAtPoint", int32(16), int32(26), uint32(1)).Store(&hit)
	if hit != child {
		t.Fatalf("group hit = %+v", hit)
	}
	nodes = []AccessibilityNode{nodes[2], nodes[1], nodes[0]}
	nodes[1].Parent = 1
	bus.publish(nodes, NewRectangle(30, 40, 300, 200))
	var reordered []accessibleReference
	accessibilityCall(t, client, bus, atspiWindow, atspiPrefix+"Accessible.GetChildren").Store(&reordered)
	if reordered[0] != roots[1] || reordered[1] != roots[0] {
		t.Fatal("keyed groups lost identity after reorder")
	}
	accessibilityCall(t, client, bus, roots[1].Path, atspiPrefix+"Accessible.GetChildren").Store(&children)
	if len(children) != 1 || children[0] != child {
		t.Fatal("child lost identity after reparenting")
	}
	var parent dbus.Variant
	accessibilityCall(t, client, bus, child.Path, "org.freedesktop.DBus.Properties.Get", atspiPrefix+"Accessible", "Parent").Store(&parent)
	var actual accessibleReference
	if err := parent.Store(&actual); err != nil || actual != roots[1] {
		t.Fatalf("reparented reference = %v: %v", parent, err)
	}
	var items []accessibleCacheItem
	accessibilityCall(t, client, bus, atspiCache, atspiPrefix+"Cache.GetItems").Store(&items)
	if len(items) != 5 || items[1].Children != 2 || items[2].Children != 1 || items[3].Parent != roots[1] {
		t.Fatalf("nested cache = %+v", items)
	}
}
