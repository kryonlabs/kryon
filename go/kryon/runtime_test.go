package kryon

import (
	"fmt"
	"image"
	"image/color"
	"os"
	"path/filepath"
	"testing"
)

func TestTextFieldCursorNavigationAndUnicodeInput(t *testing.T) {
	rt := New(AppConfig{Width: 320, Height: 200}).(*runtime)
	text := make([]byte, 64)
	copy(text, "abcde")
	cursor := int32(len("abcde"))
	focused := true

	rt.TextField(TextFieldProps{
		Text:           text,
		CursorPosition: &cursor,
		Focused:        &focused,
		FocusID:        7,
		MaxCodepoints:  63,
	})
	rt.QueueKey(KeyLeft)
	rt.QueueText("é")
	rt.TextField(TextFieldProps{
		Text:           text,
		CursorPosition: &cursor,
		Focused:        &focused,
		FocusID:        7,
		MaxCodepoints:  63,
	})

	if got, want := string(text[:zeroIndex(text)]), "abcdée"; got != want {
		t.Fatalf("text = %q, want %q", got, want)
	}
	if got, want := cursor, int32(len("abcdé")); got != want {
		t.Fatalf("cursor = %d, want %d", got, want)
	}
	if !focused {
		t.Fatal("field lost focus")
	}
}

func TestTextFieldCommitAndDelete(t *testing.T) {
	rt := New(AppConfig{}).(*runtime)
	text := make([]byte, 16)
	copy(text, "abc")
	cursor := int32(1)
	focused := true
	commit := false

	rt.QueueKey(KeyDelete)
	rt.QueueKey(KeyEnter)
	rt.TextField(TextFieldProps{
		Text:           text,
		CursorPosition: &cursor,
		Focused:        &focused,
		CommitPressed:  &commit,
		FocusID:        9,
		MaxCodepoints:  15,
	})

	if got, want := string(text[:zeroIndex(text)]), "ac"; got != want {
		t.Fatalf("text = %q, want %q", got, want)
	}
	if !commit {
		t.Fatal("enter did not set CommitPressed")
	}
}

func TestTextFieldTabTraversal(t *testing.T) {
	rt := New(AppConfig{}).(*runtime)
	aText, bText := make([]byte, 8), make([]byte, 8)
	aCursor, bCursor := int32(0), int32(0)
	aFocused, bFocused := true, false

	draw := func() {
		rt.BeginFrame()
		rt.TextField(TextFieldProps{Text: aText, CursorPosition: &aCursor, Focused: &aFocused, FocusID: 1})
		rt.TextField(TextFieldProps{Text: bText, CursorPosition: &bCursor, Focused: &bFocused, FocusID: 2})
		rt.EndFrame()
	}

	draw()
	rt.QueueKey(KeyTab)
	draw()
	if aFocused || !bFocused {
		t.Fatalf("tab focus = a:%v b:%v, want a:false b:true", aFocused, bFocused)
	}
	rt.QueueShiftKey(KeyTab)
	draw()
	draw()
	if !aFocused || bFocused {
		t.Fatalf("shift-tab focus = a:%v b:%v, want a:true b:false", aFocused, bFocused)
	}
}

func TestTapFocusesTextField(t *testing.T) {
	rt := New(AppConfig{}).(*runtime)
	aText, bText := make([]byte, 16), make([]byte, 16)
	aCursor, bCursor := int32(0), int32(0)
	aFocused, bFocused := false, false

	draw := func() {
		rt.BeginFrame()
		rt.TextField(TextFieldProps{
			Bounds:         Rectangle{X: 10, Y: 10, Width: 120, Height: 28},
			Text:           aText,
			CursorPosition: &aCursor,
			Focused:        &aFocused,
			FocusID:        101,
		})
		rt.TextField(TextFieldProps{
			Bounds:         Rectangle{X: 10, Y: 50, Width: 120, Height: 28},
			Text:           bText,
			CursorPosition: &bCursor,
			Focused:        &bFocused,
			FocusID:        102,
		})
		rt.EndFrame()
	}

	draw()
	rt.QueueTap(20, 62)
	draw()
	if aFocused || !bFocused {
		t.Fatalf("tap focus = a:%v b:%v, want a:false b:true", aFocused, bFocused)
	}
	rt.QueueText("z")
	draw()
	if got, want := string(bText[:zeroIndex(bText)]), "z"; got != want {
		t.Fatalf("typed focused field = %q, want %q", got, want)
	}
}

