package kryon

import "testing"

func TestAccessibilityTreeScopes(t *testing.T) {
	r := New(AppConfig{Width: 300, Height: 200}).(*runtime)
	defer r.Close()
	r.BeginFrame()
	r.Group(ColumnProps{Key: 700, Bounds: NewRectangle(20, 30, 240, 150)})
	r.Column(ColumnProps{Key: 701, Bounds: NewRectangle(30, 40, 200, 100)})
	r.Button(ButtonProps{ID: 702, Label: "Nested", Bounds: NewRectangle(40, 50, 80, 25)})
	r.End()
	r.Button(ButtonProps{ID: 703, Label: "Sibling", Bounds: NewRectangle(40, 90, 80, 25)})
	r.End()
	r.Button(ButtonProps{ID: 704, Label: "Outside", Bounds: NewRectangle(0, 0, 80, 25)})
	r.EndFrame()
	nodes := r.GetAccessibilitySnapshot()
	if len(nodes) != 5 {
		t.Fatalf("nodes = %+v", nodes)
	}
	for index, want := range []uint32{0, 1, 2, 1, 0} {
		if nodes[index].Parent != want || nodes[index].Key != uint64(700+index) {
			t.Fatalf("node %d = %+v, want parent %d and key %d", index, nodes[index], want, 700+index)
		}
	}
}

func TestAccessibilityTreeComposedLabels(t *testing.T) {
	r := New(AppConfig{Width: 300, Height: 200}).(*runtime)
	defer r.Close()
	r.BeginFrame()
	r.ButtonScope(ButtonProps{ID: 710, Bounds: NewRectangle(0, 0, 100, 40)})
	r.Column(ColumnProps{Key: 711})
	r.Text(TextProps{Text: "Only once"})
	r.End()
	r.End()
	r.EndFrame()
	nodes := r.GetAccessibilitySnapshot()
	if len(nodes) != 2 || nodes[0].Label != "Only once" || nodes[1].Parent != 1 {
		t.Fatalf("composed tree = %+v", nodes)
	}
}
