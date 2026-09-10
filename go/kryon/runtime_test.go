package kryon

import (
	"fmt"
	"image"
	"image/color"
	"os"
	"path/filepath"
	"reflect"
	"testing"
	"time"
)

func TestFrameClockControlsAnimationTime(t *testing.T) {
	now := time.Time{}
	reads := 0
	rt := New(AppConfig{FrameClock: func() time.Time {
		reads++
		return now
	}}).(*runtime)
	for index, elapsed := range []time.Duration{0, 20 * time.Millisecond, 60 * time.Millisecond} {
		now = time.Time{}.Add(elapsed)
		rt.BeginFrame()
		wantDelta := [...]float32{0, 20, 40}[index]
		if rt.frameDeltaMS != wantDelta || rt.elapsedTime != elapsed {
			t.Fatalf("frame %d: delta=%g elapsed=%v", index, rt.frameDeltaMS, rt.elapsedTime)
		}
		rt.EndFrame()
	}
	if reads != 3 {
		t.Fatalf("clock read %d times for three frames", reads)
	}
	other := New(AppConfig{}).(*runtime)
	other.BeginFrame()
	if other.frameStarted.IsZero() || other.elapsedTime != 0 || reads != 3 {
		t.Fatal("frame clock leaked into another runtime or replaced its default clock")
	}
	other.EndFrame()
}

func TestLoadingClockRetainsSubMillisecondStepsAfterThirtyDays(t *testing.T) {
	epoch := time.Unix(1, 0)
	now := epoch
	rt := New(AppConfig{FrameClock: func() time.Time { return now }}).(*runtime)
	rt.BeginFrame()
	rt.EndFrame()
	for _, fraction := range []time.Duration{250, 500, 750} {
		phase := 997*time.Millisecond + fraction*time.Microsecond
		elapsed := 30*24*time.Hour + phase
		now = epoch.Add(elapsed)
		rt.BeginFrame()
		frame, _ := rt.surfaceButtonFrame(ButtonProps{ID: 1, Loading: true,
			Bounds: Rectangle{Width: 80, Height: 40}}, Rectangle{}, false)
		if rt.elapsedTime != elapsed || frame.ElapsedMS != float64(elapsed)/float64(time.Millisecond) {
			t.Fatalf("long-running clock lost precision: duration=%v frame=%g", rt.elapsedTime, frame.ElapsedMS)
		}
		got := Surface_LoadingRing(80, 40, 18, frame.ElapsedMS, 0x006cffff, 0x092039ff)
		want := Surface_LoadingRing(80, 40, 18, float64(phase)/float64(time.Millisecond), 0x006cffff, 0x092039ff)
		if got != want {
			t.Fatalf("loading phase after thirty days: %+v, want %+v", got, want)
		}
		rt.EndFrame()
	}
}

func TestCStringTrimsFixedBufferAtNUL(t *testing.T) {
	buf := []byte{'s', 'e', 'c', 'r', 'e', 't', 0, 'x'}
	if got, want := CString(buf), "secret"; got != want {
		t.Fatalf("CString() = %q, want %q", got, want)
	}
	if got, want := CString([]byte("plain")), "plain"; got != want {
		t.Fatalf("CString(no nul) = %q, want %q", got, want)
	}
}

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

func TestIconButtonToolbarAndMenuBar(t *testing.T) {
	rt := New(AppConfig{Width: 360, Height: 180}).(*runtime)
	open := int32(-1)
	menus := []Menu{{
		Label: "File",
		Items: []MenuItem{
			{Kind: MenuCommand, Label: "Save", Accelerator: "Ctrl+S", ID: 101},
			{Kind: MenuSeparator},
			{Kind: MenuCommand, Label: "Quit", ID: 102},
		},
	}}
	actions := []ToolbarAction{
		{IconType: UIIconTypeSave},
		{IconType: UIIconTypeWorkbookTextColor},
	}

	rt.QueueTap(18, 16)
	res := rt.MenuBar(10, Rectangle{X: 0, Y: 0, Width: 360, Height: 30}, menus, &open)
	if got, want := res.OpenIndex, int32(0); got != want {
		t.Fatalf("menu open index = %d, want %d", got, want)
	}
	if got, want := open, int32(0); got != want {
		t.Fatalf("open pointer = %d, want %d", got, want)
	}

	rt.QueueTap(20, 43)
	res = rt.MenuBar(10, Rectangle{X: 0, Y: 0, Width: 360, Height: 30}, menus, &open)
	if got, want := res.ActivatedID, int32(101); got != want {
		t.Fatalf("activated menu id = %d, want %d", got, want)
	}
	if got, want := open, int32(-1); got != want {
		t.Fatalf("open after activation = %d, want %d", got, want)
	}

	rt.QueueTap(327, 52)
	toolbar := rt.Toolbar(ToolbarProps{
		ID:                20,
		X:                 0,
		Y:                 34,
		Width:             360,
		Height:            42,
		Actions:           actions,
		ActionIconSize:    16,
		ActionIconPadding: 5,
	})
	if got, want := toolbar.ClickedAction, int32(0); got != want {
		t.Fatalf("clicked toolbar action = %d, want %d", got, want)
	}

	var sawIcon bool
	for _, op := range rt.FrameOps() {
		if op.Kind == FrameOpIcon && op.IconType == UIIconTypeSave {
			sawIcon = true
			break
		}
	}
	if !sawIcon {
		t.Fatalf("toolbar did not record save icon op: %#v", rt.FrameOps())
	}
}

