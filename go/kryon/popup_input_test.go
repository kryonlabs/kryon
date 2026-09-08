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

func TestPopupChoiceKeyboardOwnership(t *testing.T) {
	for _, inside := range []bool{false, true} {
		r := New(AppConfig{}).(*runtime)
		selected := int32(0)
		wantSelected := int32(0)
		if inside {
			wantSelected = 1
		}
		r.QueueKey(KeySpace)
		r.BeginFrame()
		parent := r.beginPopupInput(0, NewRectangle(180, 180, 40, 40))
		child := r.beginPopupInput(1, NewRectangle(190, 190, 20, 20))
		if !inside {
			r.endPopupInput(child)
		}
		r.setFocus(25705)
		activated := r.Selectable(SelectableProps{
			Bounds: NewRectangle(10, 10, 120, 28), Label: "Choice",
			ID: 25705, Selected: &selected,
		})
		if activated != inside || selected != wantSelected {
			t.Fatalf("inside=%v: activated=%v selected=%d", inside, activated, selected)
		}
		if inside {
			r.endPopupInput(child)
		}
		r.endPopupInput(parent)
		r.EndFrame()
	}
}

func TestPopupMultiSelectKeyboardOwnership(t *testing.T) {
	for _, inside := range []bool{false, true} {
		r := New(AppConfig{}).(*runtime)
		selected := []int32{1, 0}
		count, anchor := int32(1), int32(0)
		props := MultiSelectListProps{
			Bounds: NewRectangle(10, 10, 120, 56), ID: 25706,
			Items: []string{"Alpha", "Beta"}, ItemCount: 2, Selected: selected,
			SelectedCount: &count, Anchor: &anchor, RowHeight: 28,
		}
		r.QueueKey(KeyDown)
		r.BeginFrame()
		parent := r.beginPopupInput(0, NewRectangle(180, 180, 40, 40))
		child := r.beginPopupInput(1, NewRectangle(190, 190, 20, 20))
		if !inside {
			r.endPopupInput(child)
		}
		r.setFocus(props.ID)
		clicked := r.MultiSelectList(props)
		wantClicked, wantAnchor, wantSelected := int32(-1), int32(0), int32(0)
		if inside {
			wantClicked, wantAnchor, wantSelected = 1, 1, 1
		}
		if clicked != wantClicked || anchor != wantAnchor || selected[1] != wantSelected {
			t.Fatalf("inside=%v: clicked=%d anchor=%d selected=%v", inside, clicked, anchor, selected)
		}
		if inside {
			r.endPopupInput(child)
		}
		r.endPopupInput(parent)
		r.EndFrame()
	}
}

func TestPopupTabBarKeyboardOwnership(t *testing.T) {
	for _, inside := range []bool{false, true} {
		r := New(AppConfig{Width: 240, Height: 120}).(*runtime)
		props := TabBarProps{Bounds: NewRectangle(20, 20, 180, 30), ID: 26132,
			Tabs: []Tab{{Label: "a"}, {Label: "b"}}, Count: 2, SelectedIndex: 0}
		r.QueueKey(KeyRight)
		r.BeginFrame()
		parent := r.beginPopupInput(26100, NewRectangle(10, 10, 220, 100))
		child := r.beginPopupInput(26101, NewRectangle(15, 15, 200, 80))
		if !inside {
			r.endPopupInput(child)
		}
		r.SetFocus(props.ID)
		clicked := r.TabBar(props)
		want := int32(-1)
		if inside {
			want = 1
		}
		if clicked != want {
			t.Fatalf("inside=%v clicked=%d, want %d", inside, clicked, want)
		}
		if inside {
			r.endPopupInput(child)
		}
		r.endPopupInput(parent)
		r.EndFrame()
	}
}

func TestPopupDragKeyboardOwnership(t *testing.T) {
	for _, inside := range []bool{false, true} {
		r := New(AppConfig{}).(*runtime)
		values := []float32{1}
		r.QueueKey(KeyRight)
		r.BeginFrame()
		parent := r.beginPopupInput(0, NewRectangle(180, 180, 40, 40))
		child := r.beginPopupInput(1, NewRectangle(190, 190, 20, 20))
		if !inside {
			r.endPopupInput(child)
		}
		r.setFocus(25707)
		changed := r.DragFloat(DragFloatProps{Bounds: NewRectangle(10, 10, 120, 28), ID: 25707, Values: values, ValueCount: 1, Speed: 1, Min: 0, Max: 10})
		want := float32(1)
		if inside {
			want = 2
		}
		if changed != inside || values[0] != want {
			t.Fatalf("inside=%v: changed=%v value=%v", inside, changed, values[0])
		}
		if inside {
			r.endPopupInput(child)
		}
		r.endPopupInput(parent)
		r.EndFrame()
	}
}

