package kryon

import (
	"bytes"
	"strings"
	"testing"
	"unicode/utf8"
)

func TestCompositionQueueOwnership(t *testing.T) {
	r := New(AppConfig{}).(*runtime)
	other := New(AppConfig{}).(*runtime)
	if r.SubmitTextComposition(0, "invalid", 0, 0) != 0 {
		t.Fatal("invalid phase accepted")
	}
	for i := 0; i < 16; i++ {
		if r.SubmitTextComposition(KRY_TEXT_COMPOSITION_UPDATE, strings.Repeat("x", 300), -1, -1) != 1 {
			t.Fatal("queue filled early")
		}
	}
	if r.SubmitTextComposition(KRY_TEXT_COMPOSITION_CANCEL, "", 0, 0) != 0 {
		t.Fatal("queue overflow accepted")
	}
	var event KryTextCompositionEvent
	if other.PollTextComposition(&event) != 0 || r.PollTextComposition(nil) != 0 {
		t.Fatal("queue ownership or nil polling")
	}
	if r.PollTextComposition(&event) != 1 || len(event.Text) != 255 || event.Cursor != 0 || event.SelectionLength != 0 {
		t.Fatalf("bad event: %+v", event)
	}
	r.ClearTextComposition()
	if r.PollTextComposition(&event) != 0 {
		t.Fatal("clear retained events")
	}
	truncated := strings.Repeat("x", 254) + "日"
	if r.SubmitTextComposition(KRY_TEXT_COMPOSITION_UPDATE, truncated, 254, 0) != 1 ||
		r.PollTextComposition(&event) != 1 || len(event.Text) != 254 || !utf8.ValidString(event.Text) {
		t.Fatalf("composition queue retained partial UTF-8: %q", event.Text)
	}
}

func compositionEditor(r *runtime, area bool, text []byte, cursor *int32) {
	if area {
		r.TextArea(TextAreaProps{Bounds: NewRectangle(10, 10, 120, 80), Text: text, CursorPosition: cursor, FocusID: 26100})
	} else {
		r.TextField(TextFieldProps{Bounds: NewRectangle(10, 10, 120, 28), Text: text, CursorPosition: cursor, FocusID: 26100})
	}
}

func TestCompositionPreeditCommitAndCancel(t *testing.T) {
	for _, area := range []bool{false, true} {
		r := New(AppConfig{}).(*runtime)
		text := make([]byte, 32)
		copy(text, "a")
		cursor := int32(1)
		r.setFocus(26100)
		for frame := 0; frame < 5; frame++ {
			switch frame {
			case 0:
				r.SubmitTextComposition(KRY_TEXT_COMPOSITION_UPDATE, "ni", 2, 0)
			case 2:
				r.SubmitTextComposition(KRY_TEXT_COMPOSITION_COMMIT, "日", 1, 0)
			case 3:
				r.SubmitTextComposition(KRY_TEXT_COMPOSITION_UPDATE, "cancel", 6, 0)
				r.SubmitTextComposition(KRY_TEXT_COMPOSITION_CANCEL, "", 0, 0)
			case 4:
				r.setFocus(999)
			}
			r.BeginFrame()
			compositionEditor(r, area, text, &cursor)
			r.EndFrame()
			op := r.FrameOps()[0]
			want, display := "a", "ani"
			if frame >= 2 {
				want, display = "a日", "a日"
			}
			if got := string(text[:zeroIndex(text)]); got != want {
				t.Fatalf("area=%v frame=%d buffer=%q want=%q", area, frame, got, want)
			}
			if got := op.Text; got != display {
				t.Fatalf("area=%v frame=%d display=%q want=%q", area, frame, got, display)
			}
			if frame == 0 {
				if op.Cursor != 3 || op.SelectionStart != 3 || op.SelectionEnd != 3 ||
					op.CompositionStart != 1 || op.CompositionEnd != 3 {
					t.Fatalf("area=%v: bad preedit paint metadata: %+v", area, op)
				}
				withPreedit := RenderFrame(160, 120, r.FrameOps())
				plain := append([]FrameOp(nil), r.FrameOps()...)
				plain[0].Text = want
				withoutPreedit := RenderFrame(160, 120, plain)
				if bytes.Equal(withPreedit.Pix, withoutPreedit.Pix) {
					t.Fatalf("area=%v: renderer omitted preedit", area)
				}
			}
			if frame >= 2 && cursor != 4 {
				t.Fatalf("commit cursor=%d want UTF-8 byte offset 4", cursor)
			}
		}
	}
}

