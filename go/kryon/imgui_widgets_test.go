package kryon

import (
	"fmt"
	"testing"
)

func TestNumericInputStateIsolation(t *testing.T) {
	r := New(AppConfig{}).(*runtime)
	states := make([]*numericInputState, 129)
	for i := range states {
		states[i] = r.numericInputState(numericInputKey{widgetID: int32(1 + i*128)}, fmt.Sprintf("edit-%d", i))
		states[i].cursor = int32(i % 8)
		states[i].focused = true
	}
	for i, original := range states {
		state := r.numericInputState(numericInputKey{widgetID: int32(1 + i*128)}, "must not replace focused editing")
		want := fmt.Sprintf("edit-%d", i)
		if state != original || string(state.text[:len(want)]) != want ||
			state.cursor != int32(i%8) || !state.focused {
			t.Fatalf("numeric editor %d lost independent state", i)
		}
	}
}

func TestNumericInputStepLifecycle(t *testing.T) {
	r := New(AppConfig{}).(*runtime)
	values := []int32{10}
	for scenario, want := range []int32{12, 10, 15, 15} {
		x := float32(128)
		if scenario == 1 {
			x = 104
		}
		r.QueueTap(x, 40)
		r.keyDown[340] = scenario == 2
		r.BeginFrame()
		r.Row(RowProps{Bounds: NewRectangle(20, 30, 120, 24)})
		r.BeginDisabled(scenario == 3)
		r.InputInt(InputIntProps{Bounds: NewRectangle(0, 0, 120, 24), ID: 870,
			Values: values, ValueCount: 1, Step: 2, StepFast: 5})
		r.EndDisabled()
		r.End()
		r.EndFrame()
		if values[0] != want {
			t.Fatalf("numeric step scenario %d: got %d, want %d", scenario, values[0], want)
		}
	}
}

func TestNumericInputTypingLifecycle(t *testing.T) {
	r := New(AppConfig{}).(*runtime)
	values := []int32{10}
	for frame := 0; frame < 4; frame++ {
		if frame == 0 {
			r.QueueTap(100, 40)
		}
		if frame == 1 {
			r.QueueText("5")
		}
		if frame == 2 {
			r.QueueText("9")
		}
		r.BeginFrame()
		r.Row(RowProps{Bounds: NewRectangle(20, 30, 120, 24)})
		r.BeginDisabled(frame == 2)
		changed := r.InputInt(InputIntProps{Bounds: NewRectangle(0, 0, 120, 24), ID: 871,
			Values: values, ValueCount: 1})
		r.EndDisabled()
		r.End()
		r.EndFrame()
		want := int32(105)
		if frame == 0 {
			want = 10
		}
		if values[0] != want || changed != (frame == 1) {
			t.Fatalf("typing frame %d: value=%d changed=%v, want %d/%v", frame, values[0], changed, want, frame == 1)
		}
	}
}

func TestNumericInputOriginLayout(t *testing.T) {
	r := New(AppConfig{}).(*runtime)
	r.BeginFrame()
	r.Row(RowProps{Bounds: NewRectangle(0, 0, 220, 24)})
	r.InputInt(InputIntProps{Bounds: NewRectangle(0, 0, 120, 24), ID: 872,
		Values: []int32{10}, ValueCount: 1, Step: 1})
	r.Button(ButtonProps{Bounds: NewRectangle(0, 0, 40, 24), ID: 873, Label: "next"})
	r.End()
	r.EndFrame()
	found := false
	for _, op := range r.FrameOps() {
		if op.ID == 873 {
			found = true
			if op.Bounds.X != 120 {
				t.Fatalf("numeric consumes extra row space: next x=%v", op.Bounds.X)
			}
		}
	}
	if !found {
		t.Fatal("missing next sibling")
	}
}

func TestNumericInputComponentIdentities(t *testing.T) {
	r := New(AppConfig{}).(*runtime)
	seen := map[int32]numericInputKey{}
	for kind := int32(0); kind < 3; kind++ {
		for id := int32(-1); id <= 2; id++ {
			for component := int32(0); component < 33; component++ {
				key := numericInputKey{kind, id, component}
				state := r.numericInputState(key, "0")
				if state != r.numericInputState(key, "0") {
					t.Fatalf("unstable numeric identity: %+v", key)
				}
				for offset := int32(0); offset < 3; offset++ {
					token := state.token + offset
					if previous, exists := seen[token]; exists {
						t.Fatalf("numeric ID collision: %+v and %+v", previous, key)
					}
					seen[token] = key
				}
			}
		}
	}
	r.BeginFrame()
	r.InputInt(InputIntProps{Bounds: NewRectangle(0, 0, 1700, 24), ID: 1,
		Values: make([]int32, 17), ValueCount: 17, Step: 1})
	r.InputInt(InputIntProps{Bounds: NewRectangle(0, 30, 100, 24), ID: 2,
		Values: []int32{0}, ValueCount: 1, Step: 1})
	r.EndFrame()
	ids := map[int32]bool{}
	for _, op := range r.FrameOps() {
		if op.Kind != FrameOpTextField && op.Kind != FrameOpButton {
			continue
		}
		id := op.ID
		if op.Kind == FrameOpTextField {
			id = op.FocusID
		}
		if ids[id] {
			t.Fatalf("duplicate numeric frame operation ID: %d", id)
		}
		ids[id] = true
	}
	if len(ids) != 54 {
		t.Fatalf("got %d numeric field/button IDs, want 54", len(ids))
	}
}

func TestTreeHeaderModes(t *testing.T) {
	r := New(AppConfig{}).(*runtime)
	open := false
	p := CollapsibleProps{Bounds: NewRectangle(10, 10, 180, 32), Label: "Node", Open: &open, Tree: true, Depth: 2, Selected: true, ID: 993}
	for _, mode := range []string{"leaf", "disabled", "enabled"} {
		p.Leaf, p.Disabled = mode == "leaf", mode == "disabled"
		r.QueueTap(60, 20)
		r.BeginFrame()
		changed := r.Collapsible(p)
		r.EndFrame()
		if open != (mode == "enabled") || (changed != 0) != (mode == "enabled") {
			t.Fatalf("%s toggled incorrectly", mode)
		}
		op := r.FrameOps()[0]
		if op.Bounds.X != 50 || op.Bounds.Width != 140 || !op.Selected || op.ID != 993 {
			t.Fatalf("tree header op: %+v", op)
		}
		if mode == "leaf" && op.Text != "•  Node" {
			t.Fatalf("leaf marker: %q", op.Text)
		}
	}
}

func TestCloseableCollapsible(t *testing.T) {
	r := New(AppConfig{}).(*runtime)
	open, visible := false, true
	p := CollapsibleProps{Bounds: NewRectangle(10, 10, 180, 32), Label: "A label long enough to need clipping", Open: &open, Visible: &visible, ID: 9961}
	r.QueueTap(180, 20)
	r.BeginFrame()
	changed := r.Collapsible(p)
	r.EndFrame()
	if changed != 1 || visible || open {
		t.Fatalf("close changed=%d visible=%v open=%v", changed, visible, open)
	}
	ops := r.FrameOps()
	if len(ops) != 2 || ops[0].Kind != FrameOpButton || ops[1].Kind != FrameOpText || ops[1].Text != "×" || !ops[1].Pressed {
		t.Fatalf("closeable collapsible ops: %+v", ops)
	}

	r.QueueTap(20, 20)
	r.BeginFrame()
	if r.Collapsible(p) != 0 || len(r.FrameOps()) != 0 || open {
		t.Fatal("hidden collapsible emitted output or accepted input")
	}
	r.EndFrame()

	visible = true
	p.Disabled = true
	r.QueueTap(180, 20)
	r.BeginFrame()
	changed = r.Collapsible(p)
	r.EndFrame()
	if changed != 0 || !visible || !r.FrameOps()[1].Disabled {
		t.Fatal("disabled collapsible close was active")
	}
}

func TestTreeHeaderKeyboardGates(t *testing.T) {
	r := New(AppConfig{}).(*runtime)
	open := false
	p := CollapsibleProps{Bounds: NewRectangle(10, 10, 180, 32), Open: &open, Tree: true, ID: 994}
	r.SetFocus(994)
	for _, mode := range []string{"leaf", "disabled", "scope", "enabled"} {
		p.Leaf, p.Disabled = mode == "leaf", mode == "disabled"
		r.QueueKey(KeyRight)
		r.BeginFrame()
		r.BeginDisabled(mode == "scope")
		changed := r.Collapsible(p)
		r.EndDisabled()
		r.EndFrame()
		if open != (mode == "enabled") || (changed != 0) != (mode == "enabled") {
			t.Fatalf("%s keyboard gating", mode)
		}
	}
}

func TestComboPopupLifecycle(t *testing.T) {
	r := New(AppConfig{}).(*runtime)
	selected := int32(0)
	p := ComboboxProps{Bounds: NewRectangle(10, 10, 160, 28), ID: 996, Options: []string{"One", "Two"}, SelectedIndex: &selected}
	draw := func(show, scope bool) {
		r.BeginFrame()
		r.BeginDisabled(scope)
		if show {
			r.Combobox(p)
		}
		r.EndDisabled()
		r.EndFrame()
	}
	for _, mode := range []string{"disabled", "scope", "hidden"} {
		p.Disabled = false
		r.QueueTap(20, 20)
		draw(true, false)
		if !r.openDropdowns[p.ID] {
			t.Fatalf("%s: failed to open", mode)
		}
		p.Disabled = mode == "disabled"
		r.QueueTap(20, 75)
		draw(mode != "hidden", mode == "scope")
		if r.openDropdowns[p.ID] || selected != 0 {
			t.Fatalf("%s: popup survived or selection changed", mode)
		}
		p.Disabled = false
		draw(true, false)
		if r.openDropdowns[p.ID] {
			t.Fatalf("%s: popup resurrected", mode)
		}
	}
}

func TestComboOverlayLayerAndCapture(t *testing.T) {
	r := New(AppConfig{}).(*runtime)
	selected := int32(0)
	actions := 0
	draw := func() {
		r.BeginFrame()
		if r.Button(ButtonProps{Bounds: NewRectangle(10, 42, 160, 56), Label: "Behind", ID: 997}) {
			actions++
		}
		r.BeginScroll(NewRectangle(10, 10, 180, 28), 28, nil)
		r.Combobox(ComboboxProps{Bounds: NewRectangle(10, 10, 160, 28), ID: 996, Options: []string{"One", "Two"}, SelectedIndex: &selected})
		r.EndScroll()
		r.Rect(10, 42, 160, 56, RED, BLANK)
		r.EndFrame()
	}
	r.QueueTap(20, 20)
	draw()
	img := RenderFrame(200, 120, r.FrameOps())
	// Sample the unselected row, away from its text and the selection tint.
	got := img.RGBAAt(14, 80)
	want := r.theme().surface
	if got.R != want.R || got.G != want.G || got.B != want.B {
		t.Fatalf("popup behind later paint or clipped: %v want %v", got, want)
	}
	r.QueueTap(20, 80)
	draw()
	if selected != 1 || actions != 0 || r.openDropdowns[996] {
		t.Fatalf("overlay input: selection %d background actions %d", selected, actions)
	}
}

