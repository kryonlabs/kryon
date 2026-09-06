package kryon

import (
	"strings"
	"testing"
)

func TestPopupComboKeyboardOwnership(t *testing.T) {
	for _, inside := range []bool{false, true} {
		r := New(AppConfig{}).(*runtime)
		r.QueueKey(KeySpace)
		r.BeginFrame()
		parent := r.beginPopupInput(0, NewRectangle(180, 180, 40, 40))
		child := r.beginPopupInput(1, NewRectangle(190, 190, 20, 20))
		if !inside {
			r.endPopupInput(child)
		}
		if r.popupKeyboardCaptures() == inside {
			t.Fatal("keyboard ownership depends on pointer bounds or wrong branch")
		}
		r.setFocus(25400)
		var selected int32
		r.Combobox(ComboboxProps{Bounds: NewRectangle(10, 10, 100, 28), ID: 25400,
			Options: []string{"One", "Two"}, SelectedIndex: &selected})
		if r.openDropdowns[25400] != inside {
			t.Fatalf("combo open = %v, inside top popup = %v", r.openDropdowns[25400], inside)
		}
		r.closeDropdown(25400)
		if inside {
			r.endPopupInput(child)
		}
		r.closePopupInput(1)
		if r.popupKeyboardCaptures() {
			t.Fatal("closing child did not restore parent keyboard")
		}
		r.endPopupInput(parent)
		if !r.popupKeyboardCaptures() {
			t.Fatal("parent did not capture background keyboard")
		}
		r.closePopupInput(0)
		if r.popupKeyboardCaptures() {
			t.Fatal("closing branch did not restore background keyboard")
		}
		r.EndFrame()
	}
}

func TestPopupTextKeyboardOwnership(t *testing.T) {
	for _, area := range []bool{false, true} {
		for _, inside := range []bool{false, true} {
			r := New(AppConfig{}).(*runtime)
			r.QueueText("x")
			r.BeginFrame()
			parent := r.beginPopupInput(0, NewRectangle(180, 180, 40, 40))
			child := r.beginPopupInput(1, NewRectangle(190, 190, 20, 20))
			if !inside {
				r.endPopupInput(child)
			}
			text := make([]byte, 32)
			copy(text, "a")
			cursor, focused := int32(1), true
			r.setFocus(25500)
			if area {
				r.TextArea(TextAreaProps{Bounds: NewRectangle(10, 10, 120, 80), Text: text,
					CursorPosition: &cursor, Focused: &focused, FocusID: 25500})
			} else {
				r.TextField(TextFieldProps{Bounds: NewRectangle(10, 10, 120, 28), Text: text,
					CursorPosition: &cursor, Focused: &focused, FocusID: 25500})
			}
			want := "a"
			if inside {
				want = "ax"
				r.endPopupInput(child)
			}
			if got := string(text[:zeroIndex(text)]); got != want {
				t.Fatalf("area=%v inside=%v: text=%q want=%q", area, inside, got, want)
			}
			r.endPopupInput(parent)
			r.EndFrame()
		}
	}
}

func TestPopupTabOwnership(t *testing.T) {
	for mode := 0; mode < 6; mode++ {
		r := New(AppConfig{}).(*runtime)
		start := []int32{25620, 25621, 25620, 25600, 25620, 25620}
		want := []int32{25621, 25620, 25621, 25621, 25610, 25600}
		for frame := 0; frame < 2; frame++ {
			if mode == 3 {
				r.setFocus(start[mode])
				r.QueueShiftKey(KeyTab)
			}
			if frame == 1 && mode < 3 {
				if mode == 2 {
					r.QueueShiftKey(KeyTab)
				} else {
					r.QueueKey(KeyTab)
				}
			}
			r.BeginFrame()
			r.registerField(25600)
			parent := r.beginPopupInput(0, NewRectangle(10, 10, 120, 120))
			r.registerField(25610)
			child := r.beginPopupInput(1, NewRectangle(20, 20, 60, 60))
			r.registerField(25620)
			r.registerField(25621)
			r.registerField(25620)
			if frame == 1 && mode < 3 {
				cursor, focused := int32(0), true
				r.setFocus(start[mode])
				r.TextField(TextFieldProps{Bounds: NewRectangle(20, 20, 60, 24),
					Text: make([]byte, 16), CursorPosition: &cursor, Focused: &focused,
					FocusID: start[mode]})
				if r.Focus() != want[mode] {
					t.Fatalf("editor Tab mode=%d: focus=%d want=%d", mode, r.Focus(), want[mode])
				}
			}
			r.endPopupInput(child)
			r.registerField(25611)
			r.endPopupInput(parent)
			r.registerField(25601)
			if mode == 4 {
				r.closePopupInput(1)
			}
			if mode == 5 {
				r.closePopupInput(0)
			}
			got := r.nextFocus(start[mode], mode == 2 || mode == 3)
			if got != want[mode] {
				t.Fatalf("mode=%d frame=%d: next focus=%d want=%d", mode, frame, got, want[mode])
			}
			r.EndFrame()
			if mode == 3 && r.Focus() != want[mode] {
				t.Fatalf("unclaimed Tab did not enter top popup: got %d want %d", r.Focus(), want[mode])
			}
		}
	}
}

