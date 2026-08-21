package kryon

import "testing"

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