func TestComboDismissal(t *testing.T) {
	r := New(AppConfig{}).(*runtime)
	selected := int32(0)
	p := ComboboxProps{Bounds: NewRectangle(10, 10, 160, 28), ID: 996, Options: []string{"One", "Two"}, SelectedIndex: &selected}
	draw := func() { r.BeginFrame(); r.Combobox(p); r.EndFrame() }
	r.QueueTap(20, 20)
	r.QueueKey(KeyEscape)
	draw()
	if r.openDropdowns[p.ID] {
		t.Fatal("Escape did not override simultaneous opening")
	}
	for _, mode := range []string{"escape", "outside", "empty"} {
		p.Options = []string{"One", "Two"}
		r.QueueTap(20, 20)
		draw()
		if !r.openDropdowns[p.ID] {
			t.Fatalf("%s: failed to open", mode)
		}
		switch mode {
		case "escape":
			r.QueueKey(KeyEscape)
		case "outside":
			r.QueueMouseButtonDown(MouseButtonLeft, 200, 150)
		case "empty":
			p.Options = nil
		}
		draw()
		if r.openDropdowns[p.ID] || selected != 0 {
			t.Fatalf("%s: dismissal failed", mode)
		}
		r.QueueMouseButtonUp(MouseButtonLeft, 200, 150)
		draw()
		r.QueueTap(20, 80)
		draw()
		if selected != 0 {
			t.Fatalf("%s: stale row selected", mode)
		}
	}
}

func TestRotatedTableHeader(t *testing.T) {
	r := New(AppConfig{}).(*runtime)
	p := TableViewProps{Bounds: NewRectangle(10, 10, 140, 150), Columns: []string{"Header"}, Rows: []TableRow{{Cells: []string{"Body"}}}, HeaderHeight: 80, HeaderAngle: -45, RowHeight: 28}
	r.BeginFrame()
	r.TableView(p)
	r.EndFrame()
	var rotated FrameOp
	for _, op := range r.FrameOps() {
		if op.Kind == FrameOpText && op.Row == -1 {
			rotated = op
		}
	}
	if rotated.Rotation != -45 || !rotated.HasClip || rotated.Clip.Height != 80 || !rotated.HasPolygon {
		t.Fatalf("header op: %+v", rotated)
	}
	if cell := TableCellRect(p, 0, 0); cell.Y != 90 {
		t.Fatalf("body rect: %+v", cell)
	}
	img := RenderFrame(180, 180, []FrameOp{rotated})
	count := 0
	for y := 0; y < 180; y++ {
		for x := 0; x < 180; x++ {
			pixel := img.RGBAAt(x, y)
			if pixel.R == 245 && pixel.G == 245 && pixel.B == 245 {
				continue
			}
			if x < 10 || x >= 150 || y < 10 || y >= 90 {
				t.Fatalf("rotated text escaped clip at %d,%d", x, y)
			}
			if !pointInPolygon(float32(x)+0.5, float32(y)+0.5, rotated.Polygon[:]) {
				t.Fatalf("glyph escaped slanted cell at %d,%d", x, y)
			}
			count++
		}
	}
	if count < 20 {
		t.Fatalf("rotated label not rendered: %d pixels", count)
	}
}

func TestCustomTableCellScope(t *testing.T) {
	r := New(AppConfig{}).(*runtime)
	scroll := int32(20)
	p := TableViewProps{Bounds: NewRectangle(10, 10, 200, 90), Columns: []string{"A", "B"}, Rows: []TableRow{{}, {}, {}}, ColumnOrder: []int32{1, 0}, RowHeight: 30, FreezeRows: 1, ScrollOffset: &scroll, CustomCells: true}
	r.QueueTap(120, 75)
	r.BeginFrame()
	r.TableView(p)
	cell := r.BeginTableCell(p, 1, 0)
	if cell.X != 110 || cell.Y != 50 {
		t.Fatalf("reordered scrolling cell: %+v", cell)
	}
	r.Rect(0, 0, 300, 300, RED, BLANK)
	if !r.Button(ButtonProps{Bounds: cell, ID: 1000}) {
		t.Fatal("visible cell child inactive")
	}
	r.EndTableCell()
	p.Disabled = true
	r.BeginTableCell(p, 0, 1)
	if !r.contentDisabled() {
		t.Fatal("cell did not inherit disabled")
	}
	r.EndTableCell()
	if r.contentDisabled() {
		t.Fatal("cell leaked disabled scope")
	}
	r.EndFrame()
	for _, op := range r.FrameOps() {
		if op.Color == RED && (op.Clip.Y != 70 || op.Clip.Height != 10 || op.Clip.X != 110 || op.Clip.Width != 100) {
			t.Fatalf("frozen/column cell clip: %+v", op)
		}
	}
}

func TestListBoxScope(t *testing.T) {
	r := New(AppConfig{}).(*runtime)
	for _, disabled := range []bool{false, true} {
		offset := int32(0)
		r.QueueMouseMove(30, 30)
		r.QueueMouseWheel(-1)
		r.BeginFrame()
		content := r.BeginListBox(ListBoxProps{Bounds: NewRectangle(20, 20, 120, 80), ItemCount: 4, RowHeight: 25, ScrollOffset: &offset, Disabled: disabled})
		want := int32(22)
		if disabled {
			want = 0
		}
		if offset != want || content.Width != 108 || content.Y != 21-float32(want) {
			t.Fatalf("list content: %+v offset %d", content, offset)
		}
		if r.contentDisabled() != disabled {
			t.Fatal("list disabled scope")
		}
		r.EndListBox()
		if r.contentDisabled() {
			t.Fatal("list scope not restored")
		}
		r.EndFrame()
	}
	r.BeginFrame()
	r.BeginScroll(NewRectangle(10, 10, 100, 80), 80, nil)
	r.BeginListBox(ListBoxProps{Bounds: NewRectangle(20, 20, 120, 80), ContentHeight: 200})
	r.Rect(0, 0, 300, 300, RED, BLANK)
	op := r.FrameOps()[len(r.FrameOps())-1]
	if !op.HasClip || op.Clip != NewRectangle(21, 21, 89, 69) {
		t.Fatalf("nested list clip: %+v", op)
	}
	r.EndListBox()
	r.EndScroll()
	r.EndFrame()
}

func TestListBoxKeyboardNavigation(t *testing.T) {
	r := New(AppConfig{Width: 200, Height: 120}).(*runtime)
	selected, offset := int32(0), int32(0)
	props := ListBoxProps{
		Bounds: NewRectangle(20, 20, 120, 48), ID: 26130,
		Items:         []string{"0", "1", "2", "3", "4", "5", "6", "7"},
		SelectedIndex: &selected, ScrollOffset: &offset, RowHeight: 24,
	}
	for _, step := range []struct {
		key, selected, offset int32
	}{{KeyEnd, 7, 144}, {KeyUp, 6, 144}, {KeyHome, 0, 0}} {
		r.SetFocus(props.ID)
		r.QueueKey(step.key)
		r.BeginFrame()
		if changed := r.ListBox(props); changed != 1 {
			t.Fatalf("key %d changed=%d, want 1", step.key, changed)
		}
		r.EndFrame()
		if selected != step.selected || offset != step.offset {
			t.Fatalf("key %d selected=%d offset=%d, want %d,%d", step.key, selected, offset, step.selected, step.offset)
		}
	}
	if ops := r.FrameOps(); len(ops) == 0 || !ops[0].Focused {
		t.Fatal("focused list did not emit a focus presentation")
	}
	props.Disabled = true
	r.QueueKey(KeyEnd)
	r.BeginFrame()
	if changed := r.ListBox(props); changed != 0 || selected != 0 || offset != 0 {
		t.Fatalf("disabled list changed=%d selected=%d offset=%d", changed, selected, offset)
	}
	r.EndFrame()
	props.Disabled = false
	selected = -1
	r.BeginFrame()
	if changed := r.ListBox(props); changed != 0 || selected != -1 {
		t.Fatalf("idle list changed=%d selected=%d, want 0,-1", changed, selected)
	}
	r.EndFrame()
}

func TestCanonicalTextProperties(t *testing.T) {
	r := New(AppConfig{Width: 320, Height: 240}).(*runtime)
	color := Color{R: 20, G: 40, B: 60, A: 255}
	r.BeginFrame()
	r.Text(TextProps{Bounds: NewRectangle(10, 10, 0, 0), Text: "colored", Font: Text16, Color: color, Wrap: TextWrapNone})
	r.Text(TextProps{Bounds: NewRectangle(10, 30, 0, 0), Text: "disabled", Font: Text16, Wrap: TextWrapNone, Disabled: true})
	r.Text(TextProps{Bounds: NewRectangle(10, 50, 48, 60), Text: "one two three four", Font: Text16, Color: color})
	r.LabelText("Status", "Ready", NewRectangle(10, 120, 160, 20), Text16, color)
	r.BulletText("item", NewRectangle(10, 150, 120, 20), Text16, color)
	r.EndFrame()

	ops := r.FrameOps()
	if len(ops) < 8 {
		t.Fatalf("text ops=%d, want at least 8", len(ops))
	}
	if ops[0].Text != "colored" || ops[0].Color != color {
		t.Fatalf("colored Text op=%+v", ops[0])
	}
	if ops[1].Text != "disabled" || ops[1].Color.A >= 255 {
		t.Fatalf("disabled Text op=%+v", ops[1])
	}
	wrapped := 0
	for _, op := range ops {
		if op.Kind == FrameOpText && (op.Text == "one" || op.Text == "two" || op.Text == "three" || op.Text == "four") {
			wrapped++
		}
	}
	if wrapped < 2 {
		t.Fatalf("bounded Text emitted %d wrapped lines", wrapped)
	}
	for _, op := range ops[2 : 2+wrapped] {
		if !op.HasClip || op.Clip != NewRectangle(10, 50, 48, 60) {
			t.Fatalf("bounded Text clip=%+v", op)
		}
	}
}

func TestNativeValueHelpers(t *testing.T) {
	r := New(AppConfig{Width: 320, Height: 240}).(*runtime)
	r.BeginFrame()
	r.ValueBool("Enabled", true, NewRectangle(10, 10, 140, 20), Text14, White)
	r.ValueInt("Count", -7, NewRectangle(10, 35, 140, 20), Text14, White)
	r.ValueUInt("Mask", 42, NewRectangle(10, 60, 140, 20), Text14, White)
	r.ValueFloat("Rate", 1.25, "%.1f Hz", NewRectangle(10, 85, 140, 20), Text14, White)
	r.EndFrame()
	ops := r.FrameOps()
	if len(ops) != 8 {
		t.Fatalf("value helper ops=%d, want 8", len(ops))
	}
	want := []string{"Enabled", "true", "Count", "-7", "Mask", "42", "Rate", "1.2 Hz"}
	for i, text := range want {
		if ops[i].Text != text {
			t.Fatalf("value helper op %d text=%q, want %q", i, ops[i].Text, text)
		}
	}
}

func TestNativePopupAndContextMenus(t *testing.T) {
	r := New(AppConfig{Width: 640, Height: 480}).(*runtime)
	items := []MenuItem{
		{Kind: MenuCommand, Label: "Open", Accelerator: "Ctrl+O", ID: 11},
		{Kind: MenuSeparator},
		{Kind: MenuCheck, Label: "Grid", ID: 12, Checked: true},
		{Kind: MenuCommand, Label: "Disabled", ID: 13, Disabled: true},
	}

	r.QueueTap(20, 20)
	r.BeginFrame()
	if got := r.PopupMenu(100, 10, 10, items, int32(len(items))); got != 11 {
		t.Fatalf("PopupMenu activation=%d, want 11", got)
	}
	r.EndFrame()

	submenu := []MenuItem{{Kind: MenuCommand, Label: "Child", ID: 22}}
	parents := []MenuItem{{Kind: MenuSubmenu, Label: "More", ID: 21, Submenu: submenu, SubmenuCount: 1}}
	r.QueueMouseMove(20, 20)
	r.BeginFrame()
	r.PopupMenu(101, 10, 10, parents, 1)
	r.EndFrame()
	r.QueueTap(200, 20)
	r.BeginFrame()
	if got := r.PopupMenu(101, 10, 10, parents, 1); got != 22 {
		t.Fatalf("submenu activation=%d, want 22", got)
	}
	r.EndFrame()

	open, x, y := int32(0), int32(0), int32(0)
	props := ContextMenuProps{ID: 102, Trigger: NewRectangle(40, 40, 100, 80), Items: items, ItemCount: int32(len(items)), Open: &open, X: &x, Y: &y}
	r.QueueMouseButtonUp(MouseButtonRight, 50, 50)
	r.BeginFrame()
	if got := r.ContextMenu(props); got != 0 || open != 1 || x != 50 || y != 50 {
		t.Fatalf("ContextMenu open result=%d open=%d pos=(%d,%d)", got, open, x, y)
	}
	r.EndFrame()
	r.QueueTap(60, 60)
	r.BeginFrame()
	if got := r.ContextMenu(props); got != 11 || open != 0 {
		t.Fatalf("ContextMenu activation=%d open=%d, want 11/0", got, open)
	}
	r.EndFrame()
}