func TestPopupMenuKeyboardOwnership(t *testing.T) {
	items := []MenuItem{{Kind: MenuCommand, Label: "Run", ID: 25710}}
	for _, inside := range []bool{false, true} {
		r := New(AppConfig{}).(*runtime)
		r.QueueKey(KeyEnter)
		r.BeginFrame()
		parent := r.beginPopupInput(0, NewRectangle(180, 180, 40, 40))
		child := r.beginPopupInput(1, NewRectangle(190, 190, 20, 20))
		if !inside {
			r.endPopupInput(child)
		}
		r.setFocus(25711)
		got := r.PopupMenu(25711, 10, 10, items, 1)
		want := int32(0)
		if inside {
			want = 25710
		}
		if got != want {
			t.Fatalf("inside=%v: popup menu activation=%d want=%d", inside, got, want)
		}
		if inside {
			r.endPopupInput(child)
		}
		r.endPopupInput(parent)
		r.EndFrame()
	}
	for _, inside := range []bool{false, true} {
		r := New(AppConfig{}).(*runtime)
		r.QueueKey(KeyEscape)
		r.BeginFrame()
		parent := r.beginPopupInput(0, NewRectangle(180, 180, 40, 40))
		child := r.beginPopupInput(1, NewRectangle(190, 190, 20, 20))
		if !inside {
			r.endPopupInput(child)
		}
		r.setFocus(25711)
		r.PopupMenu(25711, 10, 10, items, 1)
		wantFocus := int32(25711)
		if inside {
			wantFocus = 0
		}
		if r.Focus() != wantFocus {
			t.Fatalf("inside=%v: popup menu Escape focus=%d want=%d",
				inside, r.Focus(), wantFocus)
		}
		if inside {
			r.endPopupInput(child)
		}
		r.endPopupInput(parent)
		r.EndFrame()
	}
}

func TestPopupAcceleratorKeyboardOwnership(t *testing.T) {
	r := New(AppConfig{}).(*runtime)
	r.QueueShortcut(KeyC)
	r.BeginFrame()
	parent := r.beginPopupInput(26000, NewRectangle(10, 10, 120, 100))
	child := r.beginPopupInput(26001, NewRectangle(20, 20, 80, 60))
	copy := Accelerator{Key: KeyC, Ctrl: 1, ID: 91}

	r.endPopupInput(child)
	if got := r.AcceleratorPressed(copy); got != 0 {
		t.Fatalf("parent accelerator behind child = %d, want 0", got)
	}
	child = r.beginPopupInput(26001, NewRectangle(20, 20, 80, 60))
	if got := r.DispatchAccelerators([]Accelerator{{Key: KeyX, Ctrl: 1, ID: 90}, copy}); got != 91 {
		t.Fatalf("top popup accelerator = %d, want 91", got)
	}
	r.endPopupInput(child)
	r.closePopupInput(26001)
	if got := r.AcceleratorPressed(copy); got != 91 {
		t.Fatalf("parent accelerator after child close = %d, want 91", got)
	}
	r.endPopupInput(parent)
	if got := r.AcceleratorPressed(copy); got != 0 {
		t.Fatalf("background accelerator behind parent = %d, want 0", got)
	}
	r.closePopupInput(26000)
	if got := r.AcceleratorPressed(copy); got != 91 {
		t.Fatalf("background accelerator after popup close = %d, want 91", got)
	}
	r.EndFrame()

	r.QueueShortcut(KeyC)
	r.BeginFrame()
	r.BeginDisabled(true)
	if got := r.AcceleratorPressed(copy); got != 0 {
		t.Fatalf("disabled accelerator = %d, want 0", got)
	}
	r.EndDisabled()
	r.EndFrame()
}

func TestPopupCollapsibleKeyboardOwnership(t *testing.T) {
	for _, inside := range []bool{false, true} {
		r := New(AppConfig{}).(*runtime)
		r.QueueKey(KeyRight)
		r.BeginFrame()
		parent := r.beginPopupInput(26100, NewRectangle(10, 10, 120, 100))
		child := r.beginPopupInput(26101, NewRectangle(20, 20, 80, 60))
		if !inside {
			r.endPopupInput(child)
		}
		open := false
		r.setFocus(26110)
		r.Collapsible(CollapsibleProps{Bounds: NewRectangle(20, 20, 80, 28),
			ID: 26110, Label: "Node", Open: &open, Tree: true})
		if open != inside {
			t.Fatalf("inside=%v: popup-owned collapsible open=%v", inside, open)
		}
		if inside {
			r.endPopupInput(child)
		}
		r.endPopupInput(parent)
		r.EndFrame()
	}
}

