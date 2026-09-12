package kryon

import "testing"

func TestComposedWidgets(t *testing.T) {
	SetRuntime(New(AppConfig{Width: 320, Height: 480}))
	defer SetRuntime(nil)
	for _, tt := range []struct {
		count, selected, move, want int32
		disabled                    bool
	}{
		{0, 0, 0, -1, false}, {1, 0, 1, 0, false}, {4, 0, -1, 3, false}, {4, 3, 1, 0, false}, {4, 2, 99, 1, false}, {4, 2, 1, 2, true},
	} {
		if got := CarouselControls(CarouselControlsProps{Count: tt.count, Selected: tt.selected, Move: tt.move, Disabled: tt.disabled}); got != tt.want {
			t.Fatalf("%+v got %d", tt, got)
		}
	}
	p := CarouselControlsProps{Bounds: Rectangle{Width: 320, Height: 160}, Indicators: Rectangle{Y: 160, Width: 320, Height: 48}, Count: 4, ID: 700}
	QueueTap(284, 80)
	if got := CarouselControls(p); got != 1 {
		t.Fatalf("next arrow: %d", got)
	}
	QueueTap(208, 184)
	if got := CarouselControls(p); got != 3 {
		t.Fatalf("last dot: %d", got)
	}
}

func TestSecondaryButtonUsesQuietSurface(t *testing.T) {
	r := New(AppConfig{Width: 320, Height: 480}).(*runtime)
	r.SetCurrentTheme(int32(ThemeCobalt), 1)
	r.SetThemeSource(ThemeSourceApp)
	r.Button(ButtonProps{Bounds: Rectangle{Width: 100, Height: 48}})
	r.Button(ButtonProps{Bounds: Rectangle{Y: 60, Width: 100, Height: 48}, Tone: ButtonToneNeutral, Emphasis: ButtonEmphasisSoft})
	ops := r.FrameOps()
	if len(ops) != 2 || unpackRGBA(ops[0].Button.Appearance.Value.Background) == unpackRGBA(ops[1].Button.Appearance.Value.Background) {
		t.Fatalf("secondary button must use the quiet surface: %+v; surface=%+v button=%+v", ops, r.theme().surface, r.theme().button)
	}
}

func TestCarouselArrowsUseStandardButtonStates(t *testing.T) {
	r := New(AppConfig{Width: 320, Height: 240}).(*runtime)
	SetRuntime(r)
	defer SetRuntime(nil)
	p := CarouselControlsProps{Bounds: Rectangle{Width: 320, Height: 160}, Count: 4, ID: 700}
	for _, state := range []string{"normal", "hover", "pressed", "disabled"} {
		if state != "normal" {
			r.QueueMouseMove(280, 80)
		}
		if state == "pressed" {
			r.QueueMouseButtonDown(MouseButtonLeft, 280, 80)
		}
		p.Disabled = state == "disabled"
		r.BeginFrame()
		CarouselControls(p)
		r.EndFrame()
		ops := r.FrameOps()
		if len(ops) != 2 {
			t.Fatalf("%s: expected two standard button surfaces, got %d", state, len(ops))
		}
		next := ops[1]
		if next.Kind != FrameOpButton || !next.Button.Props.IconOnly || next.Button.Props.IconType != IconRight ||
			next.Bounds.Width != 56 || next.Bounds.Height != 56 || next.Button.Appearance.Value.Radius < 28 {
			t.Fatalf("%s: arrow is not a standard circular button: %+v", state, next)
		}
		if state == "hover" && !next.Hovered {
			t.Fatal("arrow did not enter hover state")
		}
		if state == "pressed" && !next.Pressed {
			t.Fatal("arrow did not enter pressed state")
		}
		if p.Disabled && (!next.Disabled || next.Pressed || next.Hovered) {
			t.Fatal("disabled arrow retained an active interaction state")
		}
	}
}
