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
			r.Combobox(ComboboxProps{ID: 29000, Bounds: NewRectangle(20, 20, 280, 40),
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
		want := resolveButtonStyle(r.theme(), dark, r.activeTheme,
			ButtonProps{Tone: ButtonToneAccent, Emphasis: ButtonEmphasisSoft}, ButtonStateSelected)
		if selection.Color != want.Background || selection.TextColor != want.Foreground {
			t.Fatal("selection does not inherit the accent button palette")
		}
		draw(true)
		if r.openDropdowns[29000] {
			t.Fatal("disabled dropdown retained its popup")
		}
		for _, op := range r.ops {
			if op.ID == 29000 && op.Kind == FrameOpButton {
				want := resolveButtonStyle(r.theme(), dark, r.activeTheme,
					ButtonProps{Tone: ButtonToneNeutral, Emphasis: ButtonEmphasisSoft, Disabled: true}, ButtonStateDisabled)
				if !op.Disabled || op.Focused || op.Color != want.Background || op.TextColor != want.Foreground {
					t.Fatal("disabled dropdown differs from the disabled neutral button")
				}
			}
		}
	}
}