func TestMenuKeyboardNavigation(t *testing.T) {
	r := New(AppConfig{Width: 640, Height: 480}).(*runtime)
	child := []MenuItem{{Kind: MenuCommand, Label: "Child", ID: 23}}
	file := []MenuItem{
		{Kind: MenuCommand, Label: "Open", ID: 21},
		{Kind: MenuSeparator},
		{Kind: MenuCommand, Label: "Disabled", ID: 22, Disabled: true},
		{Kind: MenuSubmenu, Label: "More", ID: 24, Submenu: child, SubmenuCount: 1},
	}
	edit := []MenuItem{{Kind: MenuCommand, Label: "Copy", ID: 31}}
	menus := []Menu{{Label: "File", Items: file, ItemCount: int32(len(file))}, {Label: "Edit", Items: edit, ItemCount: 1}}
	open := int32(-1)
	draw := func(key int32) MenuBarResult {
		r.QueueKey(key)
		r.BeginFrame()
		result := r.MenuBar(300, NewRectangle(0, 0, 360, 30), menus, &open)
		r.EndFrame()
		return result
	}

	r.SetFocus(300)
	if got := draw(KeyDown); got.OpenIndex != 0 || open != 0 {
		t.Fatalf("Down opens focused menu: result=%+v open=%d", got, open)
	}
	if got := draw(KeyEnd); got.ActivatedID != 0 {
		t.Fatalf("End activated menu item: %+v", got)
	}
	if got := draw(KeyRight); got.ActivatedID != 0 {
		t.Fatalf("Right activated submenu: %+v", got)
	}
	if got := draw(KeyEnter); got.ActivatedID != 23 || open != -1 {
		t.Fatalf("submenu Enter result=%+v open=%d, want 23/-1", got, open)
	}

	r.SetFocus(300)
	draw(KeyRight)
	if got := draw(KeyDown); got.OpenIndex != 1 || open != 1 {
		t.Fatalf("Right then Down opens next menu: result=%+v open=%d", got, open)
	}
	if got := draw(KeyEnter); got.ActivatedID != 31 || open != -1 {
		t.Fatalf("second menu Enter result=%+v open=%d, want 31/-1", got, open)
	}

	r.SetFocus(300)
	draw(KeyDown)
	draw(KeyEscape)
	if open != -1 {
		t.Fatalf("Escape left menu open at %d", open)
	}
}

func TestContextMenuKeyboardNavigationAndOwnership(t *testing.T) {
	r := New(AppConfig{Width: 640, Height: 480}).(*runtime)
	items := []MenuItem{
		{Kind: MenuCommand, Label: "Disabled", ID: 41, Disabled: true},
		{Kind: MenuSeparator},
		{Kind: MenuCommand, Label: "Run", ID: 42},
	}
	open, x, y := int32(0), int32(20), int32(20)
	props := ContextMenuProps{ID: 400, Trigger: NewRectangle(10, 10, 100, 50), Items: items, ItemCount: int32(len(items)), Open: &open, X: &x, Y: &y}

	r.QueueMouseButtonUp(MouseButtonRight, 20, 20)
	r.BeginFrame()
	r.ContextMenu(props)
	r.EndFrame()
	if open != 1 || r.Focus() != 400 {
		t.Fatalf("context open/focus=%d/%d, want 1/400", open, r.Focus())
	}
	r.QueueKey(KeyEnter)
	r.BeginFrame()
	got := r.ContextMenu(props)
	r.EndFrame()
	if got != 42 || open != 0 {
		t.Fatalf("context Enter=%d open=%d, want 42/0", got, open)
	}

	open = 1
	r.SetFocus(999)
	r.QueueKey(KeyEnter)
	r.BeginFrame()
	got = r.ContextMenu(props)
	r.EndFrame()
	if got != 0 || open != 1 {
		t.Fatalf("unfocused context consumed Enter: got=%d open=%d", got, open)
	}
	r.SetFocus(400)
	r.QueueKey(KeyEscape)
	r.BeginFrame()
	r.ContextMenu(props)
	r.EndFrame()
	if open != 0 {
		t.Fatalf("Escape left context menu open")
	}
}

func TestCollapsibleComposesInteractiveChildren(t *testing.T) {
	r := New(AppConfig{}).(*runtime)
	open := false
	actions := 0
	draw := func() {
		r.BeginFrame()
		r.Column(ColumnProps{Bounds: NewRectangle(10, 10, 200, 300), Gap: 4})
		r.Collapsible(CollapsibleProps{Bounds: NewRectangle(0, 0, 180, 200), Label: "Details", Open: &open})
		if open {
			if r.Button(ButtonProps{Bounds: NewRectangle(0, 0, 100, 28), Label: "Child", ID: 983}) {
				actions++
			}
		}
		r.End()
		r.EndFrame()
	}
	r.QueueTap(20, 20)
	draw()
	if !open {
		t.Fatal("header did not open")
	}
	ops := r.FrameOps()
	if len(ops) != 4 || ops[2].Bounds.Y != 46 {
		t.Fatalf("child should follow the 32px header and 4px gap: %+v", ops)
	}
	r.QueueTap(20, 50)
	draw()
	if actions != 1 {
		t.Fatalf("child actions=%d, want 1", actions)
	}
	r.QueueTap(20, 20)
	draw()
	if open || len(r.FrameOps()) != 3 {
		t.Fatal("closed header still rendered its child")
	}
	r.QueueTap(20, 50)
	draw()
	if actions != 1 {
		t.Fatal("closed child activated")
	}
}

func TestNativeImGuiWidgetSlice(t *testing.T) {
	r := New(AppConfig{Width: 640, Height: 480}).(*runtime)

	r.QueueTap(20, 20)
	r.BeginFrame()
	if got := r.Radio(RadioButtonProps{Bounds: NewRectangle(8, 8, 140, 28), Label: "Choice", ID: 17}); got != 17 {
		t.Fatalf("Radio click = %d, want 17", got)
	}
	r.EndFrame()

	value := int32(4)
	spin := SpinboxProps{Bounds: NewRectangle(10, 10, 120, 30), ID: 3, Min: 0, Max: 5, Step: 2, Value: &value}
	r.QueueTap(118, 20)
	r.BeginFrame()
	if !r.Spinbox(spin) || value != 5 {
		t.Fatalf("Spinbox value=%d, want 5", value)
	}
	r.EndFrame()

	selected := int32(0)
	combo := ComboboxProps{Bounds: NewRectangle(10, 10, 120, 24), ID: 44, Options: []string{"A", "B", "C"}, SelectedIndex: &selected}
	r.QueueTap(20, 20)
	r.BeginFrame()
	r.Combobox(combo)
	r.EndFrame()
	r.QueueTap(20, 74)
	r.BeginFrame()
	if !r.Combobox(combo) || selected != 1 {
		t.Fatalf("Combobox selected=%d, want 1", selected)
	}
	r.EndFrame()

	selected = 0
	tabs := NotebookProps{Bounds: NewRectangle(10, 10, 300, 120), Tabs: []string{"One", "Two"}, SelectedIndex: &selected}
	r.QueueTap(float32(10+runtimeTextWidth("One", Text16)+38), 20)
	r.BeginFrame()
	if r.Notebook(tabs) != 1 || selected != 1 {
		t.Fatalf("Notebook selected=%d, want 1", selected)
	}
	r.EndFrame()

	open := false
	r.QueueTap(20, 20)
	r.BeginFrame()
	if r.Collapsible(CollapsibleProps{Bounds: NewRectangle(10, 10, 180, 80), Label: "Details", Open: &open}) != 1 || !open {
		t.Fatal("Collapsible did not open")
	}
	r.EndFrame()

	split := int32(50)
	r.QueueMouseButtonDown(MouseButtonLeft, 51, 40)
	r.BeginFrame()
	if r.PanedView(PanedViewProps{Bounds: NewRectangle(0, 0, 200, 100), ID: 9, Vertical: true, Split: &split, MinFirst: 20, MinSecond: 20}) != 1 || split != 51 {
		t.Fatalf("PanedView split=%d, want 51", split)
	}
	r.LabelFrame(LabelFrameProps{Bounds: NewRectangle(10, 120, 180, 80), Title: "Group"})
	r.EndFrame()

	color := Color{R: 10, G: 20, B: 30, A: 255}
	r.QueueTap(75, 30)
	r.BeginFrame()
	if !r.ColorPicker(NewRectangle(10, 10, 120, 160), &color) || color.R <= 10 || color.G != 20 || color.B != 30 || color.A != 255 {
		t.Fatalf("ColorPicker color=%+v", color)
	}
	r.EndFrame()
}

func TestNativeCollectionAndDisplayWidgets(t *testing.T) {
	r := New(AppConfig{Width: 640, Height: 480}).(*runtime)

	t.Run("canvas grid", func(t *testing.T) {
		r.BeginFrame()
		r.CanvasGrid(NewRectangle(10, 20, 25, 25), 10, Color{R: 1, G: 2, B: 3, A: 255})
		lines := 0
		for _, op := range r.FrameOps() {
			if op.Kind == FrameOpLine {
				lines++
			}
		}
		if lines != 6 {
			t.Fatalf("CanvasGrid line ops=%d, want 6", lines)
		}
		r.EndFrame()
	})

	t.Run("plots", func(t *testing.T) {
		values := []float32{0, 0.25, 1, 0.5}
		r.BeginFrame()
		r.PlotLines(PlotProps{Bounds: NewRectangle(10, 10, 120, 60), Label: "Lines", Values: values, ScaleMin: 0, ScaleMax: 1})
		lines := 0
		for _, op := range r.FrameOps() {
			if op.Kind == FrameOpLine {
				lines++
			}
		}
		if lines != 3 {
			t.Fatalf("PlotLines segments=%d, want 3", lines)
		}
		r.EndFrame()

		r.BeginFrame()
		r.PlotHistogram(PlotProps{Bounds: NewRectangle(10, 10, 120, 60), Label: "Bars", Values: values, Offset: 1})
		bars := 0
		for _, op := range r.FrameOps() {
			if op.Kind == FrameOpRect && op.Color == r.theme().buttonHover {
				bars++
			}
		}
		if bars != 4 {
			t.Fatalf("PlotHistogram bars=%d, want 4", bars)
		}
		r.EndFrame()
	})

	t.Run("selectable text copy", func(t *testing.T) {
		r.QueueTap(12, 12)
		r.BeginFrame()
		r.SelectableText("copy me", 10, 10, Text16, Color{R: 255, G: 255, B: 255, A: 255})
		if ops := r.FrameOps(); len(ops) != 1 || !ops[0].Selected {
			t.Fatalf("SelectableText ops=%+v", ops)
		}
		r.EndFrame()
		r.QueueShortcut(KeyC)
		r.BeginFrame()
		r.SelectableText("copy me", 10, 10, Text16, Color{R: 255, G: 255, B: 255, A: 255})
		if r.ClipboardText() != "copy me" {
			t.Fatalf("clipboard=%q, want copy me", r.ClipboardText())
		}
		r.EndFrame()
	})

	t.Run("tree selection", func(t *testing.T) {
		selected := int32(-1)
		items := []UITreeItem{{Label: "Root", ID: 1, Expanded: 1}, {Label: "Leaf", Depth: 1, ID: 2, Selectable: 1}}
		r.QueueTap(40, 45)
		r.BeginFrame()
		changed := r.TreeView(TreeViewProps{Bounds: NewRectangle(10, 10, 180, 80), ID: 7, Items: items, SelectedID: &selected, RowHeight: 28})
		if changed != 1 || selected != 2 {
			t.Fatalf("TreeView changed=%d selected=%d, want 1/2", changed, selected)
		}
		r.EndFrame()
	})

	t.Run("source view", func(t *testing.T) {
		sx, sy := int32(0), int32(0)
		r.BeginFrame()
		scrollable := r.SourceView(SourceViewProps{Bounds: NewRectangle(0, 0, 100, 52), Text: "first\nsecond\nthird\nfourth", ScrollX: &sx, ScrollY: &sy, FontSize: Text14, LineHeight: 18, ShowLineNumbers: true})
		if scrollable != 1 {
			t.Fatal("SourceView should report scrollable content")
		}
		textOps := 0
		for _, op := range r.FrameOps() {
			if op.Kind == FrameOpText {
				textOps++
			}
		}
		if textOps < 2 {
			t.Fatalf("SourceView text ops=%d, want visible line and number", textOps)
		}
		r.EndFrame()
	})
}

