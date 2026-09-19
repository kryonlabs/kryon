package kryon

import (
	"bytes"
	"unicode/utf8"
)

// AccessibilityNode is a semantic projection of a completed frame. Secure
// editor values are omitted; other editor values exclude uncommitted preedit.
type AccessibilityNode struct {
	Bounds          Rectangle
	Role            string
	Label           string
	Focused         bool
	Disabled        bool
	Checked         bool
	FocusID         int32
	Value           string
	ReadOnly        bool
	Secure          bool
	Multiline       bool
	Generation      uint64
	Actions         uint32
	SelectionAnchor int32
	SelectionCursor int32
	Key             uint64
	Parent          uint32
	Selected        bool
	MultiSelect     bool
	ItemIndex       int32
	Offscreen       bool
}

type AccessibilitySink func([]AccessibilityNode)

type accessibilityRequest struct {
	id      int32
	kind    int32
	action  AccessibilityAction
	value   []byte
	anchor  int32
	cursor  int32
	itemKey uint64
}

type accessibilityState struct {
	pending    []accessibilityRequest
	active     []accessibilityRequest
	activation int32
	building   bool
}

// QueueAccessibilityAction accepts requests only against the current completed
// snapshot. Delivery happens at the next declaration and is revalidated there.
func QueueAccessibilityAction(focusID int32, generation uint64, action AccessibilityAction) bool {
	return activeRuntime != nil && activeRuntime.QueueAccessibilityAction(focusID, generation, action)
}

func (h *Host) QueueAccessibilityAction(focusID int32, generation uint64, action AccessibilityAction) bool {
	return h != nil && h.runtime != nil && h.runtime.QueueAccessibilityAction(focusID, generation, action)
}

func (r *runtime) QueueAccessibilityAction(focusID int32, generation uint64, action AccessibilityAction) bool {
	if action != AccessibilityActionFocus && action != AccessibilityActionActivate &&
		action != AccessibilityActionSelectAll && action != AccessibilityActionClearSelection {
		return false
	}
	return r.queueAccessibilityRequest(generation, accessibilityRequest{id: focusID, action: action})
}

func QueueAccessibilityItem(focusID int32, generation uint64, index int32, selected bool) bool {
	return activeRuntime != nil && activeRuntime.QueueAccessibilityItem(focusID, generation, index, selected)
}

func (h *Host) QueueAccessibilityItem(focusID int32, generation uint64, index int32, selected bool) bool {
	return h != nil && h.runtime != nil && h.runtime.QueueAccessibilityItem(focusID, generation, index, selected)
}

func (r *runtime) QueueAccessibilityItem(focusID int32, generation uint64, index int32, selected bool) bool {
	action := AccessibilityActionDeselectItem
	if selected {
		action = AccessibilityActionSelectItem
	}
	return r.queueAccessibilityRequest(generation, accessibilityRequest{id: focusID, action: action, anchor: index})
}

// QueueAccessibilityValue owns a bounded UTF-8 copy for atomic replacement in
// the next frame. Acceptance does not guarantee the live editor's limits fit.
func QueueAccessibilityValue(focusID int32, generation uint64, value string) bool {
	return activeRuntime != nil && activeRuntime.QueueAccessibilityValue(focusID, generation, value)
}

// QueueAccessibilitySelection uses UTF-8 byte offsets, clamped and rounded down
// to committed-text grapheme boundaries when delivered in the next frame.
func QueueAccessibilitySelection(focusID int32, generation uint64, anchor, cursor int32) bool {
	return activeRuntime != nil && activeRuntime.QueueAccessibilitySelection(focusID, generation, anchor, cursor)
}

func (h *Host) QueueAccessibilityValue(focusID int32, generation uint64, value string) bool {
	return h != nil && h.runtime != nil && h.runtime.QueueAccessibilityValue(focusID, generation, value)
}

func (h *Host) QueueAccessibilitySelection(focusID int32, generation uint64, anchor, cursor int32) bool {
	return h != nil && h.runtime != nil && h.runtime.QueueAccessibilitySelection(focusID, generation, anchor, cursor)
}

