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
		CursorPosition: &cursor, Focused: &focused, FocusID: 32100,
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

func TestTextFieldQueuedInputAfterTab(t *testing.T) {
	r := New(AppConfig{}).(*runtime)
	first, second := make([]byte, 32), make([]byte, 32)
	firstCursor, secondCursor := int32(0), int32(0)
	draw := func() {
		r.BeginFrame()
		r.TextField(TextFieldProps{Text: first, CursorPosition: &firstCursor, FocusID: 32201})
		r.TextField(TextFieldProps{Text: second, CursorPosition: &secondCursor, FocusID: 32202})
		r.EndFrame()
	}
	r.SetFocus(32201)
	draw()
	r.QueueText("α")
	r.QueueKey(KeyTab)
	r.QueueText("β")
	draw()
	if CString(first) != "α" || CString(second) != "β" || r.Focus() != 32202 {
		t.Fatalf("Tab input routing: first=%q second=%q focus=%d", CString(first), CString(second), r.Focus())
	}
}

func TestTextFieldSelectAllThenExtendUnicode(t *testing.T) {
	r := New(AppConfig{}).(*runtime)
	text := make([]byte, 32)
	copy(text, "a界β")
	cursor := int32(1)
	r.SetFocus(32203)
	r.QueueShortcut(KeyA)
	r.QueueShiftKey(KeyLeft)
	r.BeginFrame()
	r.TextField(TextFieldProps{Text: text, CursorPosition: &cursor, FocusID: 32203})
	r.EndFrame()
	if cursor != int32(len("a界")) || r.selection[32203].Anchor != 0 {
		t.Fatalf("selection after Ctrl+A, Shift+Left: cursor=%d selection=%+v", cursor, r.selection[32203])
	}
}

func TestTextFieldReadOnlyEnterDoesNotCommit(t *testing.T) {
	r := New(AppConfig{}).(*runtime)
	text := make([]byte, 16)
	copy(text, "base")
	cursor := int32(4)
	commit := false
	r.SetFocus(32204)
	r.QueueKey(KeyEnter)
	r.BeginFrame()
	r.TextField(TextFieldProps{Text: text, CursorPosition: &cursor, FocusID: 32204,
		ReadOnly: true, CommitPressed: &commit})
	r.EndFrame()
	if commit || CString(text) != "base" {
		t.Fatalf("read-only Enter: commit=%v text=%q", commit, CString(text))
	}
}

func TestTextFieldEscapeCancelsComposition(t *testing.T) {
	r := New(AppConfig{}).(*runtime)
	text := make([]byte, 32)
	copy(text, "base")
	cursor := int32(4)
	draw := func() {
		r.BeginFrame()
		r.TextField(TextFieldProps{Text: text, CursorPosition: &cursor, FocusID: 32205})
		r.EndFrame()
	}
	r.SetFocus(32205)
	r.SubmitTextComposition(KRY_TEXT_COMPOSITION_UPDATE, "ni", 2, 0)
	draw()
	r.QueueKey(KeyEscape)
	r.QueueText("blocked")
	r.SubmitTextComposition(KRY_TEXT_COMPOSITION_COMMIT, "你", 3, 0)
	draw()
	if r.Focus() != 0 || CString(text) != "base" || len(r.preedit) != 0 {
		t.Fatalf("Escape did not cancel editing: focus=%d text=%q preedit=%v", r.Focus(), CString(text), r.preedit)
	}
}

func TestTextFieldBackwardTabPreservesQueuedInput(t *testing.T) {
	r := New(AppConfig{}).(*runtime)
	first, second := make([]byte, 32), make([]byte, 32)
	a, b := int32(0), int32(0)
	draw := func() {
		r.BeginFrame()
		r.TextField(TextFieldProps{Text: first, CursorPosition: &a, FocusID: 32301})
		r.TextField(TextFieldProps{Text: second, CursorPosition: &b, FocusID: 32302})
		r.EndFrame()
	}
	r.SetFocus(32302)
	draw()
	r.QueueShiftKey(KeyTab)
	r.QueueText("界")
	draw()
	draw()
	if CString(first) != "界" || CString(second) != "" || a != 3 || r.Focus() != 32301 {
		t.Fatalf("backward Tab routing: %q %q cursor=%d focus=%d", CString(first), CString(second), a, r.Focus())
	}
	draw()
	if CString(first) != "界" {
		t.Fatal("deferred input replayed")
	}
}

func TestTextFieldHandoffDropsOnlyStaleInput(t *testing.T) {
	r := New(AppConfig{}).(*runtime)
	first, second := make([]byte, 32), make([]byte, 32)
	a, b := int32(0), int32(0)
	draw := func() {
		r.BeginFrame()
		r.TextField(TextFieldProps{Text: first, CursorPosition: &a, FocusID: 32311})
		r.TextField(TextFieldProps{Text: second, CursorPosition: &b, FocusID: 32312})
		r.EndFrame()
	}
	r.SetFocus(32312)
	draw()
	r.QueueShiftKey(KeyTab)
	r.QueueText("stale")
	draw()
	r.SetFocus(32312)
	r.QueueText("fresh")
	draw()
	if CString(first) != "" || CString(second) != "fresh" {
		t.Fatalf("changed focus replayed or lost input: first=%q second=%q", CString(first), CString(second))
	}
}