func TestNativeDialogs(t *testing.T) {
	r := New(AppConfig{Width: 640, Height: 480}).(*runtime)

	r.QueueTap(460, 300)
	r.BeginFrame()
	if got := r.MessageDialog(MessageDialogProps{Title: "Notice", Message: "Saved"}); got != 1 {
		t.Fatalf("MessageDialog result=%d, want 1", got)
	}
	r.EndFrame()

	r.QueueTap(460, 300)
	r.BeginFrame()
	if got := r.ConfirmDialog(ConfirmDialogProps{Title: "Delete", Message: "Continue?"}); got != 2 {
		t.Fatalf("ConfirmDialog result=%d, want 2", got)
	}
	r.EndFrame()

	buf := make([]byte, 32)
	cursor := int32(0)
	focused := true
	r.SetFocus(7301)
	r.QueueText("name")
	r.QueueKey(KeyEnter)
	r.BeginFrame()
	if got := r.PromptDialog(PromptDialogProps{Title: "Name", Text: buf, Cursor: &cursor, Focused: &focused}); got != 2 || CString(buf) != "name" {
		t.Fatalf("PromptDialog result=%d text=%q, want 2/name", got, CString(buf))
	}
	r.EndFrame()
}

func TestNativeNavigationAndFeedback(t *testing.T) {
	r := New(AppConfig{Width: 640, Height: 480}).(*runtime)
	r.QueueTap(100, 450)
	r.BeginFrame()
	r.TitleBar("Workspace", 44)
	r.TopNav(TopNavProps{ID: 10, Y: 44, Width: 300, Height: 40, Title: "Project"})
	r.BottomNav(BottomNavProps{ViewWidth: 640, ViewHeight: 480, Height: 60, Count: 2, Items: []BottomNavItem{{Route: 1, Label: "Home", Active: true}, {Route: 2, Label: "Settings"}}})
	r.ShowToastFor("Updated", 1)
	r.EndFrame()

	pressedNav, sawTitle, sawToast := false, false, false
	for _, op := range r.FrameOps() {
		pressedNav = pressedNav || (op.ID == 1 && op.Pressed)
		sawTitle = sawTitle || op.Text == "Workspace"
		sawToast = sawToast || op.Text == "Updated"
	}
	if !pressedNav || !sawTitle || !sawToast {
		t.Fatalf("navigation/feedback ops missing: pressed=%v title=%v toast=%v", pressedNav, sawTitle, sawToast)
	}
}

func TestScalarGestureCancelledWhenDisabled(t *testing.T) {
	for _, slider := range []bool{false, true} {
		for _, scope := range []bool{false, true} {
			t.Run(fmt.Sprintf("slider=%v/scope=%v", slider, scope), func(t *testing.T) {
				r := New(AppConfig{}).(*runtime)
				values := []float32{25}
				draw := func(disabled bool) {
					r.BeginFrame()
					if scope {
						r.BeginDisabled(disabled)
					}
					if slider {
						r.SliderFloat(SliderFloatProps{Bounds: NewRectangle(10, 10, 100, 24), ID: 981, Values: values, Min: 0, Max: 100, Disabled: disabled && !scope})
					} else {
						r.DragFloat(DragFloatProps{Bounds: NewRectangle(10, 10, 100, 24), ID: 982, Values: values, Speed: 1, Min: 0, Max: 100, Disabled: disabled && !scope})
					}
					if scope {
						r.EndDisabled()
					}
					r.EndFrame()
				}
				r.QueueMouseButtonDown(MouseButtonLeft, 35, 20)
				draw(false)
				before := values[0]
				r.QueueMouseMove(65, 20)
				draw(true)
				if values[0] != before {
					t.Fatalf("disabled gesture changed value: %g -> %g", before, values[0])
				}
				r.QueueMouseMove(85, 20)
				draw(false)
				if values[0] != before {
					t.Fatalf("cancelled gesture resumed without a press: %g -> %g", before, values[0])
				}
			})
		}
	}
}

func TestNativeDragScalars(t *testing.T) {
	r := New(AppConfig{Width: 640, Height: 480}).(*runtime)
	floats := []float32{1, 2}
	floatProps := DragFloatProps{Bounds: NewRectangle(10, 10, 200, 30), ID: 50, Label: "Position", Values: floats, ValueCount: 2, Speed: 0.1, Min: 0, Max: 10}
	r.QueueMouseButtonDown(MouseButtonLeft, 20, 20)
	r.BeginFrame()
	r.DragFloat(floatProps)
	r.EndFrame()
	r.QueueMouseMove(40, 20)
	r.BeginFrame()
	if !r.DragFloat(floatProps) || floats[0] != 3 || floats[1] != 2 {
		t.Fatalf("DragFloat values=%v, want [3 2]", floats)
	}
	r.EndFrame()
	r.QueueMouseButtonUp(MouseButtonLeft, 40, 20)
	r.BeginFrame()
	r.DragFloat(floatProps)
	r.EndFrame()

	ints := []int32{3, 4}
	intProps := DragIntProps{Bounds: NewRectangle(10, 50, 200, 30), ID: 51, Label: "Size", Values: ints, ValueCount: 2, Speed: 0.5, Min: 0, Max: 10}
	r.QueueMouseButtonDown(MouseButtonLeft, 120, 60)
	r.BeginFrame()
	r.DragInt(intProps)
	r.EndFrame()
	r.QueueMouseMove(130, 60)
	r.BeginFrame()
	if !r.DragInt(intProps) || ints[0] != 3 || ints[1] != 9 {
		t.Fatalf("DragInt values=%v, want [3 9]", ints)
	}
	r.EndFrame()
}

func TestNumericDragAndSliderCtrlClickEditing(t *testing.T) {
	r := New(AppConfig{Width: 480, Height: 240}).(*runtime)
	floats := []float32{2.5}
	drag := DragFloatProps{Bounds: NewRectangle(10, 10, 120, 30), ID: 520,
		Values: floats, ValueCount: 1, Speed: 0.1, Min: 0, Max: 10}

	r.QueueKey(KeyLeftControl)
	r.QueueTap(40, 20)
	r.BeginFrame()
	if r.DragFloat(drag) || floats[0] != 2.5 {
		t.Fatalf("Ctrl-click changed drag value before editing: %v", floats)
	}
	r.EndFrame()
	foundEditor := false
	for _, op := range r.FrameOps() {
		if op.Kind == FrameOpTextField && op.FocusID == 520 && op.Focused {
			foundEditor = true
		}
	}
	if !foundEditor {
		t.Fatal("Ctrl-click did not replace drag cell with a focused editor")
	}

	r.QueueShortcut(KeyA)
	r.QueueText("7.25")
	r.BeginFrame()
	if !r.DragFloat(drag) || floats[0] != 7.25 {
		t.Fatalf("edited drag value=%v, want 7.25", floats)
	}
	r.EndFrame()
	r.QueueKey(KeyEnter)
	r.BeginFrame()
	r.DragFloat(drag)
	r.EndFrame()
	if r.numericInputs[numericInputKey{kind: numericEditDragFloat, widgetID: 520}].focused {
		t.Fatal("Enter did not finish drag keyboard entry")
	}

	ints := []int32{4}
	slider := SliderIntProps{Bounds: NewRectangle(10, 60, 120, 30), ID: 521,
		Values: ints, ValueCount: 1, Min: 0, Max: 10}
	r.QueueKey(KeyLeftControl)
	r.QueueTap(40, 70)
	r.BeginFrame()
	r.SliderInt(slider)
	r.EndFrame()
	r.QueueShortcut(KeyA)
	r.QueueText("19")
	r.BeginFrame()
	if !r.SliderInt(slider) || ints[0] != 19 {
		t.Fatalf("temporary slider input should be unclamped: %v", ints)
	}
	r.EndFrame()

	disabled := []int32{3}
	r.QueueKey(KeyLeftControl)
	r.QueueTap(40, 110)
	r.BeginFrame()
	r.DragInt(DragIntProps{Bounds: NewRectangle(10, 100, 120, 30), ID: 522,
		Values: disabled, ValueCount: 1, Min: 0, Max: 10, Disabled: true})
	r.EndFrame()
	state := r.numericInputs[numericInputKey{kind: numericEditDragInt, widgetID: 522}]
	if state != nil && state.focused {
		t.Fatal("disabled drag accepted Ctrl-click keyboard entry")
	}
}

func TestNativeDragKeyboardNavigation(t *testing.T) {
	r := New(AppConfig{Width: 480, Height: 240}).(*runtime)
	floats := []float32{2, 5}
	ints := []int32{2, 5}
	floatProps := DragFloatProps{Bounds: NewRectangle(10, 10, 200, 30), ID: 70, Values: floats, ValueCount: 2, Speed: 0.25, Min: 0, Max: 10}
	intProps := DragIntProps{Bounds: NewRectangle(10, 50, 200, 30), ID: 71, Values: ints, ValueCount: 2, Speed: 2, Min: 0, Max: 10}
	draw := func() {
		r.BeginFrame()
		r.DragFloat(floatProps)
		r.DragInt(intProps)
		r.EndFrame()
	}
	draw()
	r.SetFocus(70)
	r.QueueKey(KeyRight)
	draw()
	if floats[0] != 2.25 {
		t.Fatalf("DragFloat Right=%v, want 2.25", floats[0])
	}
	foundFocus := false
	for _, op := range r.ops {
		if op.ID == 70 && op.Row == 0 && op.Focused && op.BorderColor == r.theme().focus {
			foundFocus = true
		}
	}
	if !foundFocus {
		t.Fatal("focused drag component lacks focus presentation")
	}
	r.QueueShiftKey(KeyRight)
	draw()
	if floats[0] != 4.75 {
		t.Fatalf("DragFloat Shift+Right=%v, want 4.75", floats[0])
	}
	r.QueueKey(KeyTab)
	draw()
	if r.Focus() == 70 {
		t.Fatal("DragFloat Tab did not reach second component")
	}
	r.QueueKey(KeyHome)
	draw()
	if floats[1] != 0 {
		t.Fatalf("DragFloat Home=%v, want 0", floats[1])
	}
	r.SetFocus(71)
	r.QueueKey(KeyRight)
	draw()
	if ints[0] != 4 {
		t.Fatalf("DragInt Right=%d, want 4", ints[0])
	}
	r.QueueKey(KeyTab)
	draw()
	r.QueueKey(KeyLeft)
	draw()
	if ints[1] != 3 {
		t.Fatalf("DragInt second Left=%d, want 3", ints[1])
	}

	floatMin, floatMax := float32(2), float32(8)
	intMin, intMax := int32(2), int32(8)
	floatRange := DragFloatRange2Props{Bounds: NewRectangle(240, 10, 200, 30), ID: 72, CurrentMin: &floatMin, CurrentMax: &floatMax, Speed: 1, Min: 0, Max: 10}
	intRange := DragIntRange2Props{Bounds: NewRectangle(240, 50, 200, 30), ID: 73, CurrentMin: &intMin, CurrentMax: &intMax, Speed: 2, Min: 0, Max: 10}
	drawRanges := func(disabled bool) {
		r.BeginFrame()
		r.BeginDisabled(disabled)
		r.DragFloatRange2(floatRange)
		r.DragIntRange2(intRange)
		r.EndDisabled()
		r.EndFrame()
	}
	drawRanges(false)
	r.SetFocus(72)
	r.QueueKey(KeyRight)
	drawRanges(false)
	if floatMin != 3 {
		t.Fatalf("DragFloatRange2 min Right=%v, want 3", floatMin)
	}
	r.QueueKey(KeyTab)
	drawRanges(false)
	r.QueueKey(KeyLeft)
	drawRanges(false)
	if floatMax != 7 {
		t.Fatalf("DragFloatRange2 max Left=%v, want 7", floatMax)
	}
	r.SetFocus(73)
	r.QueueKey(KeyRight)
	drawRanges(false)
	if intMin != 4 {
		t.Fatalf("DragIntRange2 min Right=%d, want 4", intMin)
	}
	r.QueueKey(KeyTab)
	drawRanges(false)
	r.QueueKey(KeyLeft)
	drawRanges(false)
	if intMax != 6 {
		t.Fatalf("DragIntRange2 max Left=%d, want 6", intMax)
	}
	r.SetFocus(72)
	r.QueueKey(KeyRight)
	drawRanges(true)
	if floatMin != 3 {
		t.Fatalf("disabled DragFloatRange2 changed min to %v", floatMin)
	}
}

