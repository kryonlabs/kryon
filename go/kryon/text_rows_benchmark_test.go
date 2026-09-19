package kryon

import (
	"image"
	"strings"
	"testing"
)

func BenchmarkEditableRows(b *testing.B) {
	ensureDefaultTextFont()
	for name, text := range map[string]string{
		"LongLine": strings.Repeat("Text café 日本語. ", 300),
		"Document": strings.Repeat("# Heading\nCafe\u0301 👩‍💻 क्ष 日本語\n", 250),
	} {
		b.Run(name, func(b *testing.B) {
			b.Run("Cold", func(b *testing.B) {
				b.ReportAllocs()
				for i := 0; i < b.N; i++ {
					measuredTextRows(text, 480, 16, true, false,
						func(value string, font int32) int { return runtimeTextWidthWithFont(value, font, 0) })
				}
			})
			b.Run("Warm", func(b *testing.B) {
				renderTextAreaLines(text, 480, 16, 0, true)
				b.ReportAllocs()
				b.ResetTimer()
				for i := 0; i < b.N; i++ {
					renderTextAreaLines(text, 480, 16, 0, true)
				}
			})
		})
	}
}

func BenchmarkEditablePaint(b *testing.B) {
	ensureDefaultTextFont()
	frame := image.NewRGBA(image.Rect(0, 0, 640, 480))
	op := FrameOp{Kind: FrameOpTextArea, Bounds: NewRectangle(0, 0, 640, 480),
		FontSize: 16, TextColor: BLACK, ContentOffset: Vector2{X: 10, Y: 8},
		Wrap: true, Text: strings.Repeat("Cafe\u0301 👩‍💻 क्ष 日本語\n", 500)}
	b.ReportAllocs()
	b.ResetTimer()
	for i := 0; i < b.N; i++ {
		RenderFrameInto(frame, []FrameOp{op})
	}
}
