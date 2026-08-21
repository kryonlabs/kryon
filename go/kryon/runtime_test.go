package kryon

import "testing"

func TestTextFieldCursorNavigationAndUnicodeInput(t *testing.T) {
	rt := New(AppConfig{Width: 320, Height: 200}).(*runtime)
	text := make([]byte, 64)
	copy(text, "abcde")
	cursor := int32(len("abcde"))
	focused := true

	rt.TextField(TextFieldProps{
		Text:           text,
		CursorPosition: &cursor,
		Focused:        &focused,
		FocusID:        7,
		MaxCodepoints:  63,
	})
	rt.QueueKey(KeyLeft)
	rt.QueueText("é")
	rt.TextField(TextFieldProps{
		Text:           text,
		CursorPosition: &cursor,
		Focused:        &focused,
		FocusID:        7,
		MaxCodepoints:  63,
	})

	if got, want := string(text[:zeroIndex(text)]), "abcdée"; got != want {
		t.Fatalf("text = %q, want %q", got, want)
	}
	if got, want := cursor, int32(len("abcdé")); got != want {
		t.Fatalf("cursor = %d, want %d", got, want)
	}
	if !focused {
		t.Fatal("field lost focus")
	}
}

func TestTextFieldCommitAndDelete(t *testing.T) {
	rt := New(AppConfig{}).(*runtime)
	text := make([]byte, 16)
	copy(text, "abc")
	cursor := int32(1)
	focused := true
	commit := false

	rt.QueueKey(KeyDelete)
	rt.QueueKey(KeyEnter)
	rt.TextField(TextFieldProps{
		Text:           text,
		CursorPosition: &cursor,
		Focused:        &focused,
		CommitPressed:  &commit,
		FocusID:        9,
		MaxCodepoints:  15,
	})

	if got, want := string(text[:zeroIndex(text)]), "ac"; got != want {
		t.Fatalf("text = %q, want %q", got, want)
	}
	if !commit {
		t.Fatal("enter did not set CommitPressed")
	}
}

func TestTextFieldTabTraversal(t *testing.T) {
	rt := New(AppConfig{}).(*runtime)
	aText, bText := make([]byte, 8), make([]byte, 8)
	aCursor, bCursor := int32(0), int32(0)
	aFocused, bFocused := true, false

	draw := func() {
		rt.BeginDrawing()
		rt.TextField(TextFieldProps{Text: aText, CursorPosition: &aCursor, Focused: &aFocused, FocusID: 1})
		rt.TextField(TextFieldProps{Text: bText, CursorPosition: &bCursor, Focused: &bFocused, FocusID: 2})
		rt.EndDrawing()
	}

	draw()
	rt.QueueKey(KeyTab)
	draw()
	if aFocused || !bFocused {
		t.Fatalf("tab focus = a:%v b:%v, want a:false b:true", aFocused, bFocused)
	}
	rt.QueueShiftKey(KeyTab)
	draw()
	draw()
	if !aFocused || bFocused {
		t.Fatalf("shift-tab focus = a:%v b:%v, want a:true b:false", aFocused, bFocused)
	}
}

func TestTextFieldSelectionClipboardAndSecureMode(t *testing.T) {
	rt := New(AppConfig{}).(*runtime)
	text := make([]byte, 64)
	copy(text, "abcdef")
	cursor := int32(6)
	focused := true

	rt.SetSelection(11, 1, 4)
	rt.QueueText("XY")
	rt.TextField(TextFieldProps{Text: text, CursorPosition: &cursor, Focused: &focused, FocusID: 11, MaxCodepoints: 63})
	if got, want := string(text[:zeroIndex(text)]), "aXYef"; got != want {
		t.Fatalf("selection replace = %q, want %q", got, want)
	}

	rt.SetSelection(11, 1, 3)
	rt.QueueShortcut(KeyC)
	rt.TextField(TextFieldProps{Text: text, CursorPosition: &cursor, Focused: &focused, FocusID: 11, MaxCodepoints: 63})
	if got, want := rt.ClipboardText(), "XY"; got != want {
		t.Fatalf("clipboard after copy = %q, want %q", got, want)
	}

	rt.QueueShortcut(KeyX)
	rt.TextField(TextFieldProps{Text: text, CursorPosition: &cursor, Focused: &focused, FocusID: 11, MaxCodepoints: 63})
	if got, want := string(text[:zeroIndex(text)]), "aef"; got != want {
		t.Fatalf("cut text = %q, want %q", got, want)
	}

	rt.QueueShortcut(KeyV)
	rt.TextField(TextFieldProps{Text: text, CursorPosition: &cursor, Focused: &focused, FocusID: 11, MaxCodepoints: 63})
	if got, want := string(text[:zeroIndex(text)]), "aXYef"; got != want {
		t.Fatalf("paste text = %q, want %q", got, want)
	}

	rt.SetClipboardText("old")
	rt.SetSelection(11, 1, 3)
	rt.QueueShortcut(KeyC)
	rt.TextField(TextFieldProps{Text: text, CursorPosition: &cursor, Focused: &focused, FocusID: 11, MaxCodepoints: 63, Secure: true})
	if got, want := rt.ClipboardText(), "old"; got != want {
		t.Fatalf("secure copy changed clipboard = %q, want %q", got, want)
	}
}

func TestTextFieldLongTypingDoesNotGrowFieldOrder(t *testing.T) {
	rt := New(AppConfig{}).(*runtime)
	text := make([]byte, 8192)
	cursor := int32(0)
	focused := true

	for i := 0; i < 3000; i++ {
		rt.BeginDrawing()
		rt.QueueText("a")
		rt.TextField(TextFieldProps{Text: text, CursorPosition: &cursor, Focused: &focused, FocusID: 31, MaxCodepoints: 8191})
		rt.EndDrawing()
	}
	if got, want := len(string(text[:zeroIndex(text)])), 3000; got != want {
		t.Fatalf("typed length = %d, want %d", got, want)
	}
	if got, want := len(rt.prevOrder), 1; got != want {
		t.Fatalf("field order length = %d, want %d", got, want)
	}
}