func (r *runtime) QueueAccessibilityValue(focusID int32, generation uint64, value string) bool {
	if len(value) > int(AccessibilityPolicy_AccessibilityValueByteLimit()) || !utf8.ValidString(value) {
		return false
	}
	return r.queueAccessibilityRequest(generation, accessibilityRequest{
		id: focusID, action: AccessibilityActionSetValue, value: []byte(value),
	})
}

func (r *runtime) QueueAccessibilitySelection(focusID int32, generation uint64, anchor, cursor int32) bool {
	return r.queueAccessibilityRequest(generation, accessibilityRequest{
		id: focusID, action: AccessibilityActionSetSelection, anchor: anchor, cursor: cursor,
	})
}

func (r *runtime) queueAccessibilityRequest(generation uint64, request accessibilityRequest) (accepted bool) {
	defer func() {
		if !accepted {
			clear(request.value)
		}
	}()
	focusID, action := request.id, request.action
	if r.closed || r.accessibility.building || generation == 0 || generation != uint64(r.frames) || focusID <= 0 {
		return false
	}
	kind := int32(0)
	actions := uint32(0)
	owner := -1
	for index, op := range r.ops {
		candidate := accessibilityKind(op)
		id := op.ID
		if op.FocusID != 0 {
			id = op.FocusID
		}
		if id != focusID || AccessibilityPolicy_AccessibilityActionsFor(candidate, id, false, false, false) == 0 {
			continue
		}
		if kind != 0 {
			return false
		}
		kind = candidate
		owner = index
		actions = AccessibilityPolicy_AccessibilityActionsFor(kind, id, op.Disabled || op.Loading, r.popupFocusCaptures(id), op.ReadOnly)
		if kind == int32(WidgetKindListBox) && !op.accessibilityMultiSelect {
			actions &^= uint32(AccessibilityActionSelectAll)
		}
	}
	if !AccessibilityPolicy_AccessibilityActionAllowed(actions, action) {
		return false
	}
	if action == AccessibilityActionSelectItem || action == AccessibilityActionDeselectItem {
		found := false
		for _, op := range r.ops {
			if op.accessibilityParent != owner+1 || op.Role != "option" || op.Row != request.anchor || op.Disabled {
				continue
			}
			if len(op.Text) > int(AccessibilityPolicy_AccessibilityValueByteLimit()) {
				return false
			}
			request.itemKey = op.accessibilityKey
			request.value = []byte(op.Text)
			found = true
			break
		}
		if !found {
			return false
		}
	}
	if action == AccessibilityActionSetValue {
		for _, codepoint := range string(request.value) {
			if !AccessibilityPolicy_AccessibilityValueCodepointAllowed(codepoint, kind == int32(WidgetKindTextArea)) {
				return false
			}
		}
	}
	for i, previous := range r.accessibility.pending {
		if previous.id == focusID && previous.action == action &&
			(action != AccessibilityActionSelectItem && action != AccessibilityActionDeselectItem || previous.anchor == request.anchor) {
			clear(previous.value)
			r.accessibility.pending = append(r.accessibility.pending[:i], r.accessibility.pending[i+1:]...)
			break
		}
	}
	if len(r.accessibility.pending) == 32 {
		return false
	}
	request.kind = kind
	r.accessibility.pending = append(r.accessibility.pending, request)
	return true
}

func (r *runtime) beginAccessibilityFrame() {
	r.clearAccessibilityRequests(r.accessibility.active)
	r.accessibility.active, r.accessibility.pending = r.accessibility.pending, r.accessibility.active[:0]
	r.accessibility.activation = 0
	r.accessibility.building = true
}

func (r *runtime) endAccessibilityFrame() {
	r.clearAccessibilityRequests(r.accessibility.active)
	r.accessibility.active = r.accessibility.active[:0]
	r.accessibility.activation = 0
	r.accessibility.building = false
}

func (r *runtime) clearAccessibilityRequests(requests []accessibilityRequest) {
	for _, request := range requests {
		clear(request.value)
	}
	clear(requests)
}

