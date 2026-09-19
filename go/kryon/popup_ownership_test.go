package kryon

import "testing"

func TestPopupBranchOrder(t *testing.T) {
	r := New(AppConfig{}).(*runtime)
	parents := []int32{-1, 0, 1, 0, 3, -1, 5}
	orders := []uint64{1, 100, 500, 200, 201, 300, 301}
	r.popupPanels = make(map[int32]popupInputPanel)
	for i, parent := range parents {
		r.popupPanels[int32(i)] = popupInputPanel{parent: parent, hasParent: parent >= 0, order: orders[i], alive: true}
	}
	for a := range parents {
		for b := range parents {
			if r.popupAbove(int32(a), int32(b)) != (a > b) {
				t.Fatalf("incorrect branch order: %d above %d", a, b)
			}
		}
	}
}

func TestPopupChildCannotReclaimClosedParentInput(t *testing.T) {
	r := New(AppConfig{}).(*runtime)
	r.BeginFrame()
	parent := r.beginPopupInput(10, NewRectangle(0, 0, 100, 100))
	r.closePopupInput(10)
	child := r.beginPopupInput(11, NewRectangle(0, 0, 100, 100))
	r.registerPopupFocus(12)
	if !r.popupKeyboardCaptures() || !r.popupCaptures(20, 20) || r.Focus() == 12 {
		t.Fatal("child of a closed owner regained input or focus")
	}
	r.endPopupInput(child)
	r.endPopupInput(parent)
	r.EndFrame()
	if len(r.popupPanels) != 0 {
		t.Fatal("closed popup descendants survived frame cleanup")
	}
}