func TestNativeSliders(t *testing.T) {
	r := New(AppConfig{Width: 640, Height: 480}).(*runtime)
	floats := []float32{0, 2}
	r.QueueMouseButtonDown(MouseButtonLeft, 60, 20)
	r.BeginFrame()
	if !r.SliderFloat(SliderFloatProps{Bounds: NewRectangle(10, 10, 100, 30), ID: 60, Values: floats, ValueCount: 1, Min: 0, Max: 10}) || floats[0] != 5 {
		t.Fatalf("SliderFloat values=%v, want [5 2]", floats)
	}
	r.EndFrame()
	r.QueueMouseButtonUp(MouseButtonLeft, 60, 20)
	r.BeginFrame()
	r.SliderFloat(SliderFloatProps{Bounds: NewRectangle(10, 10, 100, 30), ID: 60, Values: floats, ValueCount: 1, Min: 0, Max: 10})
	r.EndFrame()

	ints := []int32{0}
	r.QueueMouseButtonDown(MouseButtonLeft, 130, 25)
	r.BeginFrame()
	if !r.VSliderInt(SliderIntProps{Bounds: NewRectangle(120, 10, 30, 100), ID: 61, Values: ints, Min: 0, Max: 10}) || ints[0] != 9 {
		t.Fatalf("VSliderInt value=%d, want 9", ints[0])
	}
	r.EndFrame()
	r.QueueMouseButtonUp(MouseButtonLeft, 130, 25)
	r.BeginFrame()
	r.VSliderInt(SliderIntProps{Bounds: NewRectangle(120, 10, 30, 100), ID: 61, Values: ints, Min: 0, Max: 10})
	r.EndFrame()

	angle := float32(0)
	r.QueueMouseButtonDown(MouseButtonLeft, 75, 140)
	r.BeginFrame()
	if !r.SliderAngle(SliderAngleProps{Bounds: NewRectangle(10, 130, 100, 30), ID: 62, Value: &angle, MinDegrees: -180, MaxDegrees: 180}) || angle < 0.94 || angle > 0.95 {
		t.Fatalf("SliderAngle radians=%f, want about 0.942", angle)
	}
	r.EndFrame()
}

func TestNativeSliderKeyboardNavigation(t *testing.T) {
	r := New(AppConfig{Width: 640, Height: 480}).(*runtime)
	floats := []float32{0.25, 0.75}
	floatProps := SliderFloatProps{Bounds: NewRectangle(10, 10, 200, 30), ID: 600,
		Values: floats, ValueCount: 2, Min: 0, Max: 1}

	r.BeginFrame()
	r.SliderFloat(floatProps)
	r.EndFrame()
	r.SetFocus(600)
	r.QueueKey(KeyRight)
	r.BeginFrame()
	if !r.SliderFloat(floatProps) || floats[0] < 0.2599 || floats[0] > 0.2601 {
		t.Fatalf("horizontal slider Right values=%v, want first value 0.26", floats)
	}
	r.EndFrame()
	focusedPaint := false
	for _, op := range r.FrameOps() {
		if op.Kind == FrameOpRect && op.ID == 600 && op.Row == 0 && op.Focused && op.BorderColor == r.theme().focus {
			focusedPaint = true
		}
	}
	if !focusedPaint {
		t.Fatal("focused slider component lacks focus paint")
	}
	r.QueueShiftKey(KeyRight)
	r.BeginFrame()
	r.SliderFloat(floatProps)
	r.EndFrame()
	if floats[0] < 0.3599 || floats[0] > 0.3601 {
		t.Fatalf("Shift slider value=%v, want 0.36", floats[0])
	}
	r.QueueKey(KeyLeftAlt)
	r.QueueKey(KeyRight)
	r.BeginFrame()
	r.SliderFloat(floatProps)
	r.EndFrame()
	if floats[0] < 0.3609 || floats[0] > 0.3611 {
		t.Fatalf("Alt slider value=%v, want 0.361", floats[0])
	}

	r.QueueKey(KeyTab)
	r.BeginFrame()
	r.SliderFloat(floatProps)
	r.EndFrame()
	secondFocus := sliderFocusID(600, 1, false)
	if r.Focus() != secondFocus {
		t.Fatalf("slider component Tab focus=%d, want %d", r.Focus(), secondFocus)
	}
	r.QueueKey(KeyLeft)
	r.BeginFrame()
	if !r.SliderFloat(floatProps) || floats[1] < 0.7399 || floats[1] > 0.7401 {
		t.Fatalf("second slider component Left values=%v, want second value 0.74", floats)
	}
	r.EndFrame()

	ints := []int32{5}
	intProps := SliderIntProps{Bounds: NewRectangle(10, 60, 30, 120), ID: 601,
		Values: ints, ValueCount: 1, Min: 0, Max: 10}
	r.SetFocus(601)
	for _, step := range []struct {
		key  int32
		want int32
	}{{KeyUp, 6}, {KeyDown, 5}, {KeyHome, 0}, {KeyEnd, 10}} {
		r.QueueKey(step.key)
		r.BeginFrame()
		if !r.VSliderInt(intProps) || ints[0] != step.want {
			t.Fatalf("vertical slider key %d value=%d, want %d", step.key, ints[0], step.want)
		}
		r.EndFrame()
	}

	intProps.Disabled = true
	r.SetFocus(601)
	r.QueueKey(KeyLeft)
	r.BeginFrame()
	if r.VSliderInt(intProps) || ints[0] != 10 {
		t.Fatalf("disabled slider accepted keyboard input: value=%d", ints[0])
	}
	r.EndFrame()
}

func TestNativeDragRangesKeepOrderedEndpoints(t *testing.T) {
	r := New(AppConfig{Width: 640, Height: 480}).(*runtime)
	floatMin, floatMax := float32(2), float32(4)
	floatProps := DragFloatRange2Props{Bounds: NewRectangle(10, 10, 200, 30), ID: 63, Label: "Float range", CurrentMin: &floatMin, CurrentMax: &floatMax, Speed: 1, Min: 0, Max: 10, Format: "%.1f", FormatMax: "max %.1f"}
	r.QueueMouseButtonDown(MouseButtonLeft, 20, 20)
	r.BeginFrame()
	r.DragFloatRange2(floatProps)
	r.EndFrame()
	r.QueueMouseMove(80, 20)
	r.BeginFrame()
	if !r.DragFloatRange2(floatProps) || floatMin != floatMax {
		t.Fatalf("DragFloatRange2 range=[%v,%v], want ordered endpoints clamped together", floatMin, floatMax)
	}
	r.EndFrame()
	r.QueueMouseButtonUp(MouseButtonLeft, 80, 20)
	r.BeginFrame()
	r.DragFloatRange2(floatProps)
	r.EndFrame()

	intMin, intMax := int32(2), int32(8)
	intProps := DragIntRange2Props{Bounds: NewRectangle(120, 50, 200, 30), ID: 64, Label: "Int range", CurrentMin: &intMin, CurrentMax: &intMax, Speed: 1, Min: 0, Max: 10, FormatMax: "max %d"}
	r.QueueMouseButtonDown(MouseButtonLeft, 250, 60)
	r.BeginFrame()
	r.DragIntRange2(intProps)
	r.EndFrame()
	r.QueueMouseMove(200, 60)
	r.BeginFrame()
	if !r.DragIntRange2(intProps) || intMax != intMin {
		t.Fatalf("DragIntRange2 range=[%d,%d], want ordered endpoints clamped together", intMin, intMax)
	}
	r.EndFrame()
}

func TestNativeNumericInputs(t *testing.T) {
	r := New(AppConfig{Width: 640, Height: 480}).(*runtime)
	floats := []float32{1.25, 2.5}
	floatProps := InputFloatProps{Bounds: NewRectangle(10, 10, 200, 30), ID: 70, Values: floats, ValueCount: 2}
	r.QueueTap(30, 20)
	r.BeginFrame()
	r.InputFloat(floatProps)
	r.EndFrame()
	r.QueueShortcut(KeyA)
	r.QueueText("3.5")
	r.BeginFrame()
	if !r.InputFloat(floatProps) || floats[0] != 3.5 || floats[1] != 2.5 {
		t.Fatalf("InputFloat values=%v, want [3.5 2.5]", floats)
	}
	r.EndFrame()

	ints := []int32{4}
	intProps := InputIntProps{Bounds: NewRectangle(10, 50, 200, 30), ID: 71, Values: ints, Step: 2, StepFast: 10}
	r.QueueTap(198, 60)
	r.BeginFrame()
	if !r.InputInt(intProps) || ints[0] != 6 {
		t.Fatalf("InputInt step value=%d, want 6", ints[0])
	}
	r.EndFrame()
	state := r.numericInputState(numericInputKey{kind: 1, widgetID: 71, component: 0}, "6")
	r.SetFocus(state.token + 2)
	r.QueueKey(KeySpace)
	r.BeginFrame()
	if !r.InputInt(intProps) || ints[0] != 8 {
		t.Fatalf("InputInt keyboard step value=%d, want 8", ints[0])
	}
	r.EndFrame()

	doubles := []float64{1}
	doubleProps := InputDoubleProps{Bounds: NewRectangle(10, 90, 200, 30), ID: 72, Values: doubles}
	r.QueueTap(30, 100)
	r.BeginFrame()
	r.InputDouble(doubleProps)
	r.EndFrame()
	r.QueueShortcut(KeyA)
	r.QueueText("2.125")
	r.BeginFrame()
	if !r.InputDouble(doubleProps) || doubles[0] != 2.125 {
		t.Fatalf("InputDouble value=%f, want 2.125", doubles[0])
	}
	r.EndFrame()
}

