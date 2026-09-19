//go:build linux

package kryon

import "github.com/godbus/dbus/v5"

const atspiCache dbus.ObjectPath = "/org/a11y/atspi/cache"

type accessibleCacheItem struct {
	Object      accessibleReference
	Application accessibleReference
	Parent      accessibleReference
	Index       int32
	Children    int32
	Interfaces  []string
	Name        string
	Role        uint32
	Description string
	States      []uint32
}

func (b *accessibilityBus) cacheItem(object accessibleObject) accessibleCacheItem {
	properties := b.properties(object, atspiPrefix+"Accessible")
	return accessibleCacheItem{
		Object:      accessibleReference{b.name, object.path},
		Application: accessibleReference{b.name, atspiRoot},
		Parent:      properties["Parent"].Value().(accessibleReference),
		Index:       object.index, Children: properties["ChildCount"].Value().(int32),
		Interfaces: b.interfaces(object), Name: object.node.Label,
		Role: accessibleRole(object.node), States: accessibleStates(object.node),
	}
}

func (b *accessibilityBus) toplevel(path dbus.ObjectPath) accessibleObject {
	role := "frame"
	index := int32(0)
	if path == atspiRoot {
		role, index = "application", -1
	}
	return accessibleObject{path: path, index: index,
		node: AccessibilityNode{Role: role, Label: b.title, Bounds: b.bounds}}
}

func (b *accessibilityBus) cacheUpdate(object accessibleObject) {
	b.conn.Emit(atspiCache, atspiPrefix+"Cache.AddAccessible", b.cacheItem(object))
}

func (b *accessibilityBus) exportCache() error {
	return b.conn.ExportMethodTable(map[string]any{
		"GetItems": func() ([]accessibleCacheItem, *dbus.Error) {
			b.mu.Lock()
			defer b.mu.Unlock()
			items := []accessibleCacheItem{b.cacheItem(b.toplevel(atspiRoot)), b.cacheItem(b.toplevel(atspiWindow))}
			for _, child := range b.children {
				items = append(items, b.cacheItem(b.objects[child.Path]))
			}
			return items, nil
		},
	}, atspiCache, atspiPrefix+"Cache")
}