func (r *runtime) prepareAccessibility(id, kind int32, enabled bool) {
	if len(r.accessibility.active) == 0 {
		return
	}
	actions := AccessibilityPolicy_AccessibilityActionsFor(kind, id, !enabled || r.contentDisabled(), r.popupKeyboardCaptures(), false)
	for i, request := range r.accessibility.active {
		if request.id != id || id <= 0 {
			continue
		}
		if request.kind == kind && request.action != AccessibilityActionFocus && request.action != AccessibilityActionActivate {
			continue
		}
		r.accessibility.active[i].id = 0
		if request.kind != kind || !AccessibilityPolicy_AccessibilityActionAllowed(actions, request.action) {
			continue
		}
		r.setFocus(id)
		if request.action == AccessibilityActionActivate {
			r.accessibility.activation = id
		}
	}
}

func (r *runtime) applyAccessibilityText(id int32, buf []byte, cursor *int32, options textEditOptions) bool {
	if len(r.accessibility.active) == 0 || cursor == nil || len(buf) == 0 {
		return false
	}
	kind := int32(WidgetKindTextField)
	if options.multiline {
		kind = int32(WidgetKindTextArea)
	}
	actions := AccessibilityPolicy_AccessibilityActionsFor(kind, id, r.contentDisabled(), r.popupKeyboardCaptures(), options.readOnly)
	changed := false
	for i, request := range r.accessibility.active {
		if id <= 0 || request.id != id || (request.action != AccessibilityActionSetValue && request.action != AccessibilityActionSetSelection) {
			continue
		}
		r.accessibility.active[i].id = 0
		if request.kind != kind || !AccessibilityPolicy_AccessibilityActionAllowed(actions, request.action) {
			continue
		}
		length := zeroIndex(buf)
		if length == len(buf) {
			continue
		}
		var selected selection
		if request.action == AccessibilityActionSetValue {
			capacity := min(len(buf), int(AccessibilityPolicy_AccessibilityValueByteLimit())+1)
			if !AccessibilityPolicy_AccessibilityValueFits(int32(len(request.value)), int32(utf8.RuneCount(request.value)), int32(capacity), options.maxCodepoints) {
				continue
			}
			changed = !bytes.Equal(buf[:length], request.value) || changed
			clear(buf)
			copy(buf, request.value)
			selected = collapsedSelection(len(request.value))
		} else {
			text := string(buf[:length])
			selected = selection{Anchor: clampCursor(text, int(request.anchor)), Cursor: clampCursor(text, int(request.cursor))}
		}
		r.setFocus(id)
		delete(r.preedit, id)
		r.ClearTextComposition()
		*cursor = int32(selected.Cursor)
		if selected.Anchor == selected.Cursor {
			delete(r.selection, id)
		} else {
			r.selection[id] = selected
		}
	}
	return changed
}

func (r *runtime) takeAccessibilityActivation(id int32) bool {
	if id <= 0 || r.accessibility.activation != id {
		return false
	}
	r.accessibility.activation = 0
	return true
}

func accessibilityKind(op FrameOp) int32 {
	if op.Kind == FrameOpButton && accessibilityRole(op) != "button" {
		return 0
	}
	if op.accessibilityKind != 0 {
		return op.accessibilityKind
	}
	switch op.Kind {
	case FrameOpButton:
		return int32(WidgetKindButton)
	case FrameOpTextField:
		return int32(WidgetKindTextField)
	case FrameOpTextArea:
		return int32(WidgetKindTextArea)
	default:
		return 0
	}
}

// GetAccessibilitySnapshot returns an owned slice. Call after EndFrame on the
// runtime's UI thread. This host API does not install an OS screen-reader bridge.
func GetAccessibilitySnapshot() []AccessibilityNode {
	if activeRuntime == nil {
		return nil
	}
	return activeRuntime.GetAccessibilitySnapshot()
}

func SetAccessibilitySink(sink AccessibilitySink) {
	if activeRuntime != nil {
		activeRuntime.SetAccessibilitySink(sink)
	}
}

func (h *Host) GetAccessibilitySnapshot() []AccessibilityNode {
	if h == nil || h.runtime == nil {
		return nil
	}
	return h.runtime.GetAccessibilitySnapshot()
}

