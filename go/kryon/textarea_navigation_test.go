package kryon

import "testing"

func TestTextAreaMultilineNavigation(t *testing.T) {
	r := New(AppConfig{Width: 240, Height: 160}).(*runtime)
	text := make([]byte, 64)
	copy(text, "a0\nb1\nc2\nd3\ne4\nf5")
	cursor := int32(4)
	focused := true
	props := TextAreaProps{
		Bounds: NewRectangle(10, 10, 160, 60), Text: text,
		CursorPosition: &cursor, Focused: &focused, FocusID: 32100, Font: Text16, LineGap: 4,
	}
	draw := func(key int32) {
		if key != 0 {
			r.QueueKey(key)
		}
		r.BeginFrame()
		r.TextArea(props)
		r.EndFrame()
	}

	r.SetFocus(props.FocusID)
	draw(KeyDown)
	if cursor != 7 {
		t.Fatalf("Down cursor = %d, want 7", cursor)
	}
	draw(KeyPageDown)
	if cursor != 13 {
		t.Fatalf("PageDown cursor = %d, want 13", cursor)
	}
	draw(KeyPageUp)
	if cursor != 7 {
		t.Fatalf("PageUp cursor = %d, want 7", cursor)
	}
	r.QueueShiftKey(KeyPageDown)
	draw(0)
	if cursor != 13 {
		t.Fatalf("Shift+PageDown cursor = %d, want 13", cursor)
	}
	if selected := r.selection[props.FocusID]; selected.Anchor != 7 || selected.Cursor != 13 {
		t.Fatalf("Shift+PageDown selection = %+v, want {Anchor:7 Cursor:13}", selected)
	}
	draw(KeyLeft)
	if cursor != 7 {
		t.Fatalf("Left after selection cursor = %d, want collapsed start 7", cursor)
	}
	draw(KeyHome)
	if cursor != 6 {
		t.Fatalf("multiline Home cursor = %d, want line start 6", cursor)
	}
	r.QueueShortcut(KeyEnd)
	draw(0)
	if cursor != int32(len("a0\nb1\nc2\nd3\ne4\nf5")) {
		t.Fatalf("Ctrl+End cursor = %d, want buffer end", cursor)
	}
	cursor = 7
	delete(r.selection, props.FocusID)
	draw(KeyEnter)
	if got := CString(text); got != "a0\nb1\nc\n2\nd3\ne4\nf5" {
		t.Fatalf("multiline Enter text = %q", got)
	}
}
