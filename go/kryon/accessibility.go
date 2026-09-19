package kryon

// AccessibilityNode is a semantic projection of a completed frame. Secure
// editor values are omitted; other editor values exclude uncommitted preedit.
type AccessibilityNode struct {
	Bounds     Rectangle
	Role       string
	Label      string
	Focused    bool
	Disabled   bool
	Checked    bool
	FocusID    int32
	Value      string
	ReadOnly   bool
	Secure     bool
	Multiline  bool
	Generation uint64
	Actions    uint32
}

type AccessibilitySink func([]AccessibilityNode)

type accessibilityRequest struct {
	id     int32
	kind   int32
	action AccessibilityAction
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
	if r.closed || r.accessibility.building || generation == 0 || generation != uint64(r.frames) || focusID <= 0 {
		return false
	}
	kind := int32(0)
	actions := uint32(0)
	for _, op := range r.ops {
		candidate := accessibilityKind(op)
		id := op.ID
		if op.FocusID != 0 {
			id = op.FocusID
		}
		if id != focusID || AccessibilityPolicy_AccessibilityActionsFor(candidate, id, false, false) == 0 {
			continue
		}
		if kind != 0 {
			return false
		}
		kind = candidate
		actions = AccessibilityPolicy_AccessibilityActionsFor(kind, id, op.Disabled || op.Loading, r.popupFocusCaptures(id))
	}
	if !AccessibilityPolicy_AccessibilityActionAllowed(actions, action) {
		return false
	}
	for _, request := range r.accessibility.pending {
		if request.id == focusID && request.action == action {
			return true
		}
	}
	if len(r.accessibility.pending) == 32 {
		return false
	}
	r.accessibility.pending = append(r.accessibility.pending, accessibilityRequest{focusID, kind, action})
	return true
}

func (r *runtime) beginAccessibilityFrame() {
	r.accessibility.active, r.accessibility.pending = r.accessibility.pending, r.accessibility.active[:0]
	r.accessibility.activation = 0
	r.accessibility.building = true
}

func (r *runtime) endAccessibilityFrame() {
	r.accessibility.active = r.accessibility.active[:0]
	r.accessibility.activation = 0
	r.accessibility.building = false
}

func (r *runtime) prepareAccessibility(id, kind int32, enabled bool) {
	if len(r.accessibility.active) == 0 {
		return
	}
	actions := AccessibilityPolicy_AccessibilityActionsFor(kind, id, !enabled || r.contentDisabled(), r.popupKeyboardCaptures())
	for i, request := range r.accessibility.active {
		if request.id != id || id <= 0 {
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
	for _, op := range r.ops {
		role := accessibilityRole(op)
		if role == "" || role == "presentation" || role == "none" {
			continue
		}
		node := AccessibilityNode{
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
			node.Disabled, r.popupFocusCaptures(node.FocusID))
		nodes = append(nodes, node)
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
