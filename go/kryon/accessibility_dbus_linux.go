//go:build linux

package kryon

import (
	"encoding/xml"
	"os"
	"reflect"
	"slices"
	"sort"
	"strings"
	"unicode/utf8"

	"github.com/clipperhouse/uax29/v2/graphemes"
	"github.com/clipperhouse/uax29/v2/sentences"
	"github.com/clipperhouse/uax29/v2/words"
	"github.com/godbus/dbus/v5"
	"github.com/godbus/dbus/v5/introspect"
)

func accessibleInvalidArgument() *dbus.Error {
	return dbus.NewError("org.freedesktop.DBus.Error.InvalidArgs", []any{"invalid accessible index or range"})
}

func (b *accessibilityBus) export() error {
	interfaces := map[string]map[string]any{
		atspiPrefix + "Accessible":   b.accessibleMethods(),
		atspiPrefix + "Component":    b.componentMethods(),
		atspiPrefix + "Action":       b.actionMethods(),
		atspiPrefix + "Text":         b.textMethods(),
		atspiPrefix + "EditableText": b.editableMethods(),
		atspiPrefix + "Selection":    b.selectionMethods(),
		atspiPrefix + "Application": {
			"GetLocale":                func(_ accessibleObject, _ uint32) (string, *dbus.Error) { return os.Getenv("LANG"), nil },
			"GetApplicationBusAddress": func(_ accessibleObject) (string, *dbus.Error) { return "", nil },
		},
		"org.freedesktop.DBus.Properties": {
			"Get": func(object accessibleObject, iface, name string) (dbus.Variant, *dbus.Error) {
				if !slices.Contains(b.interfaces(object), iface) {
					return dbus.Variant{}, accessibleInvalidArgument()
				}
				value, ok := b.properties(object, iface)[name]
				if !ok {
					return dbus.Variant{}, accessibleInvalidArgument()
				}
				return value, nil
			},
			"GetAll": func(object accessibleObject, iface string) (map[string]dbus.Variant, *dbus.Error) {
				if !slices.Contains(b.interfaces(object), iface) {
					return nil, accessibleInvalidArgument()
				}
				return b.properties(object, iface), nil
			},
			"Set": func(object accessibleObject, iface, name string, value dbus.Variant) *dbus.Error {
				if object.path == atspiRoot && iface == atspiPrefix+"Application" && name == "Id" {
					id, ok := value.Value().(int32)
					if ok {
						b.appID = id
						return nil
					}
				}
				return dbus.NewError("org.freedesktop.DBus.Error.PropertyReadOnly", []any{"read-only property"})
			},
		},
	}
	interfaces["org.freedesktop.DBus.Introspectable"] = map[string]any{
		"Introspect": func(object accessibleObject) (string, *dbus.Error) {
			node := introspect.Node{Name: string(object.path)}
			for _, name := range append(b.interfaces(object), "org.freedesktop.DBus.Properties", "org.freedesktop.DBus.Introspectable") {
				iface := introspect.Interface{Name: name}
				for methodName, function := range interfaces[name] {
					method := introspect.Method{Name: methodName}
					typeOf := reflect.TypeOf(function)
					for i := 1; i < typeOf.NumIn(); i++ {
						method.Args = append(method.Args, introspect.Arg{Type: dbus.SignatureOfType(typeOf.In(i)).String(), Direction: "in"})
					}
					for i := 0; i < typeOf.NumOut()-1; i++ {
						method.Args = append(method.Args, introspect.Arg{Type: dbus.SignatureOfType(typeOf.Out(i)).String(), Direction: "out"})
					}
					iface.Methods = append(iface.Methods, method)
				}
				sort.Slice(iface.Methods, func(i, j int) bool { return iface.Methods[i].Name < iface.Methods[j].Name })
				if strings.HasPrefix(name, atspiPrefix) {
					for property, value := range b.properties(object, name) {
						access := "read"
						if name == atspiPrefix+"Application" && property == "Id" {
							access = "readwrite"
						}
						iface.Properties = append(iface.Properties, introspect.Property{Name: property, Type: value.Signature().String(), Access: access})
					}
					sort.Slice(iface.Properties, func(i, j int) bool { return iface.Properties[i].Name < iface.Properties[j].Name })
				}
				node.Interfaces = append(node.Interfaces, iface)
			}
			data, err := xml.Marshal(node)
			if err != nil {
				return "", dbus.MakeFailedError(err)
			}
			return introspect.IntrospectDeclarationString + string(data), nil
		},
	}
	for iface, methods := range interfaces {
		wrapped := make(map[string]any, len(methods))
		for name, function := range methods {
			wrapped[name] = b.wrapMethod(iface, function)
		}
		if err := b.conn.ExportSubtreeMethodTable(wrapped, "/org/a11y/atspi/accessible", iface); err != nil {
			return err
		}
	}
	return b.exportCache()
}