func TestNativeSpinboxUsesButtonInteraction(t *testing.T) {
	r := New(AppConfig{Width: 240, Height: 120}).(*runtime)
	value := int32(2)
	props := SpinboxProps{Bounds: NewRectangle(10, 10, 120, 30), ID: 73,
		Min: 0, Max: 4, Step: 1, Value: &value}
	draw := func(disabled bool) bool {
		r.BeginFrame()
		r.BeginDisabled(disabled)
		changed := r.Spinbox(props)
		r.EndDisabled()
		r.Button(ButtonProps{Bounds: NewRectangle(10, 54, 80, 28), ID: 74, Label: "Next"})
		r.EndFrame()
		return changed
	}

	draw(false)
	r.SetFocus(props.ID*10 + 2)
	r.QueueKey(KeyEnter)
	if !draw(false) || value != 3 {
		t.Fatalf("Spinbox keyboard value=%d, want 3", value)
	}
	focused := false
	for _, op := range r.FrameOps() {
		if op.ID == props.ID*10+2 && op.Focused {
			focused = true
		}
	}
	if !focused {
		t.Fatal("Spinbox increment button lacks focus presentation")
	}
	r.QueueKey(KeySpace)
	if draw(true) || value != 3 {
		t.Fatalf("disabled Spinbox changed value=%d", value)
	}
	draw(false)
	r.SetFocus(props.ID*10 + 1)
	r.QueueKey(KeyTab)
	draw(false)
	if r.Focus() != props.ID*10+2 {
		t.Fatalf("Spinbox Tab focus=%d, want %d", r.Focus(), props.ID*10+2)
	}
}

func TestNativeBasicImGuiWidgets(t *testing.T) {
	r := New(AppConfig{Width: 640, Height: 480}).(*runtime)
	r.QueueTap(20, 20)
	r.BeginFrame()
	if !r.SmallButton(ButtonProps{Bounds: NewRectangle(10, 10, 80, 24), Label: "Small", ID: 80}) {
		t.Fatal("SmallButton did not consume its tap")
	}
	r.EndFrame()

	r.QueueTap(20, 60)
	r.BeginFrame()
	if !r.InvisibleButton(InvisibleButtonProps{Bounds: NewRectangle(10, 50, 80, 24), ID: 81}) {
		t.Fatal("InvisibleButton did not consume its tap")
	}
	if len(r.FrameOps()) != 0 {
		t.Fatalf("InvisibleButton recorded visible operations: %#v", r.FrameOps())
	}
	r.EndFrame()

	r.QueueTap(20, 100)
	r.BeginFrame()
	if !r.ArrowButton(ArrowButtonProps{Bounds: NewRectangle(10, 90, 30, 24), ID: 82, Direction: ArrowDown}) {
		t.Fatal("ArrowButton did not consume its tap")
	}
	r.Bullet(NewRectangle(50, 90, 20, 20))
	r.Separator(NewRectangle(80, 90, 100, 20), 0)
	ops := r.FrameOps()
	if len(ops) != 3 || ops[0].Text != "v" || ops[1].Kind != FrameOpRect || ops[2].Kind != FrameOpLine {
		t.Fatalf("basic ImGui widget ops=%#v", ops)
	}
	r.EndFrame()
}

func TestNativeSelectionAndImageWidgets(t *testing.T) {
	r := New(AppConfig{Width: 640, Height: 480}).(*runtime)
	selected := int32(0)
	r.QueueTap(20, 20)
	r.BeginFrame()
	if !r.Selectable(SelectableProps{Bounds: NewRectangle(10, 10, 120, 28), ID: 83, Label: "Choice", Selected: &selected}) || selected != 1 {
		t.Fatalf("Selectable selected=%d, want 1", selected)
	}
	r.EndFrame()

	flags := int32(1)
	r.QueueTap(20, 60)
	r.BeginFrame()
	if !r.CheckboxFlags(CheckboxFlagsProps{Bounds: NewRectangle(10, 50, 140, 28), ID: 84, Label: "Feature", Flags: &flags, FlagsValue: 4}) || flags != 5 {
		t.Fatalf("CheckboxFlags flags=%d, want 5", flags)
	}
	r.EndFrame()

	picture := PictureProps{AssetPath: "tile.png", Bounds: NewRectangle(10, 90, 48, 32), Tint: White, Fit: PictureFitContain}
	r.BeginFrame()
	r.ImageWithBg(ImageWithBgProps{Picture: picture, Background: Color{R: 10, G: 20, B: 30, A: 255}})
	ops := r.FrameOps()
	if len(ops) != 2 || ops[0].Kind != FrameOpRect || ops[1].Kind != FrameOpPicture {
		t.Fatalf("ImageWithBg ops=%#v", ops)
	}
	r.EndFrame()

	picture.Bounds = NewRectangle(70, 90, 48, 32)
	r.QueueTap(80, 100)
	r.BeginFrame()
	if !r.ImageButton(ImageButtonProps{Picture: picture, Background: Color{R: 40, G: 50, B: 60, A: 255}, ID: 85}) {
		t.Fatal("ImageButton did not consume its tap")
	}
	ops = r.FrameOps()
	if len(ops) != 2 || ops[0].Kind != FrameOpButton || ops[1].Kind != FrameOpPicture {
		t.Fatalf("ImageButton ops=%#v", ops)
	}
	r.EndFrame()
}

func TestFocusableChoiceAndImageWidgets(t *testing.T) {
	r := New(AppConfig{Width: 640, Height: 480}).(*runtime)
	checkbox, selected, flags := int32(0), int32(0), int32(0)
	picture := PictureProps{AssetPath: "tile.png", Bounds: NewRectangle(10, 170, 48, 32), Tint: White, Fit: PictureFitContain}
	activations := make(map[int32]int)
	draw := func(disableFlags bool) {
		r.BeginFrame()
		if r.Checkbox(900, 10, 10, "Check", &checkbox) {
			activations[900]++
		}
		if r.Selectable(SelectableProps{Bounds: NewRectangle(10, 50, 140, 28), ID: 901, Label: "Choice", Selected: &selected}) {
			activations[901]++
		}
		r.BeginDisabled(disableFlags)
		if r.CheckboxFlags(CheckboxFlagsProps{Bounds: NewRectangle(10, 90, 140, 28), ID: 902, Label: "Flag", Flags: &flags, FlagsValue: 4}) {
			activations[902]++
		}
		r.EndDisabled()
		if got := r.Radio(RadioButtonProps{Bounds: NewRectangle(10, 130, 140, 28), ID: 903, Label: "Radio"}); got != 0 {
			activations[got]++
		}
		if r.ImageButton(ImageButtonProps{Picture: picture, Background: Black, ID: 904}) {
			activations[904]++
		}
		if r.InvisibleButton(InvisibleButtonProps{Bounds: NewRectangle(70, 170, 48, 32), ID: 905}) {
			activations[905]++
		}
		if r.ColorButton(ColorButtonProps{Bounds: NewRectangle(10, 220, 100, 32), ID: 906, Label: "Color", Color: Color{R: 255, A: 255}}) {
			activations[906]++
		}
		r.EndFrame()
	}

	draw(false)
	for _, tc := range []struct {
		id  int32
		key int32
	}{
		{900, KeySpace}, {901, KeyEnter}, {902, KeySpace},
		{903, KeyEnter}, {904, KeySpace}, {905, KeyEnter}, {906, KeySpace},
	} {
		r.SetFocus(tc.id)
		r.QueueKey(tc.key)
		draw(false)
		if activations[tc.id] != 1 {
			t.Fatalf("widget %d keyboard activations=%d, want 1", tc.id, activations[tc.id])
		}
	}
	if checkbox != 1 || selected != 1 || flags != 4 {
		t.Fatalf("choice state checkbox=%d selected=%d flags=%d", checkbox, selected, flags)
	}

	r.SetFocus(901)
	r.QueueKey(KeyTab)
	draw(false)
	if r.Focus() != 902 {
		t.Fatalf("choice Tab focus=%d, want 902", r.Focus())
	}

	r.SetFocus(902)
	r.QueueKey(KeySpace)
	draw(true)
	if flags != 4 || activations[902] != 1 {
		t.Fatal("disabled choice accepted keyboard activation")
	}

	r.SetFocus(901)
	draw(false)
	focused := false
	for _, op := range r.FrameOps() {
		if op.ID == 901 && op.Focused {
			focused = true
		}
	}
	if !focused {
		t.Fatal("focused selectable has no focus presentation")
	}
}

func TestToggleKeyboardNavigation(t *testing.T) {
	r := New(AppConfig{Width: 240, Height: 120}).(*runtime)
	value := int32(0)
	draw := func(disabled bool) bool {
		r.BeginFrame()
		r.BeginDisabled(disabled)
		activated := r.Toggle(907, 10, 10, 120, 34, &value, "Off", "On")
		r.EndDisabled()
		r.Button(ButtonProps{Bounds: NewRectangle(10, 54, 80, 28), ID: 908, Label: "Next"})
		r.EndFrame()
		return activated
	}

	draw(false)
	r.SetFocus(907)
	r.QueueKey(KeySpace)
	activated := draw(false)
	if !activated || value != 1 {
		t.Fatalf("toggle Space activation/state = %v/%d, want true/1", activated, value)
	}
	focused := false
	for _, op := range r.FrameOps() {
		if op.ID == 907 && op.Focused && op.BorderColor == r.theme().focus {
			focused = true
		}
	}
	if !focused {
		t.Fatal("focused toggle has no focus presentation")
	}

	r.SetFocus(907)
	r.QueueKey(KeyTab)
	draw(false)
	if r.Focus() != 908 {
		t.Fatalf("toggle Tab focus=%d, want 908", r.Focus())
	}

	r.SetFocus(907)
	r.QueueKey(KeyEnter)
	if draw(true) || value != 1 {
		t.Fatalf("disabled toggle activation/state = true/%d, want false/1", value)
	}
}

func TestNativeSeparatorText(t *testing.T) {
	r := New(AppConfig{Width: 320, Height: 200}).(*runtime)
	r.BeginFrame()
	r.SeparatorText(SeparatorTextProps{Bounds: NewRectangle(10, 20, 200, 24), Label: "Section", Font: Text14})
	r.EndFrame()
	ops := r.FrameOps()
	if len(ops) != 2 || ops[0].Kind != FrameOpText || ops[0].Text != "Section" || ops[1].Kind != FrameOpLine {
		t.Fatalf("SeparatorText ops=%#v", ops)
	}
	if ops[1].Bounds.X <= ops[0].Bounds.X+ops[0].Bounds.Width {
		t.Fatalf("SeparatorText rule overlaps label: text=%+v line=%+v", ops[0].Bounds, ops[1].Bounds)
	}
}

func TestNativeTabItemControls(t *testing.T) {
	r := New(AppConfig{Width: 320, Height: 200}).(*runtime)
	r.QueueTap(20, 20)
	r.BeginFrame()
	if !r.TabItemButton(TabItemButtonProps{Bounds: NewRectangle(10, 10, 60, 28), ID: 86, Label: "+", Font: Text14}) {
		t.Fatal("TabItemButton did not consume its tap")
	}
	r.EndFrame()

	tabs := []Tab{{Label: "One", Closeable: true}, {Label: "Two", Closeable: true}}
	selected, closed := int32(0), int32(-1)
	props := ClosableTabBarProps{Bounds: NewRectangle(10, 60, 200, 30), Tabs: tabs, Count: 2, SelectedIndex: &selected, Font: Text14, ClosedIndex: &closed}
	r.QueueTap(195, 70)
	r.BeginFrame()
	if clicked := r.ClosableTabBar(props); clicked != -1 || closed != 1 || selected != 0 {
		t.Fatalf("close result clicked=%d closed=%d selected=%d", clicked, closed, selected)
	}
	r.EndFrame()

	r.QueueTap(130, 70)
	r.BeginFrame()
	if clicked := r.ClosableTabBar(props); clicked != 1 || closed != -1 || selected != 1 {
		t.Fatalf("select result clicked=%d closed=%d selected=%d", clicked, closed, selected)
	}
	r.EndFrame()
}