func TestTextFieldWidgetWorkflowBackspaceCommitAndFocusSwitch(t *testing.T) {
	rt := New(AppConfig{Width: 320, Height: 180}).(*runtime)
	first, second := make([]byte, 32), make([]byte, 32)
	copy(first, "first")
	copy(second, "second")
	firstCursor, secondCursor := int32(len("first")), int32(len("second"))
	firstFocused, secondFocused := true, false
	firstCommit, secondCommit := false, false

	draw := func() {
		rt.BeginFrame()
		rt.TextField(TextFieldProps{
			Bounds:         Rectangle{X: 20, Y: 20, Width: 180, Height: 30},
			Text:           first,
			CursorPosition: &firstCursor,
			Focused:        &firstFocused,
			CommitPressed:  &firstCommit,
			FocusID:        501,
			MaxCodepoints:  31,
			Font:           Text16,
		})
		rt.TextField(TextFieldProps{
			Bounds:         Rectangle{X: 20, Y: 64, Width: 180, Height: 30},
			Text:           second,
			CursorPosition: &secondCursor,
			Focused:        &secondFocused,
			CommitPressed:  &secondCommit,
			FocusID:        502,
			MaxCodepoints:  31,
			Font:           Text16,
		})
		rt.EndFrame()
	}

	draw()
	rt.QueueText("XYZ")
	draw()
	if got, want := string(first[:zeroIndex(first)]), "firstXYZ"; got != want {
		t.Fatalf("first typed text = %q, want %q", got, want)
	}

	rt.QueueKey(KeyBackspace)
	draw()
	if got, want := string(first[:zeroIndex(first)]), "firstXY"; got != want {
		t.Fatalf("first after backspace = %q, want %q", got, want)
	}
	if got, want := firstCursor, int32(len("firstXY")); got != want {
		t.Fatalf("first cursor after backspace = %d, want %d", got, want)
	}

	rt.QueueKey(KeyEnter)
	draw()
	if !firstCommit {
		t.Fatal("enter did not commit the focused first field")
	}

	rt.QueueTap(84, 74)
	draw()
	if firstFocused || !secondFocused {
		t.Fatalf("focus after tapping second field = first:%v second:%v, want first:false second:true", firstFocused, secondFocused)
	}

	rt.QueueKey(KeyBackspace)
	draw()
	if got, want := string(second[:zeroIndex(second)]), "secon"; got != want {
		t.Fatalf("second after backspace = %q, want %q", got, want)
	}
	if got, want := string(first[:zeroIndex(first)]), "firstXY"; got != want {
		t.Fatalf("first changed while second focused = %q, want %q", got, want)
	}

	rt.QueueText("d!")
	draw()
	if got, want := string(second[:zeroIndex(second)]), "second!"; got != want {
		t.Fatalf("second typed text = %q, want %q", got, want)
	}
}

func TestButtonConsumesTapInsideBounds(t *testing.T) {
	rt := New(AppConfig{}).(*runtime)

	rt.BeginFrame()
	rt.QueueTap(40, 25)
	clicked := rt.Button(ButtonProps{
		Bounds: Rectangle{X: 20, Y: 10, Width: 80, Height: 32},
		Label:  "Save",
		ID:     201,
	})
	missed := rt.Button(ButtonProps{
		Bounds: Rectangle{X: 120, Y: 10, Width: 80, Height: 32},
		Label:  "Cancel",
		ID:     202,
	})
	rt.EndFrame()

	if !clicked {
		t.Fatal("tap inside first button did not click")
	}
	if missed {
		t.Fatal("tap was not consumed by first matching button")
	}
}

func TestTextFieldSelectionClipboardAndSecureMode(t *testing.T) {
	rt := New(AppConfig{}).(*runtime)
	text := make([]byte, 64)
	copy(text, "abcdef")
	cursor := int32(6)
	focused := true

	rt.SetSelection(11, 1, 4)
	rt.QueueText("XY")
	rt.TextField(TextFieldProps{Text: text, CursorPosition: &cursor, Focused: &focused, FocusID: 11, MaxCodepoints: 63})
	if got, want := string(text[:zeroIndex(text)]), "aXYef"; got != want {
		t.Fatalf("selection replace = %q, want %q", got, want)
	}

	rt.SetSelection(11, 1, 3)
	rt.QueueShortcut(KeyC)
	rt.TextField(TextFieldProps{Text: text, CursorPosition: &cursor, Focused: &focused, FocusID: 11, MaxCodepoints: 63})
	if got, want := rt.ClipboardText(), "XY"; got != want {
		t.Fatalf("clipboard after copy = %q, want %q", got, want)
	}

	rt.QueueShortcut(KeyX)
	rt.TextField(TextFieldProps{Text: text, CursorPosition: &cursor, Focused: &focused, FocusID: 11, MaxCodepoints: 63})
	if got, want := string(text[:zeroIndex(text)]), "aef"; got != want {
		t.Fatalf("cut text = %q, want %q", got, want)
	}

	rt.QueueShortcut(KeyV)
	rt.TextField(TextFieldProps{Text: text, CursorPosition: &cursor, Focused: &focused, FocusID: 11, MaxCodepoints: 63})
	if got, want := string(text[:zeroIndex(text)]), "aXYef"; got != want {
		t.Fatalf("paste text = %q, want %q", got, want)
	}

	rt.SetClipboardText("old")
	rt.SetSelection(11, 1, 3)
	rt.QueueShortcut(KeyC)
	rt.TextField(TextFieldProps{Text: text, CursorPosition: &cursor, Focused: &focused, FocusID: 11, MaxCodepoints: 63, Secure: true})
	if got, want := rt.ClipboardText(), "old"; got != want {
		t.Fatalf("secure copy changed clipboard = %q, want %q", got, want)
	}
}

