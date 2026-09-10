package kryon

import "testing"

func TestComboHorizontalViewport(t *testing.T) {
	for _, bounds := range []Rectangle{NewRectangle(-20, 10, 160, 28), NewRectangle(200, 10, 160, 28), NewRectangle(10, 10, 400, 28)} {
		r := New(AppConfig{Width: 240, Height: 240}).(*runtime)
		selected := int32(0)
		draw := func() {
			r.BeginFrame()
			r.Combobox(ComboboxProps{Bounds: bounds, ID: 25002, Options: []string{"One", "Two"}, SelectedIndex: &selected})
			r.EndFrame()
		}
		r.SetFocus(25002)
		r.QueueKey(KeySpace)
		draw()
		panel := r.popupPanels[25002].bounds
		if panel.X < 0 || panel.X+panel.Width > 240 || panel.Width <= 0 {
			t.Fatalf("button %+v: popup %+v escapes screen", bounds, panel)
		}
		if r.popupCaptures(-1, 80) || r.popupCaptures(241, 80) {
			t.Fatal("popup captures off-screen coordinates")
		}
		r.QueueTap(panel.X+12, 80)
		draw()
		if selected != 1 || r.openDropdowns[25002] {
			t.Fatalf("button %+v: shifted row did not select", bounds)
		}
	}
}

func TestComboScrollbarDismissal(t *testing.T) {
	for mode := 0; mode < 3; mode++ {
		r := New(AppConfig{Width: 240, Height: 240}).(*runtime)
		selected := int32(0)
		draw := func(disabled, missing bool) {
			r.BeginFrame()
			if !missing {
				r.Combobox(ComboboxProps{Bounds: NewRectangle(10, 10, 160, 28), ID: 25001,
					Options: make([]string, 131), SelectedIndex: &selected, Disabled: disabled})
			}
			r.EndFrame()
		}
		r.SetFocus(25001)
		r.QueueKey(KeySpace)
		draw(false, false)
		r.QueueMouseButtonDown(MouseButtonLeft, 166, 50)
		draw(false, false)
		if r.scrollDragOffset == nil || r.scrollDragOffset != r.dropdownOffsets[25001] {
			t.Fatal("popup scrollbar did not acquire its offset")
		}
		if mode == 0 {
			r.QueueKey(KeyEscape)
		}
		draw(mode == 1, mode == 2)
		if r.scrollDragOffset != nil || r.dropdownOffsets[25001] != nil || r.openDropdowns[25001] {
			t.Fatalf("dismissal mode %d retained scrollbar state", mode)
		}
	}
}

func TestComboConstrainedViewport(t *testing.T) {
	for _, y := range []int32{10, 200} {
		r := New(AppConfig{Width: 240, Height: 240}).(*runtime)
		options := make([]string, 131)
		selected := int32(0)
		draw := func() {
			r.BeginFrame()
			r.Combobox(ComboboxProps{Bounds: NewRectangle(10, float32(y), 160, 28), ID: 25000,
				Options: options, SelectedIndex: &selected})
			r.EndFrame()
		}
		r.SetFocus(25000)
		r.QueueKey(KeySpace)
		draw()
		panel := r.popupPanels[25000].bounds
		if panel.Y < 0 || panel.Y+panel.Height > 240 || panel.Height >= 131*28 {
			t.Fatalf("y=%d: unconstrained panel %+v", y, panel)
		}
		if y == 200 && panel.Y+panel.Height > float32(y) {
			t.Fatalf("menu did not flip above owner: %+v", panel)
		}
		r.QueueMouseMove(20, panel.Y+10)
		r.QueueMouseWheel(-1)
		draw()
		if *r.dropdownOffsets[25000] != 42 {
			t.Fatalf("y=%d: wheel offset=%d", y, *r.dropdownOffsets[25000])
		}
		draw()
		if *r.dropdownOffsets[25000] != 42 {
			t.Fatal("idle frame snapped wheel position back to highlight")
		}
		r.QueueKey(KeyEnd)
		draw()
		if selected != 0 {
			t.Fatal("navigation committed selection")
		}
		rows, found := 0, false
		var last Rectangle
		for _, op := range r.ops {
			if op.ID != 25000 || op.Kind != FrameOpText || !op.HasClip || op.Row < 0 {
				continue
			}
			rows++
			if op.Clip.Y < panel.Y || op.Clip.Y+op.Clip.Height > panel.Y+panel.Height {
				t.Fatalf("row escapes popup: %+v", op)
			}
			if op.Row == 130 {
				last, found = op.Bounds, true
			}
		}
		if !found || rows > int(panel.Height/28)+1 {
			t.Fatalf("visible rows=%d, last found=%v", rows, found)
		}
		r.QueueTap(last.X+2, last.Y+2)
		draw()
		if selected != 130 || r.openDropdowns[25000] || r.dropdownOffsets[25000] != nil {
			t.Fatal("revealed last row did not select and retire scroll state")
		}
	}
}
