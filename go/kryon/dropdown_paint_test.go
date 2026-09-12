package kryon

import "testing"

func TestDropdownUsesButtonPaletteAndKeepsSelectionDistinct(t *testing.T) {
	for _, dark := range []bool{false, true} {
		r := New(AppConfig{Width: 400, Height: 360}).(*runtime)
		theme := ThemeDefaultLight()
		if dark {
			theme = ThemeDefaultDark()
		}
		// A non-blue accent catches hardcoded proposal colors.
		theme.Colors.Accent = Color{170, 60, 110, 255}
		theme.Colors.AccentHover = Color{190, 80, 130, 255}
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
		for i := range r.ops {
			op := &r.ops[i]
			if op.ID != 29000 {
				continue
			}
			if op.Kind == FrameOpText && op.Row >= 0 {
				if !op.HasClip || op.Clip.X+op.Clip.Width > 264 {
					t.Fatal("menu label can overlap the selection indicator")
				}
			}
			if op.Kind == FrameOpSurface && op.Selected {
				selection = op
			} else if op.Kind == FrameOpSurface && op.Focused {
				highlight = op
			}
		}
		if selection == nil || highlight == nil || selection.Color == highlight.Color {
			t.Fatal("selection and navigation highlight must remain distinct")
		}
		if selection.Color.R <= selection.Color.B || selection.Color.R <= selection.Color.G {
			t.Fatal("selection did not follow the custom rose accent")
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

func TestDropdownInheritsCompleteButtonStyle(t *testing.T) {
	for _, dark := range []bool{false, true} {
		r := New(AppConfig{Width: 400, Height: 400}).(*runtime)
		if dark {
			r.SetTheme(ThemeDefaultDark())
		} else {
			r.SetTheme(ThemeDefaultLight())
		}
		for _, state := range []ButtonState{ButtonStateNormal, ButtonStateHover,
			ButtonStatePressed, ButtonStateFocus, ButtonStateDisabled} {
			button := resolveButtonStyle(r.theme(), r.effectiveDark(), r.activeTheme,
				ButtonProps{Tone: ButtonToneNeutral, Emphasis: ButtonEmphasisSoft}, state)
			got := r.dropdownStyle(0, false, state)
			if state == ButtonStateNormal || state == ButtonStateDisabled {
				if got != button {
					t.Fatalf("dark=%v state=%v: dropdown trigger changed the base Button style", dark, state)
				}
				continue
			}
			if got.FontSize != button.FontSize || got.PaddingX != button.PaddingX ||
				got.PaddingY != button.PaddingY || got.IconSize != button.IconSize ||
				got.Opacity != button.Opacity {
				t.Fatalf("dark=%v state=%v: dropdown trigger changed Button metrics", dark, state)
			}
			if got.Material != MaterialGlass ||
				got.Fields&(StyleBackgroundEnd|StyleMaterial) != StyleBackgroundEnd|StyleMaterial {
				t.Fatalf("dark=%v state=%v: dropdown trigger missed interactive glass treatment", dark, state)
			}
			if got.BackgroundEnd == button.BackgroundEnd || got.Border == button.Border {
				t.Fatalf("dark=%v state=%v: dropdown trigger missed accent response", dark, state)
			}
		}
		button := r.dropdownStyle(0, false, ButtonStateNormal)
		metrics := Dropdown_Content(packStyle(button), 1.5)
		if metrics.Font != button.FontSize*1.5 || metrics.Icon != button.IconSize*1.5 ||
			metrics.Padding != button.PaddingX*1.5 || metrics.Gap != button.Gap*1.5 {
			t.Fatal("dropdown content did not inherit scaled Button metrics")
		}
	}
}
