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

func TestHostLongRunningTextInputStaysBounded(t *testing.T) {
	host := NewHost(AppConfig{Width: 320, Height: 180})
	rt := host.Runtime().(*runtime)
	first := make([]byte, 8192)
	second := make([]byte, 8192)
	firstCursor, secondCursor := int32(0), int32(0)
	firstFocused, secondFocused := false, false

	draw := func() {
		BeginFrame()
		Column(ColumnProps{
			Bounds:  Rectangle{X: 12, Y: 12, Width: 240, Height: 96},
			Gap:     6,
			Padding: 4,
			Key:     Key("long-host-input"),
		})
		TextField(TextFieldProps{
			Bounds:         Rectangle{Width: 180, Height: 28},
			Text:           first,
			CursorPosition: &firstCursor,
			Focused:        &firstFocused,
			FocusID:        201,
			MaxCodepoints:  8191,
			Font:           Text16,
		})
		TextField(TextFieldProps{
			Bounds:         Rectangle{Width: 180, Height: 28},
			Text:           second,
			CursorPosition: &secondCursor,
			Focused:        &secondFocused,
			FocusID:        202,
			MaxCodepoints:  8191,
			Font:           Text16,
		})
		End()
		EndFrame()
	}

	host.Draw(draw)
	host.QueueTap(24, 24)
	host.Draw(draw)

	const typed = 2048
	for i := 0; i < typed; i++ {
		if i > 0 && i%256 == 0 {
			host.QueueKey(KeyTab)
			host.Draw(draw)
		}
		host.QueueText("x")
		host.Draw(draw)
		host.QueueKey(KeyLeft)
		host.Draw(draw)
		host.QueueKey(KeyRight)
		host.Draw(draw)
	}

	firstText := string(first[:zeroIndex(first)])
	secondText := string(second[:zeroIndex(second)])
	if got := len(firstText) + len(secondText); got != typed {
		t.Fatalf("typed length across fields = %d, want %d", got, typed)
	}
	if firstText == "" || secondText == "" {
		t.Fatalf("tab traversal did not distribute typing: first=%d second=%d", len(firstText), len(secondText))
	}
	if got, want := len(rt.prevOrder), 2; got != want {
		t.Fatalf("field order length after long run = %d, want %d", got, want)
	}
	if got := len(host.FrameOps()); got > 5 {
		t.Fatalf("frame op count grew after long run = %d, want <= 5: %#v", got, host.FrameOps())
	}
	if got := len(rt.selection); got != 0 {
		t.Fatalf("selection map retained collapsed cursor entries = %d, want 0", got)
	}
}