func TestNativeTabBarKeyboardNavigation(t *testing.T) {
	r := New(AppConfig{Width: 360, Height: 180}).(*runtime)
	tabs := []Tab{{Label: "One"}, {Label: "Disabled", Disabled: true}, {Label: "Three", Closeable: true}}
	selected, closed := int32(0), int32(-1)
	props := TabBarProps{Bounds: NewRectangle(10, 10, 300, 32), Tabs: tabs, Count: 3,
		SelectedIndex: selected, ClosedIndex: &closed, ID: 920}
	draw := func(disabled bool) int32 {
		props.SelectedIndex = selected
		r.BeginFrame()
		r.BeginDisabled(disabled)
		clicked := r.TabBar(props)
		r.EndDisabled()
		r.Button(ButtonProps{Bounds: NewRectangle(10, 54, 80, 28), ID: 921, Label: "Next"})
		r.EndFrame()
		if clicked >= 0 {
			selected = clicked
		}
		return clicked
	}

	draw(false)
	r.SetFocus(props.ID)
	r.QueueKey(KeyRight)
	if clicked := draw(false); clicked != 2 || selected != 2 {
		t.Fatalf("Right did not skip disabled tab: clicked=%d selected=%d", clicked, selected)
	}
	r.QueueKey(KeyDelete)
	if clicked := draw(false); clicked != -1 || closed != 2 {
		t.Fatalf("Delete close result clicked=%d closed=%d", clicked, closed)
	}
	r.QueueKey(KeyHome)
	if clicked := draw(false); clicked != 0 || selected != 0 {
		t.Fatalf("Home result clicked=%d selected=%d", clicked, selected)
	}
	focused := false
	for _, op := range r.FrameOps() {
		if op.ID == props.ID && op.Row == 0 && op.Focused && op.BorderColor == r.theme().focus {
			focused = true
		}
	}
	if !focused {
		t.Fatal("selected tab lacks focus presentation")
	}
	r.QueueKey(KeyRight)
	if clicked := draw(true); clicked != -1 || selected != 0 {
		t.Fatalf("disabled tab bar changed selection: clicked=%d selected=%d", clicked, selected)
	}
	r.SetFocus(props.ID)
	r.QueueKey(KeyTab)
	draw(false)
	if r.Focus() != 921 {
		t.Fatalf("Tab focus=%d, want 921", r.Focus())
	}
}

func TestNativeTabBarRichSignals(t *testing.T) {
	r := New(AppConfig{Width: 320, Height: 120}).(*runtime)
	tabs := []Tab{{Label: "Alpha"}, {Label: "Beta"}, {Label: "Gamma"}}
	scroll := int32(0)
	selectedBounds := Rectangle{}
	doubleClicked, middleClicked := int32(-1), int32(-1)
	props := TabBarProps{Bounds: NewRectangle(10, 10, 180, 30), Tabs: tabs, Count: 3,
		SelectedIndex: 2, MinTabWidth: 100, MaxTabWidth: 100, ScrollOffset: &scroll,
		FocusSelected: true, DoubleClickedIndex: &doubleClicked,
		SelectedTabBounds: &selectedBounds, MiddleClickedIndex: &middleClicked, ID: 922}

	r.BeginFrame()
	r.TabBar(props)
	r.EndFrame()
	if scroll <= 0 || selectedBounds.X+selectedBounds.Width > props.Bounds.X+props.Bounds.Width+1 {
		t.Fatalf("selected tab was not revealed: scroll=%d bounds=%+v", scroll, selectedBounds)
	}

	props.FocusSelected = false
	props.SelectedIndex = 0
	scroll = 0
	r.QueueTap(30, 20)
	r.BeginFrame()
	r.TabBar(props)
	r.EndFrame()
	r.QueueTap(30, 20)
	r.BeginFrame()
	r.TabBar(props)
	r.EndFrame()
	if doubleClicked != 0 {
		t.Fatalf("double-click index=%d, want 0", doubleClicked)
	}

	r.QueueMouseButton(MouseButtonMiddle, 130, 20)
	r.BeginFrame()
	r.TabBar(props)
	r.EndFrame()
	if middleClicked != 1 {
		t.Fatalf("middle-click index=%d, want 1", middleClicked)
	}

	from, to := int32(-1), int32(-1)
	props.ReorderedFromIndex, props.ReorderedToIndex = &from, &to
	r.QueueMouseButtonDown(MouseButtonLeft, 30, 20)
	r.BeginFrame()
	r.TabBar(props)
	r.EndFrame()
	r.QueueMouseMove(170, 20)
	r.BeginFrame()
	r.TabBar(props)
	r.EndFrame()
	r.QueueMouseButtonUp(MouseButtonLeft, 170, 20)
	r.BeginFrame()
	r.TabBar(props)
	r.EndFrame()
	if from != 0 || to != 2 {
		t.Fatalf("reorder=%d->%d, want 0->2", from, to)
	}
}

func TestNativeTabBarsOwnIndependentDefaultScroll(t *testing.T) {
	r := New(AppConfig{Width: 420, Height: 160}).(*runtime)
	tabs := []Tab{{Label: "Alpha"}, {Label: "Beta"}, {Label: "Gamma"}}
	first := TabBarProps{Bounds: NewRectangle(10, 10, 160, 30), Tabs: tabs,
		Count: 3, SelectedIndex: 2, MinTabWidth: 100, MaxTabWidth: 100,
		FocusSelected: true, ID: 924}
	second := first
	second.Bounds = NewRectangle(210, 10, 160, 30)
	second.SelectedIndex = 0
	second.FocusSelected = false
	second.ID = 925

	r.BeginFrame()
	r.TabBar(first)
	r.TabBar(second)
	r.EndFrame()
	firstScroll := r.tabScroll[first.ID]
	if firstScroll <= 0 {
		t.Fatalf("first default scroll=%d, want a revealed selected tab", firstScroll)
	}
	if got := r.tabScroll[second.ID]; got != 0 {
		t.Fatalf("second default scroll inherited %d from first tab bar", got)
	}

	first.FocusSelected = false
	r.BeginFrame()
	r.TabBar(first)
	r.TabBar(second)
	r.EndFrame()
	if got := r.tabScroll[first.ID]; got != firstScroll {
		t.Fatalf("default scroll did not persist: got %d want %d", got, firstScroll)
	}
}

func TestNativeComposedTabBarScope(t *testing.T) {
	r := New(AppConfig{Width: 320, Height: 180}).(*runtime)
	tabs := []Tab{{Label: "One"}, {Label: "Two"}}
	selected := int32(0)
	props := TabBarProps{Bounds: NewRectangle(10, 10, 200, 30), Tabs: tabs,
		Count: 2, ID: 926}
	visible := int32(-1)

	r.BeginFrame()
	if !r.BeginTabBar(props, &selected) {
		t.Fatal("BeginTabBar rejected valid props")
	}
	if r.BeginTabItem(0) {
		visible = 0
		r.Button(ButtonProps{Bounds: NewRectangle(20, 60, 80, 28), ID: 927, Label: "First"})
		r.EndTabItem()
	}
	if r.BeginTabItem(1) {
		t.Fatal("BeginTabItem exposed an unselected tab")
	}
	r.EndTabBar()
	r.EndFrame()
	if visible != 0 {
		t.Fatalf("visible tab=%d, want 0", visible)
	}

	r.QueueTap(160, 20)
	r.BeginFrame()
	if !r.BeginTabBar(props, &selected) {
		t.Fatal("BeginTabBar rejected second frame")
	}
	if r.BeginTabItem(0) {
		t.Fatal("old tab remained visible after header selection")
	}
	if r.BeginTabItem(1) {
		visible = 1
		r.Checkbox(928, 20, 60, "Second", &visible)
		r.EndTabItem()
	}
	r.EndTabBar()
	r.EndFrame()
	if selected != 1 || visible != 1 {
		t.Fatalf("selected/visible=%d/%d, want 1/1", selected, visible)
	}

	if r.BeginTabBar(TabBarProps{}, &selected) {
		t.Fatal("BeginTabBar accepted invalid props")
	}
	mustPanic := func(call func()) {
		t.Helper()
		defer func() {
			if recover() == nil {
				t.Fatal("unbalanced tab scope did not panic")
			}
		}()
		call()
	}
	mustPanic(func() { r.BeginTabItem(0) })
	mustPanic(func() { r.EndTabItem() })
	mustPanic(func() { r.EndTabBar() })
	if !r.BeginTabBar(props, &selected) {
		t.Fatal("BeginTabBar rejected frame-balance fixture")
	}
	mustPanic(func() { r.EndFrame() })
	r.EndTabBar()
}

func TestNativeTypedDragDrop(t *testing.T) {
	r := New(AppConfig{Width: 320, Height: 200}).(*runtime)
	payload := []byte("item-42")
	output := make([]byte, 16)
	accepted := int32(0)
	source := DragDropSourceProps{Bounds: NewRectangle(10, 10, 80, 30), ID: 87, Type: "ITEM", Data: payload, DataSize: int32(len(payload))}
	target := DragDropTargetProps{Bounds: NewRectangle(120, 10, 100, 30), ID: 88, Type: "ITEM", Output: output, OutputSize: int32(len(output)), AcceptedSize: &accepted}

	r.QueueMouseButtonDown(MouseButtonLeft, 20, 20)
	r.BeginFrame()
	if !r.DragDropSource(source) {
		t.Fatal("DragDropSource did not activate on press")
	}
	if r.DragDropTarget(target) {
		t.Fatal("DragDropTarget accepted before release")
	}
	r.EndFrame()

	r.QueueMouseMove(150, 20)
	r.QueueMouseButtonUp(MouseButtonLeft, 150, 20)
	r.BeginFrame()
	if !r.DragDropSource(source) {
		t.Fatal("DragDropSource did not retain payload through release frame")
	}
	if !r.DragDropTarget(target) {
		t.Fatal("DragDropTarget did not accept matching released payload")
	}
	r.EndFrame()
	if accepted != int32(len(payload)) || string(output[:accepted]) != string(payload) {
		t.Fatalf("accepted=%d output=%q, want %q", accepted, output[:accepted], payload)
	}
}

func TestManyComboIdentities(t *testing.T) {
	r := New(AppConfig{}).(*runtime)
	selected := make([]int32, 41)
	draw := func(first int) {
		r.BeginFrame()
		for i := first; i < len(selected); i++ {
			x := float32(300)
			if i == 0 {
				x = 10
			}
			r.Combobox(ComboboxProps{Bounds: NewRectangle(x, 10, 160, 28), ID: int32(20000 + i),
				Options: []string{"One", "Two"}, SelectedIndex: &selected[i]})
		}
		r.EndFrame()
	}
	r.QueueTap(20, 20)
	draw(0)
	if !r.openDropdowns[20000] || !r.popupCaptures(20, 70) {
		t.Fatal("41 combos lost first owner's open state")
	}
	r.QueueTap(20, 75)
	draw(0)
	if selected[0] != 1 {
		t.Fatal("41 combos lost first owner's selection")
	}
	for _, value := range selected[1:] {
		if value != 0 {
			t.Fatal("combo selection leaked across identities")
		}
	}
	r.QueueTap(20, 20)
	draw(0)
	draw(1)
	if r.openDropdowns[20000] || r.popupCaptures(20, 70) {
		t.Fatal("missing combo retained open state or capture")
	}
}

func TestLargeComboOptions(t *testing.T) {
	r := New(AppConfig{Width: 240, Height: 6000}).(*runtime)
	options := make([]string, 131)
	for i := range options {
		options[i] = "item"
	}
	selected := int32(0)
	draw := func() {
		r.BeginFrame()
		r.Combobox(ComboboxProps{Bounds: NewRectangle(10, 10, 160, 28), ID: 21000,
			Options: options, SelectedIndex: &selected})
		r.EndFrame()
	}
	r.QueueTap(20, 20)
	draw()
	r.QueueTap(20, 42+130*28+10)
	draw()
	if selected != 130 {
		t.Fatalf("large combo selected %d, want 130", selected)
	}
}

