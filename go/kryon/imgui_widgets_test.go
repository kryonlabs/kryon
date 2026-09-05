package kryon

import "testing"

func TestNativeTextHelpers(t *testing.T) {
	r := New(AppConfig{Width: 320, Height: 240}).(*runtime)
	color := Color{R: 20, G: 40, B: 60, A: 255}
	r.BeginFrame()
	r.TextColored("colored", 10, 10, Text16, color)
	r.TextDisabled("disabled", 10, 30, Text16)
	r.TextWrapped("one two three four", NewRectangle(10, 50, 48, 60), Text16, color)
	r.LabelText("Status", "Ready", NewRectangle(10, 120, 160, 20), Text16, color)
	r.BulletText("item", NewRectangle(10, 150, 120, 20), Text16, color)
	r.EndFrame()

	ops := r.FrameOps()
	if len(ops) < 8 {
		t.Fatalf("text helper ops=%d, want at least 8", len(ops))
	}
	if ops[0].Text != "colored" || ops[0].Color != color {
		t.Fatalf("TextColored op=%+v", ops[0])
	}
	if ops[1].Text != "disabled" || ops[1].Color.A >= 255 {
		t.Fatalf("TextDisabled op=%+v", ops[1])
	}
	wrapped := 0
	for _, op := range ops {
		if op.Kind == FrameOpText && (op.Text == "one" || op.Text == "two" || op.Text == "three" || op.Text == "four") {
			wrapped++
		}
	}
	if wrapped < 2 {
		t.Fatalf("TextWrapped emitted %d wrapped lines", wrapped)
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

func TestNativeTooltip(t *testing.T) {
	r := New(AppConfig{Width: 240, Height: 160}).(*runtime)
	props := TooltipProps{Trigger: NewRectangle(20, 20, 80, 30), Text: "A tooltip with wrapped help text", Font: Text14, MaxWidth: 90}
	r.QueueMouseMove(40, 30)
	r.BeginFrame()
	if !r.Tooltip(props) {
		t.Fatal("Tooltip should be visible over its trigger")
	}
	ops := r.FrameOps()
	if len(ops) < 4 || ops[0].Kind != FrameOpRect || ops[1].Kind != FrameOpRect {
		t.Fatalf("Tooltip ops=%+v", ops)
	}
	r.EndFrame()

	r.QueueMouseMove(180, 120)
	r.BeginFrame()
	if r.Tooltip(props) || len(r.FrameOps()) != 0 {
		t.Fatal("Tooltip rendered outside its trigger")
	}
	r.EndFrame()
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
