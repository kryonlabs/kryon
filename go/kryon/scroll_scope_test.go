package kryon

import "testing"

func TestScrollScopeDragLosesPopupOwnership(t *testing.T) {
	r := New(AppConfig{Width: 240, Height: 180}).(*runtime)
	offset := int32(0)
	r.QueueMouseButtonDown(MouseButtonLeft, 95, 8)
	r.BeginFrame()
	owner := r.beginPopupInput(10, NewRectangle(0, 0, 100, 100))
	r.ScrollScope(NewRectangle(0, 0, 100, 50), 200, &offset)
	r.ScrollEndScope()
	if r.scrollDragOffset != &offset {
		t.Fatal("scrollbar did not acquire drag")
	}
	r.closePopupInput(10)
	r.mousePos.Y = 45
	before := offset
	r.ScrollScope(NewRectangle(0, 0, 100, 50), 200, &offset)
	r.ScrollEndScope()
	if r.scrollDragOffset != nil || offset != before {
		t.Fatal("scrollbar continued after its popup owner closed")
	}
	r.endPopupInput(owner)
	r.EndFrame()
}

func TestScrollScopeCancelsDragWhenContentShrinks(t *testing.T) {
	r := New(AppConfig{Width: 240, Height: 180}).(*runtime)
	offset := int32(0)
	r.QueueMouseButtonDown(MouseButtonLeft, 95, 8)
	r.BeginFrame()
	r.ScrollScope(NewRectangle(0, 0, 100, 50), 200, &offset)
	r.ScrollEndScope()
	r.ScrollScope(NewRectangle(0, 0, 100, 50), 10, &offset)
	r.ScrollEndScope()
	if r.scrollDragOffset != nil || offset != 0 {
		t.Fatal("removed scrollbar retained its drag")
	}
	r.EndFrame()
}
