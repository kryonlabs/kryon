package kryon

import "testing"

func TestAccessibilityListSelectionAndIdentity(t *testing.T) {
	r := New(AppConfig{Width: 240, Height: 180}).(*runtime)
	defer r.Close()
	selected, scroll := int32(-1), int32(0)
	props := ListBoxProps{ID: 81, Bounds: NewRectangle(10, 10, 120, 40), RowHeight: 20,
		Items: []string{"Alpha", "Beta", "Gamma", "Delta"}, ItemKeys: []int32{1, 2, 3, 4},
		SelectedIndex: &selected, ScrollOffset: &scroll}
	draw := func() int32 {
		r.BeginFrame()
		changed := r.ListBox(props)
		r.EndFrame()
		return changed
	}
	draw()
	nodes := r.GetAccessibilitySnapshot()
	if len(nodes) != 5 || nodes[0].Role != "listbox" || nodes[4].Parent != 1 || !nodes[4].Offscreen {
		t.Fatalf("list tree = %+v", nodes)
	}
	if !r.QueueAccessibilityItem(81, nodes[0].Generation, 3, true) || draw() != 1 || selected != 3 || scroll <= 0 {
		t.Fatalf("offscreen selection = %d scroll=%d", selected, scroll)
	}
	nodes = r.GetAccessibilitySnapshot()
	if !nodes[4].Selected || nodes[4].Offscreen {
		t.Fatal("selection was not revealed")
	}
	if !r.QueueAccessibilityItem(81, nodes[0].Generation, 1, true) {
		t.Fatal("selection not queued")
	}
	props.Items[1], props.Items[2] = props.Items[2], props.Items[1]
	props.ItemKeys[1], props.ItemKeys[2] = props.ItemKeys[2], props.ItemKeys[1]
	draw()
	if selected != 2 {
		t.Fatal("queued selection did not follow the stable item key")
	}
	if !r.QueueAccessibilityItem(81, accessibilityNode(t, r, 81).Generation, 0, true) {
		t.Fatal("selection not queued")
	}
	props.Disabled = true
	draw()
	if selected != 2 || r.QueueAccessibilityItem(81, accessibilityNode(t, r, 81).Generation, 1, true) {
		t.Fatal("disabled list accepted or applied a selection")
	}
	props.Disabled = false
	draw()
	if !r.QueueAccessibilityItem(81, accessibilityNode(t, r, 81).Generation, 0, true) {
		t.Fatal("selection not queued")
	}
	props.ItemKeys[0] = 99
	draw()
	if selected != 2 {
		t.Fatal("removed item request selected its replacement")
	}
	if r.QueueAccessibilityAction(81, accessibilityNode(t, r, 81).Generation, AccessibilityActionSelectAll) {
		t.Fatal("single-selection list accepted SelectAll")
	}
}

func TestAccessibilityMultiSelection(t *testing.T) {
	r := New(AppConfig{Width: 240, Height: 180}).(*runtime)
	defer r.Close()
	selected := []int32{0, 0, 0}
	count, anchor := int32(0), int32(-1)
	props := ListBoxProps{ID: 82, Bounds: NewRectangle(0, 0, 120, 80), RowHeight: 20,
		Items: []string{"A", "B", "C"}, Selected: selected, SelectedCount: &count, Anchor: &anchor}
	draw := func() {
		r.BeginFrame()
		r.ListBox(props)
		r.EndFrame()
	}
	draw()
	generation := accessibilityNode(t, r, 82).Generation
	if !r.QueueAccessibilityItem(82, generation, 0, true) || !r.QueueAccessibilityItem(82, generation, 2, true) {
		t.Fatal("distinct selections were not queued")
	}
	draw()
	if count != 2 || selected[0] != 1 || selected[1] != 0 || selected[2] != 1 {
		t.Fatalf("multi selection = %v count=%d", selected, count)
	}
	if !r.QueueAccessibilityItem(82, accessibilityNode(t, r, 82).Generation, 0, false) {
		t.Fatal("deselection rejected")
	}
	draw()
	if count != 1 || selected[0] != 0 {
		t.Fatal("deselection not delivered")
	}
	if !r.QueueAccessibilityAction(82, accessibilityNode(t, r, 82).Generation, AccessibilityActionSelectAll) {
		t.Fatal("select all rejected")
	}
	draw()
	if count != 3 {
		t.Fatal("select all not delivered")
	}
	if !r.QueueAccessibilityAction(82, accessibilityNode(t, r, 82).Generation, AccessibilityActionClearSelection) {
		t.Fatal("clear selection rejected")
	}
	draw()
	if count != 0 {
		t.Fatal("clear selection not delivered")
	}
}
