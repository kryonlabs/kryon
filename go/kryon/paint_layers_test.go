package kryon

import "testing"

func TestPaintLayersNestedOrderAndPixels(t *testing.T) {
	r := New(AppConfig{}).(*runtime)
	r.BeginFrame()
	r.Rect(0, 0, 80, 80, BLACK, BLANK)
	parent := r.beginPaintLayer(1)
	r.Rect(10, 10, 60, 60, GREEN, BLANK)
	child := r.beginPaintLayer(2)
	r.Rect(20, 20, 30, 30, BLUE, BLANK)
	r.endPaintLayer(child)
	// Even parent paint declared after the nested popup stays below it.
	r.Rect(25, 25, 10, 10, YELLOW, BLANK)
	r.endPaintLayer(parent)
	r.Rect(0, 0, 80, 80, RED, BLANK)
	sibling := r.beginPaintLayer(3)
	r.Rect(40, 40, 20, 20, ORANGE, BLANK)
	r.endPaintLayer(sibling)
	r.appendPaintLayers(func(int32) bool { return true })
	img := RenderFrame(80, 80, r.FrameOps())
	for _, test := range []struct {
		x, y int
		want Color
	}{{5, 5, RED}, {15, 15, GREEN}, {30, 30, BLUE}, {45, 45, ORANGE}} {
		got := img.RGBAAt(test.x, test.y)
		if got.R != test.want.R || got.G != test.want.G || got.B != test.want.B {
			t.Fatalf("layer pixel (%d,%d)=%v, want %v", test.x, test.y, got, test.want)
		}
	}
}

func TestPaintLayersRestoreLayoutClipAndDisabledState(t *testing.T) {
	r := New(AppConfig{}).(*runtime)
	r.BeginFrame()
	r.Row(RowProps{Bounds: NewRectangle(10, 10, 120, 24)})
	r.Button(ButtonProps{Bounds: NewRectangle(0, 0, 20, 24), ID: 10})
	clip := NewRectangle(10, 10, 120, 24)
	r.scrollClips = append(r.scrollClips, clip)
	r.BeginDisabled(true)
	layer := r.beginPaintLayer(1)
	r.Row(RowProps{Bounds: NewRectangle(40, 50, 80, 24)})
	r.Button(ButtonProps{Bounds: NewRectangle(0, 0, 20, 24), ID: 11})
	r.End()
	r.BeginDisabled(true)
	r.scrollClips = append(r.scrollClips, NewRectangle(0, 0, 1, 1))
	r.endPaintLayer(layer)
	if r.disabledCount != 1 || len(r.disabledStack) != 1 || len(r.scrollClips) != 1 || r.scrollClips[0] != clip {
		t.Fatal("paint scope did not restore its parent's disabled/clip state")
	}
	r.EndDisabled()
	r.Button(ButtonProps{Bounds: NewRectangle(0, 0, 20, 24), ID: 12})
	r.End()
	r.appendPaintLayers(func(int32) bool { return true })
	found := map[int32]bool{}
	for _, op := range r.FrameOps() {
		switch op.ID {
		case 11:
			found[11] = true
			if op.Bounds.X != 40 || op.Bounds.Y != 50 || op.HasClip || !op.Disabled {
				t.Fatalf("layer widget inherited owner layout/clip or lost disabled state: %+v", op)
			}
		case 12:
			found[12] = true
			if op.Bounds.X != 30 || !op.HasClip || op.Clip != clip || op.Disabled {
				t.Fatalf("parent widget did not resume layout/clip/disabled state: %+v", op)
			}
		}
	}
	if !found[11] || !found[12] {
		t.Fatal("missing composed widgets")
	}
}

func TestPaintLayersOwnerRemovalAndFrameReset(t *testing.T) {
	r := New(AppConfig{}).(*runtime)
	r.BeginFrame()
	parent := r.beginPaintLayer(1)
	r.Rect(0, 0, 10, 10, RED, BLANK)
	child := r.beginPaintLayer(2)
	r.Rect(0, 0, 10, 10, GREEN, BLANK)
	r.endPaintLayer(child)
	r.endPaintLayer(parent)
	r.appendPaintLayers(func(id int32) bool { return id == 2 })
	if len(r.FrameOps()) != 0 {
		t.Fatal("removed parent left its nested popup visible")
	}
	r.BeginFrame()
	r.appendPaintLayers(func(int32) bool { return true })
	if len(r.FrameOps()) != 0 || len(r.paintLayers) != 0 {
		t.Fatal("old frame layer survived frame reset")
	}
}

func TestPaintLayersRejectUnbalancedAndForeignTokens(t *testing.T) {
	mustPanic := func(f func()) {
		t.Helper()
		defer func() {
			if recover() == nil {
				t.Error("invalid layer scope was accepted")
			}
		}()
		f()
	}
	r := New(AppConfig{}).(*runtime)
	other := New(AppConfig{}).(*runtime)
	r.BeginFrame()
	other.BeginFrame()
	parent := r.beginPaintLayer(1)
	foreign := other.beginPaintLayer(1)
	mustPanic(func() { r.endPaintLayer(foreign) })
	child := r.beginPaintLayer(2)
	mustPanic(func() { r.endPaintLayer(parent) })
	mustPanic(func() { r.EndFrame() })
	mustPanic(func() { r.BeginFrame() })
	r.endPaintLayer(child)
	r.endPaintLayer(parent)
	other.endPaintLayer(foreign)
	r.BeginFrame()
	current := r.beginPaintLayer(1)
	mustPanic(func() { r.endPaintLayer(parent) })
	r.endPaintLayer(current)
}
