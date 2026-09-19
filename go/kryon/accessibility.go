package kryon

// AccessibilityNode is a semantic projection of a completed frame. Secure
// editor values are omitted; other editor values exclude uncommitted preedit.
type AccessibilityNode struct {
	Bounds    Rectangle
	Role      string
	Label     string
	Focused   bool
	Disabled  bool
	Checked   bool
	FocusID   int32
	Value     string
	ReadOnly  bool
	Secure    bool
	Multiline bool
}

type AccessibilitySink func([]AccessibilityNode)

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
