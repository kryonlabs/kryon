package kryui

import "testing"

func TestGridCellEditorOwnsTextField(t *testing.T) {
	e := NewGridCellEditor(91, 32)
	e.Begin("=0.1+0.05")
	if !e.Active() || e.Text() != "=0.1+0.05" {
		t.Fatalf("active=%v text=%q", e.Active(), e.Text())
	}
	e.SetText("changed")
	if e.Text() != "changed" {
		t.Fatalf("text=%q", e.Text())
	}
	e.Cancel()
	if e.Active() {
		t.Fatal("editor remained active after cancel")
	}
}
