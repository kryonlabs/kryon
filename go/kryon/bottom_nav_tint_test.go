package kryon

import "testing"

func TestNavigationBarIconTint(t *testing.T) {
	ClearStylePacks()
	defer ClearStylePacks()
	if !RegisterBuiltInStylePacks() {
		t.Fatal("built-in styles did not register")
	}
	r := New(AppConfig{Width: 640, Height: 480}).(*runtime)
	r.BeginFrame()
	r.NavigationBar(NavigationBarProps{ViewWidth: 640, ViewHeight: 480, Items: []NavigationBarItem{{Route: 1, Icon: Texture2D{ID: 42}}, {Route: 2, Icon: Texture2D{ID: 42}, Disabled: true}}})
	r.EndFrame()
	count := 0
	for _, op := range r.FrameOps() {
		if op.Kind != FrameOpIcon {
			continue
		}
		want := Color{R: 0x8d, G: 0x91, B: 0x9a, A: 255}
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