func TestPopupButtonKeyboardOwnership(t *testing.T) {
	for _, key := range []int32{KeyEnter, KeySpace} {
		for _, inside := range []bool{false, true} {
			r := New(AppConfig{}).(*runtime)
			r.QueueKey(key)
			r.BeginFrame()
			parent := r.beginPopupInput(0, NewRectangle(180, 180, 40, 40))
			child := r.beginPopupInput(1, NewRectangle(190, 190, 20, 20))
			if !inside {
				r.endPopupInput(child)
			}
			r.setFocus(25700)
			p := ButtonProps{Bounds: NewRectangle(10, 10, 120, 28), Label: "Action", ID: 25700}
			if got := r.Button(p); got != inside {
				t.Fatalf("key=%d inside=%v: activated=%v", key, inside, got)
			}
			if inside {
				if r.Button(p) {
					t.Fatal("button replayed consumed activation")
				}
				r.endPopupInput(child)
			}
			r.endPopupInput(parent)
			r.EndFrame()
		}
	}
}

func TestPopupTabMissingOwner(t *testing.T) {
	r := New(AppConfig{}).(*runtime)
	for frame := 0; frame < 3; frame++ {
		if frame != 0 {
			r.QueueKey(KeyTab)
		}
		r.BeginFrame()
		r.registerField(25800)
		if frame < 2 {
			parent := r.beginPopupInput(0, NewRectangle(10, 10, 120, 120))
			r.registerField(25810)
			if frame == 0 {
				child := r.beginPopupInput(1, NewRectangle(20, 20, 60, 60))
				r.registerField(25820)
				r.setFocus(25820)
				r.endPopupInput(child)
			}
			r.endPopupInput(parent)
		}
		r.EndFrame()
		want := []int32{25820, 25810, 25800}[frame]
		if r.Focus() != want {
			t.Fatalf("frame=%d: missing owner retained Tab capture: got %d want %d", frame, r.Focus(), want)
		}
	}
}

func TestPopupTextDismissalDoesNotReplay(t *testing.T) {
	for _, area := range []bool{false, true} {
		r := New(AppConfig{}).(*runtime)
		text := make([]byte, 32)
		copy(text, "a")
		cursor, focused := int32(1), true
		for frame := 0; frame < 3; frame++ {
			if frame != 1 {
				r.QueueText("x")
			}
			r.BeginFrame()
			parent := r.beginPopupInput(0, NewRectangle(10, 10, 120, 120))
			if frame == 0 {
				child := r.beginPopupInput(1, NewRectangle(20, 20, 60, 60))
				r.endPopupInput(child)
			} else {
				r.closePopupInput(1)
			}
			r.setFocus(25900)
			focused = true
			if area {
				r.TextArea(TextAreaProps{Bounds: NewRectangle(10, 10, 120, 80), Text: text,
					CursorPosition: &cursor, Focused: &focused, FocusID: 25900})
			} else {
				r.TextField(TextFieldProps{Bounds: NewRectangle(10, 10, 120, 28), Text: text,
					CursorPosition: &cursor, Focused: &focused, FocusID: 25900})
			}
			r.endPopupInput(parent)
			r.EndFrame()
			want := "a"
			if frame == 2 {
				want = "ax"
			}
			if got := string(text[:zeroIndex(text)]); got != want {
				t.Fatalf("area=%v frame=%d: got %q want %q", area, frame, got, want)
			}
		}
	}
}