func TestTextFieldLongTypingDoesNotGrowFieldOrder(t *testing.T) {
	rt := New(AppConfig{}).(*runtime)
	text := make([]byte, 8192)
	cursor := int32(0)
	focused := true

	for i := 0; i < 3000; i++ {
		rt.BeginFrame()
		rt.QueueText("a")
		rt.TextField(TextFieldProps{Text: text, CursorPosition: &cursor, Focused: &focused, FocusID: 31, MaxCodepoints: 8191})
		rt.EndFrame()
	}
	if got, want := len(string(text[:zeroIndex(text)])), 3000; got != want {
		t.Fatalf("typed length = %d, want %d", got, want)
	}
	if got, want := len(rt.prevOrder), 1; got != want {
		t.Fatalf("field order length = %d, want %d", got, want)
	}
}

func TestPackageInputHelpersDriveActiveRuntime(t *testing.T) {
	rt := New(AppConfig{}).(*runtime)
	SetRuntime(rt)
	defer SetRuntime(nil)

	text := make([]byte, 32)
	cursor := int32(0)
	focused := false

	BeginFrame()
	TextField(TextFieldProps{
		Bounds:         Rectangle{X: 10, Y: 10, Width: 140, Height: 28},
		Text:           text,
		CursorPosition: &cursor,
		Focused:        &focused,
		FocusID:        71,
		MaxCodepoints:  31,
	})
	EndFrame()

	QueueTap(20, 20)
	QueueText("abc")
	BeginFrame()
	TextField(TextFieldProps{
		Bounds:         Rectangle{X: 10, Y: 10, Width: 140, Height: 28},
		Text:           text,
		CursorPosition: &cursor,
		Focused:        &focused,
		FocusID:        71,
		MaxCodepoints:  31,
	})
	EndFrame()

	if got, want := string(text[:zeroIndex(text)]), "abc"; got != want {
		t.Fatalf("package QueueText result = %q, want %q", got, want)
	}
	if !focused {
		t.Fatal("package QueueTap did not focus field")
	}

	SetSelection(71, 0, 3)
	QueueShortcut(KeyC)
	BeginFrame()
	TextField(TextFieldProps{
		Bounds:         Rectangle{X: 10, Y: 10, Width: 140, Height: 28},
		Text:           text,
		CursorPosition: &cursor,
		Focused:        &focused,
		FocusID:        71,
		MaxCodepoints:  31,
	})
	EndFrame()

	if got, want := ClipboardText(), "abc"; got != want {
		t.Fatalf("package clipboard = %q, want %q", got, want)
	}

	QueueTap(25, 70)
	BeginFrame()
	clicked := Button(ButtonProps{
		Bounds: Rectangle{X: 10, Y: 58, Width: 90, Height: 32},
		Label:  "Save",
		ID:     72,
	})
	EndFrame()
	if !clicked {
		t.Fatal("package QueueTap did not click button")
	}
}

func TestColumnPlacesZeroOriginFields(t *testing.T) {
	rt := New(AppConfig{}).(*runtime)
	aText, bText := make([]byte, 16), make([]byte, 16)
	aCursor, bCursor := int32(0), int32(0)
	aFocused, bFocused := false, false

	draw := func() {
		rt.BeginFrame()
		rt.Column(ColumnProps{
			Bounds:  Rectangle{X: 10, Y: 20, Width: 180, Height: 120},
			Gap:     4,
			Padding: 5,
		})
		rt.TextField(TextFieldProps{
			Bounds:         Rectangle{Width: 100, Height: 20},
			Text:           aText,
			CursorPosition: &aCursor,
			Focused:        &aFocused,
			FocusID:        81,
		})
		rt.TextField(TextFieldProps{
			Bounds:         Rectangle{Width: 100, Height: 20},
			Text:           bText,
			CursorPosition: &bCursor,
			Focused:        &bFocused,
			FocusID:        82,
		})
		rt.End()
		rt.EndFrame()
	}

	draw()
	rt.QueueTap(20, 54)
	rt.QueueText("b")
	draw()

	if aFocused || !bFocused {
		t.Fatalf("column tap focus = a:%v b:%v, want a:false b:true", aFocused, bFocused)
	}
	if got, want := string(bText[:zeroIndex(bText)]), "b"; got != want {
		t.Fatalf("column-placed field text = %q, want %q", got, want)
	}
}

func TestRowPlacesZeroOriginButtons(t *testing.T) {
	rt := New(AppConfig{}).(*runtime)

	rt.QueueTap(108, 24)
	rt.BeginFrame()
	rt.Row(ColumnProps{
		Bounds:  Rectangle{X: 10, Y: 10, Width: 240, Height: 40},
		Gap:     8,
		Padding: 4,
	})
	first := rt.Button(ButtonProps{
		Bounds: Rectangle{Width: 80, Height: 28},
		Label:  "Save",
		ID:     91,
	})
	second := rt.Button(ButtonProps{
		Bounds: Rectangle{Width: 80, Height: 28},
		Label:  "Cancel",
		ID:     92,
	})
	rt.End()
	rt.EndFrame()

	if first {
		t.Fatal("row tap clicked first button, want second")
	}
	if !second {
		t.Fatal("row tap did not click second button")
	}
}

func TestDirectPackageButtonString(t *testing.T) {
	rt := New(AppConfig{}).(*runtime)
	SetRuntime(rt)
	defer SetRuntime(nil)

	QueueTap(36, 12)
	BeginFrame()
	clicked := Button("Save")
	EndFrame()

	if !clicked {
		t.Fatal("direct Button string did not consume tap")
	}
}