// Wrap the typed method once, keeping identity validation and locking identical
// across every D-Bus entry point. The wire signature excludes dbus.Message.
func (b *accessibilityBus) wrapMethod(iface string, function any) any {
	value := reflect.ValueOf(function)
	typeOf := value.Type()
	inputs := []reflect.Type{reflect.TypeOf(dbus.Message{})}
	outputs := make([]reflect.Type, typeOf.NumOut())
	for i := 1; i < typeOf.NumIn(); i++ {
		inputs = append(inputs, typeOf.In(i))
	}
	for i := range outputs {
		outputs[i] = typeOf.Out(i)
	}
	return reflect.MakeFunc(reflect.FuncOf(inputs, outputs, false), func(arguments []reflect.Value) []reflect.Value {
		b.mu.Lock()
		defer b.mu.Unlock()
		object, err := b.object(arguments[0].Interface().(dbus.Message))
		if err == nil && strings.HasPrefix(iface, atspiPrefix) && !slices.Contains(b.interfaces(object), iface) {
			err = dbus.NewError("org.freedesktop.DBus.Error.UnknownInterface", []any{"unsupported accessible interface"})
		}
		if err != nil {
			result := make([]reflect.Value, len(outputs))
			for i, output := range outputs {
				result[i] = reflect.Zero(output)
			}
			result[len(result)-1] = reflect.ValueOf(err)
			return result
		}
		arguments[0] = reflect.ValueOf(object)
		return value.Call(arguments)
	}).Interface()
}

func (b *accessibilityBus) accessibleMethods() map[string]any {
	children := func(object accessibleObject) []accessibleReference {
		if object.path == atspiRoot {
			return []accessibleReference{{b.name, atspiWindow}}
		}
		if object.path == atspiWindow {
			return append([]accessibleReference{}, b.roots...)
		}
		return append([]accessibleReference{}, object.children...)
	}
	type relation struct {
		Kind    uint32
		Targets []accessibleReference
	}
	return map[string]any{
		"GetChildren": func(object accessibleObject) ([]accessibleReference, *dbus.Error) { return children(object), nil },
		"GetChildAtIndex": func(object accessibleObject, index int32) (accessibleReference, *dbus.Error) {
			values := children(object)
			if index < 0 || int(index) >= len(values) {
				return accessibleReference{}, accessibleInvalidArgument()
			}
			return values[index], nil
		},
		"GetIndexInParent":     func(object accessibleObject) (int32, *dbus.Error) { return object.index, nil },
		"GetRole":              func(object accessibleObject) (uint32, *dbus.Error) { return accessibleRole(object.node), nil },
		"GetRoleName":          func(object accessibleObject) (string, *dbus.Error) { return object.node.Role, nil },
		"GetLocalizedRoleName": func(object accessibleObject) (string, *dbus.Error) { return object.node.Role, nil },
		"GetState":             func(object accessibleObject) ([]uint32, *dbus.Error) { return accessibleStates(object.node), nil },
		"GetAttributes":        func(_ accessibleObject) (map[string]string, *dbus.Error) { return map[string]string{}, nil },
		"GetRelationSet":       func(_ accessibleObject) ([]relation, *dbus.Error) { return []relation{}, nil },
		"GetApplication": func(_ accessibleObject) (accessibleReference, *dbus.Error) {
			return accessibleReference{b.name, atspiRoot}, nil
		},
		"GetInterfaces": func(object accessibleObject) ([]string, *dbus.Error) { return b.interfaces(object), nil },
	}
}

