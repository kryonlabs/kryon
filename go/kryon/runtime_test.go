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