func TestDirectPackageTextFieldStringKeepsCursorState(t *testing.T) {
	rt := New(AppConfig{}).(*runtime)
	SetRuntime(rt)
	defer SetRuntime(nil)

	value := "abc"
	BeginFrame()
	TextField("Name", &value)
	EndFrame()

	QueueTap(36, 12)
	QueueKey(KeyLeft)
	QueueText("Z")
	BeginFrame()
	changed := TextField("Name", &value)
	EndFrame()

	if !changed {
		t.Fatal("direct TextField string did not report change")
	}
	if got, want := value, "abZc"; got != want {
		t.Fatalf("direct TextField value = %q, want %q", got, want)
	}
}

func TestDirectPackageTextFieldStateIsFrameScoped(t *testing.T) {
	rt := New(AppConfig{}).(*runtime)
	SetRuntime(rt)
	defer SetRuntime(nil)

	first := "one"
	second := "two"

	BeginFrame()
	TextField("First", &first)
	EndFrame()
	if got, want := len(directTextFields), 1; got != want {
		t.Fatalf("direct field states = %d, want %d", got, want)
	}

	BeginFrame()
	TextField("Second", &second)
	EndFrame()
	if got, want := len(directTextFields), 1; got != want {
		t.Fatalf("direct field states after swap = %d, want %d", got, want)
	}

	for i := 0; i < 2000; i++ {
		QueueTap(36, 12)
		QueueText("a")
		BeginFrame()
		TextField("Second", &second)
		EndFrame()
	}
	if got, want := len(directTextFields), 1; got != want {
		t.Fatalf("direct field states after long typing = %d, want %d", got, want)
	}
	if got, want := len(second), len("two")+2000; got != want {
		t.Fatalf("direct field length = %d, want %d", got, want)
	}
}

func TestFrameOpsRecordRenderableNativeFrame(t *testing.T) {
	rt := New(AppConfig{}).(*runtime)
	text := make([]byte, 32)
	copy(text, "secret")
	cursor := int32(len("secret"))
	focused := true

	rt.QueueTap(18, 42)
	rt.BeginFrame()
	rt.ClearBackground(WHITE)
	rt.Column(ColumnProps{Bounds: Rectangle{X: 10, Y: 10, Width: 180, Height: 160}, Gap: 6, Padding: 4, Key: Key("ops")})
	rt.Text("geld", 0, 0, Text16, BLACK)
	clicked := rt.Button(ButtonProps{Bounds: Rectangle{Width: 90, Height: 28}, Label: "Save", ID: 7, Font: Text16})
	rt.TextField(TextFieldProps{
		Bounds:         Rectangle{Width: 120, Height: 28},
		Text:           text,
		CursorPosition: &cursor,
		Focused:        &focused,
		FocusID:        8,
		Font:           Text16,
		Secure:         true,
	})
	rt.End()
	rt.EndFrame()

	if !clicked {
		t.Fatal("tap did not click recorded button")
	}
	ops := rt.FrameOps()
	if len(ops) != 6 {
		t.Fatalf("frame op count = %d, want 6: %#v", len(ops), ops)
	}
	if ops[0].Kind != FrameOpBackground || ops[0].Color != WHITE {
		t.Fatalf("background op = %#v", ops[0])
	}
	if ops[1].Kind != FrameOpColumn || ops[1].Bounds.X != 10 || ops[1].Bounds.Y != 10 {
		t.Fatalf("column op = %#v", ops[1])
	}
	if ops[2].Kind != FrameOpText || ops[2].Text != "geld" {
		t.Fatalf("text op = %#v", ops[2])
	}
	if ops[3].Kind != FrameOpButton || ops[3].Text != "Save" || !ops[3].Pressed {
		t.Fatalf("button op = %#v", ops[3])
	}
	if ops[4].Kind != FrameOpTextField || ops[4].Text != "******" || !ops[4].Secure {
		t.Fatalf("text field op = %#v", ops[4])
	}
	if ops[5].Kind != FrameOpEnd {
		t.Fatalf("end op = %#v", ops[5])
	}
}

func TestFrameOpsResetEachFrame(t *testing.T) {
	rt := New(AppConfig{}).(*runtime)

	rt.BeginFrame()
	rt.Text("first", 10, 10, Text16, BLACK)
	rt.EndFrame()
	if got, want := len(rt.FrameOps()), 1; got != want {
		t.Fatalf("first frame op count = %d, want %d", got, want)
	}

	rt.BeginFrame()
	rt.Rect(1, 2, 3, 4, RED)
	rt.EndFrame()
	ops := rt.FrameOps()
	if len(ops) != 1 || ops[0].Kind != FrameOpRect {
		t.Fatalf("second frame ops = %#v, want one rect", ops)
	}
}

