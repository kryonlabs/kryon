package kryon

import "testing"

// No-style contract (plan/style/07): without an active style pack, widgets
// emit no product chrome - no colors, borders, radii, or materials - while
// content (text, images) and behavior still flow. Explicit primitive calls
// with caller-supplied colors are out of scope here.

func noStyleOps(t *testing.T, draw func(r *runtime)) []FrameOp {
	t.Helper()
	ClearStylePacks()
	t.Cleanup(ClearStylePacks)
	r := New(AppConfig{Width: 240, Height: 160}).(*runtime)
	r.BeginFrame()
	draw(r)
	r.EndFrame()
	return r.FrameOps()
}

func assertNoStyleChrome(t *testing.T, name string, ops []FrameOp) {
	t.Helper()
	for _, op := range ops {
		switch op.Kind {
		case FrameOpBackground:
			// Platform background fallback is a system preference, allowed.
		case FrameOpRect:
			if op.Color != (Color{}) || op.BorderColor != (Color{}) || op.Radius != 0 ||
				op.BorderWidth != 0 || op.Material != 0 {
				t.Errorf("%s: unstyled rect leaked chrome: color=%v border=%v radius=%g width=%g material=%d",
					name, op.Color, op.BorderColor, op.Radius, op.BorderWidth, op.Material)
			}
		case FrameOpButton:
			if value := op.Button.Appearance.Value; value.Fields != 0 {
				t.Errorf("%s: unstyled button leaked style fields: %+v", name, value)
			}
		case FrameOpText, FrameOpImage, FrameOpLine, FrameOpIcon:
			// Content: recorded unstyled is exactly the contract.
		default:
			if op.Color != (Color{}) || op.Radius != 0 || op.Material != 0 {
				t.Errorf("%s: unstyled op %q leaked chrome: color=%v radius=%g material=%d",
					name, op.Kind, op.Color, op.Radius, op.Material)
			}
		}
	}
}

func countOps(ops []FrameOp, kinds ...FrameOpKind) int {
	count := 0
	for _, op := range ops {
		for _, kind := range kinds {
			if op.Kind == kind {
				count++
			}
		}
	}
	return count
}

func TestNoStyleButtonAndText(t *testing.T) {
	ops := noStyleOps(t, func(r *runtime) {
		r.Button(ButtonProps{Bounds: Rectangle{X: 10, Y: 10, Width: 80, Height: 28}, Label: "Save", ID: 1})
		r.Text(TextProps{Bounds: Rectangle{X: 10, Y: 50}, Text: "Plain"})
	})
	assertNoStyleChrome(t, "button", ops)
	if countOps(ops, FrameOpButton) != 1 {
		t.Fatalf("button op missing without a pack: %d", len(ops))
	}
	if countOps(ops, FrameOpText) < 1 {
		t.Fatalf("text content missing without a pack: %d ops", len(ops))
	}
}

func TestNoStyleTextInputAndDropdown(t *testing.T) {
	text := make([]byte, 8)
	copy(text, "edit")
	cursor := int32(0)
	selected := int32(0)
	ops := noStyleOps(t, func(r *runtime) {
		r.TextField(TextFieldProps{Bounds: Rectangle{X: 10, Y: 10, Width: 120, Height: 28},
			Text: text, CursorPosition: &cursor})
		r.Dropdown(DropdownProps{Bounds: Rectangle{X: 10, Y: 46, Width: 120, Height: 28},
			ID: 2, Options: []string{"One", "Two"}, SelectedIndex: &selected})
	})
	assertNoStyleChrome(t, "text-input+dropdown", ops)
	if countOps(ops, FrameOpText) < 1 {
		t.Fatalf("field or dropdown content missing: %d ops", len(ops))
	}
}

func TestNoStyleSelectionWidgets(t *testing.T) {
	flags := int32(1)
	selected := int32(0)
	toggle := int32(0)
	ops := noStyleOps(t, func(r *runtime) {
		r.Checkbox(CheckboxProps{Bounds: Rectangle{X: 10, Y: 10, Width: 120, Height: 28},
			ID: 1, Label: "Feature", Flags: &flags, FlagsValue: 2})
		r.Radio(RadioProps{Bounds: Rectangle{X: 10, Y: 42, Width: 120, Height: 28},
			ID: 2, Label: "Choice", Checked: true})
		r.Selectable(SelectableProps{Bounds: Rectangle{X: 10, Y: 74, Width: 120, Height: 28},
			ID: 3, Label: "Pick", Selected: &selected})
		r.Toggle(ToggleProps{Bounds: Rectangle{X: 10, Y: 106, Width: 120, Height: 28},
			ID: 4, OffLabel: "Off", OnLabel: "On", Value: &toggle})
	})
	assertNoStyleChrome(t, "selection widgets", ops)
	if countOps(ops, FrameOpText) < 4 {
		t.Fatalf("selection labels missing without a pack: %d ops", len(ops))
	}
}

func TestNoStyleListAndTable(t *testing.T) {
	listSelected := int32(0)
	ops := noStyleOps(t, func(r *runtime) {
		r.ListBox(ListBoxProps{Bounds: Rectangle{X: 10, Y: 10, Width: 120, Height: 60},
			ID: 1, Items: []string{"One", "Two", "Three"}, SelectedIndex: &listSelected})
		r.TableView(TableViewProps{Bounds: Rectangle{X: 10, Y: 78, Width: 200, Height: 60},
			Columns: []string{"Name", "Value"},
			Rows: []TableRow{{Cells: []string{"a", "1"}}, {Cells: []string{"b", "2"}}}})
	})
	assertNoStyleChrome(t, "list+table", ops)
	if countOps(ops, FrameOpText) < 5 {
		t.Fatalf("list and table content missing: %d ops", len(ops))
	}
}

func TestNoStyleNavigationBarAndModal(t *testing.T) {
	ops := noStyleOps(t, func(r *runtime) {
		r.NavigationBar(NavigationBarProps{Items: []NavigationBarItem{
			{Label: "Home"}, {Label: "Search"},
		}})
		r.Modal(ModalProps{Title: "Confirm", Message: "No style"})
	})
	assertNoStyleChrome(t, "navbar+modal", ops)
	if countOps(ops, FrameOpText) < 2 {
		t.Fatalf("navigation labels missing without a pack: %d ops", len(ops))
	}
}

func TestNoStyleImageContentSurvives(t *testing.T) {
	ops := noStyleOps(t, func(r *runtime) {
		r.Image(ImageProps{AssetPath: "tile.png", Bounds: Rectangle{X: 10, Y: 10, Width: 48, Height: 32}})
	})
	assertNoStyleChrome(t, "image", ops)
	images := 0
	for _, op := range ops {
		if op.Kind == FrameOpImage {
			images++
			if op.Color != (Color{255, 255, 255, 255}) {
				t.Fatalf("unstyled image tint must stay fully visible: %+v", op.Color)
			}
		}
	}
	if images != 1 {
		t.Fatalf("image content missing without a pack: %d ops", len(ops))
	}
}
