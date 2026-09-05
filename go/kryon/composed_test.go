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
	QueueTap(232, 184)
	if got := CarouselControls(p); got != 3 {
		t.Fatalf("last dot: %d", got)
	}
}

func TestSecondaryButtonUsesQuietSurface(t *testing.T) {
	r := New(AppConfig{Width: 320, Height: 480}).(*runtime)
	r.SetCurrentTheme(int32(ThemeCobalt), 1)
	r.SetThemeSource(ThemeSourceApp)
	r.Button(ButtonProps{Bounds: Rectangle{Width: 100, Height: 48}, Style: ButtonStylePrimary})
	r.Button(ButtonProps{Bounds: Rectangle{Y: 60, Width: 100, Height: 48}, Style: ButtonStyleSecondary})
	ops := r.FrameOps()
	if len(ops) != 2 || ops[0].Color == ops[1].Color {
		t.Fatalf("secondary button must use the quiet surface: %+v; surface=%+v button=%+v", ops, r.theme().surface, r.theme().button)
	}
}