func TestIconRenderDrawsTintedPixels(t *testing.T) {
	img := RenderFrame(48, 48, []FrameOp{{
		Kind:     FrameOpIcon,
		Bounds:   Rectangle{X: 8, Y: 8, Width: 24, Height: 24},
		Color:    Color{R: 210, G: 30, B: 40, A: 255},
		IconType: UIIconTypeWorkbookFillColor,
		IconSize: 24,
	}})
	if got := countPixels(img, color.RGBA{R: 210, G: 30, B: 40, A: 255}); got < 40 {
		t.Fatalf("rendered tinted icon pixels = %d, want visible icon", got)
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

func TestNestedDisabledScopeUsesButtonStyleAndSuppressesInput(t *testing.T) {
	rt := New(AppConfig{}).(*runtime)
	bounds := Rectangle{X: 20, Y: 10, Width: 80, Height: 32}

	rt.BeginFrame()
	rt.QueueTap(40, 25)
	rt.BeginDisabled(true)
	rt.BeginDisabled(false)
	if rt.Button(ButtonProps{Bounds: bounds, Label: "Blocked", ID: 203}) {
		t.Fatal("button activated inside nested disabled scope")
	}
	rt.EndDisabled()
	rt.EndDisabled()
	if !rt.Button(ButtonProps{Bounds: bounds, Label: "Enabled", ID: 204}) {
		t.Fatal("button did not activate after leaving disabled scope")
	}
	ops := rt.FrameOps()
	rt.EndFrame()

	if len(ops) == 0 || !ops[0].Disabled {
		t.Fatal("disabled scope did not mark recorded content disabled")
	}
	want := resolveButtonStyle(rt.theme(), rt.effectiveDark(), rt.activeTheme,
		ButtonProps{Disabled: true}, ButtonStateDisabled)
	if unpackRGBA(ops[0].Button.Appearance.Value.Background) != want.Background || unpackRGBA(ops[0].Button.Appearance.Value.Foreground) != want.Foreground || unpackRGBA(ops[0].Button.Appearance.Value.Border) != want.Border {
		t.Fatal("disabled scope did not use the canonical disabled button style")
	}
	if rt.contentDisabled() {
		t.Fatal("disabled scope remained active after balanced end")
	}
}

func TestDeepDisabledScopes(t *testing.T) {
	rt := New(AppConfig{}).(*runtime)
	for _, outer := range []bool{false, true} {
		rt.BeginDisabled(outer)
		for depth := 0; depth < 130; depth++ {
			rt.BeginDisabled(depth == 100)
		}
		if !rt.contentDisabled() {
			t.Fatal("deep scope did not disable content")
		}
		for depth := 129; depth >= 0; depth-- {
			rt.EndDisabled()
			if got, want := rt.contentDisabled(), outer || depth > 100; got != want {
				t.Fatalf("outer=%v depth=%d: disabled=%v, want %v", outer, depth, got, want)
			}
		}
		rt.EndDisabled()
		if rt.contentDisabled() {
			t.Fatal("outer scope did not restore content")
		}
	}
}

func TestScrollScopeClipsAndRestoresChildren(t *testing.T) {
	r := New(AppConfig{}).(*runtime)
	offset := int32(0)
	r.QueueMouseMove(30, 30)
	r.QueueMouseWheel(-1)
	r.BeginFrame()
	content := r.BeginScroll(NewRectangle(10, 10, 100, 60), 200, &offset)
	if offset != 42 || content.Y != -32 {
		t.Fatalf("content=%v offset=%d", content, offset)
	}
	r.QueueTap(20, 90)
	if r.Button(ButtonProps{Bounds: NewRectangle(10, 80, 100, 28), Label: "clipped"}) {
		t.Fatal("clipped child activated")
	}
	r.Rect(10, 0, 100, 160, RED)
	r.BeginScroll(NewRectangle(20, 30, 100, 60), 100, nil)
	r.Rect(0, 0, 160, 160, BLUE)
	r.EndScroll()
	r.EndScroll()
	if !r.Button(ButtonProps{Bounds: NewRectangle(10, 80, 100, 28), Label: "outside"}) {
		t.Fatal("parent input not restored")
	}
	r.EndFrame()
	img := RenderFrame(180, 180, r.FrameOps())
	for _, sample := range []struct {
		x, y int
		c    Color
	}{{15, 20, RED}, {30, 40, BLUE}, {115, 40, RAYWHITE}} {
		got := color.RGBAModel.Convert(img.At(sample.x, sample.y)).(color.RGBA)
		if got != (color.RGBA{sample.c.R, sample.c.G, sample.c.B, sample.c.A}) {
			t.Fatalf("pixel %d,%d=%v", sample.x, sample.y, got)
		}
	}
}

func TestScrollThumbDrag(t *testing.T) {
	r := New(AppConfig{}).(*runtime)
	offset := int32(0)
	draw := func() {
		r.BeginFrame()
		content := r.BeginScroll(NewRectangle(10, 10, 100, 60), 200, &offset)
		if content.Width != 90 {
			t.Fatal("scrollbar space not reserved")
		}
		if r.Button(ButtonProps{Bounds: NewRectangle(100, 10, 10, 60)}) {
			t.Fatal("child activated through scrollbar")
		}
		r.EndScroll()
		r.EndFrame()
	}
	r.QueueMouseButtonDown(MouseButtonLeft, 105, 20)
	draw()
	if offset != 0 {
		t.Fatalf("thumb jumped on press: %d", offset)
	}
	r.QueueMouseMove(105, 110)
	draw()
	if offset != 140 {
		t.Fatalf("drag offset=%d", offset)
	}
	r.QueueMouseButtonUp(MouseButtonLeft, 105, 110)
	draw()
	r.QueueMouseMove(105, 20)
	draw()
	if offset != 140 {
		t.Fatal("released thumb kept dragging")
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

func TestDirectPackageButtonProps(t *testing.T) {
	var drawButton func(ButtonProps) bool = Button
	rt := New(AppConfig{}).(*runtime)
	SetRuntime(rt)
	defer SetRuntime(nil)

	QueueTap(36, 12)
	BeginFrame()
	clicked := drawButton(ButtonProps{Label: "Save"})
	EndFrame()

	if !clicked {
		t.Fatal("typed Button did not consume tap")
	}
}

func TestTypedPackageButtonsDoNotDeriveIdentityFromLabels(t *testing.T) {
	rt := New(AppConfig{}).(*runtime)
	SetRuntime(rt)
	defer SetRuntime(nil)

	QueueTap(130, 30)
	BeginFrame()
	first := Button(ButtonProps{Bounds: Rectangle{X: 20, Y: 20}, Label: "Save"})
	second := Button(ButtonProps{Bounds: Rectangle{X: 120, Y: 20}, Label: "Save"})
	EndFrame()
	if first || !second {
		t.Fatalf("same-label activation: first=%t second=%t", first, second)
	}
	var buttons []FrameOp
	for _, op := range rt.FrameOps() {
		if op.Kind == FrameOpButton {
			buttons = append(buttons, op)
		}
	}
	if len(buttons) != 2 || buttons[0].ID == buttons[1].ID {
		t.Fatalf("same-label button instances share identity: %+v", buttons)
	}
	want := rt.resolveButtonProps(ButtonProps{Label: "Save"})
	for _, button := range buttons {
		if button.Bounds.Width != want.Bounds.Width || button.Bounds.Height != want.Bounds.Height {
			t.Fatalf("package Button bypassed shared measurement: %+v, want %+v", button.Bounds, want.Bounds)
		}
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

func TestButtonsAssignStableAutomaticIDs(t *testing.T) {
	rt := New(AppConfig{Width: 320, Height: 200}).(*runtime)
	draw := func() (int32, int32) {
		rt.BeginFrame()
		rt.Button(ButtonProps{Bounds: Rectangle{X: 10, Y: 10, Width: 80, Height: 30}, Label: "First"})
		rt.Button(ButtonProps{Bounds: Rectangle{X: 10, Y: 50, Width: 80, Height: 30}, Label: "Second"})
		ops := rt.FrameOps()
		rt.EndFrame()
		return ops[0].ID, ops[1].ID
	}

	first, second := draw()
	if first == 0 || second == 0 || first == second {
		t.Fatalf("automatic IDs must be non-zero and unique: %d, %d", first, second)
	}
	nextFirst, nextSecond := draw()
	if first != nextFirst || second != nextSecond {
		t.Fatalf("automatic IDs changed between frames: (%d, %d) then (%d, %d)", first, second, nextFirst, nextSecond)
	}
}

func TestFrameOpsRecordRenderableNativeFrame(t *testing.T) {
	rt := New(AppConfig{}).(*runtime)
	text := make([]byte, 32)
	copy(text, "secret")
	cursor := int32(len("secret"))
	focused := true

	rt.QueueTap(18, 50)
	rt.BeginFrame()
	rt.ClearBackground(WHITE)
	rt.Column(ColumnProps{Bounds: Rectangle{X: 10, Y: 10, Width: 180, Height: 160}, Gap: 6, Padding: 4, Key: Key("ops")})
	rt.Text(TextProps{Bounds: NewRectangle(0, 0, 0, 0), Text: "geld", Font: Text16, Color: BLACK, Wrap: TextWrapNone})
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

func TestPageAPIsRecordSemanticFrameOps(t *testing.T) {
	rt := New(AppConfig{Width: 320, Height: 240}).(*runtime)

	rt.ReplaceRoute("/docs#install")
	rt.BeginFrame()
	rt.Page(PageProps{
		Title:        "Docs",
		Description:  "Kryon docs",
		CanonicalURL: "https://example.test/docs",
		ThemeColor:   Color{R: 1, G: 2, B: 3, A: 255},
		Background:   WHITE,
		Gap:          8,
		Padding:      12,
	})
	rt.Heading(HeadingProps{Text: "Install", Level: 2})
	rt.Link(LinkProps{Text: "Read more", Href: "/more", Bounds: Rectangle{Width: 96, Height: 24}})
	rt.PagePicture(PictureProps{AssetPath: "hero.png", Bounds: Rectangle{Width: 120, Height: 60}, Tint: WHITE}, "Hero")
	rt.End()
	rt.PageGrid(GridProps{Bounds: Rectangle{X: 10, Y: 140, Width: 200, Height: 80}, Columns: 2, Gap: 4, Padding: 4})
	rt.Text(TextProps{Bounds: NewRectangle(0, 0, 0, 0), Text: "A", Font: Text16, Color: BLACK, Wrap: TextWrapNone})
	rt.Text(TextProps{Bounds: NewRectangle(0, 0, 0, 0), Text: "B", Font: Text16, Color: BLACK, Wrap: TextWrapNone})
	rt.End()
	rt.EndFrame()

	if rt.pageTitle != "Docs" || rt.pageDescription != "Kryon docs" || rt.pageCanonicalURL != "https://example.test/docs" {
		t.Fatalf("page metadata = title %q description %q canonical %q", rt.pageTitle, rt.pageDescription, rt.pageCanonicalURL)
	}
	if rt.pageThemeColor != (Color{R: 1, G: 2, B: 3, A: 255}) {
		t.Fatalf("theme color = %#v", rt.pageThemeColor)
	}
	if rt.GetRoutePath() != "/docs" || rt.GetRouteHash() != "#install" {
		t.Fatalf("route = %q %q", rt.GetRoutePath(), rt.GetRouteHash())
	}
	if rt.GetRouteVersion() != 1 {
		t.Fatalf("route version = %d, want 1", rt.GetRouteVersion())
	}
	rt.ReplaceRoute("/docs#install")
	if rt.GetRouteVersion() != 1 {
		t.Fatalf("same route version = %d, want 1", rt.GetRouteVersion())
	}
	rt.PushRoute("/docs#usage")
	if rt.GetRouteVersion() != 2 {
		t.Fatalf("changed route version = %d, want 2", rt.GetRouteVersion())
	}

	var sawPage, sawHeading, sawLink, sawPicture, sawGrid bool
	for _, op := range rt.FrameOps() {
		switch {
		case op.Kind == FrameOpPage && op.Semantic == UISemanticPage && op.Bounds.Width == 320:
			sawPage = true
		case op.Kind == FrameOpText && op.Semantic == UISemanticHeading && op.Level == 2 && op.Text == "Install":
			sawHeading = true
		case op.Kind == FrameOpText && op.Semantic == UISemanticLink && op.Href == "/more":
			sawLink = true
		case op.Kind == FrameOpPicture && op.Semantic == UISemanticPicture && op.AltText == "Hero":
			sawPicture = true
		case op.Kind == FrameOpGrid && op.Columns == 2:
			sawGrid = true
		}
	}
	if !sawPage || !sawHeading || !sawLink || !sawPicture || !sawGrid {
		t.Fatalf("missing semantic ops: page=%v heading=%v link=%v picture=%v grid=%v ops=%#v", sawPage, sawHeading, sawLink, sawPicture, sawGrid, rt.FrameOps())
	}
}

func TestFrameOpsResetEachFrame(t *testing.T) {
	rt := New(AppConfig{}).(*runtime)

	rt.BeginFrame()
	rt.Text(TextProps{Bounds: NewRectangle(10, 10, 0, 0), Text: "first", Font: Text16, Color: BLACK, Wrap: TextWrapNone})
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
	sortDirection := int32(0)
	scroll := int32(0)
	props := TableViewProps{
		Bounds:             Rectangle{X: 10, Y: 10, Width: 300, Height: 140},
		ID:                 41,
		Columns:            []string{"section", "label", "units"},
		Rows:               []TableRow{{Cells: []string{"banks", "checking", "10"}}, {Cells: []string{"cash", "wallet", "5"}}},
		ColumnWidths:       []int32{90, 140, 70},
		SelectedRow:        &selectedRow,
		SelectedColumn:     &selectedColumn,
		ActivatedRow:       &activatedRow,
		ActivatedColumn:    &activatedColumn,
		RightClickedRow:    &rightRow,
		RightClickedColumn: &rightColumn,
		SortColumn:         &sortColumn,
		SortDirection:      &sortDirection,
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
	if sortDirection != 1 {
		t.Fatalf("initial sort direction = %d, want ascending (1)", sortDirection)
	}
	if selectedRow != -1 || selectedColumn != 2 {
		t.Fatalf("header selection = %d,%d, want -1,2", selectedRow, selectedColumn)
	}

	rt.QueueTap(240, 20)
	rt.BeginFrame()
	rt.TableView(props)
	rt.EndFrame()
	if sortDirection != -1 {
		t.Fatalf("second header click direction = %d, want descending (-1)", sortDirection)
	}
	rt.QueueTap(240, 20)
	rt.BeginFrame()
	rt.TableView(props)
	rt.EndFrame()
	if sortDirection != 0 {
		t.Fatalf("third header click direction = %d, want unsorted (0)", sortDirection)
	}
}

func TestTableViewColumnVisibilityOrderAndCellColors(t *testing.T) {
	rt := New(AppConfig{Width: 360, Height: 220}).(*runtime)
	selectedRow, selectedColumn := int32(-1), int32(-1)
	textColor := Color{R: 10, G: 20, B: 30, A: 255}
	background := Color{R: 40, G: 50, B: 60, A: 255}
	props := TableViewProps{
		Bounds:         Rectangle{X: 10, Y: 10, Width: 300, Height: 140},
		ID:             141,
		Columns:        []string{"A", "hidden", "C"},
		Rows:           []TableRow{{Cells: []string{"a", "b", "c"}, TextColors: []Color{textColor, {}, {}}, BackgroundColors: []Color{{}, {}, background}}},
		ColumnWidths:   []int32{90, 140, 70},
		ColumnEnabled:  []int32{1, 0, 1},
		ColumnOrder:    []int32{2, 1, 0},
		SelectedRow:    &selectedRow,
		SelectedColumn: &selectedColumn,
		RowHeight:      24,
	}

	rt.QueueTap(30, 52)
	rt.BeginFrame()
	changed := rt.TableView(props)
	rt.EndFrame()
	if changed == 0 || selectedRow != 0 || selectedColumn != 2 {
		t.Fatalf("first displayed cell selection = changed %d, %d,%d; want changed, 0,2", changed, selectedRow, selectedColumn)
	}

	headerColumns := make([]int32, 0, 2)
	paintedBackground, paintedText := false, false
	for _, op := range rt.FrameOps() {
		if op.Kind == FrameOpText && op.Row == -1 {
			headerColumns = append(headerColumns, op.Column)
		}
		if op.Row == 0 && op.Column == 2 && op.Kind == FrameOpRect && op.Color == background {
			paintedBackground = true
		}
		if op.Row == 0 && op.Column == 0 && op.Kind == FrameOpText && op.Color == textColor {
			paintedText = true
		}
		if op.Column == 1 {
			t.Fatalf("hidden column emitted frame op: %#v", op)
		}
	}
	if !reflect.DeepEqual(headerColumns, []int32{2, 0}) {
		t.Fatalf("header display order = %v, want [2 0]", headerColumns)
	}
	if !paintedBackground || !paintedText {
		t.Fatalf("custom cell colors missing: background=%v text=%v", paintedBackground, paintedText)
	}

	rt.QueueKey(KeyRight)
	rt.BeginFrame()
	rt.TableView(props)
	rt.EndFrame()
	if selectedColumn != 0 {
		t.Fatalf("right arrow selected logical column %d, want reordered column 0", selectedColumn)
	}
	rt.QueueKey(KeyLeft)
	rt.BeginFrame()
	rt.TableView(props)
	rt.EndFrame()
	if selectedColumn != 2 {
		t.Fatalf("left arrow selected logical column %d, want reordered column 2", selectedColumn)
	}
}

func TestDisabledTableViewSuppressesInteractionAndDimsOps(t *testing.T) {
	rt := New(AppConfig{Width: 320, Height: 180}).(*runtime)
	selectedRow, selectedColumn := int32(0), int32(0)
	sortColumn, sortDirection := int32(-1), int32(0)
	scroll := int32(0)
	props := TableViewProps{
		Bounds:         Rectangle{X: 10, Y: 10, Width: 220, Height: 100},
		ID:             142,
		Columns:        []string{"A", "B"},
		Rows:           []TableRow{{Cells: []string{"a", "b"}}, {Cells: []string{"c", "d"}}, {Cells: []string{"e", "f"}}, {Cells: []string{"g", "h"}}},
		SelectedRow:    &selectedRow,
		SelectedColumn: &selectedColumn,
		SortColumn:     &sortColumn,
		SortDirection:  &sortDirection,
		ScrollOffset:   &scroll,
		RowHeight:      24,
		Disabled:       true,
	}

	rt.SetFocus(props.ID)
	rt.QueueTap(150, 20)
	rt.QueueTap(150, 52)
	rt.QueueKey(KeyRight)
	rt.QueueMouseMove(50, 70)
	rt.QueueMouseWheel(-1)
	rt.BeginFrame()
	changed := rt.TableView(props)
	rt.EndFrame()
	if changed != 0 || selectedRow != 0 || selectedColumn != 0 || sortColumn != -1 || sortDirection != 0 || scroll != 0 {
		t.Fatalf("disabled table changed: changed=%d selected=%d,%d sort=%d/%d scroll=%d", changed, selectedRow, selectedColumn, sortColumn, sortDirection, scroll)
	}
	if len(rt.FrameOps()) == 0 {
		t.Fatal("disabled table emitted no frame operations")
	}
	for _, op := range rt.FrameOps() {
		if !op.Disabled {
			t.Fatalf("disabled table emitted enabled operation: %#v", op)
		}
	}
}

func TestTableViewResizesDisplayedColumns(t *testing.T) {
	rt := New(AppConfig{Width: 340, Height: 180}).(*runtime)
	widths := []int32{100, 100, 100}
	selectedRow, selectedColumn := int32(-1), int32(-1)
	props := TableViewProps{
		Bounds: Rectangle{X: 10, Y: 10, Width: 300, Height: 110}, ID: 143,
		Columns: []string{"A", "B", "C"}, Rows: []TableRow{{Cells: []string{"a", "b", "c"}}},
		ColumnWidths: widths, SelectedRow: &selectedRow, SelectedColumn: &selectedColumn,
		Resizable: true, MinColumnWidth: 48, RowHeight: 24,
	}

	rt.QueueMouseButtonDown(MouseButtonLeft, 108, 20)
	rt.BeginFrame()
	changed := rt.TableView(props)
	rt.EndFrame()
	if changed != 0 {
		t.Fatalf("resize press changed table before movement: %d", changed)
	}
	rt.QueueMouseMove(138, 20)
	rt.BeginFrame()
	changed = rt.TableView(props)
	rt.EndFrame()
	if changed == 0 || widths[0] != 130 {
		t.Fatalf("resized first column = %d changed=%d, want 130 and changed", widths[0], changed)
	}
	rt.QueueMouseButtonUp(MouseButtonLeft, 138, 20)
	rt.BeginFrame()
	rt.TableView(props)
	rt.EndFrame()

	rt.QueueTap(120, 52)
	rt.BeginFrame()
	rt.TableView(props)
	rt.EndFrame()
	if selectedColumn != 0 {
		t.Fatalf("resized hit-test selected column %d, want 0", selectedColumn)
	}

	separatorLines := 0
	for _, op := range rt.FrameOps() {
		if op.Kind == FrameOpLine && op.Row == 0 && op.Bounds.Height == 30 {
			separatorLines++
		}
	}
	if separatorLines != 3 {
		t.Fatalf("resize separator lines = %d, want 3", separatorLines)
	}
}

func TestTableFrozenRowsClipPartialScroll(t *testing.T) {
	rt := New(AppConfig{}).(*runtime)
	rows := make([]TableRow, 8)
	for i := range rows {
		rows[i] = TableRow{Cells: []string{""}, BackgroundColors: []Color{BLUE}}
	}
	rows[0].BackgroundColors = []Color{RED}
	scroll := int32(60)
	rt.BeginFrame()
	rt.TableView(TableViewProps{Bounds: NewRectangle(10, 10, 180, 102), Columns: []string{"Name"}, Rows: rows, RowHeight: 24, FreezeRows: 1, ScrollOffset: &scroll})
	rt.EndFrame()
	img := RenderFrame(220, 160, rt.FrameOps())
	for _, sample := range []struct {
		y    int
		want Color
	}{{60, RED}, {65, BLUE}, {115, RAYWHITE}} {
		got := color.RGBAModel.Convert(img.At(100, sample.y)).(color.RGBA)
		want := color.RGBA{sample.want.R, sample.want.G, sample.want.B, sample.want.A}
		if got != want {
			t.Fatalf("pixel y=%d: got %v want %v", sample.y, got, want)
		}
	}
}

func TestTableRowsRespectParentScrollClip(t *testing.T) {
	r := New(AppConfig{}).(*runtime)
	r.BeginFrame()
	r.BeginScroll(NewRectangle(10, 10, 100, 60), 120, nil)
	r.TableView(TableViewProps{Bounds: NewRectangle(10, 10, 150, 120), Columns: []string{""}, Rows: []TableRow{
		{Cells: []string{""}, BackgroundColors: []Color{RED}},
		{Cells: []string{""}, BackgroundColors: []Color{RED}},
	}, RowHeight: 24})
	r.EndScroll()
	r.EndFrame()
	img := RenderFrame(200, 160, r.FrameOps())
	for _, p := range []image.Point{{50, 75}, {120, 50}} {
		if got := color.RGBAModel.Convert(img.At(p.X, p.Y)).(color.RGBA); got != (color.RGBA{245, 245, 245, 255}) {
			t.Fatalf("table escaped parent at %v: %v", p, got)
		}
	}
}

func TestTableViewFreezesRowsWhileScrolling(t *testing.T) {
	rt := New(AppConfig{Width: 300, Height: 180}).(*runtime)
	rows := make([]TableRow, 8)
	for i := range rows {
		rows[i] = TableRow{Cells: []string{fmt.Sprintf("row %d", i)}}
	}
	selectedRow, selectedColumn := int32(-1), int32(-1)
	scroll := int32(48)
	props := TableViewProps{
		Bounds: Rectangle{X: 10, Y: 10, Width: 180, Height: 102}, ID: 144,
		Columns: []string{"Name"}, Rows: rows, SelectedRow: &selectedRow,
		SelectedColumn: &selectedColumn, ScrollOffset: &scroll, RowHeight: 24,
		FreezeRows: 1,
	}

	rt.BeginFrame()
	rt.TableView(props)
	rt.EndFrame()
	frozenY, scrolledY := float32(-1), float32(-1)
	for _, op := range rt.FrameOps() {
		if op.Kind != FrameOpText || op.Column != 0 {
			continue
		}
		if op.Row == 0 {
			frozenY = op.Bounds.Y
		}
		if op.Row == 3 {
			scrolledY = op.Bounds.Y
		}
	}
	if frozenY != 46 || scrolledY != 70 {
		t.Fatalf("frozen/scrolled text y = %.0f/%.0f, want 46/70", frozenY, scrolledY)
	}

	rt.QueueTap(30, 45)
	rt.BeginFrame()
	rt.TableView(props)
	rt.EndFrame()
	if selectedRow != 0 {
		t.Fatalf("frozen-row hit selected %d, want 0", selectedRow)
	}
	rt.QueueTap(30, 70)
	rt.BeginFrame()
	rt.TableView(props)
	rt.EndFrame()
	if selectedRow != 3 {
		t.Fatalf("scrolled-row hit selected %d, want 3", selectedRow)
	}
}

func TestDisabledListAndTreeViewsSuppressInteractionAndDimOps(t *testing.T) {
	rt := New(AppConfig{Width: 320, Height: 180}).(*runtime)
	listSelected, listScroll := int32(0), int32(0)
	list := ListBoxProps{
		Bounds: Rectangle{X: 10, Y: 10, Width: 160, Height: 48}, ID: 151,
		Items: []string{"a", "b", "c", "d"}, SelectedIndex: &listSelected,
		ScrollOffset: &listScroll, RowHeight: 24, Disabled: true,
	}
	rt.QueueTap(30, 45)
	rt.QueueMouseMove(30, 30)
	rt.QueueMouseWheel(-1)
	rt.BeginFrame()
	changed := rt.ListBox(list)
	rt.EndFrame()
	if changed != 0 || listSelected != 0 || listScroll != 0 {
		t.Fatalf("disabled list changed: changed=%d selected=%d scroll=%d", changed, listSelected, listScroll)
	}
	for _, op := range rt.FrameOps() {
		if !op.Disabled {
			t.Fatalf("disabled list emitted enabled operation: %#v", op)
		}
	}

	treeSelected, treeScroll := int32(1), int32(0)
	tree := TreeViewProps{
		Bounds: Rectangle{X: 10, Y: 10, Width: 160, Height: 48}, ID: 152,
		Items:      []UITreeItem{{Label: "a", ID: 1, Selectable: 1}, {Label: "b", ID: 2, Selectable: 1}, {Label: "c", ID: 3, Selectable: 1}},
		SelectedID: &treeSelected, ScrollOffset: &treeScroll, RowHeight: 24, Disabled: true,
	}
	rt.QueueTap(30, 45)
	rt.QueueMouseMove(30, 30)
	rt.QueueMouseWheel(-1)
	rt.BeginFrame()
	changed = rt.TreeView(tree)
	rt.EndFrame()
	if changed != 0 || treeSelected != 1 || treeScroll != 0 {
		t.Fatalf("disabled tree changed: changed=%d selected=%d scroll=%d", changed, treeSelected, treeScroll)
	}
	for _, op := range rt.FrameOps() {
		if !op.Disabled {
			t.Fatalf("disabled tree emitted enabled operation: %#v", op)
		}
	}
}

func TestTableViewDragSelectsRange(t *testing.T) {
	rt := New(AppConfig{Width: 320, Height: 220}).(*runtime)
	selectedRow, selectedCol := int32(-1), int32(-1)
	startRow, startCol := int32(-1), int32(-1)
	endRow, endCol := int32(-1), int32(-1)
	props := TableViewProps{
		Bounds:               Rectangle{X: 10, Y: 10, Width: 220, Height: 150},
		ID:                   42,
		Columns:              []string{"A", "B", "C"},
		Rows:                 []TableRow{{Cells: []string{"1", "2", "3"}}, {Cells: []string{"4", "5", "6"}}, {Cells: []string{"7", "8", "9"}}},
		ColumnWidths:         []int32{60, 60, 60},
		SelectedRow:          &selectedRow,
		SelectedColumn:       &selectedCol,
		SelectionStartRow:    &startRow,
		SelectionStartColumn: &startCol,
		SelectionEndRow:      &endRow,
		SelectionEndColumn:   &endCol,
		RowHeight:            28,
	}

	rt.QueueMouseButtonDown(MouseButtonLeft, 42, 58)
	rt.TableView(props)
	rt.QueueMouseMove(152, 113)
	rt.TableView(props)
	rt.QueueMouseButtonUp(MouseButtonLeft, 152, 113)
	rt.TableView(props)

	if startRow != 0 || startCol != 0 || endRow != 2 || endCol != 2 {
		t.Fatalf("drag selection = start %d,%d end %d,%d; want 0,0 to 2,2", startRow, startCol, endRow, endCol)
	}
	selected := 0
	for _, op := range rt.FrameOps() {
		if op.Kind == FrameOpRect && op.Selected && op.Row >= 0 && op.Column >= 0 {
			selected++
		}
	}
	if selected < 9 {
		t.Fatalf("selected cell ops = %d, want full 3x3 range", selected)
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
		Rows:           []TableRow{{Cells: []string{"banks", "checking", "10"}}},
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
		Rows:           []TableRow{{Cells: []string{"1", "cash", "10"}}, {Cells: []string{"2", "bank", "20"}}},
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

func TestTableViewClipboardShortcuts(t *testing.T) {
	rt := New(AppConfig{Width: 360, Height: 220}).(*runtime)
	selectedRow := int32(0)
	selectedColumn := int32(1)
	pastedText := ""
	pastedRow := int32(-1)
	pastedColumn := int32(-1)
	props := TableViewProps{
		Bounds:         Rectangle{X: 10, Y: 10, Width: 300, Height: 140},
		ID:             50,
		Columns:        []string{"section", "label", "units"},
		Rows:           []TableRow{{Cells: []string{"banks", "checking", "10"}}, {Cells: []string{"cash", "wallet", "5"}}},
		ColumnWidths:   []int32{90, 140, 70},
		SelectedRow:    &selectedRow,
		SelectedColumn: &selectedColumn,
		PastedText:     &pastedText,
		PastedRow:      &pastedRow,
		PastedColumn:   &pastedColumn,
		RowHeight:      24,
	}

	rt.SetFocus(50)
	rt.QueueShortcut(KeyC)
	rt.BeginFrame()
	rt.TableView(props)
	rt.EndFrame()
	if got, want := rt.ClipboardText(), "checking"; got != want {
		t.Fatalf("cell clipboard = %q, want %q", got, want)
	}
	if selectedRow != 0 || selectedColumn != 1 {
		t.Fatalf("cell copy changed selection to %d,%d", selectedRow, selectedColumn)
	}

	selectedColumn = -1
	rt.QueueShortcut(KeyC)
	rt.BeginFrame()
	rt.TableView(props)
	rt.EndFrame()
	if got, want := rt.ClipboardText(), "banks\tchecking\t10"; got != want {
		t.Fatalf("row clipboard = %q, want %q", got, want)
	}

	selectedRow = -1
	selectedColumn = 2
	rt.QueueShortcut(KeyC)
	rt.BeginFrame()
	rt.TableView(props)
	rt.EndFrame()
	if got, want := rt.ClipboardText(), "10\n5"; got != want {
		t.Fatalf("column clipboard = %q, want %q", got, want)
	}
	if selectedRow != -1 || selectedColumn != 2 {
		t.Fatalf("column copy changed selection to %d,%d", selectedRow, selectedColumn)
	}

	copyText := "editable-id"
	selectedRow = 1
	selectedColumn = 1
	props.CopyText = &copyText
	rt.QueueShortcut(KeyC)
	rt.BeginFrame()
	rt.TableView(props)
	rt.EndFrame()
	if got := rt.ClipboardText(); got != "editable-id" {
		t.Fatalf("override clipboard = %q, want editable-id", got)
	}

	rt.SetClipboardText("new\tvalues")
	rt.QueueShortcut(KeyV)
	rt.BeginFrame()
	changed := rt.TableView(props)
	rt.EndFrame()
	if changed == 0 {
		t.Fatal("paste did not report table change")
	}
	if pastedText != "new\tvalues" || pastedRow != 1 || pastedColumn != 1 {
		t.Fatalf("paste = %q at %d,%d, want new values at 1,1", pastedText, pastedRow, pastedColumn)
	}
}

func TestMouseButtonDownAndReleaseState(t *testing.T) {
	rt := New(AppConfig{Width: 200, Height: 120}).(*runtime)

	rt.QueueMouseButtonDown(MouseButtonLeft, 10, 20)
	rt.BeginFrame()
	if !rt.MouseButtonPressed(MouseButtonLeft) {
		t.Fatal("button down should report pressed on first frame")
	}
	if !rt.MouseButtonDown(MouseButtonLeft) {
		t.Fatal("button down should remain held")
	}
	if rt.MouseButtonReleased(MouseButtonLeft) {
		t.Fatal("button down should not report released")
	}
	rt.EndFrame()

	rt.BeginFrame()
	if rt.MouseButtonPressed(MouseButtonLeft) {
		t.Fatal("held button should not report pressed on following frame")
	}
	if !rt.MouseButtonDown(MouseButtonLeft) {
		t.Fatal("button should remain down after frame")
	}
	rt.EndFrame()

	rt.QueueMouseMove(15, 25)
	rt.QueueMouseButtonUp(MouseButtonLeft, 30, 40)
	rt.BeginFrame()
	if rt.MouseButtonDown(MouseButtonLeft) {
		t.Fatal("button should not remain down after release")
	}
	if !rt.MouseButtonReleased(MouseButtonLeft) {
		t.Fatal("button release not reported")
	}
	if got, want := rt.MousePosition(), (Vector2{X: 30, Y: 40}); got != want {
		t.Fatalf("release position = %#v, want %#v", got, want)
	}
	rt.EndFrame()
}

func TestTableViewKeyboardNavigationScrollAndRendering(t *testing.T) {
	rt := New(AppConfig{Width: 360, Height: 260}).(*runtime)
	selectedRow := int32(0)
	selectedColumn := int32(0)
	activatedRow := int32(-1)
	activatedColumn := int32(-1)
	scroll := int32(0)
	rows := make([]TableRow, 20)
	for i := range rows {
		rows[i] = TableRow{Cells: []string{"section", fmt.Sprintf("row %d", i), fmt.Sprintf("%d", i)}}
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

func TestTableViewClickFocusEnablesArrowNavigation(t *testing.T) {
	rt := New(AppConfig{Width: 360, Height: 220}).(*runtime)
	selectedRow := int32(-1)
	selectedColumn := int32(-1)
	props := TableViewProps{
		Bounds:         Rectangle{X: 10, Y: 10, Width: 300, Height: 140},
		ID:             52,
		Columns:        []string{"A", "B", "C"},
		Rows:           []TableRow{{Cells: []string{"a1", "b1", "c1"}}, {Cells: []string{"a2", "b2", "c2"}}},
		ColumnWidths:   []int32{100, 100, 100},
		SelectedRow:    &selectedRow,
		SelectedColumn: &selectedColumn,
		RowHeight:      24,
	}

	rt.QueueTap(116, 52)
	rt.BeginFrame()
	rt.TableView(props)
	rt.EndFrame()
	if selectedRow != 0 || selectedColumn != 1 {
		t.Fatalf("clicked selection = %d,%d, want 0,1", selectedRow, selectedColumn)
	}
	if rt.Focus() != 52 {
		t.Fatalf("focus after click = %d, want table focus 52", rt.Focus())
	}

	rt.QueueKey(KeyRight)
	rt.BeginFrame()
	rt.TableView(props)
	rt.EndFrame()
	if selectedRow != 0 || selectedColumn != 2 {
		t.Fatalf("right selection = %d,%d, want 0,2", selectedRow, selectedColumn)
	}

	rt.QueueKey(KeyDown)
	rt.BeginFrame()
	rt.TableView(props)
	rt.EndFrame()
	if selectedRow != 1 || selectedColumn != 2 {
		t.Fatalf("down selection = %d,%d, want 1,2", selectedRow, selectedColumn)
	}

	rt.QueueKey(KeyLeft)
	rt.BeginFrame()
	rt.TableView(props)
	rt.EndFrame()
	if selectedRow != 1 || selectedColumn != 1 {
		t.Fatalf("left selection = %d,%d, want 1,1", selectedRow, selectedColumn)
	}

	rt.QueueKey(KeyUp)
	rt.BeginFrame()
	rt.TableView(props)
	rt.EndFrame()
	if selectedRow != 0 || selectedColumn != 1 {
		t.Fatalf("up selection = %d,%d, want 0,1", selectedRow, selectedColumn)
	}

	rt.QueueKey(KeyEscape)
	rt.BeginFrame()
	rt.TableView(props)
	rt.EndFrame()
	if selectedRow != -1 || selectedColumn != -1 {
		t.Fatalf("escape selection = %d,%d, want -1,-1", selectedRow, selectedColumn)
	}
}

func TestTableViewUsesExplicitSystemTheme(t *testing.T) {
	resetSystemThemeForTest()
	defer resetSystemThemeForTest()
	t.Setenv("KRYON_THEME_MODE", "dark")
	t.Setenv("GTK_THEME", "KryonMissingTheme")
	rt := New(AppConfig{Width: 240, Height: 160}).(*runtime)
	rt.SetThemeSource(ThemeSourceSystem)
	selectedRow := int32(0)
	selectedColumn := int32(0)
	props := TableViewProps{
		Bounds:         Rectangle{X: 10, Y: 10, Width: 180, Height: 80},
		ID:             61,
		Columns:        []string{"label"},
		Rows:           []TableRow{{Cells: []string{"row"}}},
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

func TestExplicitSystemThemeSelectionIsNeutral(t *testing.T) {
	resetSystemThemeForTest()
	defer resetSystemThemeForTest()
	t.Setenv("GTK_THEME", "KryonMissingTheme")
	t.Setenv("KRYON_THEME_MODE", "")
	t.Setenv("COLOR_SCHEME", "")
	t.Setenv("QT_STYLE_OVERRIDE", "")
	t.Setenv("XDG_CURRENT_DESKTOP", "")
	temp := t.TempDir()
	t.Setenv("HOME", filepath.Join(temp, "home"))
	t.Setenv("XDG_CONFIG_HOME", filepath.Join(temp, "config"))
	t.Setenv("XDG_DATA_HOME", filepath.Join(temp, "data"))
	rt := New(AppConfig{Width: 240, Height: 160}).(*runtime)
	rt.SetThemeSource(ThemeSourceSystem)
	theme := rt.theme()
	assertNotBlueSelection(t, theme.selectedHot)
}

func assertNotBlueSelection(t *testing.T, color Color) {
	t.Helper()
	if color.B > color.R+20 && color.B > color.G+20 {
		t.Fatalf("selection is blue-biased: %#v", color)
	}
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
	rt.SetThemeSource(ThemeSourceSystem)
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

func TestTextInputFrameOpsCarryModernThemeStyle(t *testing.T) {
	rt := New(AppConfig{Width: 240, Height: 160}).(*runtime)
	rt.SetThemeSource(ThemeSourceApp)
	rt.SetCurrentTheme(int32(ThemeCobalt), 1)
	SetRuntime(rt)
	defer SetRuntime(nil)

	text := make([]byte, 32)
	copy(text, "abcde")
	cursor := int32(4)
	focused := true
	areaFocused := false
	rt.SetSelection(77, 1, 4)
	rt.SetSelection(78, 1, 4)

	BeginFrame()
	TextField(TextFieldProps{
		Bounds:         Rectangle{X: 10, Y: 10, Width: 160, Height: 32},
		Text:           text,
		CursorPosition: &cursor,
		Focused:        &focused,
		FocusID:        77,
		Font:           Text16,
	})
	TextArea(TextAreaProps{
		Bounds:         Rectangle{X: 10, Y: 54, Width: 160, Height: 64},
		Text:           text,
		CursorPosition: &cursor,
		Focused:        &areaFocused,
		FocusID:        78,
		Font:           Text16,
	})
	EndFrame()

	wantFocused := rt.textInputStyle(true, false)
	wantIdle := rt.textInputStyle(false, false)
	foundField := false
	foundArea := false
	ops := FrameOps()
	for _, op := range ops {
		if op.Kind != FrameOpTextField && op.Kind != FrameOpTextArea {
			continue
		}
		want := wantFocused
		if op.Kind == FrameOpTextArea {
			want = wantIdle
		}
		if got := op.Color; got != want.Background {
			t.Fatalf("%s background = %#v, want %#v", op.Kind, got, want.Background)
		}
		if got := op.BorderColor; got != want.Border {
			t.Fatalf("%s border = %#v, want %#v", op.Kind, got, want.Border)
		}
		if got := op.FocusColor; got != want.Focus {
			t.Fatalf("%s focus = %#v, want %#v", op.Kind, got, want.Focus)
		}
		if got := op.TextColor; got != want.Foreground {
			t.Fatalf("%s text = %#v, want %#v", op.Kind, got, want.Foreground)
		}
		if got := op.Material; got != want.Material {
			t.Fatalf("%s material = %#v, want %#v", op.Kind, got, want.Material)
		}
		if got := op.Radius; got != want.Radius {
			t.Fatalf("%s radius = %#v, want %#v", op.Kind, got, want.Radius)
		}
		if got := op.BorderWidth; got != want.BorderWidth {
			t.Fatalf("%s border width = %#v, want %#v", op.Kind, got, want.BorderWidth)
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
		if op.Kind == FrameOpTextField {
			foundField = true
		} else {
			foundArea = true
		}
	}
	if !foundField || !foundArea {
		t.Fatalf("text input ops not found: field=%v area=%v ops=%#v",
			foundField, foundArea, ops)
	}
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
	Text(TextProps{Bounds: NewRectangle(12, 12, 0, 0), Text: "geld", Font: Text16, Color: BLACK, Wrap: TextWrapNone})
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
	Text(TextProps{Bounds: NewRectangle(4, 4, 0, 0), Text: "shot", Font: Text16, Color: BLACK, Wrap: TextWrapNone})
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

func countPixels(img interface {
	Bounds() image.Rectangle
	RGBAAt(int, int) color.RGBA
}, target color.RGBA) int {
	count := 0
	bounds := img.Bounds()
	for y := bounds.Min.Y; y < bounds.Max.Y; y++ {
		for x := bounds.Min.X; x < bounds.Max.X; x++ {
			if img.RGBAAt(x, y) == target {
				count++
			}
		}
	}
	return count
}

func rgbaTest(c Color) color.RGBA {
	return color.RGBA{R: c.R, G: c.G, B: c.B, A: c.A}
}

func TestTabBarRecordsOpsAndSelectsOnClick(t *testing.T) {
	r := New(AppConfig{Width: 400, Height: 100}).(*runtime)
	defer SetRuntime(nil)
	SetRuntime(r)
	tabs := []Tab{{Label: "Alpha"}, {Label: "Beta"}, {Label: "Gamma"}}
	sel := int32(1)

	BeginFrame()
	EndFrame()
	// Hover over the third tab (each 400/3 wide), click it.
	r.QueueMouseMove(350, 10)
	r.QueueMouseButton(MouseButtonLeft, 350, 10)
	BeginFrame() // the queued input lands on the next frame's state
	clicked := TabBar(TabBarProps{Bounds: NewRectangle(0, 0, 400, 30), Tabs: tabs,
		Count: int32(len(tabs)), SelectedIndex: sel, ID: 901})
	if clicked >= 0 {
		sel = clicked
	}
	ops := FrameOps()
	buttons := 0
	var active *FrameOp
	for i, op := range ops {
		if op.Kind == FrameOpButton {
			buttons++
			active = &ops[i]
		}
	}
	if buttons != len(tabs) {
		t.Fatalf("recorded %d tab buttons, want %d", buttons, len(tabs))
	}
	if clicked != 2 || sel != 2 {
		t.Fatalf("click selected %d (sel=%d), want 2", clicked, sel)
	}
	if active == nil || active.Text != "Gamma" || !active.Pressed {
		t.Fatalf("last tab op = %+v, want active Gamma", active)
	}
	EndFrame()
}

func TestTabBarEmptyLabels(t *testing.T) {
	r := New(AppConfig{Width: 100, Height: 30}).(*runtime)
	defer SetRuntime(nil)
	SetRuntime(r)
	BeginFrame()
	if got := TabBar(TabBarProps{Bounds: NewRectangle(0, 0, 100, 30)}); got != -1 {
		t.Fatalf("TabBar with no labels = %d, want -1", got)
	}
	EndFrame()
}

// TestCanvasSpaceInputInvariant locks the coordinate contract: the widget
// canvas is the configured window size, pointer events arrive in
// window-relative coordinates, and widgets consume taps exactly at their
// drawn bounds. A change that translates events by the window's root
// position (a past regression) fails this test.
func TestCanvasSpaceInputInvariant(t *testing.T) {
	rt := New(AppConfig{Width: 1280, Height: 800}).(*runtime)
	v := int32(0)
	draw := func() bool {
		rt.BeginFrame()
		pressed := rt.Checkbox(7, 16, 88, "enable", &v)
		rt.EndFrame()
		return pressed
	}

	// A tap inside the drawn bounds flips the value.
	rt.QueueMouseButtonDown(MouseButtonLeft, 27, 99)
	if !draw() {
		t.Fatal("tap inside bounds not consumed")
	}
	if v != 1 {
		t.Fatalf("value = %d, want 1 after inside tap", v)
	}

	// A tap far outside leaves it alone.
	rt.QueueMouseButtonDown(MouseButtonLeft, 600, 500)
	if draw() {
		t.Fatal("tap outside bounds consumed")
	}
	if v != 1 {
		t.Fatalf("value = %d, outside tap must not flip", v)
	}

	// The invariant: events are already canvas-relative. Feeding
	// bounds+offset (what a bogus root-position translation would do)
	// must NOT hit the widget.
	rt.QueueMouseButtonDown(MouseButtonLeft, 27+4, 99+310)
	if draw() {
		t.Fatal("offset tap consumed — input is being translated somewhere")
	}
	if v != 1 {
		t.Fatalf("value = %d, translated tap must not flip", v)
	}
}