func TestLongComboLabelOwnership(t *testing.T) {
	r := New(AppConfig{}).(*runtime)
	labelBytes := make([]byte, 512)
	for i := range labelBytes {
		labelBytes[i] = 'q'
	}
	label := string(labelBytes)
	options := []string{label}
	selected := int32(0)
	r.QueueTap(20, 20)
	r.BeginFrame()
	r.Combobox(ComboboxProps{Bounds: NewRectangle(10, 10, 160, 28), ID: 22000,
		Options: options, SelectedIndex: &selected})
	options[0] = "changed"
	r.EndFrame()
	found := false
	for _, op := range r.FrameOps() {
		if op.Kind == FrameOpText && op.Bounds.Y >= 42 && op.Text == label {
			found = true
		}
	}
	if !found {
		t.Fatal("deferred combo did not own the complete long label")
	}
}

func TestComboKeyboardNavigation(t *testing.T) {
	r := New(AppConfig{}).(*runtime)
	options := make([]string, 131)
	selected := int32(0)
	for step, key := range []int32{0, KeyEnd, KeyUp, KeyEnter, 0, KeyHome, KeyEscape, 0, KeyHome, KeyDown, KeyEnter} {
		if key == 0 {
			r.QueueTap(20, 20)
		} else {
			r.QueueKey(key)
		}
		r.BeginFrame()
		r.Combobox(ComboboxProps{Bounds: NewRectangle(10, 10, 160, 28), ID: 23000,
			Options: options, SelectedIndex: &selected})
		r.EndFrame()
		want := int32(0)
		if step >= 3 {
			want = 129
		}
		if step == 10 {
			want = 1
		}
		if selected != want {
			t.Fatalf("keyboard step %d: selected=%d, want %d", step, selected, want)
		}
	}
}

func TestComboKeyboardOpen(t *testing.T) {
	for _, key := range []int32{KeyEnter, 335, KeySpace, KeyDown} {
		for mode := 0; mode < 3; mode++ {
			r := New(AppConfig{}).(*runtime)
			selected := int32(1)
			r.QueueKey(key)
			r.BeginFrame()
			r.SetFocus(24000)
			r.BeginDisabled(mode == 2)
			r.Combobox(ComboboxProps{Bounds: NewRectangle(10, 10, 160, 28), ID: 24000,
				Options: []string{"One", "Two"}, SelectedIndex: &selected, Disabled: mode == 1})
			r.EndDisabled()
			r.EndFrame()
			if r.openDropdowns[24000] != (mode == 0) || selected != 1 {
				t.Fatalf("opening key %d mode %d: open=%v selected=%d", key, mode, r.openDropdowns[24000], selected)
			}
			found := false
			for _, op := range r.ops {
				if op.Kind != FrameOpButton || op.ID != 24000 {
					continue
				}
				found = true
				if op.Focused != (mode == 0) {
					t.Fatalf("opening key %d mode %d: focused paint=%v", key, mode, op.Focused)
				}
				if mode == 0 && op.BorderColor != r.theme().focus {
					t.Fatalf("opening key %d: focused dropdown lacks focus border", key)
				}
			}
			if !found {
				t.Fatalf("opening key %d mode %d: missing dropdown paint", key, mode)
			}
		}
	}
}

func TestNativeMultiSelectListModifiers(t *testing.T) {
	r := New(AppConfig{Width: 320, Height: 240}).(*runtime)
	selected := []int32{1, 0, 0}
	selectedCount, anchor := int32(1), int32(0)
	props := MultiSelectListProps{Bounds: NewRectangle(10, 10, 180, 84), ID: 89, Items: []string{"Alpha", "Beta", "Gamma"}, ItemCount: 3, Selected: selected, SelectedCount: &selectedCount, Anchor: &anchor, RowHeight: 28}

	r.QueueTap(20, 48)
	r.BeginFrame()
	if clicked := r.MultiSelectList(props); clicked != 1 || selectedCount != 1 || anchor != 1 || selected[1] != 1 || selected[0] != 0 {
		t.Fatalf("plain selection clicked=%d selected=%v count=%d anchor=%d", clicked, selected, selectedCount, anchor)
	}
	r.EndFrame()

	r.QueueKey(KeyLeftControl)
	r.QueueTap(20, 76)
	r.BeginFrame()
	if clicked := r.MultiSelectList(props); clicked != 2 || selectedCount != 2 || anchor != 2 || selected[1] != 1 || selected[2] != 1 {
		t.Fatalf("control selection clicked=%d selected=%v count=%d anchor=%d", clicked, selected, selectedCount, anchor)
	}
	r.EndFrame()

	r.QueueKey(KeyLeftShift)
	r.QueueTap(20, 20)
	r.BeginFrame()
	if clicked := r.MultiSelectList(props); clicked != 0 || selectedCount != 3 || selected[0] != 1 || selected[1] != 1 || selected[2] != 1 {
		t.Fatalf("shift range clicked=%d selected=%v count=%d anchor=%d", clicked, selected, selectedCount, anchor)
	}
	r.EndFrame()

	r.QueueKey(KeyLeftControl)
	r.QueueTap(20, 48)
	r.BeginFrame()
	if clicked := r.MultiSelectList(props); clicked != 1 || selectedCount != 2 || selected[1] != 0 {
		t.Fatalf("control toggle clicked=%d selected=%v count=%d anchor=%d", clicked, selected, selectedCount, anchor)
	}
	r.EndFrame()
}

func TestNativeMultiSelectListKeyboardNavigation(t *testing.T) {
	r := New(AppConfig{Width: 320, Height: 240}).(*runtime)
	selected := []int32{1, 0, 0}
	selectedCount, anchor := int32(1), int32(0)
	props := MultiSelectListProps{Bounds: NewRectangle(10, 10, 180, 84), ID: 89, Items: []string{"Alpha", "Beta", "Gamma"}, ItemCount: 3, Selected: selected, SelectedCount: &selectedCount, Anchor: &anchor, RowHeight: 28}
	draw := func() int32 {
		r.BeginFrame()
		clicked := r.MultiSelectList(props)
		r.Button(ButtonProps{Bounds: NewRectangle(10, 110, 80, 28), ID: 90, Label: "Next"})
		r.EndFrame()
		return clicked
	}

	draw()
	r.SetFocus(89)
	r.QueueKey(KeyDown)
	if clicked := draw(); clicked != 1 || anchor != 1 || selectedCount != 1 || selected[1] != 1 || selected[0] != 0 {
		t.Fatalf("Down selection clicked=%d selected=%v count=%d anchor=%d", clicked, selected, selectedCount, anchor)
	}
	r.QueueKey(KeyLeftShift)
	r.QueueKey(KeyDown)
	if clicked := draw(); clicked != 2 || anchor != 2 || selectedCount != 2 || selected[1] != 1 || selected[2] != 1 {
		t.Fatalf("Shift+Down range clicked=%d selected=%v count=%d anchor=%d", clicked, selected, selectedCount, anchor)
	}
	r.QueueKey(KeySpace)
	if clicked := draw(); clicked != 2 || selectedCount != 1 || selected[2] != 0 || selected[1] != 1 {
		t.Fatalf("Space toggle clicked=%d selected=%v count=%d anchor=%d", clicked, selected, selectedCount, anchor)
	}
	r.QueueKey(KeyLeftControl)
	r.QueueKey(KeyHome)
	if clicked := draw(); clicked != -1 || anchor != 0 || selectedCount != 1 || selected[1] != 1 {
		t.Fatalf("Control+Home cursor clicked=%d selected=%v count=%d anchor=%d", clicked, selected, selectedCount, anchor)
	}
	r.QueueKey(KeyEnter)
	if clicked := draw(); clicked != 0 || selectedCount != 1 || selected[0] != 1 || selected[1] != 0 {
		t.Fatalf("Enter selection clicked=%d selected=%v count=%d anchor=%d", clicked, selected, selectedCount, anchor)
	}
	foundFocus := false
	for _, op := range r.ops {
		if op.ID == 89 && op.Row == 0 && op.Focused && op.BorderColor == r.theme().focus {
			foundFocus = true
		}
	}
	if !foundFocus {
		t.Fatal("focused multi-select row lacks focus presentation")
	}
	r.QueueKey(KeyTab)
	draw()
	if r.Focus() != 90 {
		t.Fatalf("Tab focus=%d, want 90", r.Focus())
	}

	r.SetFocus(89)
	r.QueueKey(KeySpace)
	r.BeginFrame()
	r.BeginDisabled(true)
	clicked := r.MultiSelectList(props)
	r.EndDisabled()
	r.EndFrame()
	if clicked != -1 || selectedCount != 1 || selected[0] != 1 {
		t.Fatalf("disabled keyboard changed selection: clicked=%d selected=%v count=%d", clicked, selected, selectedCount)
	}
}

func TestNativeColorWidgets(t *testing.T) {
	r := New(AppConfig{Width: 640, Height: 480}).(*runtime)
	rgb := []float32{0, 0.25, 0.75}
	r.QueueMouseButtonDown(MouseButtonLeft, 40, 20)
	r.BeginFrame()
	if !r.ColorEdit3(ColorEditProps{Bounds: NewRectangle(10, 10, 180, 30), ID: 90, Values: rgb, ValueCount: 3}) || rgb[0] != 0.5 {
		t.Fatalf("ColorEdit3 values=%v, want red 0.5", rgb)
	}
	r.EndFrame()
	r.QueueMouseButtonUp(MouseButtonLeft, 40, 20)
	r.BeginFrame()
	r.ColorEdit3(ColorEditProps{Bounds: NewRectangle(10, 10, 180, 30), ID: 90, Values: rgb, ValueCount: 3})
	r.EndFrame()

	rgba := []float32{0.1, 0.2, 0.3, 0}
	r.QueueMouseButtonDown(MouseButtonLeft, 60, 200)
	r.BeginFrame()
	if !r.ColorPicker4(ColorEditProps{Bounds: NewRectangle(10, 60, 100, 200), ID: 91, Values: rgba, ValueCount: 4}) || rgba[3] != 0.5 {
		t.Fatalf("ColorPicker4 values=%v, want alpha 0.5", rgba)
	}
	r.EndFrame()
	r.QueueMouseButtonUp(MouseButtonLeft, 60, 200)
	r.BeginFrame()
	r.ColorPicker4(ColorEditProps{Bounds: NewRectangle(10, 60, 100, 200), ID: 91, Values: rgba, ValueCount: 4})
	r.EndFrame()

	r.QueueTap(150, 20)
	r.BeginFrame()
	if !r.ColorButton(ColorButtonProps{Bounds: NewRectangle(130, 10, 60, 30), ID: 92, Label: "Tint", Color: Color{20, 40, 60, 128}}) {
		t.Fatal("ColorButton did not consume its tap")
	}
	ops := r.FrameOps()
	if len(ops) != 4 || ops[3].Kind != FrameOpButton || ops[3].Color.A != 128 {
		t.Fatalf("ColorButton ops=%#v", ops)
	}
	r.EndFrame()
}

func TestPanedDragOutsideHandle(t *testing.T) {
	r := New(AppConfig{}).(*runtime)
	split := int32(90)
	p := PanedViewProps{Bounds: NewRectangle(10, 10, 240, 80), ID: 9450, Vertical: true, Split: &split, MinFirst: 40, MinSecond: 40}
	r.QueueMouseButtonDown(MouseButtonLeft, 100, 30)
	r.BeginFrame()
	r.PanedView(p)
	r.EndFrame()
	r.QueueMouseMove(190, 30)
	r.BeginFrame()
	r.PanedView(p)
	r.EndFrame()
	if split != 180 {
		t.Fatalf("drag outside handle: got %d", split)
	}
	r.QueueMouseButtonUp(MouseButtonLeft, 190, 30)
	r.BeginFrame()
	r.PanedView(p)
	r.EndFrame()
	r.QueueMouseMove(100, 30)
	r.BeginFrame()
	r.PanedView(p)
	r.EndFrame()
	if split != 180 {
		t.Fatalf("released divider moved: %d", split)
	}
}
