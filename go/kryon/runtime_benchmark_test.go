package kryon

import "testing"

func BenchmarkTextFieldWidgetWorkflow(b *testing.B) {
	rt := New(AppConfig{Width: 320, Height: 180}).(*runtime)
	first, second := make([]byte, 8192), make([]byte, 8192)
	firstCursor, secondCursor := int32(0), int32(0)
	firstFocused, secondFocused := true, false

	draw := func() {
		rt.BeginFrame()
		rt.TextField(TextFieldProps{
			Bounds:         Rectangle{X: 20, Y: 20, Width: 220, Height: 30},
			Text:           first,
			CursorPosition: &firstCursor,
			Focused:        &firstFocused,
			FocusID:        601,
			MaxCodepoints:  8191,
			Font:           Text16,
		})
		rt.TextField(TextFieldProps{
			Bounds:         Rectangle{X: 20, Y: 64, Width: 220, Height: 30},
			Text:           second,
			CursorPosition: &secondCursor,
			Focused:        &secondFocused,
			FocusID:        602,
			MaxCodepoints:  8191,
			Font:           Text16,
		})
		rt.EndFrame()
	}

	b.ReportAllocs()
	draw()
	b.ResetTimer()
	for i := 0; i < b.N; i++ {
		if i > 0 && i%512 == 0 {
			rt.QueueKey(KeyTab)
			draw()
		}
		rt.QueueText("x")
		draw()
		rt.QueueKey(KeyLeft)
		draw()
		rt.QueueKey(KeyRight)
		draw()
		if firstCursor > 7800 || secondCursor > 7800 {
			clear(first)
			clear(second)
			firstCursor, secondCursor = 0, 0
		}
	}
}
