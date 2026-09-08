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

func TestTextFieldWordNavigationAndDeletion(t *testing.T) {
	r := New(AppConfig{Width: 240, Height: 100}).(*runtime)
	text := make([]byte, 64)
	copy(text, "alpha beta.gamma")
	cursor := int32(len("alpha beta.gamma"))
	focused := true
	props := TextFieldProps{
		Bounds: NewRectangle(10, 10, 180, 32), Text: text,
		CursorPosition: &cursor, Focused: &focused, FocusID: 32101,
	}
	draw := func() {
		r.BeginFrame()
		r.TextField(props)
		r.EndFrame()
	}

	r.SetFocus(props.FocusID)
	r.QueueShortcut(KeyLeft)
	draw()
	if cursor != 11 {
		t.Fatalf("Ctrl+Left cursor = %d, want 11", cursor)
	}
	r.queueModifiedKey(KeyLeft, true, true)
	draw()
	if selected := r.selection[props.FocusID]; selected.Anchor != 11 || selected.Cursor != 10 {
		t.Fatalf("Ctrl+Shift+Left selection = %+v, want {Anchor:11 Cursor:10}", selected)
	}

	cursor = int32(len("alpha beta.gamma"))
	delete(r.selection, props.FocusID)
	r.QueueShortcut(KeyBackspace)
	draw()
	if got := CString(text); got != "alpha beta." || cursor != 11 {
		t.Fatalf("Ctrl+Backspace = %q cursor %d, want %q cursor 11", got, cursor, "alpha beta.")
	}
	r.QueueShortcut(KeyBackspace)
	draw()
	if got := CString(text); got != "alpha beta" || cursor != 10 {
		t.Fatalf("second Ctrl+Backspace = %q cursor %d, want %q cursor 10", got, cursor, "alpha beta")
	}
	cursor = 6
	delete(r.selection, props.FocusID)
	r.QueueShortcut(KeyDelete)
	draw()
	if got := CString(text); got != "alpha " || cursor != 6 {
		t.Fatalf("Ctrl+Delete = %q cursor %d, want %q cursor 6", got, cursor, "alpha ")
	}

	clear(text)
	copy(text, "alpha beta")
	props.Secure = true
	cursor = 5
	delete(r.selection, props.FocusID)
	r.QueueShortcut(KeyLeft)
	draw()
	if cursor != 0 {
		t.Fatalf("secure Ctrl+Left cursor = %d, want 0", cursor)
	}
	r.QueueShortcut(KeyRight)
	draw()
	if cursor != int32(len("alpha beta")) {
		t.Fatalf("secure Ctrl+Right cursor = %d, want buffer end", cursor)
	}
	r.QueueShortcut(KeyBackspace)
	draw()
	if got := CString(text); got != "" || cursor != 0 {
		t.Fatalf("secure Ctrl+Backspace = %q cursor %d, want empty at 0", got, cursor)
	}
}