type accessibleRectangle struct{ X, Y, Width, Height int32 }

func (b *accessibilityBus) extents(object accessibleObject, coordinates uint32) (accessibleRectangle, *dbus.Error) {
	if coordinates > 2 {
		return accessibleRectangle{}, accessibleInvalidArgument()
	}
	rect := object.node.Bounds
	if coordinates == 0 && object.path != atspiRoot && object.path != atspiWindow {
		rect.X += b.bounds.X
		rect.Y += b.bounds.Y
	} else if coordinates != 0 && (object.path == atspiRoot || object.path == atspiWindow) {
		rect.X, rect.Y = 0, 0
	} else if coordinates == 2 && object.parent != atspiWindow {
		parent := b.objects[object.parent]
		rect.X -= parent.node.Bounds.X
		rect.Y -= parent.node.Bounds.Y
	}
	return accessibleRectangle{int32(rect.X), int32(rect.Y), int32(rect.Width), int32(rect.Height)}, nil
}

func (b *accessibilityBus) componentMethods() map[string]any {
	contains := func(object accessibleObject, x, y int32, coordinates uint32) (bool, *dbus.Error) {
		rect, err := b.extents(object, coordinates)
		return err == nil && x >= rect.X && y >= rect.Y && x-rect.X < rect.Width && y-rect.Y < rect.Height, err
	}
	return map[string]any{
		"GetExtents": b.extents,
		"GetPosition": func(object accessibleObject, coordinates uint32) (int32, int32, *dbus.Error) {
			rect, err := b.extents(object, coordinates)
			return rect.X, rect.Y, err
		},
		"GetSize": func(object accessibleObject) (int32, int32, *dbus.Error) {
			return int32(object.node.Bounds.Width), int32(object.node.Bounds.Height), nil
		},
		"Contains": contains,
		"GetAccessibleAtPoint": func(object accessibleObject, x, y int32, coordinates uint32) (accessibleReference, *dbus.Error) {
			if coordinates > 2 {
				return accessibleReference{}, accessibleInvalidArgument()
			}
			// Convert once to window coordinates; parent coordinates belong to
			// the queried object, not to each candidate child.
			windowX, windowY := x, y
			if coordinates == 0 {
				windowX -= int32(b.bounds.X)
				windowY -= int32(b.bounds.Y)
			} else if coordinates == 2 && object.parent != atspiWindow {
				parent := b.objects[object.parent]
				windowX += int32(parent.node.Bounds.X)
				windowY += int32(parent.node.Bounds.Y)
			}
			for i := len(b.children) - 1; i >= 0; i-- {
				child := b.objects[b.children[i].Path]
				parent := child.parent
				for parent != "" && parent != atspiWindow && parent != object.path {
					parent = b.objects[parent].parent
				}
				if parent != object.path && object.path != atspiRoot && object.path != atspiWindow {
					continue
				}
				if hit, _ := contains(child, windowX, windowY, 1); hit {
					return b.children[i], nil
				}
			}
			if hit, err := contains(object, x, y, coordinates); err != nil {
				return accessibleReference{}, err
			} else if hit {
				return accessibleReference{b.name, object.path}, nil
			}
			return accessibleReference{Path: atspiNull}, nil
		},
		"GrabFocus": func(object accessibleObject) (bool, *dbus.Error) {
			return b.queue(object, AccessibilityActionFocus, "", 0, 0), nil
		},
		"GetLayer":     func(_ accessibleObject) (uint32, *dbus.Error) { return 3, nil },
		"GetMDIZOrder": func(_ accessibleObject) (int16, *dbus.Error) { return -1, nil },
		"GetAlpha":     func(_ accessibleObject) (float64, *dbus.Error) { return 1, nil },
	}
}