func TestPopupWheelOwnershipAcrossScrollableWidgets(t *testing.T) {
	bounds := NewRectangle(10, 10, 100, 100)
	draws := map[string]func(*runtime, *int32){
		"scroll": func(r *runtime, offset *int32) {
			r.BeginScroll(bounds, 600, offset)
			r.EndScroll()
		},
		"list": func(r *runtime, offset *int32) {
			r.ListBox(ListBoxProps{Bounds: bounds, Items: make([]string, 20), ScrollOffset: offset})
		},
		"tree": func(r *runtime, offset *int32) {
			r.TreeView(TreeViewProps{Bounds: bounds, Items: make([]UITreeItem, 20), ScrollOffset: offset})
		},
		"source": func(r *runtime, offset *int32) {
			r.SourceView(SourceViewProps{Bounds: bounds, Text: strings.Repeat("line\n", 20), ScrollY: offset})
		},
		"table": func(r *runtime, offset *int32) {
			r.TableView(TableViewProps{Bounds: bounds, Columns: []string{"Value"}, Rows: make([]TableRow, 20), ScrollOffset: offset})
		},
	}
	for name, draw := range draws {
		t.Run(name, func(t *testing.T) {
			r := New(AppConfig{}).(*runtime)
			r.BeginFrame()
			owner := r.beginPopupInput(20, bounds)
			r.endPopupInput(owner)
			r.EndFrame()
			r.QueueMouseMove(40, 60)
			r.QueueMouseWheel(-1)
			r.BeginFrame()
			var background, foreground int32
			draw(r, &background)
			if background != 0 || r.mouseWheel != -1 {
				t.Fatal("background scroller consumed popup wheel before owner declaration")
			}
			owner = r.beginPopupInput(20, bounds)
			draw(r, &foreground)
			if foreground <= 0 {
				t.Fatal("popup's ordinary scrollable widget did not scroll")
			}
			r.endPopupInput(owner)
			r.EndFrame()
			r.closePopupInput(20)
			for _, mode := range []string{"disabled", "clipped", "normal"} {
				r.QueueMouseWheel(-1)
				r.BeginFrame()
				r.BeginDisabled(mode == "disabled")
				if mode == "clipped" {
					r.scrollClips = append(r.scrollClips, NewRectangle(0, 0, 1, 1))
				}
				var offset int32
				draw(r, &offset)
				if (offset > 0) != (mode == "normal") {
					t.Fatalf("wheel mode %s: offset=%d", mode, offset)
				}
				r.EndDisabled()
				r.EndFrame()
			}
		})
	}
}

func TestPopupInputNestedOwnership(t *testing.T) {
	r := New(AppConfig{}).(*runtime)
	panel := NewRectangle(10, 10, 100, 100)
	childPanel := NewRectangle(40, 40, 100, 100)
	// Populate last-frame panels before testing earlier background widgets.
	r.BeginFrame()
	parent := r.beginPopupInput(0, panel)
	child := r.beginPopupInput(2, childPanel)
	r.endPopupInput(child)
	r.endPopupInput(parent)
	r.EndFrame()
	r.QueueTap(60, 60)
	r.BeginFrame()
	button := ButtonProps{Bounds: NewRectangle(50, 50, 30, 24), ID: 50}
	if r.Button(button) {
		t.Fatal("earlier background widget stole popup input")
	}
	parent = r.beginPopupInput(0, panel)
	if r.Button(button) {
		t.Fatal("parent stole nested popup input before child declaration")
	}
	child = r.beginPopupInput(2, childPanel)
	if !r.Button(button) {
		t.Fatal("ordinary nested button did not receive input")
	}
	r.endPopupInput(child)
	if !r.popupCaptures(60, 60) || r.popupCaptures(20, 20) {
		t.Fatal("parent capture not restored independently of child")
	}
	r.endPopupInput(parent)
	if r.Button(button) {
		t.Fatal("later background widget stole popup input")
	}
	r.EndFrame()
	// Closing only the child must immediately return its overlap to the parent.
	r.closePopupInput(2)
	r.QueueTap(60, 60)
	r.BeginFrame()
	parent = r.beginPopupInput(0, panel)
	if !r.Button(button) {
		t.Fatal("closing child did not restore parent interaction")
	}
	r.endPopupInput(parent)
	r.EndFrame()
}

