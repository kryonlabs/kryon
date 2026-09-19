package kryon

import (
	"fmt"
	"strings"
	"testing"
)

// These measure CPU-side frame construction, not presentation or GPU latency.
func BenchmarkFrameWorkloads(b *testing.B) {
	b.Run("WrappedDocument", func(b *testing.B) {
		r := New(AppConfig{Width: 1200, Height: 800}).(*runtime)
		paragraphs := make([]string, 24)
		for i := range paragraphs {
			paragraphs[i] = fmt.Sprintf("Section %d. ", i) + strings.Repeat(
				"Editing cafe\u0301, \u65e5\u672c\u8a9e, and \U0001f469\u200d\U0001f4bb should preserve complete characters. ", 12)
		}
		draw := func() {
			r.BeginFrame()
			for i, text := range paragraphs {
				r.Text(TextProps{Text: text, Bounds: NewRectangle(float32(i%3)*400, float32(i/3)*100, 380, 96), Font: Text16})
			}
			r.EndFrame()
		}
		benchmarkFrame(b, r, draw)
	})
	b.Run("LargeForm", func(b *testing.B) {
		r := New(AppConfig{Width: 1200, Height: 800}).(*runtime)
		fields := make([][]byte, 128)
		cursors := make([]int32, len(fields))
		for i := range fields {
			fields[i] = make([]byte, 128)
			copy(fields[i], fmt.Sprintf("Field %d: cafe\u0301", i))
		}
		draw := func() {
			r.BeginFrame()
			for i, text := range fields {
				r.TextField(TextFieldProps{Bounds: NewRectangle(float32(i%4)*300, float32(i/4)*24, 290, 24),
					Text: text, CursorPosition: &cursors[i], FocusID: int32(i + 1)})
			}
			r.EndFrame()
		}
		benchmarkFrame(b, r, draw)
	})
	b.Run("MultilingualEditor", func(b *testing.B) {
		r := New(AppConfig{Width: 1200, Height: 800}).(*runtime)
		text := make([]byte, 32768)
		length := copy(text, strings.Repeat("Cafe\u0301 \U0001f469\u200d\U0001f4bb \u0915\u094d\u0937 \u65e5\u672c\u8a9e\n", 500))
		cursor := int32(length)
		r.SetFocus(1)
		frame := 0
		draw := func() {
			r.QueueKey([]int32{KeyLeft, KeyRight}[frame%2])
			frame++
			r.BeginFrame()
			r.TextArea(TextAreaProps{Bounds: NewRectangle(0, 0, 1200, 800), Text: text,
				CursorPosition: &cursor, FocusID: 1})
			r.EndFrame()
		}
		benchmarkFrame(b, r, draw)
	})
}

func benchmarkFrame(b *testing.B, r *runtime, draw func()) {
	b.Helper()
	draw()
	b.ReportAllocs()
	b.ResetTimer()
	for i := 0; i < b.N; i++ {
		draw()
	}
	b.ReportMetric(float64(len(r.ops)), "ops/frame")
}