func (b *accessibilityBus) actionMethods() map[string]any {
	name := func(_ accessibleObject, index int32) (string, *dbus.Error) {
		if index != 0 {
			return "", accessibleInvalidArgument()
		}
		return "activate", nil
	}
	type action struct{ Name, Description, KeyBinding string }
	return map[string]any{
		"GetName": name, "GetLocalizedName": name,
		"GetDescription": name,
		"GetKeyBinding": func(_ accessibleObject, index int32) (string, *dbus.Error) {
			if index != 0 {
				return "", accessibleInvalidArgument()
			}
			return "", nil
		},
		"GetActions": func(_ accessibleObject) ([]action, *dbus.Error) { return []action{{Name: "activate"}}, nil },
		"DoAction": func(object accessibleObject, index int32) (bool, *dbus.Error) {
			if index != 0 {
				return false, accessibleInvalidArgument()
			}
			return b.queue(object, AccessibilityActionActivate, "", 0, 0), nil
		},
	}
}

func accessibleTextRange(node AccessibilityNode, start, end int32) (string, *dbus.Error) {
	text := accessibleText(node)
	first, ok := byteOffset(text, start)
	if end == -1 {
		end = int32(utf8.RuneCountInString(text))
	}
	last, valid := byteOffset(text, end)
	if !ok || !valid || end < start {
		return "", accessibleInvalidArgument()
	}
	return text[first:last], nil
}

func (b *accessibilityBus) selectText(object accessibleObject, start, end int32) bool {
	if object.node.Secure {
		return false
	}
	first, ok := byteOffset(object.node.Value, start)
	last, valid := byteOffset(object.node.Value, end)
	return ok && valid && b.queue(object, AccessibilityActionSetSelection, "", first, last)
}

func (b *accessibilityBus) textMethods() map[string]any {
	attributes := func(object accessibleObject) (map[string]string, int32, int32, *dbus.Error) {
		return map[string]string{}, 0, int32(utf8.RuneCountInString(accessibleText(object.node))), nil
	}
	return map[string]any{
		"GetText": func(object accessibleObject, start, end int32) (string, *dbus.Error) {
			return accessibleTextRange(object.node, start, end)
		},
		"GetCharacterAtOffset": func(object accessibleObject, offset int32) (int32, *dbus.Error) {
			text, err := accessibleTextRange(object.node, offset, offset+1)
			if err != nil {
				return 0, err
			}
			codepoint, _ := utf8.DecodeRuneInString(text)
			return codepoint, nil
		},
		"GetStringAtOffset": func(object accessibleObject, offset int32, granularity uint32) (string, int32, int32, *dbus.Error) {
			return accessibleStringAtOffset(accessibleText(object.node), offset, granularity)
		},
		"SetCaretOffset": func(object accessibleObject, offset int32) (bool, *dbus.Error) {
			return b.selectText(object, offset, offset), nil
		},
		"GetNSelections": func(object accessibleObject) (int32, *dbus.Error) {
			if object.node.SelectionAnchor != object.node.SelectionCursor && !object.node.Secure {
				return 1, nil
			}
			return 0, nil
		},
		"GetSelection": func(object accessibleObject, index int32) (int32, int32, *dbus.Error) {
			if index != 0 || object.node.Secure || object.node.SelectionAnchor == object.node.SelectionCursor {
				return 0, 0, accessibleInvalidArgument()
			}
			start := scalarOffset(object.node.Value, object.node.SelectionAnchor)
			end := scalarOffset(object.node.Value, object.node.SelectionCursor)
			return min(start, end), max(start, end), nil
		},
		"AddSelection": func(object accessibleObject, start, end int32) (bool, *dbus.Error) {
			return object.node.SelectionAnchor == object.node.SelectionCursor && b.selectText(object, start, end), nil
		},
		"SetSelection": func(object accessibleObject, index, start, end int32) (bool, *dbus.Error) {
			return index == 0 && b.selectText(object, start, end), nil
		},
		"RemoveSelection": func(object accessibleObject, index int32) (bool, *dbus.Error) {
			cursor := scalarOffset(object.node.Value, object.node.SelectionCursor)
			return index == 0 && b.selectText(object, cursor, cursor), nil
		},
		"GetDefaultAttributes": func(_ accessibleObject) (map[string]string, *dbus.Error) { return map[string]string{}, nil },
		"GetAttributeValue":    func(_ accessibleObject, _ int32, _ string) (string, *dbus.Error) { return "", nil },
		"GetAttributes": func(object accessibleObject, _ int32) (map[string]string, int32, int32, *dbus.Error) {
			return attributes(object)
		},
		"GetAttributeRun": func(object accessibleObject, _ int32, _ bool) (map[string]string, int32, int32, *dbus.Error) {
			return attributes(object)
		},
	}
}

