package kryon

import "testing"

func TestBottomNavIconTint(t *testing.T) {
	for _, tint := range []Color{{R: 32, G: 36, B: 40, A: 255}, {R: 240, G: 242, B: 244, A: 255}, {}} {
		r := New(AppConfig{Width: 640, Height: 480}).(*runtime)
		r.BeginFrame()
		r.BottomNav(BottomNavProps{ViewWidth: 640, ViewHeight: 480, IconColor: tint, Items: []BottomNavItem{{Route: 1, Icon: Texture2D{ID: 42}}, {Route: 2, Icon: Texture2D{ID: 42}, Disabled: true}}})
		r.EndFrame()
		want := tint
		if want.A == 0 {
			want = Color{R: 255, G: 255, B: 255, A: 255}
		}
		count := 0
		for _, op := range r.FrameOps() {
			if op.Kind != FrameOpIcon {
				continue
			}
			if op.ID == 2 {
				want.A = 150
			}
			if op.Color != want {
				t.Fatalf("icon %d: got %+v want %+v", op.ID, op.Color, want)
			}
			count++
		}
		if count != 2 {
			t.Fatalf("got %d icons", count)
		}
	}
}