func TestTableViewSelectionActivationAndSort(t *testing.T) {
	rt := New(AppConfig{Width: 360, Height: 220}).(*runtime)
	selectedRow := int32(-1)
	selectedColumn := int32(-1)
	activatedRow := int32(-1)
	activatedColumn := int32(-1)
	rightRow := int32(-1)
	rightColumn := int32(-1)
	sortColumn := int32(-1)
	scroll := int32(0)
	props := TableViewProps{
		Bounds:             Rectangle{X: 10, Y: 10, Width: 300, Height: 140},
		ID:                 41,
		Columns:            []string{"section", "label", "units"},
		Rows:               []UITableRow{{Cells: []string{"banks", "checking", "10"}}, {Cells: []string{"cash", "wallet", "5"}}},
		ColumnWidths:       []int32{90, 140, 70},
		SelectedRow:        &selectedRow,
		SelectedColumn:     &selectedColumn,
		ActivatedRow:       &activatedRow,
		ActivatedColumn:    &activatedColumn,
		RightClickedRow:    &rightRow,
		RightClickedColumn: &rightColumn,
		SortColumn:         &sortColumn,
		ScrollOffset:       &scroll,
		RowHeight:          24,
	}

	rt.QueueTap(116, 52)
	rt.BeginFrame()
	changed := rt.TableView(props)
	rt.EndFrame()
	if changed == 0 {
		t.Fatal("table click did not report change")
	}
	if selectedRow != 0 || selectedColumn != 1 {
		t.Fatalf("selected cell = %d,%d, want 0,1", selectedRow, selectedColumn)
	}
	if rt.Focus() != 41 {
		t.Fatalf("table focus = %d, want 41", rt.Focus())
	}

	rt.QueueTap(116, 52)
	rt.BeginFrame()
	rt.TableView(props)
	rt.EndFrame()
	if activatedRow != 0 || activatedColumn != 1 {
		t.Fatalf("activated cell = %d,%d, want 0,1", activatedRow, activatedColumn)
	}

	activatedRow, activatedColumn = -1, -1
	rt.QueueTap(116, 52)
	rt.QueueTap(116, 52)
	rt.BeginFrame()
	rt.TableView(props)
	rt.EndFrame()
	if activatedRow != 0 || activatedColumn != 1 {
		t.Fatalf("batched double-click activated cell = %d,%d, want 0,1", activatedRow, activatedColumn)
	}

	rt.QueueMouseButton(MouseButtonRight, 260, 76)
	rt.BeginFrame()
	rt.TableView(props)
	rt.EndFrame()
	if rightRow != 1 || rightColumn != 2 {
		t.Fatalf("right-clicked cell = %d,%d, want 1,2", rightRow, rightColumn)
	}

	rt.QueueTap(240, 20)
	rt.BeginFrame()
	rt.TableView(props)
	rt.EndFrame()
	if sortColumn != 2 {
		t.Fatalf("sort column = %d, want 2", sortColumn)
	}
	if selectedRow != -1 || selectedColumn != 2 {
		t.Fatalf("header selection = %d,%d, want -1,2", selectedRow, selectedColumn)
	}
}

func TestTableViewSelectionPaintsCellNotWholeRow(t *testing.T) {
	rt := New(AppConfig{Width: 360, Height: 220}).(*runtime)
	selectedRow := int32(0)
	selectedColumn := int32(1)
	scroll := int32(0)
	props := TableViewProps{
		Bounds:         Rectangle{X: 10, Y: 10, Width: 300, Height: 140},
		ID:             42,
		Columns:        []string{"section", "label", "units"},
		Rows:           []UITableRow{{Cells: []string{"banks", "checking", "10"}}},
		ColumnWidths:   []int32{90, 140, 70},
		SelectedRow:    &selectedRow,
		SelectedColumn: &selectedColumn,
		ScrollOffset:   &scroll,
		RowHeight:      24,
	}

	rt.BeginFrame()
	rt.TableView(props)
	rt.EndFrame()

	var selectedCell bool
	for _, op := range rt.FrameOps() {
		if op.Kind != FrameOpRect || !op.Selected {
			continue
		}
		if op.Row == 0 && op.Column == 1 && op.Bounds.Width == 140 {
			selectedCell = true
			continue
		}
		if op.Row == 0 && op.Bounds.Width == props.Bounds.Width {
			t.Fatalf("table painted a full selected row: %#v", op)
		}
	}
	if !selectedCell {
		t.Fatal("table did not paint the selected cell")
	}
}

func TestTableViewPaintsFullRowAndColumnSelections(t *testing.T) {
	rt := New(AppConfig{Width: 360, Height: 220}).(*runtime)
	selectedRow := int32(1)
	selectedColumn := int32(-1)
	scroll := int32(0)
	props := TableViewProps{
		Bounds:         Rectangle{X: 10, Y: 10, Width: 300, Height: 140},
		ID:             43,
		Columns:        []string{"#", "A", "B"},
		Rows:           []UITableRow{{Cells: []string{"1", "cash", "10"}}, {Cells: []string{"2", "bank", "20"}}},
		ColumnWidths:   []int32{40, 140, 120},
		SelectedRow:    &selectedRow,
		SelectedColumn: &selectedColumn,
		ScrollOffset:   &scroll,
		RowHeight:      24,
	}

	rt.BeginFrame()
	rt.TableView(props)
	rt.EndFrame()
	var fullRow bool
	for _, op := range rt.FrameOps() {
		if op.Kind == FrameOpRect && op.Selected && op.Row == 1 && op.Column == -1 && op.Bounds.Width == props.Bounds.Width {
			fullRow = true
		}
	}
	if !fullRow {
		t.Fatal("table did not paint the full selected row")
	}

	selectedRow = -1
	selectedColumn = 2
	rt.BeginFrame()
	rt.TableView(props)
	rt.EndFrame()
	headerSelected, bodySelected := false, false
	for _, op := range rt.FrameOps() {
		if op.Kind != FrameOpRect || !op.Selected || op.Column != 2 {
			continue
		}
		if op.Row == -1 {
			headerSelected = true
		}
		if op.Row == 0 || op.Row == 1 {
			bodySelected = true
		}
	}
	if !headerSelected || !bodySelected {
		t.Fatalf("table full-column selection missing header/body: header=%v body=%v", headerSelected, bodySelected)
	}
}

