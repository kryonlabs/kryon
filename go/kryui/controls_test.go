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

func TestPasswordFieldIsSecureByDefault(t *testing.T) {
	f := NewPasswordField(7, 32)
	if !f.Secure() {
		t.Fatal("password field must start secure")
	}
	f.SetText("secret")
	if got := f.Text(); got != "secret" {
		t.Fatalf("Text() = %q, want secret", got)
	}
	f.SetSecure(false)
	if f.Secure() {
		t.Fatal("SetSecure(false) did not reveal field")
	}
}
