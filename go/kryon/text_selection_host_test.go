package kryon

import (
	"testing"
	"unicode/utf8"
)

func TestSelectableTextDragCopiesOriginalRange(t *testing.T) {
	ClearStylePacks()
	t.Cleanup(ClearStylePacks)
	r := New(AppConfig{}).(*runtime)
	props := TextProps{Text: "Cafe\u0301 👩‍💻 日本語", Bounds: NewRectangle(10, 10, 300, 30),
		Font: 16, Wrap: TextWrapNone, Selectable: true}
	draw := func() []FrameOp {
		r.BeginFrame()
		r.Text(props)
		r.EndFrame()
		return r.FrameOps()
	}
	r.QueueMouseButtonDown(MouseButtonLeft, 10, 12)
	draw()
	end := len("Cafe\u0301 👩‍💻")
	x := props.Bounds.X + float32(runtimeTextWidthWithFont(props.Text[:end], 16, 0))
	r.QueueMouseMove(x, 12)
	ops := draw()
	if len(ops) != 1 || !ops[0].Selected || ops[0].SelectionStart != 0 || ops[0].SelectionEnd != int32(end) {
		t.Fatalf("drag selection = %+v", ops)
	}
	plain := ops[0]
	plain.Selected = false
	before, after := RenderFrame(320, 60, []FrameOp{plain}), RenderFrame(320, 60, ops)
	different := false
	for i := range before.Pix {
		different = different || before.Pix[i] != after.Pix[i]
	}
	if !different {
		t.Fatal("selected range was not painted")
	}
	r.QueueMouseButtonUp(MouseButtonLeft, x, 12)
	r.QueueShortcut(KeyC)
	draw()
	if r.ClipboardText() != props.Text[:end] || !utf8.ValidString(r.ClipboardText()) {
		t.Fatalf("copied %q, want %q", r.ClipboardText(), props.Text[:end])
	}
}

func TestSelectableTextWrappedRangeAndCollapsedCopy(t *testing.T) {
	r := New(AppConfig{}).(*runtime)
	props := TextProps{Text: "alpha   beta\ngamma", Bounds: NewRectangle(10, 10, 50, 120),
		Font: 16, Selectable: true}
	draw := func() {
		r.BeginFrame()
		r.Text(props)
		r.EndFrame()
	}
	r.SetClipboardText("unchanged")
	r.QueueTap(10, 10)
	r.QueueShortcut(KeyC)
	draw()
	if r.ClipboardText() != "unchanged" {
		t.Fatal("collapsed selection replaced clipboard")
	}
	r.QueueMouseButtonDown(MouseButtonLeft, 10, 10)
	draw()
	r.QueueMouseMove(500, 500)
	draw()
	r.QueueMouseButtonUp(MouseButtonLeft, 500, 500)
	r.QueueShortcut(KeyC)
	draw()
	if r.ClipboardText() != props.Text {
		t.Fatalf("wrapped copy lost original whitespace: %q", r.ClipboardText())
	}
}

func TestSelectableLineMappingPreservesWhitespaceAndUnicode(t *testing.T) {
	source := "é  文\n👩‍💻"
	lines := selectableLines(source, []string{"é 文", "👩‍💻"})
	if lines[0].start != 0 || lines[0].end != len("é  文") ||
		lines[1].start != len("é  文\n") || lines[1].end != len(source) {
		t.Fatalf("source mapping = %+v", lines)
	}
}

func TestSelectableTextReleasesShortcutsToEditor(t *testing.T) {
	r := New(AppConfig{}).(*runtime)
	text := make([]byte, 32)
	copy(text, "editor")
	cursor, focused := int32(0), false
	draw := func() {
		r.BeginFrame()
		r.Text(TextProps{Text: "label", Bounds: NewRectangle(10, 10, 120, 30), Selectable: true})
		r.TextField(TextFieldProps{Bounds: NewRectangle(10, 60, 120, 30), Text: text,
			CursorPosition: &cursor, Focused: &focused, FocusID: 701})
		r.EndFrame()
	}
	r.QueueTap(10, 10)
	r.QueueShortcut(KeyA)
	draw()
	r.QueueTap(20, 70)
	r.QueueShortcut(KeyA)
	r.QueueShortcut(KeyC)
	draw()
	if r.ClipboardText() != "editor" || r.selectableText != 0 {
		t.Fatalf("selectable label stole editor input: copied %q", r.ClipboardText())
	}
}
