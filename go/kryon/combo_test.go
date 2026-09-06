package kryon

import "testing"

func countComboOps(ops []FrameOp, id int32) int {
	n := 0
	for _, op := range ops {
		if op.ID == id {
			n++
		}
	}
	return n
}

func TestComposedComboOrdinaryContentsAndLayout(t *testing.T) {
	r := New(AppConfig{Width: 240, Height: 180}).(*runtime)
	open := true
	r.BeginFrame()
	r.Column(ColumnProps{Bounds: NewRectangle(10, 10, 180, 120), Gap: 5})
	r.Text(TextProps{Bounds: NewRectangle(0, 0, 0, 0), Text: "before", Font: 14, Color: BLACK, Wrap: TextWrapNone})
	if !r.BeginCombo(ComboProps{Bounds: NewRectangle(0, 0, 100, 28), PopupSize: NewVector2(120, 80), Preview: "Choose", ID: 27000, Open: &open, Flags: ComboPopupAlignLeft}) {
		t.Fatal("open composed combo returned false")
	}
	r.Column(ColumnProps{Bounds: NewRectangle(12, 45, 100, 60), Gap: 3})
	if r.Button(ButtonProps{Bounds: NewRectangle(0, 0, 90, 24), Label: "Action", ID: 27001}) {
		t.Fatal("unexpected action")
	}
	text := make([]byte, 16)
	copy(text, "edit")
	cursor := int32(4)
	r.TextField(TextFieldProps{Bounds: NewRectangle(0, 0, 90, 24), Text: text, CursorPosition: &cursor, FocusID: 27002})
	r.End()
	r.EndCombo()
	r.Text(TextProps{Bounds: NewRectangle(0, 0, 0, 0), Text: "after", Font: 14, Color: BLACK, Wrap: TextWrapNone})
	r.End()
	r.EndFrame()
	ops := r.FrameOps()
	textFound := false
	for _, op := range ops {
		if op.FocusID == 27002 {
			textFound = true
		}
	}
	if countComboOps(ops, 27001) == 0 || !textFound {
		t.Fatalf("ordinary popup children were not composited: %+v", ops)
	}
	if len(r.layout) != 0 || len(r.paintLayerScopes) != 0 || len(r.popupInputScopes) != 0 {
		t.Fatal("combo did not restore scopes")
	}
	child := -1
	after := -1
	for i, op := range ops {
		if op.ID == 27001 {
			child = i
		}
		if op.Kind == FrameOpText && op.Text == "after" {
			after = i
		}
	}
	if child <= after {
		t.Fatalf("popup child did not composite above later parent: child=%d after=%d", child, after)
	}
}

func TestComposedComboNestedInputAndExplicitClose(t *testing.T) {
	r := New(AppConfig{Width: 240, Height: 180}).(*runtime)
	outerOpen, innerOpen := true, true
	r.QueueTap(36, 94)
	r.BeginFrame()
	if !r.BeginCombo(ComboProps{Bounds: NewRectangle(10, 10, 100, 28), PopupSize: NewVector2(150, 130), Preview: "Outer", ID: 27100, Open: &outerOpen, Flags: ComboPopupAlignLeft}) {
		t.Fatal("outer closed")
	}
	if !r.BeginCombo(ComboProps{Bounds: NewRectangle(20, 50, 100, 28), PopupSize: NewVector2(100, 70), Preview: "Inner", ID: 27101, Open: &innerOpen, Flags: ComboPopupAlignLeft}) {
		t.Fatal("inner closed")
	}
	if !r.Button(ButtonProps{Bounds: NewRectangle(20, 82, 90, 24), Label: "Nested action", ID: 27102}) {
		t.Fatal("nested ordinary button did not receive tap")
	}
	r.CloseCombo()
	r.EndCombo()
	if innerOpen {
		t.Fatal("CloseCombo did not update caller state")
	}
	r.EndCombo()
	r.EndFrame()
	if countComboOps(r.FrameOps(), 27102) != 0 {
		t.Fatal("explicitly closed layer was painted")
	}
	if !outerOpen {
		t.Fatal("closing child closed parent")
	}
}

func TestComposedComboPlacementFlagsAndMissingOwner(t *testing.T) {
	for _, tc := range []struct {
		name       string
		flags      ComboFlags
		x, y, w, h float32
	}{
		{"right", ComboHeightSmall, 90, 40, 120, 112},
		{"left-fit", ComboPopupAlignLeft | ComboWidthFitPreview | ComboNoArrowButton | ComboNoPreview | ComboHeightLarge, 170, 0, 70, 180},
	} {
		t.Run(tc.name, func(t *testing.T) {
			r := New(AppConfig{Width: 240, Height: 180}).(*runtime)
			open := true
			r.BeginFrame()
			p := ComboProps{Bounds: NewRectangle(170, 152, 40, 28), PopupSize: NewVector2(tc.w, 0), Preview: "A very long preview", ID: 27200, Open: &open, Flags: tc.flags}
			if !r.BeginCombo(p) {
				t.Fatal("combo closed")
			}
			r.EndCombo()
			r.EndFrame()
			var bg *FrameOp
			for i := range r.ops {
				if r.ops[i].ID == 27200 && r.ops[i].Kind == FrameOpRect {
					bg = &r.ops[i]
				}
			}
			if bg == nil {
				t.Fatal("popup background missing")
			}
			if bg.Bounds.X != tc.x || bg.Bounds.Y != tc.y || bg.Bounds.Width != tc.w || bg.Bounds.Height != tc.h {
				t.Fatalf("popup=%+v", bg.Bounds)
			}
			r.BeginFrame()
			r.EndFrame()
			if open {
				t.Fatal("missing combo owner did not close caller state")
			}
		})
	}
}

func TestComposedComboScopeBalance(t *testing.T) {
	r := New(AppConfig{}).(*runtime)
	mustPanic := func(f func()) {
		t.Helper()
		defer func() {
			if recover() == nil {
				t.Error("invalid combo scope was accepted")
			}
		}()
		f()
	}
	mustPanic(func() { r.EndCombo() })
	open := true
	r.BeginFrame()
	r.BeginCombo(ComboProps{Bounds: NewRectangle(0, 0, 100, 28), PopupSize: NewVector2(100, 60), ID: 27300, Open: &open})
	mustPanic(func() { r.EndFrame() })
}
