package kryon

import "testing"

func TestDropdownUsesButtonPaletteAndKeepsSelectionDistinct(t *testing.T) {
	ClearStylePacks()
	defer ClearStylePacks()
	if !RegisterBuiltInStylePacks() {
		t.Fatal("built-in styles did not register")
	}
	for _, dark := range []bool{false, true} {
		r := New(AppConfig{Width: 400, Height: 360}).(*runtime)
		theme := ThemeDefaultLight()
		if dark {
			theme = ThemeDefaultDark()
		}
		r.SetTheme(theme)
		selected := int32(1)
		draw := func(disabled bool) {
			r.BeginFrame()
			r.Dropdown(DropdownProps{ID: 29000, Bounds: NewRectangle(20, 20, 280, 40),
				Options:       []string{"Light", "Dark", "A very long option label that must leave room for the checkmark"},
				SelectedIndex: &selected, Disabled: disabled})
			r.EndFrame()
		}
		r.SetFocus(29000)
		r.QueueKey(KeySpace)
		draw(false)
		r.QueueKey(KeyDown)
		draw(false)
		var selection, highlight *FrameOp
		checkLeftByRow := map[int32]float32{}
		textRightByRow := map[int32]float32{}
		for i := range r.ops {
			op := &r.ops[i]
			if op.ID != 29000 {
				continue
			}
			if op.Kind == FrameOpText && op.Row >= 0 {
				if !op.HasClip {
					t.Fatal("menu label is not clipped")
				}
				textRightByRow[op.Row] = op.Clip.X + op.Clip.Width
			}
			if op.Kind == FrameOpIcon && op.IconType == IconCheck && op.Row >= 0 {
				checkLeftByRow[op.Row] = op.Bounds.X
			}
			if op.Kind == FrameOpSurface && op.Selected {
				selection = op
			} else if op.Kind == FrameOpSurface && op.Focused {
				highlight = op
			}
		}
		for row, textRight := range textRightByRow {
			if checkLeft, ok := checkLeftByRow[row]; ok && textRight > checkLeft {
				t.Fatal("menu label can overlap the selection indicator")
			}
		}
		if selection == nil || highlight == nil || selection.Color == highlight.Color {
			t.Fatal("selection and navigation highlight must remain distinct")
		}
		if selection.Color != (Color{0xc9, 0xa8, 0xff, 0xff}) {
			t.Fatal("selection did not follow the active KSS accent")
		}
		draw(true)
		if r.openDropdowns[29000] {
			t.Fatal("disabled dropdown retained its popup")
		}
		for _, op := range r.ops {
			if op.ID == 29000 && op.Kind == FrameOpButton {
				want := r.dropdownStyle(0, false, ButtonStateDisabled)
				if !op.Disabled || op.Focused || unpackRGBA(op.Button.Appearance.Value.Background) != want.Background || unpackRGBA(op.Button.Appearance.Value.Foreground) != want.Foreground {
					t.Fatal("disabled dropdown differs from the disabled neutral button")
				}
			}
		}
	}
}

func TestDropdownRichOptionsSkipDisabledRows(t *testing.T) {
	r := New(AppConfig{Width: 400, Height: 400}).(*runtime)
	selected := int32(0)
	items := []DropdownOption{
		{Label: "First", IconType: IconSun},
		{Label: "Unavailable", Disabled: true, SeparatorBefore: true},
		{Label: "Last", IconType: IconMoon},
		{Label: "Unavailable end", Disabled: true},
	}
	draw := func() {
		r.BeginFrame()
		r.Dropdown(DropdownProps{ID: 29001, Bounds: NewRectangle(20, 20, 280, 40),
			Items: items, SelectedIndex: &selected})
		r.EndFrame()
	}
	r.SetFocus(29001)
	r.QueueKey(KeySpace)
	draw()
	icons, separators := 0, 0
	for _, op := range r.ops {
		if op.ID == 29001 && op.Kind == FrameOpIcon {
			icons++
		}
		if op.ID == 29001 && op.Kind == FrameOpLine {
			separators++
		}
	}
	if icons < 3 || separators == 0 {
		t.Fatal("rich dropdown omitted its icons or separator")
	}
	for _, key := range []int32{KeyDown, KeyEnd, KeyDown} {
		r.QueueKey(key)
		draw()
		if r.dropdownHighlight[29001] != 2 {
			t.Fatalf("key %d highlighted disabled row %d", key, r.dropdownHighlight[29001])
		}
	}
	r.QueueKey(KeyEnter)
	draw()
	if selected != 2 || r.openDropdowns[29001] {
		t.Fatal("enabled selection did not commit and close")
	}
	selected = 3
	r.QueueKey(KeySpace)
	draw()
	draw()
	if r.dropdownHighlight[29001] != 2 {
		t.Fatal("opening a disabled final selection did not find an enabled row")
	}
	for i := range items {
		items[i].Disabled = true
	}
	r.QueueKey(KeyEnter)
	draw()
	if selected != 3 {
		t.Fatal("all-disabled menu committed a selection")
	}
}