func TestTableViewKeyboardNavigationScrollAndRendering(t *testing.T) {
	rt := New(AppConfig{Width: 360, Height: 260}).(*runtime)
	selectedRow := int32(0)
	selectedColumn := int32(0)
	activatedRow := int32(-1)
	activatedColumn := int32(-1)
	scroll := int32(0)
	rows := make([]UITableRow, 20)
	for i := range rows {
		rows[i] = UITableRow{Cells: []string{"section", fmt.Sprintf("row %d", i), fmt.Sprintf("%d", i)}}
	}
	props := TableViewProps{
		Bounds:          Rectangle{X: 10, Y: 10, Width: 310, Height: 118},
		ID:              51,
		Columns:         []string{"section", "label", "units"},
		Rows:            rows,
		ColumnWidths:    []int32{90, 150, 70},
		SelectedRow:     &selectedRow,
		SelectedColumn:  &selectedColumn,
		ActivatedRow:    &activatedRow,
		ActivatedColumn: &activatedColumn,
		ScrollOffset:    &scroll,
		RowHeight:       22,
	}

	rt.SetFocus(51)
	rt.QueueKey(KeyDown)
	rt.QueueKey(KeyRight)
	rt.QueueKey(KeyF2)
	rt.BeginFrame()
	rt.ClearBackground(RAYWHITE)
	rt.TableView(props)
	rt.EndFrame()
	if selectedRow != 1 || selectedColumn != 1 {
		t.Fatalf("keyboard selected cell = %d,%d, want 1,1", selectedRow, selectedColumn)
	}
	if activatedRow != 1 || activatedColumn != 1 {
		t.Fatalf("keyboard activated cell = %d,%d, want 1,1", activatedRow, activatedColumn)
	}

	rt.QueueMouseWheel(-2)
	rt.mousePos = Vector2{X: 40, Y: 70}
	rt.BeginFrame()
	rt.TableView(props)
	rt.EndFrame()
	if scroll == 0 {
		t.Fatal("mouse wheel inside table did not scroll")
	}

	ops := rt.FrameOps()
	seenTable, seenCell := false, false
	for _, op := range ops {
		if op.Kind == FrameOpTable && op.ID == 51 {
			seenTable = true
		}
		if op.Kind == FrameOpText && op.Row >= 0 && op.Column == 1 {
			seenCell = true
		}
	}
	if !seenTable || !seenCell {
		t.Fatalf("table frame ops missing table/cell: table=%v cell=%v ops=%#v", seenTable, seenCell, ops)
	}

	img := RenderFrame(360, 260, ops)
	if got := countPixelsNot(img, rgbaTest(RAYWHITE)); got < 1000 {
		t.Fatalf("rendered table changed only %d pixels, want visible table", got)
	}
}

func TestTableViewUsesSystemThemeByDefault(t *testing.T) {
	resetSystemThemeForTest()
	defer resetSystemThemeForTest()
	t.Setenv("KRYON_THEME_MODE", "dark")
	t.Setenv("GTK_THEME", "KryonMissingTheme")
	rt := New(AppConfig{Width: 240, Height: 160}).(*runtime)
	selectedRow := int32(0)
	selectedColumn := int32(0)
	props := TableViewProps{
		Bounds:         Rectangle{X: 10, Y: 10, Width: 180, Height: 80},
		ID:             61,
		Columns:        []string{"label"},
		Rows:           []UITableRow{{Cells: []string{"row"}}},
		SelectedRow:    &selectedRow,
		SelectedColumn: &selectedColumn,
		RowHeight:      24,
	}

	rt.BeginFrame()
	rt.TableView(props)
	rt.EndFrame()

	if got, want := rt.GetThemeBackground(), (Color{0x14, 0x12, 0x18, 255}); got != want {
		t.Fatalf("default system dark background = %#v, want %#v", got, want)
	}
	ops := rt.FrameOps()
	for _, op := range ops {
		if op.Kind == FrameOpRect && op.Bounds == props.Bounds {
			if op.Color == WHITE || op.Color == RAYWHITE {
				t.Fatalf("table used hard-coded light surface under system dark theme: %#v", op)
			}
			return
		}
	}
	t.Fatalf("table surface op not found: %#v", ops)
}

