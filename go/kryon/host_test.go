package kryon

import "testing"

func TestHostFrameQueuesInputAndRenders(t *testing.T) {
	host := NewHost(AppConfig{Width: 220, Height: 140})
	value := "abc"

	draw := func() {
		BeginFrame()
		ClearBackground(RAYWHITE)
		Column(ColumnProps{
			Bounds:  Rectangle{X: 10, Y: 10, Width: 190, Height: 110},
			Gap:     6,
			Padding: 4,
			Key:     Key("host-test"),
		})
		Text("Host", 0, 0, Text16, BLACK)
		TextField("Name", &value)
		Button("Save")
		End()
		EndFrame()
	}

	img := host.Frame(draw)
	if got := img.Bounds().Dx(); got != 220 {
		t.Fatalf("host render width = %d, want 220", got)
	}
	if got := img.Bounds().Dy(); got != 140 {
		t.Fatalf("host render height = %d, want 140", got)
	}
	if got := countPixelsNot(img, rgbaTest(RAYWHITE)); got < 700 {
		t.Fatalf("host first frame changed only %d pixels, want visible UI", got)
	}

	host.QueueTap(20, 44)
	host.QueueText("x")
	img = host.Frame(draw)
	if got, want := value, "xabc"; got != want {
		t.Fatalf("host text after tap/type = %q, want %q", got, want)
	}
	if got := countPixelsNot(img, rgbaTest(RAYWHITE)); got < 700 {
		t.Fatalf("host typed frame changed only %d pixels, want visible UI", got)
	}

	ops := host.FrameOps()
	seenField := false
	for _, op := range ops {
		if op.Kind == FrameOpTextField && op.Text == "xabc" && op.Focused {
			seenField = true
		}
	}
	if !seenField {
		t.Fatalf("host frame ops missing focused edited field: %#v", ops)
	}
}

func TestHostRestoresActiveRuntime(t *testing.T) {
	previous := New(AppConfig{Width: 77, Height: 66})
	SetRuntime(previous)
	defer SetRuntime(nil)

	host := NewHost(AppConfig{Width: 120, Height: 90})
	host.Frame(func() {
		BeginFrame()
		Text("host", 4, 4, Text16, BLACK)
		EndFrame()
	})

	if activeRuntime != previous {
		t.Fatal("host did not restore previous active runtime")
	}
	if got, want := GetScreenWidth(), int32(77); got != want {
		t.Fatalf("active runtime width after host frame = %d, want %d", got, want)
	}
}