func TestPopupSelectableTextKeyboardOwnership(t *testing.T) {
	for _, inside := range []bool{false, true} {
		r := New(AppConfig{}).(*runtime)
		r.QueueTap(12, 12)
		r.BeginFrame()
		r.SelectableText("copy me", 10, 10, Text16, WHITE)
		r.EndFrame()
		r.SetClipboardText("seed")
		r.QueueShortcut(KeyC)
		r.BeginFrame()
		parent := r.beginPopupInput(26200, NewRectangle(10, 10, 120, 100))
		child := r.beginPopupInput(26201, NewRectangle(20, 20, 80, 60))
		if !inside {
			r.endPopupInput(child)
		}
		r.SelectableText("copy me", 10, 10, Text16, WHITE)
		want := "seed"
		if inside {
			want = "copy me"
			r.endPopupInput(child)
		}
		if got := r.ClipboardText(); got != want {
			t.Fatalf("inside=%v: selectable copy=%q want %q", inside, got, want)
		}
		r.endPopupInput(parent)
		r.EndFrame()
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

func TestPopupOwnsActiveScalarSliderAndTableResizeDrags(t *testing.T) {
	panel := NewRectangle(20, 20, 120, 100)

	t.Run("new popup preempts background drag", func(t *testing.T) {
		r := New(AppConfig{Width: 260, Height: 180}).(*runtime)
		values := []float32{10}
		props := DragFloatProps{Bounds: NewRectangle(30, 30, 80, 24), ID: 391, Values: values, Speed: 1, Min: 0, Max: 500}
		r.QueueMouseButtonDown(MouseButtonLeft, 40, 40)
		r.BeginFrame()
		r.DragFloat(props)
		r.EndFrame()

		r.QueueMouseMove(80, 40)
		r.BeginFrame()
		owner := r.beginPopupInput(390, panel)
		r.endPopupInput(owner)
		if r.DragFloat(props) || values[0] != 10 || r.drag.active {
			t.Fatalf("background drag survived new popup: value=%g active=%v", values[0], r.drag.active)
		}
		r.EndFrame()
	})

	t.Run("drag value", func(t *testing.T) {
		r := New(AppConfig{Width: 260, Height: 180}).(*runtime)
		values := []float32{10}
		props := DragFloatProps{Bounds: NewRectangle(30, 30, 80, 24), ID: 401, Values: values, Speed: 1, Min: 0, Max: 500}
		r.QueueMouseButtonDown(MouseButtonLeft, 40, 40)
		r.BeginFrame()
		owner := r.beginPopupInput(400, panel)
		r.DragFloat(props)
		r.endPopupInput(owner)
		r.EndFrame()

		r.QueueMouseMove(200, 40)
		r.BeginFrame()
		owner = r.beginPopupInput(400, panel)
		if !r.DragFloat(props) || values[0] != 170 {
			t.Fatalf("popup drag outside bounds value=%g, want 170", values[0])
		}
		r.endPopupInput(owner)
		r.closePopupInput(400)
		before := values[0]
		r.QueueMouseMove(230, 40)
		if r.DragFloat(props) || values[0] != before || r.drag.active {
			t.Fatalf("dismissed popup drag leaked to background: value=%g active=%v", values[0], r.drag.active)
		}
		r.EndFrame()
	})

	t.Run("slider", func(t *testing.T) {
		r := New(AppConfig{Width: 260, Height: 180}).(*runtime)
		values := []float32{0}
		props := SliderFloatProps{Bounds: NewRectangle(30, 30, 80, 24), ID: 411, Values: values, Min: 0, Max: 100}
		r.QueueMouseButtonDown(MouseButtonLeft, 50, 40)
		r.BeginFrame()
		owner := r.beginPopupInput(410, panel)
		r.SliderFloat(props)
		r.endPopupInput(owner)
		r.EndFrame()

		r.QueueMouseMove(90, 40)
		r.BeginFrame()
		owner = r.beginPopupInput(410, panel)
		r.SliderFloat(props)
		r.endPopupInput(owner)
		before := values[0]
		r.closePopupInput(410)
		r.QueueMouseMove(30, 40)
		if r.SliderFloat(props) || values[0] != before || r.slider.active {
			t.Fatalf("dismissed popup slider leaked to background: value=%g active=%v", values[0], r.slider.active)
		}
		r.EndFrame()
	})

	t.Run("splitter", func(t *testing.T) {
		r := New(AppConfig{Width: 260, Height: 180}).(*runtime)
		split := int32(50)
		props := PanedViewProps{Bounds: NewRectangle(30, 30, 100, 80), ID: 416, Vertical: true, Split: &split, MinFirst: 20, MinSecond: 20}
		r.QueueMouseButtonDown(MouseButtonLeft, 80, 40)
		r.BeginFrame()
		owner := r.beginPopupInput(415, panel)
		r.PanedView(props)
		r.endPopupInput(owner)
		r.EndFrame()

		r.QueueMouseMove(110, 40)
		r.BeginFrame()
		owner = r.beginPopupInput(415, panel)
		if r.PanedView(props) == 0 || split != 80 {
			t.Fatalf("popup splitter value=%d, want 80", split)
		}
		r.endPopupInput(owner)
		r.closePopupInput(415)
		before := split
		r.QueueMouseMove(60, 40)
		r.PanedView(props)
		if split != before || r.drag.active {
			t.Fatalf("dismissed popup splitter leaked to background: split=%d active=%v", split, r.drag.active)
		}
		r.EndFrame()
	})

	t.Run("table resize", func(t *testing.T) {
		r := New(AppConfig{Width: 300, Height: 200}).(*runtime)
		widths := []int32{70, 70}
		props := TableViewProps{Bounds: NewRectangle(25, 25, 140, 90), ID: 421,
			Columns: []string{"A", "B"}, Rows: []TableRow{{Cells: []string{"a", "b"}}},
			ColumnWidths: widths, Resizable: true, MinColumnWidth: 32}
		r.QueueMouseButtonDown(MouseButtonLeft, 93, 35)
		r.BeginFrame()
		owner := r.beginPopupInput(420, NewRectangle(20, 20, 160, 110))
		r.TableView(props)
		r.endPopupInput(owner)
		r.EndFrame()

		r.QueueMouseMove(123, 35)
		r.BeginFrame()
		owner = r.beginPopupInput(420, NewRectangle(20, 20, 160, 110))
		if r.TableView(props) == 0 || widths[0] != 100 {
			t.Fatalf("popup table resize width=%d, want 100", widths[0])
		}
		r.endPopupInput(owner)
		r.closePopupInput(420)
		before := widths[0]
		r.QueueMouseMove(153, 35)
		r.TableView(props)
		if widths[0] != before || r.tableResize.active {
			t.Fatalf("dismissed popup resize leaked to background: width=%d active=%v", widths[0], r.tableResize.active)
		}
		r.EndFrame()
	})
}

func TestPopupTableKeyboardOwnership(t *testing.T) {
	for _, inside := range []bool{false, true} {
		r := New(AppConfig{Width: 240, Height: 160}).(*runtime)
		selectedRow, selectedColumn := int32(0), int32(0)
		props := TableViewProps{
			Bounds: NewRectangle(20, 20, 100, 80), ID: 26120,
			Columns: []string{"A"}, Rows: []TableRow{{Cells: []string{"a"}}, {Cells: []string{"b"}}},
			SelectedRow: &selectedRow, SelectedColumn: &selectedColumn,
		}

		r.QueueKey(KeyDown)
		r.BeginFrame()
		parent := r.beginPopupInput(26100, NewRectangle(10, 10, 140, 120))
		child := r.beginPopupInput(26101, NewRectangle(15, 15, 120, 100))
		if !inside {
			r.endPopupInput(child)
		}
		r.SetFocus(26120)
		r.TableView(props)
		want := int32(0)
		if inside {
			want = 1
		}
		if selectedRow != want {
			t.Fatalf("inside=%v selected row=%d, want %d", inside, selectedRow, want)
		}
		if inside {
			r.endPopupInput(child)
		}
		r.endPopupInput(parent)
		r.EndFrame()
	}
}

func TestPopupListBoxKeyboardOwnership(t *testing.T) {
	for _, inside := range []bool{false, true} {
		r := New(AppConfig{Width: 200, Height: 120}).(*runtime)
		selected, offset := int32(0), int32(0)
		props := ListBoxProps{
			Bounds: NewRectangle(20, 20, 100, 48), ID: 26131,
			Items: []string{"a", "b"}, SelectedIndex: &selected,
			ScrollOffset: &offset, RowHeight: 24,
		}
		r.QueueKey(KeyDown)
		r.BeginFrame()
		parent := r.beginPopupInput(26100, NewRectangle(10, 10, 140, 100))
		child := r.beginPopupInput(26101, NewRectangle(15, 15, 120, 80))
		if !inside {
			r.endPopupInput(child)
		}
		r.SetFocus(props.ID)
		r.ListBox(props)
		want := int32(0)
		if inside {
			want = 1
		}
		if selected != want {
			t.Fatalf("inside=%v selected=%d, want %d", inside, selected, want)
		}
		if inside {
			r.endPopupInput(child)
		}
		r.endPopupInput(parent)
		r.EndFrame()
	}
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