func TestPopupDragDropOwnershipAndClipping(t *testing.T) {
	r := New(AppConfig{}).(*runtime)
	panel := NewRectangle(10, 10, 180, 100)
	payload := []byte("item")
	output := make([]byte, 8)
	accepted := int32(99)
	source := DragDropSourceProps{Bounds: panel, ID: 31, Type: "ITEM", Data: payload}
	target := DragDropTargetProps{Bounds: panel, ID: 32, Type: "ITEM", Output: output, AcceptedSize: &accepted}
	r.BeginFrame()
	owner := r.beginPopupInput(30, panel)
	r.endPopupInput(owner)
	r.EndFrame()
	r.QueueMouseButtonDown(MouseButtonLeft, 40, 40)
	r.BeginFrame()
	if r.DragDropSource(source) || r.dragDrop.active {
		t.Fatal("background source stole popup press")
	}
	owner = r.beginPopupInput(30, panel)
	r.scrollClips = []Rectangle{NewRectangle(0, 0, 1, 1)}
	if r.DragDropSource(source) {
		t.Fatal("clipped source activated")
	}
	r.scrollClips = nil
	if !r.DragDropSource(source) {
		t.Fatal("popup source did not activate")
	}
	r.endPopupInput(owner)
	r.EndFrame()
	payload[0] = 'X'
	r.QueueMouseButtonUp(MouseButtonLeft, 40, 40)
	r.BeginFrame()
	if r.DragDropTarget(target) || accepted != 0 || !r.mouseReleased[MouseButtonLeft] || !r.dragDrop.active {
		t.Fatal("background target consumed popup release or payload")
	}
	owner = r.beginPopupInput(30, panel)
	r.scrollClips = []Rectangle{NewRectangle(0, 0, 1, 1)}
	if r.DragDropTarget(target) {
		t.Fatal("clipped target accepted payload")
	}
	r.scrollClips = nil
	target.Disabled = true
	if r.DragDropTarget(target) {
		t.Fatal("disabled target accepted payload")
	}
	target.Disabled = false
	if !r.DragDropTarget(target) || accepted != 4 || string(output[:accepted]) != "item" {
		t.Fatal("popup target did not receive the owned payload after rejected targets")
	}
	if r.DragDropTarget(target) {
		t.Fatal("release accepted twice")
	}
	r.endPopupInput(owner)
	r.EndFrame()
}

func TestPopupInputBranchOrder(t *testing.T) {
	r := New(AppConfig{}).(*runtime)
	panel := NewRectangle(0, 0, 100, 100)
	r.BeginFrame()
	a := r.beginPopupInput(1, panel)
	ac := r.beginPopupInput(2, panel)
	r.endPopupInput(ac)
	r.endPopupInput(a)
	b := r.beginPopupInput(3, panel)
	bc := r.beginPopupInput(4, panel)
	r.endPopupInput(bc)
	r.endPopupInput(b)
	r.EndFrame()
	r.BeginFrame()
	// Re-declaring an ancestor must not put it above its previous-frame child.
	b = r.beginPopupInput(3, panel)
	if !r.popupCaptures(20, 20) {
		t.Fatal("ancestor captured child's overlap")
	}
	bc = r.beginPopupInput(4, panel)
	if r.popupCaptures(20, 20) {
		t.Fatal("top branch child blocked")
	}
	r.endPopupInput(bc)
	r.endPopupInput(b)
	a = r.beginPopupInput(1, panel)
	ac = r.beginPopupInput(2, panel)
	if r.popupCaptures(20, 20) {
		t.Fatal("later sibling branch is not topmost")
	}
	r.endPopupInput(ac)
	r.endPopupInput(a)
	r.EndFrame()
}

func TestPopupInputMissingOwnerAndIsolation(t *testing.T) {
	r := New(AppConfig{}).(*runtime)
	other := New(AppConfig{}).(*runtime)
	panel := NewRectangle(0, 0, 100, 100)
	r.BeginFrame()
	a := r.beginPopupInput(1, panel)
	b := r.beginPopupInput(2, panel)
	r.endPopupInput(b)
	r.endPopupInput(a)
	r.EndFrame()
	if !r.popupCaptures(20, 20) || other.popupCaptures(20, 20) {
		t.Fatal("popup ownership leaked between runtimes")
	}
	r.BeginFrame()
	r.EndFrame()
	if r.popupCaptures(20, 20) || len(r.popupPanels) != 0 {
		t.Fatal("missing owners retained popup capture")
	}
	r.BeginFrame()
	a = r.beginPopupInput(1, panel)
	b = r.beginPopupInput(2, panel)
	r.closePopupInput(1)
	if len(r.popupPanels) != 0 || !r.popupCaptures(20, 20) {
		t.Fatal("parent close did not invalidate active descendants")
	}
	r.endPopupInput(b)
	r.endPopupInput(a)
	if r.popupCaptures(20, 20) {
		t.Fatal("closed popup still blocks background")
	}
}