func (h *Host) SetAccessibilitySink(sink AccessibilitySink) {
	if h != nil && h.runtime != nil {
		h.runtime.SetAccessibilitySink(sink)
	}
}

func (r *runtime) SetAccessibilitySink(sink AccessibilitySink) {
	r.accessibilitySink = sink
}

func (r *runtime) recordAccessibleText(text string) bool {
	for i := len(r.layout) - 1; i >= 0; i-- {
		owner := r.layout[i].accessibilityOwner
		if owner > 0 && owner <= len(r.ops) && r.ops[owner-1].Kind == FrameOpButton {
			if r.ops[owner-1].AccessibleLabel == "" {
				r.ops[owner-1].AccessibleLabel = text
			}
			return true
		}
	}
	return false
}

func (r *runtime) GetAccessibilitySnapshot() []AccessibilityNode {
	var nodes []AccessibilityNode
	parents := make([]uint32, len(r.ops)+1)
	for index, op := range r.ops {
		parent := uint32(0)
		if op.accessibilityParent > 0 && op.accessibilityParent <= index {
			parent = parents[op.accessibilityParent]
		}
		parents[index+1] = parent
		role := accessibilityRole(op)
		if role == "" || role == "presentation" || role == "none" {
			continue
		}
		node := AccessibilityNode{
			Key: op.accessibilityKey, Parent: parent,
			Selected: op.Selected && role == "option", MultiSelect: op.accessibilityMultiSelect, ItemIndex: op.Row, Offscreen: op.accessibilityOffscreen,
			Bounds: op.Bounds, Role: role, Label: op.Text,
			Disabled: op.Disabled || op.Loading, FocusID: op.ID,
			ReadOnly: op.ReadOnly, Secure: op.Secure,
			Checked: op.Selected && (role == "checkbox" || role == "radio"),
		}
		if op.FocusID != 0 {
			node.FocusID = op.FocusID
		}
		switch role {
		case "main", "group", "text", "img", "progressbar":
			node.FocusID = 0
		}
		if node.Key == 0 && node.FocusID > 0 {
			node.Key = uint64(node.FocusID)
		}
		if op.AccessibleBounds.Width > 0 && op.AccessibleBounds.Height > 0 {
			node.Bounds = op.AccessibleBounds
		}
		if op.AccessibleLabel != "" {
			node.Label = op.AccessibleLabel
		}
		if role == "textbox" {
			node.Label = op.AccessibleLabel
			node.Multiline = op.Kind == FrameOpTextArea
			if !op.Secure {
				node.Value = op.AccessibleValue
				node.SelectionAnchor = op.accessibilityAnchor
				node.SelectionCursor = op.accessibilityCursor
			}
		} else if role == "img" {
			node.Label = op.AltText
		}
		focused := op.Focused
		if node.FocusID > 0 {
			focused = node.FocusID == r.focusID && !r.popupFocusCaptures(node.FocusID)
		}
		node.Focused = !node.Disabled && focused
		node.Generation = uint64(r.frames)
		node.Actions = AccessibilityPolicy_AccessibilityActionsFor(accessibilityKind(op), node.FocusID,
			node.Disabled, r.popupFocusCaptures(node.FocusID), node.ReadOnly)
		if role == "listbox" && !node.MultiSelect {
			node.Actions &^= uint32(AccessibilityActionSelectAll)
		}
		nodes = append(nodes, node)
		parents[index+1] = uint32(len(nodes))
	}
	return nodes
}

func accessibilityRole(op FrameOp) string {
	if op.Role != "" {
		return op.Role
	}
	switch op.Kind {
	case FrameOpButton:
		return "button"
	case FrameOpTextField, FrameOpTextArea:
		return "textbox"
	case FrameOpText:
		return "text"
	case FrameOpScreen, FrameOpPage:
		return "main"
	case FrameOpColumn, FrameOpRow, FrameOpStack, FrameOpGrid, FrameOpGroup, FrameOpSection:
		return "group"
	case FrameOpImage:
		return "img"
	case FrameOpTable:
		return "table"
	default:
		return ""
	}
}
