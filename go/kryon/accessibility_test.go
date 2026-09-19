package kryon

import "testing"

func TestAccessibilitySnapshotEditorPrivacyAndState(t *testing.T) {
	r := New(AppConfig{Width: 500, Height: 300}).(*runtime)
	password, notes := make([]byte, 64), make([]byte, 64)
	copy(password, "private-password")
	copy(notes, "Saved notes")
	flags, enabled := int32(6), int32(1)
	pc, nc := int32(16), int32(11)
	r.SetFocus(11)
	r.BeginFrame()
	r.Button(ButtonProps{Bounds: NewRectangle(0, 0, 120, 30), Label: "Save", ID: 11})
	r.TextField(TextFieldProps{Bounds: NewRectangle(0, 40, 160, 30), Text: password, CursorPosition: &pc, FocusID: 12, Secure: true})
	r.TextArea(TextAreaProps{Bounds: NewRectangle(0, 80, 160, 60), Text: notes, CursorPosition: &nc, FocusID: 13, ReadOnly: true, Placeholder: "Notes"})
	r.Checkbox(CheckboxProps{Bounds: NewRectangle(0, 150, 120, 30), ID: 14, Label: "Flags", Flags: &flags, FlagsValue: 2})
	r.Toggle(ToggleProps{Bounds: NewRectangle(0, 190, 120, 30), ID: 15, Value: &enabled, Disabled: true, OffLabel: "Off", OnLabel: "On"})
	r.DisabledScope(true)
	r.Button(ButtonProps{Bounds: NewRectangle(200, 0, 120, 30), ID: 16, Label: "Disabled"})
	r.DisabledEndScope()
	r.EndFrame()
	nodes := r.GetAccessibilitySnapshot()
	byID := make(map[int32]AccessibilityNode)
	for _, node := range nodes {
		if node.FocusID != 0 {
			if _, exists := byID[node.FocusID]; exists {
				t.Fatalf("duplicate semantic node for %d", node.FocusID)
			}
			byID[node.FocusID] = node
		}
		if node.Value == "private-password" || node.Label == "private-password" {
			t.Fatal("password leaked into accessibility snapshot")
		}
	}
	if !byID[11].Focused || byID[11].Label != "Save" {
		t.Fatal("button focus or name missing")
	}
	if node := byID[12]; !node.Secure || node.Value != "" || node.Label != "" {
		t.Fatalf("secure input metadata: %+v", node)
	}
	if node := byID[13]; !node.ReadOnly || !node.Multiline || node.Value != "Saved notes" || node.Label != "Notes" {
		t.Fatalf("read-only area metadata: %+v", node)
	}
	if node := byID[14]; node.Role != "checkbox" || !node.Checked || node.Bounds.Width != 120 {
		t.Fatalf("flag checkbox metadata: %+v", node)
	}
	if node := byID[15]; !node.Checked || !node.Disabled || node.Focused || node.Label != "On" {
		t.Fatalf("toggle metadata: %+v", node)
	}
	if !byID[16].Disabled {
		t.Fatal("inherited disabled state missing")
	}
	nodes[0].Label = "changed by caller"
	if r.GetAccessibilitySnapshot()[0].Label == nodes[0].Label {
		t.Fatal("snapshot aliases caller-mutable storage")
	}
}

func TestAccessibilityComposedLabelsAndEmptyFrame(t *testing.T) {
	host := NewHost(AppConfig{Width: 400, Height: 200})
	defer host.Close()
	r := host.Runtime().(*runtime)
	calls, lastCount := 0, -1
	host.SetAccessibilitySink(func(nodes []AccessibilityNode) {
		calls++
		lastCount = len(nodes)
	})
	host.Draw(func() {
		BeginFrame()
		r.ButtonScope(ButtonProps{Bounds: NewRectangle(0, 0, 120, 40), ID: 21})
		Column(ColumnProps{})
		Text(TextProps{Text: "Nested label", Bounds: NewRectangle(0, 0, 30, 100)})
		End()
		End()
		EndFrame()
	})
	labels := 0
	for _, node := range host.GetAccessibilitySnapshot() {
		if node.Label == "Nested label" {
			labels++
			if node.Role != "button" {
				t.Fatal("button child was exposed as a duplicate label")
			}
		}
	}
	if labels != 1 || calls != 1 || lastCount == 0 {
		t.Fatal("composed label or snapshot callback missing")
	}
	host.Draw(func() {
		BeginFrame()
		EndFrame()
	})
	if calls != 2 || lastCount != 0 {
		t.Fatal("empty frame did not retire accessibility nodes")
	}
	host.SetAccessibilitySink(nil)
	host.Draw(func() {
		BeginFrame()
		EndFrame()
	})
	if calls != 2 {
		t.Fatal("removed sink was still called")
	}
}

func TestAccessibilityEditorValueExcludesPreedit(t *testing.T) {
	r := New(AppConfig{Width: 300, Height: 100}).(*runtime)
	text := make([]byte, 64)
	copy(text, "saved")
	cursor := int32(5)
	draw := func() {
		r.BeginFrame()
		r.TextField(TextFieldProps{Bounds: NewRectangle(0, 0, 200, 30), Text: text, CursorPosition: &cursor, FocusID: 31})
		r.EndFrame()
	}
	r.SetFocus(31)
	draw()
	r.SubmitTextComposition(KRY_TEXT_COMPOSITION_UPDATE, "preedit", 7, 0)
	draw()
	nodes := r.GetAccessibilitySnapshot()
	if len(nodes) != 1 || nodes[0].Value != "saved" || !nodes[0].Focused {
		t.Fatalf("preedit changed the committed accessibility value: %+v", nodes)
	}
}
