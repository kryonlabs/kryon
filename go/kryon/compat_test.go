package kryon

import "testing"

func TestCompatDrawingAndInputAPI(t *testing.T) {
	rt := New(AppConfig{Width: 200, Height: 120})
	SetRuntime(rt)
	defer SetRuntime(nil)

	QueueTap(12, 18)
	QueueKey(KeyDown)
	QueueText("x")
	BeginDrawing()
	if got := GetMousePosition(); got.X != 12 || got.Y != 18 {
		t.Fatalf("mouse position = %#v, want 12,18", got)
	}
	if !IsMouseButtonPressed(MouseButtonLeft) {
		t.Fatal("left mouse button not reported")
	}
	if !IsKeyPressed(KeyDown) {
		t.Fatal("key down not reported")
	}
	if got := GetCharPressed(); got != 'x' {
		t.Fatalf("char pressed = %q, want x", rune(got))
	}
	DrawRectangleRec(NewRectangle(1, 2, 30, 20), RED)
	DrawRectangleLinesEx(NewRectangle(4, 5, 30, 20), 1, BLUE)
	DrawLine(0, 0, 20, 20, BLACK)
	DrawTextEx(Font{}, "geld", NewVector2(8, 9), 16, 1, BLACK)
	EndDrawing()

	if got := len(FrameOps()); got < 4 {
		t.Fatalf("compat drawing produced %d ops, want at least 4", got)
	}
}
