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
Progress[role=Label] { typeface: test-retained-semibold; font-size: 14; foreground: #708090; }
Plot { typeface: test-retained-semibold; font-size: 13; foreground: #8090a0; }
Slider[role=Label] { typeface: test-retained-semibold; font-size: 12; foreground: #90a0b0; }
Toggle[role=Label] { typeface: test-retained-semibold; font-size: 11; foreground: #a0b0c0; }
Drag { typeface: test-retained-semibold; font-size: 10; foreground: #b0c0d0; }
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
	rt.BeginFrame()
	rt.Heading(HeadingProps{Text: "Title"})
	rt.ParagraphText(ParagraphTextProps{Text: "Body", Bounds: Rectangle{Width: 200}})
	rt.Link(LinkProps{Text: "Docs"})
	rt.Separator(SeparatorProps{Bounds: Rectangle{Width: 160, Height: 24}, Label: "Group"})
	rt.Checkbox(CheckboxProps{Bounds: Rectangle{Width: 160, Height: 32}, Label: "Check", Value: &checked})
	rt.Progress(ProgressProps{Bounds: Rectangle{Width: 180, Height: 24}, Min: 0, Max: 100, Value: 35, Label: "Loading"})
	rt.Plot(PlotProps{Bounds: Rectangle{Width: 180, Height: 60}, Label: "Trend", Overlay: "Now", Values: []float32{1, 3}, ValueCount: 2})
	rt.Slider(SliderProps{Bounds: Rectangle{Width: 180, Height: 42}, ID: 301, Label: "Gain", FloatValues: sliderValues, ValueCount: 1, Min: 0, Max: 1})
	rt.Toggle(ToggleProps{Bounds: Rectangle{Width: 0, Height: 0}, ID: 302, Value: &toggled, OffLabel: "Off", OnLabel: "On"})
	rt.Drag(DragProps{Bounds: Rectangle{Width: 180, Height: 32}, ID: 303, Label: "Drag", FloatValues: dragValues, ValueCount: 1, Min: 0, Max: 10})
	rt.ListBox(ListBoxProps{Bounds: Rectangle{Width: 180, Height: 54}, ID: 304, Items: []string{"List Item"}, SelectedIndex: &listSelected})
	rt.ListBox(ListBoxProps{Bounds: Rectangle{Width: 180, Height: 54}, ID: 305, Items: []string{"Multi A", "Multi B"}, Selected: multiSelected, SelectedCount: &multiSelectedCount})
	rt.TableView(TableViewProps{Bounds: Rectangle{Width: 220, Height: 78}, ID: 306, Columns: []string{"Column"}, Rows: []TableRow{{Cells: []string{"Cell"}}}, ColumnWidths: []int32{120}, SelectedRow: &tableSelectedRow, SelectedColumn: &tableSelectedColumn})
	rt.Menu(MenuProps{ID: 307, Mode: MenuModeBar, Bounds: Rectangle{Width: 220, Height: 30}, Menus: []MenuGroup{{Label: "File", Items: []MenuItem{{Kind: MenuCommand, Label: "Save", ID: 1}}}}, OpenIndex: &openMenu})
	rt.Toast(ToastProps{Message: "Saved", Seconds: 1})
	rt.EndFrame()

	want := map[string]bool{
		"Title":     false,
		"Body":      false,
		"Docs":      false,
		"Group":     false,
		"Check":     false,
		"Loading":   false,
		"Trend":     false,
		"Now":       false,
		"Gain":      false,
		"Off":       false,
		"On":        false,
		"Drag":      false,
		"List Item": false,
		"Multi A":   false,
		"Multi B":   false,
		"Column":    false,
		"Cell":      false,
		"File":      false,
		"Save":      false,
		"Saved":     false,
	}
	for _, op := range rt.FrameOps() {
		if op.Kind != FrameOpText {
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
