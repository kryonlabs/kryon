package kryon

import (
	"os"
	"testing"
)

func TestTextUsesActiveKSS(t *testing.T) {
	ClearStylePacks()
	defer ClearStylePacks()
	data, err := os.ReadFile("../../fonts/noto/NotoSans-SemiBold.ttf")
	if err != nil {
		t.Fatal(err)
	}
	id, ok := registerFontData("test-kss-semibold", ".ttf", data)
	if !ok {
		t.Fatal("could not register the semibold test face")
	}
	if !RegisterStylePackSource(`
@pack app.text;
Text {
  foreground: #123456;
  font-size: 21;
  typeface: test-kss-semibold;
}
`, "Text", "") {
		t.Fatal("style pack did not register")
	}

	r := New(AppConfig{Width: 320, Height: 200}).(*runtime)
	r.BeginFrame()
	r.Text(TextProps{Text: "styled", Wrap: TextWrapNone})
	r.Text(TextProps{Text: "explicit", Color: Color{0xaa, 0xbb, 0xcc, 0xff}, Wrap: TextWrapNone})
	r.EndFrame()

	var styled, explicit *FrameOp
	for i := range r.ops {
		if r.ops[i].Kind != FrameOpText {
			continue
		}
		switch r.ops[i].Text {
		case "styled":
			styled = &r.ops[i]
		case "explicit":
			explicit = &r.ops[i]
		}
	}
	if styled == nil || styled.Color != (Color{0x12, 0x34, 0x56, 0xff}) ||
		styled.FontSize != 21 || styled.FontID != id {
		t.Fatalf("text did not use KSS: %#v", styled)
	}
	if explicit == nil || explicit.Color != (Color{0xaa, 0xbb, 0xcc, 0xff}) {
		t.Fatalf("explicit text color did not win during migration: %#v", explicit)
	}
}