func TestCompositionPaintReplacesSelectionAndUsesUTF8Offsets(t *testing.T) {
	for _, area := range []bool{false, true} {
		r := New(AppConfig{}).(*runtime)
		text := make([]byte, 32)
		copy(text, "aXYZz")
		cursor := int32(4)
		r.setFocus(26100)
		r.selection[26100] = selection{Anchor: 1, Cursor: 4}
		r.SubmitTextComposition(KRY_TEXT_COMPOSITION_UPDATE, "日ab", 3, 1)
		r.BeginFrame()
		compositionEditor(r, area, text, &cursor)
		r.EndFrame()

		if got := string(text[:zeroIndex(text)]); got != "aXYZz" {
			t.Fatalf("area=%v: preedit mutated committed text: %q", area, got)
		}
		op := r.FrameOps()[0]
		if op.Text != "a日abz" || op.Cursor != 4 ||
			op.SelectionStart != 4 || op.SelectionEnd != 5 ||
			op.CompositionStart != 1 || op.CompositionEnd != 6 {
			t.Fatalf("area=%v: bad selected preedit paint metadata: %+v", area, op)
		}
		withUnderline := RenderFrame(180, 120, r.FrameOps())
		withoutUnderlineOps := append([]FrameOp(nil), r.FrameOps()...)
		withoutUnderlineOps[0].CompositionStart = 0
		withoutUnderlineOps[0].CompositionEnd = 0
		withoutUnderline := RenderFrame(180, 120, withoutUnderlineOps)
		if bytes.Equal(withUnderline.Pix, withoutUnderline.Pix) {
			t.Fatalf("area=%v: renderer omitted composition underline", area)
		}
	}

	view, ok := makeTextCompositionView("ab", 1, 1,
		KryTextCompositionEvent{Text: "日", Cursor: 2, SelectionLength: 8})
	if !ok || view.cursor != 1 || view.selectionEnd != 4 {
		t.Fatalf("composition view did not clamp UTF-8 offsets: %+v", view)
	}
}

func TestPopupCompositionDismissalDoesNotReplay(t *testing.T) {
	for _, area := range []bool{false, true} {
		r := New(AppConfig{}).(*runtime)
		text := make([]byte, 32)
		copy(text, "a")
		cursor := int32(1)
		for frame := 0; frame < 3; frame++ {
			if frame != 1 {
				r.SubmitTextComposition(KRY_TEXT_COMPOSITION_UPDATE, "ni", 2, 0)
				r.SubmitTextComposition(KRY_TEXT_COMPOSITION_COMMIT, "x", 1, 0)
			}
			r.BeginFrame()
			parent := r.beginPopupInput(0, NewRectangle(10, 10, 120, 120))
			if frame == 0 {
				child := r.beginPopupInput(1, NewRectangle(20, 20, 60, 60))
				r.endPopupInput(child)
			} else {
				r.closePopupInput(1)
			}
			r.setFocus(26100)
			compositionEditor(r, area, text, &cursor)
			r.endPopupInput(parent)
			r.EndFrame()
			want := "a"
			if frame == 2 {
				want = "ax"
			}
			if got := string(text[:zeroIndex(text)]); got != want {
				t.Fatalf("area=%v frame=%d buffer=%q want=%q", area, frame, got, want)
			}
		}
	}
}

func TestCompositionPreeditRetiresWithEditor(t *testing.T) {
	for _, cause := range []string{"missing", "focus", "disabled", "popup"} {
		r := New(AppConfig{}).(*runtime)
		text := make([]byte, 16)
		copy(text, "a")
		cursor := int32(1)
		r.setFocus(26100)
		r.SubmitTextComposition(KRY_TEXT_COMPOSITION_UPDATE, "ni", 2, 0)
		r.BeginFrame()
		compositionEditor(r, false, text, &cursor)
		r.EndFrame()
		r.BeginFrame()
		switch cause {
		case "focus":
			r.setFocus(999)
			compositionEditor(r, false, text, &cursor)
		case "disabled":
			r.BeginDisabled(true)
			compositionEditor(r, false, text, &cursor)
			r.EndDisabled()
		case "popup":
			popup := r.beginPopupInput(1, NewRectangle(20, 20, 60, 60))
			r.endPopupInput(popup)
			compositionEditor(r, false, text, &cursor)
		}
		r.EndFrame()
		if len(r.preedit) != 0 {
			t.Fatalf("%s editor retained preedit", cause)
		}
		r.closePopupInput(1)
		r.setFocus(26100)
		r.BeginFrame()
		compositionEditor(r, false, text, &cursor)
		r.EndFrame()
		if r.FrameOps()[0].Text != "a" {
			t.Fatalf("%s editor revived preedit", cause)
		}
	}
}