func TestSystemThemeReadsXFCEXSettingsAndGTKCSS(t *testing.T) {
	resetSystemThemeForTest()
	defer resetSystemThemeForTest()
	temp := t.TempDir()
	config := filepath.Join(temp, "config")
	themeDir := filepath.Join(temp, "home", ".themes", "Demo-Dark", "gtk-3.0")
	if err := os.MkdirAll(filepath.Join(config, "xfce4", "xfconf", "xfce-perchannel-xml"), 0o755); err != nil {
		t.Fatal(err)
	}
	if err := os.MkdirAll(themeDir, 0o755); err != nil {
		t.Fatal(err)
	}
	xsettings := `<channel name="xsettings" version="1.0"><property name="Net" type="empty"><property name="ThemeName" type="string" value="Demo-Dark"/></property></channel>`
	if err := os.WriteFile(filepath.Join(config, "xfce4", "xfconf", "xfce-perchannel-xml", "xsettings.xml"), []byte(xsettings), 0o644); err != nil {
		t.Fatal(err)
	}
	css := `
@define-color theme_fg_color #C3C7D1;
@define-color theme_bg_color #161925;
@define-color theme_base_color #181b28;
@define-color theme_selected_bg_color #c50ed2;
@define-color theme_selected_fg_color #fefefe;
@define-color borders #090a0f;
`
	if err := os.WriteFile(filepath.Join(themeDir, "gtk.css"), []byte(css), 0o644); err != nil {
		t.Fatal(err)
	}
	t.Setenv("HOME", filepath.Join(temp, "home"))
	t.Setenv("XDG_CONFIG_HOME", config)
	t.Setenv("XDG_DATA_HOME", filepath.Join(temp, "data"))
	t.Setenv("GTK_THEME", "")
	t.Setenv("KRYON_THEME_MODE", "")

	rt := New(AppConfig{Width: 240, Height: 160}).(*runtime)
	if got, want := rt.GetThemeBackground(), (Color{0x18, 0x1b, 0x28, 0xff}); got != want {
		t.Fatalf("system CSS background = %#v, want %#v", got, want)
	}
	if got, want := rt.GetThemeSurface(), (Color{0x16, 0x19, 0x25, 0xff}); got != want {
		t.Fatalf("system CSS surface = %#v, want %#v", got, want)
	}
	if got, want := rt.GetThemeLink(), (Color{0xc5, 0x0e, 0xd2, 0xff}); got != want {
		t.Fatalf("system CSS selected/link = %#v, want %#v", got, want)
	}
	theme := rt.theme()
	if got, want := theme.selected, (Color{0xc5, 0x0e, 0xd2, 0xff}); got != want {
		t.Fatalf("system CSS selected = %#v, want %#v", got, want)
	}
	if got, want := theme.selectedHot, (Color{0xc5, 0x0e, 0xd2, 0xff}); got != want {
		t.Fatalf("system CSS selected hot = %#v, want %#v", got, want)
	}
	if got, want := theme.selectedText, (Color{0xfe, 0xfe, 0xfe, 0xff}); got != want {
		t.Fatalf("system CSS selected text = %#v, want %#v", got, want)
	}
	if got, want := theme.border, (Color{0x09, 0x0a, 0x0f, 0xff}); got != want {
		t.Fatalf("system CSS border = %#v, want %#v", got, want)
	}
	if got, want := theme.focus, (Color{0xc5, 0x0e, 0xd2, 0xff}); got != want {
		t.Fatalf("system CSS focus = %#v, want %#v", got, want)
	}
	if !systemPrefersDark() {
		t.Fatal("system CSS palette should prefer dark")
	}
}

func TestTextFieldFrameOpsCarryThemeSelectionColors(t *testing.T) {
	rt := New(AppConfig{Width: 240, Height: 160}).(*runtime)
	rt.SetThemeSource(ThemeSourceApp)
	rt.SetCurrentTheme(int32(ThemeCobalt), 1)
	SetRuntime(rt)
	defer SetRuntime(nil)

	text := make([]byte, 32)
	copy(text, "abcde")
	cursor := int32(4)
	focused := true
	rt.SetSelection(77, 1, 4)

	BeginFrame()
	TextField(TextFieldProps{
		Bounds:         Rectangle{X: 10, Y: 10, Width: 160, Height: 32},
		Text:           text,
		CursorPosition: &cursor,
		Focused:        &focused,
		FocusID:        77,
		Font:           Text16,
	})
	EndFrame()

	ops := FrameOps()
	for _, op := range ops {
		if op.Kind != FrameOpTextField {
			continue
		}
		if got, want := op.BorderColor, rt.theme().focus; got != want {
			t.Fatalf("focused field border = %#v, want focus %#v", got, want)
		}
		if got, want := op.SelectionColor, rt.theme().selectedHot; got != want {
			t.Fatalf("selection color = %#v, want %#v", got, want)
		}
		if got, want := op.SelectedTextColor, rt.theme().selectedText; got != want {
			t.Fatalf("selected text color = %#v, want %#v", got, want)
		}
		if op.SelectionStart != 1 || op.SelectionEnd != 4 {
			t.Fatalf("selection range = %d..%d, want 1..4", op.SelectionStart, op.SelectionEnd)
		}
		return
	}
	t.Fatalf("text field op not found: %#v", ops)
}

func TestAppThemeCatalogHonorsThemeID(t *testing.T) {
	resetSystemThemeForTest()
	defer resetSystemThemeForTest()
	rt := New(AppConfig{Width: 240, Height: 160}).(*runtime)
	rt.SetThemeSource(ThemeSourceApp)
	rt.SetCurrentTheme(int32(ThemeMint), 1)

	if got, want := rt.GetThemeBackground(), (Color{0x12, 0x2d, 0x28, 0xff}); got != want {
		t.Fatalf("mint dark background = %#v, want %#v", got, want)
	}
	if got, want := rt.GetThemeButton(), (Color{0x25, 0x55, 0x4b, 0xff}); got != want {
		t.Fatalf("mint dark button = %#v, want %#v", got, want)
	}
	rt.SetCurrentTheme(999, 0)
	if got, want := rt.GetThemeBackground(), (Color{0xc0, 0xc0, 0xc0, 0xff}); got != want {
		t.Fatalf("out-of-range theme background = %#v, want default mono %#v", got, want)
	}
}

