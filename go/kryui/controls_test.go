package kryui

import "testing"

func TestTextFieldOwnsBuffer(t *testing.T) {
	f := NewTextField(7, 8)
	f.SetText("abcdefghijk")
	if got := f.Text(); got != "abcdefg" {
		t.Fatalf("Text() = %q, want capacity-limited value", got)
	}
	f.Clear()
	if got := f.Text(); got != "" {
		t.Fatalf("Text() after Clear = %q", got)
	}
}

func TestTextAreaOwnsBuffer(t *testing.T) {
	a := NewTextArea(9, 16)
	a.SetText("hello\nworld")
	if got := a.Text(); got != "hello\nworld" {
		t.Fatalf("Text() = %q", got)
	}
}