func TestRetainedTextOpsUseKSSTypeface(t *testing.T) {
	ClearStylePacks()
	defer ClearStylePacks()
	data, err := os.ReadFile("../../fonts/noto/NotoSans-SemiBold.ttf")
	if err != nil {
		t.Fatal(err)
	}
	id, ok := registerFontData("test-retained-semibold", ".ttf", data)
	if !ok {
		t.Fatal("could not register the semibold test face")
	}
	if !RegisterStylePackSource(`
@pack app.retained_text_face;
Heading { typeface: test-retained-semibold; font-size: 23; foreground: #102030; }
ParagraphText { typeface: test-retained-semibold; font-size: 17; foreground: #203040; }
Link { typeface: test-retained-semibold; font-size: 19; foreground: #304050; }
Toast[role=Label] { typeface: test-retained-semibold; font-size: 18; foreground: #405060; }
Checkbox[role=Label] { typeface: test-retained-semibold; font-size: 16; foreground: #506070; }
Separator[role=Label] { typeface: test-retained-semibold; font-size: 15; foreground: #607080; }
Segment { typeface: test-retained-semibold; font-size: 16; foreground: #687888; }
Progress[role=Label] { typeface: test-retained-semibold; font-size: 14; foreground: #708090; }
Plot { typeface: test-retained-semibold; font-size: 13; foreground: #8090a0; }
Slider[role=Label] { typeface: test-retained-semibold; font-size: 12; foreground: #90a0b0; }
Toggle[role=Label] { typeface: test-retained-semibold; font-size: 11; foreground: #a0b0c0; }
Drag { typeface: test-retained-semibold; font-size: 10; foreground: #b0c0d0; }
DragValue { typeface: test-retained-semibold; font-size: 16; foreground: #b0c0e0; }
Tab { typeface: test-retained-semibold; font-size: 16; foreground: #b1c1d1; }
ColorPickerSwatch { typeface: test-retained-semibold; font-size: 16; foreground: #b2c2d2; }
Modal[role=Title] { typeface: test-retained-semibold; font-size: 16; foreground: #b3c3d3; }
Modal[role=Message] { typeface: test-retained-semibold; font-size: 16; foreground: #b4c4d4; }
TitleBar[role=Title] { typeface: test-retained-semibold; font-size: 16; foreground: #b5c5d5; }
NavigationBarItem { typeface: test-retained-semibold; font-size: 16; foreground: #b6c6d6; }
SpinboxValue { typeface: test-retained-semibold; font-size: 16; foreground: #b7c7d7; }
Fieldset { typeface: test-retained-semibold; font-size: 16; foreground: #b8c8d8; }
Collapsible[role=Header] { typeface: test-retained-semibold; font-size: 16; foreground: #b9c9d9; }
Collapsible[role=Close] { typeface: test-retained-semibold; font-size: 16; foreground: #bacada; }
TreeViewItem { typeface: test-retained-semibold; font-size: 16; foreground: #bbcbdb; }
ListBoxItem { typeface: test-retained-semibold; font-size: 16; foreground: #c0d0e0; }
ListBoxMultiItem { typeface: test-retained-semibold; font-size: 16; foreground: #d0e0f0; }
MenuItem { typeface: test-retained-semibold; font-size: 16; foreground: #e0f0ff; }
TableView[role=Header] { typeface: test-retained-semibold; font-size: 16; foreground: #f0ffff; }
TableView[role=Cell] { typeface: test-retained-semibold; font-size: 16; foreground: #fff0ff; }
TableView[role=Selection] { typeface: test-retained-semibold; font-size: 16; foreground: #fffff0; }
`, "Retained Text Face", "") {
		t.Fatal("style pack did not register")
	}

	rt := New(AppConfig{Width: 420, Height: 260}).(*runtime)
	checked := int32(0)
	toggled := int32(0)
	sliderValues := []float32{0.5}
	dragValues := []float32{2}
	listSelected := int32(0)
	multiSelected := []int32{1, 0}
	multiSelectedCount := int32(0)
	tableSelectedRow := int32(0)
	tableSelectedColumn := int32(0)
	openMenu := int32(0)
	spinValue := int32(42)
	collapsibleOpen := true
	collapsibleVisible := true
	treeSelected := int32(1)
	segmentSelected := int32(0)
	paragraphY := int32(0)
	rt.BeginFrame()
	rt.Heading(HeadingProps{Text: "Title"})
	rt.ParagraphText(ParagraphTextProps{Text: "Body", Bounds: Rectangle{Width: 200}})
	rt.Paragraph(ParagraphSpec{Text: "Spec Body", Width: 180}, 0, &paragraphY)
	rt.Link(LinkProps{Text: "Docs"})
	rt.Separator(SeparatorProps{Bounds: Rectangle{Width: 160, Height: 24}, Label: "Group"})
	rt.Checkbox(CheckboxProps{Bounds: Rectangle{Width: 160, Height: 32}, Label: "Check", Value: &checked})
	rt.SegmentedControl(SegmentedControlProps{Bounds: Rectangle{Width: 180, Height: 32}, ID: 312, Options: []SegmentOption{{Label: "Segment"}}, OptionCount: 1, SelectedIndex: &segmentSelected})
	rt.Progress(ProgressProps{Bounds: Rectangle{Width: 180, Height: 24}, Min: 0, Max: 100, Value: 35, Label: "Loading"})
	rt.Plot(PlotProps{Bounds: Rectangle{Width: 180, Height: 60}, Label: "Trend", Overlay: "Now", Values: []float32{1, 3}, ValueCount: 2})
	rt.Slider(SliderProps{Bounds: Rectangle{Width: 180, Height: 42}, ID: 301, Label: "Gain", FloatValues: sliderValues, ValueCount: 1, Min: 0, Max: 1})
	rt.Toggle(ToggleProps{Bounds: Rectangle{Width: 0, Height: 0}, ID: 302, Value: &toggled, OffLabel: "Off", OnLabel: "On"})
	rt.Drag(DragProps{Bounds: Rectangle{Width: 180, Height: 32}, ID: 303, Label: "Drag", FloatValues: dragValues, ValueCount: 1, Min: 0, Max: 10})
	rt.TabBar(TabBarProps{Bounds: Rectangle{Width: 220, Height: 32}, Tabs: []Tab{{Label: "Tab One", Closeable: true}}, Count: 1, SelectedIndex: 0})
	rt.ColorPicker(ColorPickerProps{Bounds: Rectangle{Width: 220, Height: 96}, ID: 308, Label: "Swatch", Values: []float32{0.1, 0.2, 0.3}, ValueCount: 3, Picker: true})
	rt.Modal(ModalProps{Title: "Dialog", Message: "Dialog body", Actions: []ModalAction{{Label: "OK"}}})
	rt.TitleBar(TitleBarProps{Title: "Screen", Height: 44})
	rt.NavigationBar(NavigationBarProps{ViewWidth: 220, ViewHeight: 120, Items: []NavigationBarItem{{Route: 9, Label: "Home", Active: true}}, Count: 1})
	rt.Spinbox(SpinboxProps{Bounds: Rectangle{Width: 120, Height: 32}, ID: 309, Value: &spinValue})
	rt.Fieldset(FieldsetProps{Bounds: Rectangle{Width: 160, Height: 64}, Title: "Set"})
	rt.Collapsible(CollapsibleProps{Bounds: Rectangle{Width: 180, Height: 32}, ID: 310, Label: "Fold", Open: &collapsibleOpen, Visible: &collapsibleVisible})
	rt.TreeView(TreeViewProps{Bounds: Rectangle{Width: 180, Height: 32}, ID: 311, Items: []TreeItem{{ID: 1, Label: "Tree Node", Selectable: 1}}, SelectedID: &treeSelected})
	rt.ListBox(ListBoxProps{Bounds: Rectangle{Width: 180, Height: 54}, ID: 304, Items: []string{"List Item"}, SelectedIndex: &listSelected})
	rt.ListBox(ListBoxProps{Bounds: Rectangle{Width: 180, Height: 54}, ID: 305, Items: []string{"Multi A", "Multi B"}, Selected: multiSelected, SelectedCount: &multiSelectedCount})
	rt.TableView(TableViewProps{Bounds: Rectangle{Width: 220, Height: 78}, ID: 306, Columns: []string{"Column"}, Rows: []TableRow{{Cells: []string{"Cell"}}}, ColumnWidths: []int32{120}, SelectedRow: &tableSelectedRow, SelectedColumn: &tableSelectedColumn})
	rt.Menu(MenuProps{ID: 307, Mode: MenuModeBar, Bounds: Rectangle{Width: 220, Height: 30}, Menus: []MenuGroup{{Label: "File", Items: []MenuItem{{Kind: MenuCommand, Label: "Save", ID: 1}}}}, OpenIndex: &openMenu})
	rt.Toast(ToastProps{Message: "Saved", Seconds: 1})
	rt.EndFrame()

	want := map[string]bool{
		"Title":       false,
		"Body":        false,
		"Spec Body":   false,
		"Docs":        false,
		"Group":       false,
		"Check":       false,
		"Segment":     false,
		"Loading":     false,
		"Trend":       false,
		"Now":         false,
		"Gain":        false,
		"Off":         false,
		"On":          false,
		"Drag":        false,
		"2.000":       false,
		"Tab One":     false,
		"×":           false,
		"Swatch":      false,
		"Dialog":      false,
		"Dialog body": false,
		"Screen":      false,
		"Home":        false,
		"42":          false,
		"Set":         false,
		"v  Fold":     false,
		"Tree Node":   false,
		"List Item":   false,
		"Multi A":     false,
		"Multi B":     false,
		"Column":      false,
		"Cell":        false,
		"File":        false,
		"Save":        false,
		"Saved":       false,
	}
	for _, op := range rt.FrameOps() {
		if op.Kind != FrameOpText && op.Kind != FrameOpButton {
			continue
		}
		if _, ok := want[op.Text]; !ok {
			continue
		}
		if op.FontID != id {
			t.Fatalf("%q did not carry the KSS typeface: %+v", op.Text, op)
		}
		want[op.Text] = true
	}
	for text, saw := range want {
		if !saw {
			t.Fatalf("missing retained text op %q in %+v", text, rt.FrameOps())
		}
	}
}
