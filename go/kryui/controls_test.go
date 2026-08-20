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

func TestTextAreaContentVersionTracksTextChanges(t *testing.T) {
	a := NewTextArea(9, 16)
	initial := a.contentVersion
	if initial == 0 {
		t.Fatal("content version must start non-zero so native layout caching is enabled")
	}
	a.SetText("")
	if a.contentVersion != initial {
		t.Fatalf("empty SetText changed version: got %d, want %d", a.contentVersion, initial)
	}
	a.SetText("hello")
	changed := a.contentVersion
	if changed == initial {
		t.Fatal("SetText with new content must bump content version")
	}
	a.SetText("hello")
	if a.contentVersion != changed {
		t.Fatalf("SetText with same content changed version: got %d, want %d", a.contentVersion, changed)
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
