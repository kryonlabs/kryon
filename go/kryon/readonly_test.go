package kryon

import (
	"bytes"
	"testing"
)

func TestReadOnlyEditorsPreserveInputAndCancelPreedit(t *testing.T) {
	for _, area := range []bool{false, true} {
		r := New(AppConfig{}).(*runtime)
		text := make([]byte, 32)
		copy(text, "base")
		cursor := int32(4)
		draw := func(readOnly bool) {
			if area {
				r.TextArea(TextAreaProps{Bounds: NewRectangle(10, 10, 120, 80), Text: text,
					CursorPosition: &cursor, FocusID: 26200, ReadOnly: readOnly})
			} else {
				r.TextField(TextFieldProps{Bounds: NewRectangle(10, 10, 120, 28), Text: text,
					CursorPosition: &cursor, FocusID: 26200, ReadOnly: readOnly})
			}
		}
		r.setFocus(26200)
		r.SubmitTextComposition(KRY_TEXT_COMPOSITION_UPDATE, "ni", 2, 0)
		r.BeginFrame()
		draw(false)
		r.EndFrame()
		if r.FrameOps()[0].Text != "baseni" {
			t.Fatal("preedit not established")
		}
		// Bytes beyond the terminating NUL also belong to the caller.
		text[20] = 99
		before := append([]byte(nil), text...)
		r.QueueShortcut(KeyA)
		r.QueueShortcut(KeyC)
		r.QueueShortcut(KeyX)
		r.QueueShortcut(KeyV)
		r.QueueText("blocked")
		r.QueueKey(KeyBackspace)
		r.QueueKey(KeyDelete)
		r.SubmitTextComposition(KRY_TEXT_COMPOSITION_COMMIT, "blocked", 7, 0)
		r.BeginFrame()
		draw(true)
		r.EndFrame()
		if !bytes.Equal(text, before) || r.ClipboardText() != "base" || r.Focus() != 26200 {
			t.Fatalf("area=%v: read-only buffer/copy/focus contract failed", area)
		}
		op := r.FrameOps()[0]
		if !op.ReadOnly || !op.Focused || op.Text != "base" || len(r.preedit) != 0 {
			t.Fatalf("area=%v: stale preedit or metadata", area)
		}
		withoutCaret := RenderFrame(160, 120, []FrameOp{op})
		op.ReadOnly = false
		withCaret := RenderFrame(160, 120, []FrameOp{op})
		if bytes.Equal(withoutCaret.Pix, withCaret.Pix) {
			t.Fatalf("area=%v: read-only caret paint not suppressed", area)
		}
		r.BeginFrame()
		draw(false)
		r.EndFrame()
		if string(text[:zeroIndex(text)]) != "base" || r.FrameOps()[0].Text != "base" {
			t.Fatal("read-only input replayed on re-enable")
		}
		r.SetSelection(26200, 4, 4)
		r.QueueText("x")
		r.BeginFrame()
		draw(false)
		r.EndFrame()
		if string(text[:zeroIndex(text)]) != "basex" {
			t.Fatal("fresh text rejected after re-enable")
		}
	}
}