func accessibleStringAtOffset(text string, offset int32, granularity uint32) (string, int32, int32, *dbus.Error) {
	position, valid := byteOffset(text, offset)
	if !valid || granularity > 4 {
		return "", 0, 0, accessibleInvalidArgument()
	}
	if int(position) == len(text) {
		return "", offset, offset, nil
	}
	var iterator interface {
		Next() bool
		Value() string
		Start() int
		End() int
	}
	switch granularity {
	case 0:
		iterator = graphemes.FromString(text)
	case 1:
		iterator = words.FromString(text)
	case 2:
		iterator = sentences.FromString(text)
	case 3, 4:
		start := strings.LastIndexByte(text[:position], '\n') + 1
		end := len(text)
		if next := strings.IndexByte(text[position:], '\n'); next >= 0 {
			end = int(position) + next + 1
		}
		return text[start:end], scalarOffset(text, int32(start)), scalarOffset(text, int32(end)), nil
	}
	for iterator.Next() {
		if int(position) < iterator.End() {
			return iterator.Value(), scalarOffset(text, int32(iterator.Start())), scalarOffset(text, int32(iterator.End())), nil
		}
	}
	return "", offset, offset, nil
}

func (b *accessibilityBus) editableMethods() map[string]any {
	return map[string]any{
		"SetTextContents": func(object accessibleObject, value string) (bool, *dbus.Error) {
			return b.queue(object, AccessibilityActionSetValue, value, 0, 0), nil
		},
		"InsertText": func(object accessibleObject, position int32, value string, length int32) (bool, *dbus.Error) {
			if object.node.Secure || !utf8.ValidString(value) || length < 0 {
				return false, nil
			}
			offset, valid := byteOffset(object.node.Value, position)
			end := min(int(length), len(value))
			if !valid || !utf8.ValidString(value[:end]) {
				return false, nil
			}
			text := object.node.Value[:offset] + value[:end] + object.node.Value[offset:]
			return b.queue(object, AccessibilityActionSetValue, text, 0, 0), nil
		},
		"DeleteText": func(object accessibleObject, start, end int32) (bool, *dbus.Error) {
			if object.node.Secure || end < start {
				return false, nil
			}
			first, valid := byteOffset(object.node.Value, start)
			last, fits := byteOffset(object.node.Value, end)
			if !valid || !fits {
				return false, nil
			}
			return b.queue(object, AccessibilityActionSetValue, object.node.Value[:first]+object.node.Value[last:], 0, 0), nil
		},
	}
}