func TestRenderCurrentFramePaintsNativeOps(t *testing.T) {
	rt := New(AppConfig{Width: 240, Height: 140}).(*runtime)
	SetRuntime(rt)
	defer SetRuntime(nil)

	text := make([]byte, 32)
	copy(text, "demo")
	cursor := int32(4)
	focused := true

	BeginFrame()
	ClearBackground(RAYWHITE)
	Text("geld", 12, 12, Text16, BLACK)
	Button(ButtonProps{Bounds: Rectangle{X: 12, Y: 40, Width: 82, Height: 28}, Label: "Save", ID: 1, Font: Text16})
	TextField(TextFieldProps{
		Bounds:         Rectangle{X: 12, Y: 82, Width: 140, Height: 32},
		Text:           text,
		CursorPosition: &cursor,
		Focused:        &focused,
		FocusID:        2,
		Font:           Text16,
	})
	EndFrame()

	img := RenderCurrentFrame()
	if got := img.Bounds().Dx(); got != 240 {
		t.Fatalf("render width = %d, want 240", got)
	}
	if got := img.Bounds().Dy(); got != 140 {
		t.Fatalf("render height = %d, want 140", got)
	}
	if got := countPixelsNot(img, rgbaTest(RAYWHITE)); got < 700 {
		t.Fatalf("rendered frame changed only %d pixels, want visible native UI", got)
	}
}

func TestTakeScreenshotWritesCurrentFramePNG(t *testing.T) {
	rt := New(AppConfig{Width: 80, Height: 50}).(*runtime)
	SetRuntime(rt)
	defer SetRuntime(nil)

	BeginFrame()
	ClearBackground(WHITE)
	Text("shot", 4, 4, Text16, BLACK)
	EndFrame()

	path := filepath.Join(t.TempDir(), "shot.png")
	TakeScreenshot(path)
	info, err := os.Stat(path)
	if err != nil {
		t.Fatalf("screenshot was not written: %v", err)
	}
	if info.Size() == 0 {
		t.Fatal("screenshot file is empty")
	}
}

func TestRendererHasPortfolioGlyphs(t *testing.T) {
	for _, r := range "Δƒ…€£¥₿↑↓←→" {
		if _, ok := glyphPattern(r); !ok {
			t.Fatalf("renderer missing glyph for %q", r)
		}
	}
}

func TestRendererUsesRegisteredUIFontData(t *testing.T) {
	data, err := os.ReadFile("../../fonts/noto/NotoSans-Regular.ttf")
	if err != nil {
		t.Fatalf("read test font: %v", err)
	}
	if !RegisterUIFontData("test-noto", ".ttf", data, nil) {
		t.Fatal("RegisterUIFontData rejected valid TTF")
	}
	UseUIFont("test-noto")

	img := RenderFrame(220, 80, []FrameOp{
		{Kind: FrameOpBackground, Color: WHITE},
		{Kind: FrameOpText, Bounds: Rectangle{X: 8, Y: 8, Width: 200, Height: 28}, Text: "Geld Δƒ", FontSize: Text24, Color: BLACK},
	})
	if got := countPixelsNot(img, rgbaTest(WHITE)); got < 250 {
		t.Fatalf("registered UI font rendered only %d pixels, want real glyph rasterization", got)
	}

	if measured, fallback := MeasureTextEx(Font{}, "iiii", 24, 1).X, float32(len([]rune("iiii")))*24*0.55; measured == fallback {
		t.Fatalf("MeasureTextEx used fallback width %.2f after registering UI font", measured)
	}
}

func TestRenderFrameClipsOutOfBoundsOps(t *testing.T) {
	img := RenderFrame(32, 24, []FrameOp{
		{Kind: FrameOpBackground, Color: WHITE},
		{Kind: FrameOpRect, Bounds: Rectangle{X: -10, Y: -8, Width: 18, Height: 16}, Color: BLUE},
		{Kind: FrameOpLine, Bounds: Rectangle{X: -4, Y: 23, Width: 40, Height: -30}, Color: RED},
		{Kind: FrameOpText, Bounds: Rectangle{X: 2, Y: 4, Width: 40, Height: 16}, Text: "A", FontSize: Text16, Color: BLACK},
	})
	if got := img.Bounds().Dx(); got != 32 {
		t.Fatalf("render width = %d, want 32", got)
	}
	if got := countPixelsNot(img, rgbaTest(WHITE)); got == 0 {
		t.Fatal("clipped render produced a blank image")
	}
}

func countPixelsNot(img interface {
	Bounds() image.Rectangle
	RGBAAt(int, int) color.RGBA
}, bg color.RGBA) int {
	count := 0
	bounds := img.Bounds()
	for y := bounds.Min.Y; y < bounds.Max.Y; y++ {
		for x := bounds.Min.X; x < bounds.Max.X; x++ {
			if img.RGBAAt(x, y) != bg {
				count++
			}
		}
	}
	return count
}

func rgbaTest(c Color) color.RGBA {
	return color.RGBA{R: c.R, G: c.G, B: c.B, A: c.A}
}