func TestDropdownStyleComesFromKSS(t *testing.T) {
	ClearStylePacks()
	defer ClearStylePacks()
	if !RegisterBuiltInStylePacks() {
		t.Fatal("built-in styles did not register")
	}

	r := New(AppConfig{Width: 400, Height: 400}).(*runtime)
	trigger := r.dropdownStyle(0, false, ButtonStateNormal)
	if trigger.Background != (Color{0x17, 0x1c, 0x25, 0xff}) ||
		trigger.Material != MaterialFlat || trigger.Radius != 8 {
		t.Fatalf("material dropdown did not come from KSS: %#v", trigger)
	}
	selected := r.dropdownStyle(2, true, ButtonStateNormal)
	if selected.Background != (Color{0xc9, 0xa8, 0xff, 0xff}) ||
		selected.Foreground != (Color{0x17, 0x10, 0x22, 0xff}) {
		t.Fatalf("selected dropdown row did not use KSS accent facts: %#v", selected)
	}
	metrics := Dropdown_Content(packStyle(trigger), 1.5)
	if metrics.Font != trigger.FontSize*1.5 || metrics.Icon != trigger.IconSize*1.5 ||
		metrics.Padding != trigger.PaddingX*1.5 || metrics.Gap != trigger.Gap*1.5 {
		t.Fatal("dropdown content did not inherit scaled KSS metrics")
	}

	if !SetActiveStylePack("kryon.lightfield") {
		t.Fatal("lightfield did not activate")
	}
	premium := r.dropdownStyle(0, false, ButtonStateNormal)
	if premium.Material != MaterialLightfield ||
		premium.BackgroundEnd != (Color{0x24, 0x2b, 0x38, 0xee}) {
		t.Fatalf("lightfield dropdown was not opt-in KSS: %#v", premium)
	}
}

func TestDropdownAmbientSurfaceComesFromKSS(t *testing.T) {
	ClearStylePacks()
	defer ClearStylePacks()
	if !RegisterStylePackSource(`
@pack test.dropdown_ambient;
tokens {
  color {
    app-surface: #101820;
    trigger: #263449;
    ink: #eef5ff;
    rule: #5a6b7d;
  }
  length { radius: 4; border: 2; }
  material { flat: Flat; }
}
Surface { background: app-surface; material: flat; }
Dropdown { background: trigger; foreground: ink; border: rule; radius: radius; border-width: border; material: flat; }
`, "Dropdown Ambient", "") || !SetActiveStylePack("test.dropdown_ambient") {
		t.Fatal("test dropdown ambient style did not activate")
	}
	r := New(AppConfig{Width: 240, Height: 160}).(*runtime)
	selected := int32(0)

	r.BeginFrame()
	r.Dropdown(DropdownProps{
		ID:            9910,
		Bounds:        Rectangle{X: 10, Y: 12, Width: 140, Height: 32},
		Options:       []string{"One", "Two"},
		SelectedIndex: &selected,
	})
	r.EndFrame()

	for _, op := range r.FrameOps() {
		if op.ID == 9910 && op.Kind == FrameOpButton {
			if op.Button.Material.Ambient != 0x101820ff {
				t.Fatalf("dropdown trigger ambient = %#x, want KSS surface", op.Button.Material.Ambient)
			}
			return
		}
	}
	t.Fatalf("dropdown trigger op not found: %+v", r.FrameOps())
}
