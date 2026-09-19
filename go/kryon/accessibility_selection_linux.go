//go:build linux

package kryon

import "github.com/godbus/dbus/v5"

func (b *accessibilityBus) selectedChildren(object accessibleObject) []accessibleReference {
	selected := make([]accessibleReference, 0)
	for _, reference := range object.children {
		if b.objects[reference.Path].node.Selected {
			selected = append(selected, reference)
		}
	}
	return selected
}

func (b *accessibilityBus) selectionMethods() map[string]any {
	selectChild := func(object accessibleObject, index int32, selected bool) bool {
		if index < 0 || int(index) >= len(object.children) {
			return false
		}
		child := b.objects[object.children[index].Path]
		if child.node.Disabled {
			return false
		}
		action := AccessibilityActionDeselectItem
		if selected {
			action = AccessibilityActionSelectItem
		}
		return b.queue(object, action, "", child.node.ItemIndex, 0)
	}
	return map[string]any{
		"GetSelectedChild": func(object accessibleObject, index int32) (accessibleReference, *dbus.Error) {
			children := b.selectedChildren(object)
			if index < 0 || int(index) >= len(children) {
				return accessibleReference{Path: atspiNull}, nil
			}
			return children[index], nil
		},
		"SelectChild": func(object accessibleObject, index int32) (bool, *dbus.Error) {
			return selectChild(object, index, true), nil
		},
		"DeselectChild": func(object accessibleObject, index int32) (bool, *dbus.Error) {
			return selectChild(object, index, false), nil
		},
		"DeselectSelectedChild": func(object accessibleObject, index int32) (bool, *dbus.Error) {
			children := b.selectedChildren(object)
			if index < 0 || int(index) >= len(children) {
				return false, nil
			}
			return selectChild(object, b.objects[children[index].Path].index, false), nil
		},
		"IsChildSelected": func(object accessibleObject, index int32) (bool, *dbus.Error) {
			if index < 0 || int(index) >= len(object.children) {
				return false, nil
			}
			return b.objects[object.children[index].Path].node.Selected, nil
		},
		"SelectAll": func(object accessibleObject) (bool, *dbus.Error) {
			return b.queue(object, AccessibilityActionSelectAll, "", 0, 0), nil
		},
		"ClearSelection": func(object accessibleObject) (bool, *dbus.Error) {
			return b.queue(object, AccessibilityActionClearSelection, "", 0, 0), nil
		},
	}
}
