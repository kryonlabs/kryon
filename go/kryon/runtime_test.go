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

func TestIconActionToolbarAndMenuBar(t *testing.T) {
	rt := New(AppConfig{Width: 360, Height: 180}).(*runtime)
	open := int32(-1)
	menus := []MenuGroup{{
		Label: "File",
		Items: []MenuItem{
			{Kind: MenuCommand, Label: "Save", Accelerator: "Ctrl+S", ID: 101},
			{Kind: MenuSeparator},
			{Kind: MenuCommand, Label: "Quit", ID: 102},
		},
	}}
	actions := []ToolbarAction{
		{IconType: IconSave},
		{IconType: IconWorkbookTextColor},
	}

	rt.QueueTap(18, 16)
	res := rt.Menu(MenuProps{ID: 10, Mode: MenuModeBar, Bounds: Rectangle{X: 0, Y: 0, Width: 360, Height: 30}, Menus: menus, OpenIndex: &open})
	if got, want := res.OpenIndex, int32(0); got != want {
		t.Fatalf("menu open index = %d, want %d", got, want)
	}
	if got, want := open, int32(0); got != want {
		t.Fatalf("open pointer = %d, want %d", got, want)
	}

	rt.QueueTap(20, 43)
	res = rt.Menu(MenuProps{ID: 10, Mode: MenuModeBar, Bounds: Rectangle{X: 0, Y: 0, Width: 360, Height: 30}, Menus: menus, OpenIndex: &open})
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
	if got, want := toolbar.ClickedAction, int32(1); got != want {
		t.Fatalf("clicked toolbar action = %d, want %d", got, want)
	}

	var sawIcon bool
	for _, op := range rt.FrameOps() {
		if op.Kind == FrameOpIcon && op.IconType == IconSave {
			sawIcon = true
			break
		}
	}
	if !sawIcon {
		t.Fatalf("toolbar did not record save icon op: %#v", rt.FrameOps())
	}
}

func TestIconActionUsesButtonStyleSheet(t *testing.T) {
	ClearStylePacks()
	t.Cleanup(ClearStylePacks)
	if !RegisterStylePackSource(`
@pack test.icon_action;
tokens {
  color {
    face: #223142;
    ink: #e9f1ff;
    rule: #506070;
  }
  length { radius: 5; border: 2; }
  material { flat: Flat; }
}
Button { background: face; foreground: ink; border: rule; radius: radius; border-width: border; material: flat; }
`, "Test Icon Action", "") || !SetActiveStylePack("test.icon_action") {
		t.Fatal("test icon action style did not activate")
	}
	rt := New(AppConfig{Width: 120, Height: 80}).(*runtime)

	rt.iconAction(iconActionProps{
		Bounds:      Rectangle{X: 10, Y: 12, Width: 32, Height: 32},
		IconType:    IconSave,
		IconSize:    16,
		IconPadding: 4,
		FocusID:     701,
	})

	var sawButton, sawIcon bool
	for _, op := range rt.FrameOps() {
		if op.ID != 701 {
			continue
		}
		switch op.Kind {
		case FrameOpButton:
			sawButton = true
			style := unpackStyle(op.Button.Appearance.Value)
			if style.Background != (Color{R: 0x22, G: 0x31, B: 0x42, A: 0xff}) || style.Border != (Color{R: 0x50, G: 0x60, B: 0x70, A: 0xff}) || style.Radius != 5 || style.BorderWidth != 2 {
				t.Fatalf("icon action button style op = %+v", op)
			}
		case FrameOpIcon:
			sawIcon = true
			if op.Color != (Color{R: 0xe9, G: 0xf1, B: 0xff, A: 0xff}) {
				t.Fatalf("icon action tint = %+v", op)
			}
		}
	}
	if !sawButton || !sawIcon {
		t.Fatalf("missing styled icon action ops: button=%v icon=%v ops=%+v", sawButton, sawIcon, rt.FrameOps())
	}
}

func TestModalUsesStyleSheet(t *testing.T) {
	ClearStylePacks()
	t.Cleanup(ClearStylePacks)
	if !RegisterStylePackSource(`
@pack test.modal;
tokens {
  color {
    panel: #202836;
    ink: #ecf3ff;
    action: #7ae2ba;
    action-ink: #042017;
    rule: #566578;
  }
  length { radius: 8; border: 2; }
  material { flat: Flat; }
}
Modal[role=Scrim] { background: #111111; opacity: 0.5; material: flat; }
Modal[role=Panel] { background: panel; foreground: ink; border: rule; radius: radius; border-width: border; material: flat; }
Modal[role=Title] { foreground: ink; font-size: 16; opacity: 0.91; }
Modal[role=Message] { foreground: ink; font-size: 16; opacity: 0.73; }
Modal[role=Action] { background: panel; foreground: ink; border: rule; radius: radius; border-width: border; font-size: 18; material: flat; }
Modal[role=Action][tone=Accent] { background: action; foreground: action-ink; border: action; radius: radius; border-width: border; font-size: 18; material: flat; }
TextField { background: panel; foreground: ink; border: rule; focus: action; radius: radius; border-width: border; font-size: 19; material: flat; }
`, "Test Modal", "") || !SetActiveStylePack("test.modal") {
		t.Fatal("test modal style did not activate")
	}
	rt := New(AppConfig{Width: 420, Height: 280}).(*runtime)
	text := make([]byte, 32)
	copy(text, "prompt")
	cursor := int32(6)
	focused := true

	rt.Modal(ModalProps{
		Title:          "Notice",
		Message:        "Styled",
		Text:           text,
		TextSize:       int32(len(text)),
		CursorPosition: &cursor,
		Focused:        &focused,
		FocusID:        90,
		Actions: []ModalAction{{
			Label:    "OK",
			Tone:     ButtonToneAccent,
			Emphasis: ButtonEmphasisFilled,
		}},
		ActionCount: 1,
	})

	var sawScrim, sawPanel, sawTitle, sawMessage, sawButton, sawPrompt bool
	for _, op := range rt.FrameOps() {
		switch {
		case op.Kind == FrameOpRect && op.Bounds.Width == 420 && op.Bounds.Height == 280:
			sawScrim = true
			if op.Color != (Color{R: 0x11, G: 0x11, B: 0x11, A: 0x7f}) {
				t.Fatalf("modal scrim style op = %+v", op)
			}
		case op.Kind == FrameOpRect && op.Color == (Color{R: 0x20, G: 0x28, B: 0x36, A: 0xff}):
			sawPanel = true
			if op.BorderColor != (Color{R: 0x56, G: 0x65, B: 0x78, A: 0xff}) || op.BorderWidth != 2 || op.Radius != 8 {
				t.Fatalf("modal panel style op = %+v", op)
			}
		case op.Kind == FrameOpText && op.Text == "Notice":
			sawTitle = true
			if op.Color != (Color{R: 0xec, G: 0xf3, B: 0xff, A: 0xff}) || op.Opacity != 0.91 {
				t.Fatalf("modal title style op = %+v", op)
			}
		case op.Kind == FrameOpText && op.Text == "Styled":
			sawMessage = true
			if op.Color != (Color{R: 0xec, G: 0xf3, B: 0xff, A: 0xff}) || op.Opacity != 0.73 {
				t.Fatalf("modal message style op = %+v", op)
			}
		case op.Kind == FrameOpButton && op.Text == "OK":
			sawButton = true
			style := unpackStyle(op.Button.Appearance.Value)
			if op.Button.Font != 18 || style.FontSize != 18 || style.Background != (Color{R: 0x7a, G: 0xe2, B: 0xba, A: 0xff}) || style.Foreground != (Color{R: 0x04, G: 0x20, B: 0x17, A: 0xff}) || style.Border != (Color{R: 0x7a, G: 0xe2, B: 0xba, A: 0xff}) {
				t.Fatalf("modal action style op = %+v", op)
			}
		case op.Kind == FrameOpTextField:
			sawPrompt = true
			if op.FontSize != 19 {
				t.Fatalf("modal prompt font = %d, want 19: %+v", op.FontSize, op)
			}
		}
	}
	if !sawScrim || !sawPanel || !sawTitle || !sawMessage || !sawButton || !sawPrompt {
		t.Fatalf("missing styled modal ops: scrim=%v panel=%v title=%v message=%v button=%v prompt=%v ops=%+v", sawScrim, sawPanel, sawTitle, sawMessage, sawButton, sawPrompt, rt.FrameOps())
	}
}

func TestTitleBarUsesStyleSheet(t *testing.T) {
	ClearStylePacks()
	t.Cleanup(ClearStylePacks)
	if !RegisterStylePackSource(`
@pack test.titlebar;
tokens {
  color {
    surface: #182231;
    ink: #f1f5ff;
    button: #28384c;
    button-ink: #dbe8ff;
    rule: #506172;
  }
  length { radius: 6; border: 2; }
  material { flat: Flat; }
}
TitleBar[role=Bar] { background: surface; foreground: ink; border: rule; radius: radius; border-width: border; material: flat; }
TitleBar[role=Title] { foreground: ink; font-size: 20; opacity: 0.82; }
TitleBar[role=Action] { background: button; foreground: button-ink; border: rule; radius: radius; border-width: border; material: flat; }
`, "Test TitleBar", "") || !SetActiveStylePack("test.titlebar") {
		t.Fatal("test title bar style did not activate")
	}
	rt := New(AppConfig{Width: 240, Height: 120}).(*runtime)

	rt.TitleBar(TitleBarProps{Title: "Workspace", Height: 44, HasLeadingAction: true})

	var sawBar, sawTitle, sawLeading bool
	for _, op := range rt.FrameOps() {
		switch {
		case op.Kind == FrameOpRect && op.Bounds.Width == 240 && op.Bounds.Height == 44:
			sawBar = true
			if op.Color != (Color{R: 0x18, G: 0x22, B: 0x31, A: 0xff}) || op.BorderColor != (Color{R: 0x50, G: 0x61, B: 0x72, A: 0xff}) || op.BorderWidth != 2 || op.Radius != 6 {
				t.Fatalf("title bar surface op = %+v", op)
			}
		case op.Kind == FrameOpText && op.Text == "Workspace":
			sawTitle = true
			if op.Color != (Color{R: 0xf1, G: 0xf5, B: 0xff, A: 0xff}) || op.Opacity != 0.82 {
				t.Fatalf("title bar text op = %+v", op)
			}
		case op.Kind == FrameOpButton:
			sawLeading = true
			style := unpackStyle(op.Button.Appearance.Value)
			if style.Background != (Color{R: 0x28, G: 0x38, B: 0x4c, A: 0xff}) || style.Foreground != (Color{R: 0xdb, G: 0xe8, B: 0xff, A: 0xff}) || style.Border != (Color{R: 0x50, G: 0x61, B: 0x72, A: 0xff}) {
				t.Fatalf("title bar leading style op = %+v", op)
			}
		}
	}
	if !sawBar || !sawTitle || !sawLeading {
		t.Fatalf("missing styled title bar ops: bar=%v title=%v leading=%v ops=%+v", sawBar, sawTitle, sawLeading, rt.FrameOps())
	}
}

func TestToolbarUsesStyleSheet(t *testing.T) {
	ClearStylePacks()
	t.Cleanup(ClearStylePacks)
	if !RegisterStylePackSource(`
@pack test.toolbar;
tokens {
  color {
    surface: #1b2635;
    ink: #eaf2ff;
    button: #2d3d52;
    button-ink: #cfe0ff;
    rule: #596a7c;
  }
  length { radius: 4; border: 2; }
  material { flat: Flat; }
}
Toolbar[role=Bar] { background: surface; foreground: ink; border: rule; radius: radius; border-width: border; material: flat; }
Toolbar[role=Divider] { border: rule; }
Toolbar[role=Action] { background: button; foreground: button-ink; border: rule; radius: radius; border-width: border; material: flat; }
`, "Test Toolbar", "") || !SetActiveStylePack("test.toolbar") {
		t.Fatal("test toolbar style did not activate")
	}
	rt := New(AppConfig{Width: 320, Height: 120}).(*runtime)

	rt.Toolbar(ToolbarProps{
		ID:     8,
		X:      0,
		Y:      12,
		Width:  240,
		Height: 40,
		Actions: []ToolbarAction{{
			IconType: IconSave,
		}},
		ActionCount: 1,
	})

	var sawBar, sawDivider, sawButton, sawIcon bool
	for _, op := range rt.FrameOps() {
		switch {
		case op.Kind == FrameOpRect && op.Bounds.X == 0 && op.Bounds.Y == 12 && op.Bounds.Width == 240 && op.Bounds.Height == 40:
			sawBar = true
			if op.Color != (Color{R: 0x1b, G: 0x26, B: 0x35, A: 0xff}) || op.BorderColor != (Color{R: 0x59, G: 0x6a, B: 0x7c, A: 0xff}) || op.BorderWidth != 2 || op.Radius != 4 {
				t.Fatalf("toolbar surface op = %+v", op)
			}
		case op.Kind == FrameOpLine && op.Color == (Color{R: 0x59, G: 0x6a, B: 0x7c, A: 0xff}):
			sawDivider = true
		case op.Kind == FrameOpButton && op.ID == 801:
			sawButton = true
			style := unpackStyle(op.Button.Appearance.Value)
			if style.Background != (Color{R: 0x2d, G: 0x3d, B: 0x52, A: 0xff}) || style.Foreground != (Color{R: 0xcf, G: 0xe0, B: 0xff, A: 0xff}) || style.Border != (Color{R: 0x59, G: 0x6a, B: 0x7c, A: 0xff}) {
				t.Fatalf("toolbar action style op = %+v", op)
			}
		case op.Kind == FrameOpIcon && op.ID == 801:
			sawIcon = true
			if op.Color != (Color{R: 0xcf, G: 0xe0, B: 0xff, A: 0xff}) {
				t.Fatalf("toolbar icon tint op = %+v", op)
			}
		}
	}
	if !sawBar || !sawDivider || !sawButton || !sawIcon {
		t.Fatalf("missing styled toolbar ops: bar=%v divider=%v button=%v icon=%v ops=%+v", sawBar, sawDivider, sawButton, sawIcon, rt.FrameOps())
	}
}

func TestSegmentedControlUsesSegmentStyleSheet(t *testing.T) {
	ClearStylePacks()
	t.Cleanup(ClearStylePacks)
	if !RegisterStylePackSource(`
@pack test.segmented;
tokens {
  color {
    segment: #24364a;
    ink: #edf5ff;
    selected: #7ae2ba;
    selected-ink: #042017;
    rule: #596a7c;
  }
  length { radius: 5; border: 2; }
  material { flat: Flat; }
}
SegmentedControl { background: #101820; foreground: ink; border: rule; radius: radius; border-width: border; material: flat; }
Segment { background: segment; foreground: ink; border: rule; radius: radius; border-width: border; font-size: 19; material: flat; }
Segment:selected { background: selected; foreground: selected-ink; border: selected; radius: radius; border-width: border; font-size: 19; material: flat; }
`, "Test Segmented", "") || !SetActiveStylePack("test.segmented") {
		t.Fatal("test segmented style did not activate")
	}
	rt := New(AppConfig{Width: 260, Height: 120}).(*runtime)
	selected := int32(1)

	rt.SegmentedControl(SegmentedControlProps{
		Bounds:        Rectangle{X: 10, Y: 12, Width: 220, Height: 32},
		ID:            17,
		Options:       []SegmentOption{{Label: "One"}, {Label: "Two"}},
		OptionCount:   2,
		SelectedIndex: &selected,
	})

	var sawNormal, sawSelected bool
	for _, op := range rt.FrameOps() {
		if op.Kind != FrameOpButton {
			continue
		}
		style := unpackStyle(op.Button.Appearance.Value)
		switch op.Text {
		case "One":
			sawNormal = true
			if op.Button.Font != 19 || style.FontSize != 19 || style.Background != (Color{R: 0x24, G: 0x36, B: 0x4a, A: 0xff}) || style.Foreground != (Color{R: 0xed, G: 0xf5, B: 0xff, A: 0xff}) || style.Border != (Color{R: 0x59, G: 0x6a, B: 0x7c, A: 0xff}) {
				t.Fatalf("normal segment style op = %+v", op)
			}
		case "Two":
			sawSelected = true
			if op.Button.Font != 19 || style.FontSize != 19 || style.Background != (Color{R: 0x7a, G: 0xe2, B: 0xba, A: 0xff}) || style.Foreground != (Color{R: 0x04, G: 0x20, B: 0x17, A: 0xff}) || style.Border != (Color{R: 0x7a, G: 0xe2, B: 0xba, A: 0xff}) {
				t.Fatalf("selected segment style op = %+v", op)
			}
		}
	}
	if !sawNormal || !sawSelected {
		t.Fatalf("missing segmented ops: normal=%v selected=%v ops=%+v", sawNormal, sawSelected, rt.FrameOps())
	}
}

func TestCollapsibleUsesStyleSheet(t *testing.T) {
	ClearStylePacks()
	t.Cleanup(ClearStylePacks)
	if !RegisterStylePackSource(`
@pack test.collapsible;
tokens {
  color {
    face: #233248;
    ink: #edf4ff;
    rule: #5e7188;
  }
  length { radius: 5; border: 2; }
  material { flat: Flat; }
}
Collapsible[role=Header] { background: face; foreground: ink; border: rule; radius: radius; border-width: border; font-size: 18; material: flat; }
Collapsible[role=Header]:disabled { foreground: #8090a4; opacity: 0.42; }
Collapsible[role=TreeHeader] { background: #00000000; foreground: ink; border: #00000000; radius: radius; border-width: 0; material: flat; }
Collapsible[role=Close] { foreground: ink; font-size: 17; }
Collapsible[role=Close]:disabled { foreground: #8090a4; opacity: 0.37; }
`, "Test Collapsible", "") || !SetActiveStylePack("test.collapsible") {
		t.Fatal("test collapsible style did not activate")
	}
	rt := New(AppConfig{Width: 260, Height: 120}).(*runtime)
	open, visible := false, true

	rt.Collapsible(CollapsibleProps{
		Bounds:  Rectangle{X: 10, Y: 12, Width: 180, Height: 32},
		Label:   "Details",
		Open:    &open,
		Visible: &visible,
		ID:      991,
	})

	var sawHeader, sawClose bool
	for _, op := range rt.FrameOps() {
		switch {
		case op.Kind == FrameOpButton && op.ID == 991:
			sawHeader = true
			if op.Color != (Color{R: 0x23, G: 0x32, B: 0x48, A: 0xff}) || op.BorderColor != (Color{R: 0x5e, G: 0x71, B: 0x88, A: 0xff}) || op.TextColor != (Color{R: 0xed, G: 0xf4, B: 0xff, A: 0xff}) || op.BorderWidth != 2 || op.Radius != 5 || op.FontSize != 18 {
				t.Fatalf("collapsible header style op = %+v", op)
			}
		case op.Kind == FrameOpText && op.Text == "×":
			sawClose = true
			if op.Color != (Color{R: 0xed, G: 0xf4, B: 0xff, A: 0xff}) || op.FontSize != 17 {
				t.Fatalf("collapsible close style op = %+v", op)
			}
		}
	}
	if !sawHeader || !sawClose {
		t.Fatalf("missing styled collapsible ops: header=%v close=%v ops=%+v", sawHeader, sawClose, rt.FrameOps())
	}

	rt.BeginFrame()
	rt.Collapsible(CollapsibleProps{
		Bounds:   Rectangle{X: 10, Y: 48, Width: 180, Height: 32},
		Label:    "Node",
		Tree:     true,
		Selected: true,
		ID:       992,
	})
	rt.EndFrame()
	var sawTree bool
	for _, op := range rt.FrameOps() {
		if op.Kind == FrameOpButton && op.ID == 992 {
			sawTree = true
			if op.Color != BLANK || op.BorderColor != BLANK {
				t.Fatalf("tree collapsible should keep transparent chrome: %+v", op)
			}
		}
	}
	if !sawTree {
		t.Fatalf("missing tree collapsible op: %+v", rt.FrameOps())
	}

	rt.BeginFrame()
	disabledVisible := true
	rt.Collapsible(CollapsibleProps{
		Bounds:   Rectangle{X: 10, Y: 84, Width: 180, Height: 32},
		Label:    "Disabled",
		Visible:  &disabledVisible,
		Disabled: true,
		ID:       993,
	})
	rt.EndFrame()
	var sawDisabledHeader, sawDisabledClose bool
	for _, op := range rt.FrameOps() {
		switch {
		case op.Kind == FrameOpButton && op.ID == 993:
			sawDisabledHeader = true
			if !op.Disabled || op.TextColor != (Color{R: 0x80, G: 0x90, B: 0xa4, A: 0x6b}) || op.Opacity != 0.42 {
				t.Fatalf("disabled collapsible header style op = %+v", op)
			}
		case op.Kind == FrameOpText && op.Text == "×":
			sawDisabledClose = true
			if !op.Disabled || op.Color != (Color{R: 0x80, G: 0x90, B: 0xa4, A: 0xff}) || op.Opacity != 0.37 {
				t.Fatalf("disabled collapsible close style op = %+v", op)
			}
		}
	}
	if !sawDisabledHeader || !sawDisabledClose {
		t.Fatalf("missing disabled collapsible ops: header=%v close=%v ops=%+v", sawDisabledHeader, sawDisabledClose, rt.FrameOps())
	}
}

func TestTableViewPaintUsesStyleSheet(t *testing.T) {
	ClearStylePacks()
	t.Cleanup(ClearStylePacks)
	if !RegisterStylePackSource(`
@pack test.table;
tokens {
  color {
    surface: #111923;
    row-text: #cbd6e4;
    header: #263449;
    header-ink: #eef5ff;
    header-selected: #3b66ff;
    header-selected-ink: #ffffff;
    selected: #6f42c1;
    selected-ink: #fff7ff;
    rule: #5a6b7d;
  }
  length { radius: 4; border: 2; }
  material { flat: Flat; }
}
TableView[role=Panel] { background: surface; border: rule; radius: radius; border-width: border; material: flat; }
TableView[role=Header] { background: header; foreground: header-ink; border: rule; radius: radius; border-width: border; font-size: 17; opacity: 0.81; material: flat; }
TableView[role=Header]:selected { background: header-selected; foreground: header-selected-ink; border: header-selected; font-size: 19; opacity: 0.91; material: flat; }
TableView[role=Cell] { foreground: row-text; font-size: 14; opacity: 0.72; }
TableView[role=Selection] { background: selected; foreground: selected-ink; border: selected; radius: radius; border-width: border; font-size: 16; opacity: 0.83; material: flat; }
TableView[role=Divider] { border: rule; }
`, "Test Table", "") || !SetActiveStylePack("test.table") {
		t.Fatal("test table style did not activate")
	}
	rt := New(AppConfig{Width: 360, Height: 220}).(*runtime)
	selectedRow := int32(-1)
	selectedColumn := int32(1)
	scroll := int32(0)

	rt.BeginFrame()
	rt.TableView(TableViewProps{
		Bounds:         Rectangle{X: 10, Y: 10, Width: 260, Height: 92},
		ID:             77,
		Columns:        []string{"Name", "Value"},
		Rows:           []TableRow{{Cells: []string{"plain", "chosen"}}},
		ColumnWidths:   []int32{120, 140},
		SelectedRow:    &selectedRow,
		SelectedColumn: &selectedColumn,
		ScrollOffset:   &scroll,
		RowHeight:      28,
		Resizable:      true,
	})
	rt.EndFrame()

	var sawSurface, sawNormalHeader, sawSelectedHeader, sawDivider, sawBodyText, sawSelectedCell, sawSelectedText bool
	for _, op := range rt.FrameOps() {
		switch {
		case op.Kind == FrameOpRect && op.Bounds == (Rectangle{X: 10, Y: 10, Width: 260, Height: 92}):
			sawSurface = true
			if op.Color != (Color{R: 0x11, G: 0x19, B: 0x23, A: 0xff}) || op.BorderColor != (Color{R: 0x5a, G: 0x6b, B: 0x7d, A: 0xff}) || op.BorderWidth != 2 || op.Radius != 4 {
				t.Fatalf("table surface style op = %+v", op)
			}
		case op.Kind == FrameOpRect && op.Row == -1 && op.Column == 0:
			sawNormalHeader = true
			if op.Color != (Color{R: 0x26, G: 0x34, B: 0x49, A: 0xff}) || op.BorderColor != (Color{R: 0x5a, G: 0x6b, B: 0x7d, A: 0xff}) {
				t.Fatalf("table normal header style op = %+v", op)
			}
		case op.Kind == FrameOpText && op.Row == -1 && op.Column == 0:
			if op.Color != (Color{R: 0xee, G: 0xf5, B: 0xff, A: 0xff}) || op.FontSize != 17 || op.Opacity != 0.81 {
				t.Fatalf("table normal header text style op = %+v", op)
			}
		case op.Kind == FrameOpRect && op.Row == -1 && op.Column == 1 && op.Selected:
			sawSelectedHeader = true
			if op.Color != (Color{R: 0x3b, G: 0x66, B: 0xff, A: 0xff}) || op.BorderColor != (Color{R: 0x3b, G: 0x66, B: 0xff, A: 0xff}) {
				t.Fatalf("table selected header style op = %+v", op)
			}
		case op.Kind == FrameOpText && op.Row == -1 && op.Column == 1:
			if op.Color != (Color{R: 0xff, G: 0xff, B: 0xff, A: 0xff}) || op.FontSize != 19 || op.Opacity != 0.91 {
				t.Fatalf("table selected header text style op = %+v", op)
			}
		case op.Kind == FrameOpLine && op.Column == 0:
			sawDivider = true
			if op.Color != (Color{R: 0x5a, G: 0x6b, B: 0x7d, A: 0xff}) {
				t.Fatalf("table divider style op = %+v", op)
			}
		case op.Kind == FrameOpText && op.Row == 0 && op.Column == 0:
			sawBodyText = true
			if op.Color != (Color{R: 0xcb, G: 0xd6, B: 0xe4, A: 0xff}) || op.FontSize != 14 || op.Opacity != 0.72 {
				t.Fatalf("table body text style op = %+v", op)
			}
		case op.Kind == FrameOpRect && op.Row == 0 && op.Column == 1 && op.Selected:
			sawSelectedCell = true
			if op.Color != (Color{R: 0x6f, G: 0x42, B: 0xc1, A: 0xff}) || op.BorderColor != (Color{R: 0x6f, G: 0x42, B: 0xc1, A: 0xff}) {
				t.Fatalf("table selected cell style op = %+v", op)
			}
		case op.Kind == FrameOpText && op.Row == 0 && op.Column == 1:
			sawSelectedText = true
			if op.Color != (Color{R: 0xff, G: 0xf7, B: 0xff, A: 0xff}) || op.FontSize != 16 || op.Opacity != 0.83 {
				t.Fatalf("table selected cell text style op = %+v", op)
			}
		}
	}
	if !sawSurface || !sawNormalHeader || !sawSelectedHeader || !sawDivider || !sawBodyText || !sawSelectedCell || !sawSelectedText {
		t.Fatalf("missing styled table ops: surface=%v normalHeader=%v selectedHeader=%v divider=%v bodyText=%v selectedCell=%v selectedText=%v ops=%+v",
			sawSurface, sawNormalHeader, sawSelectedHeader, sawDivider, sawBodyText, sawSelectedCell, sawSelectedText, rt.FrameOps())
	}
}

func TestMenuPaintUsesStyleSheet(t *testing.T) {
	ClearStylePacks()
	t.Cleanup(ClearStylePacks)
	if !RegisterStylePackSource(`
@pack test.menu;
tokens {
  color {
    bar: #101820;
    panel: #202a36;
    item: #2f6bff;
    ink: #f5f2ff;
    rule: #708090;
  }
  length { radius: 5; border: 2; }
  material { flat: Flat; }
}
Menu[role=Bar] { background: bar; foreground: ink; border: rule; radius: radius; border-width: border; material: flat; }
Menu[role=Popup] { background: panel; foreground: ink; border: rule; radius: radius; border-width: border; material: flat; }
MenuItem { foreground: ink; font-size: 18; opacity: 0.72; }
MenuItem:selected { background: item; foreground: ink; border: rule; radius: radius; border-width: border; material: flat; }
MenuSeparator { border: rule; foreground: rule; material: flat; }
`, "Test Menu", "") || !SetActiveStylePack("test.menu") {
		t.Fatal("test menu style did not activate")
	}
	rt := New(AppConfig{Width: 240, Height: 120}).(*runtime)
	open := int32(0)
	menus := []MenuGroup{{
		Label: "File",
		Items: []MenuItem{
			{Kind: MenuCommand, Label: "Save", ID: 101},
			{Kind: MenuSeparator},
		},
	}}

	rt.Menu(MenuProps{ID: 10, Mode: MenuModeBar, Bounds: Rectangle{X: 0, Y: 0, Width: 240, Height: 30}, Menus: menus, OpenIndex: &open})

	var sawBar, sawPanel, sawBarText, sawItemText bool
	for _, op := range rt.FrameOps() {
		if op.Kind == FrameOpRect && op.Bounds == (Rectangle{X: 0, Y: 0, Width: 240, Height: 30}) {
			sawBar = true
			if op.Color != (Color{R: 0x10, G: 0x18, B: 0x20, A: 0xff}) || op.BorderWidth != 2 || op.Radius != 5 {
				t.Fatalf("menu bar style op = %+v", op)
			}
		}
		if op.Kind == FrameOpRect && op.Color == (Color{R: 0x20, G: 0x2a, B: 0x36, A: 0xff}) {
			sawPanel = true
			if op.BorderColor != (Color{R: 0x70, G: 0x80, B: 0x90, A: 0xff}) || op.BorderWidth != 2 || op.Radius != 5 {
				t.Fatalf("menu panel style op = %+v", op)
			}
		}
		if op.Kind == FrameOpText && op.Text == "File" {
			sawBarText = true
			if op.FontSize != 18 || op.Opacity != 0.72 {
				t.Fatalf("menu bar text style op = %+v", op)
			}
		}
		if op.Kind == FrameOpText && op.Text == "Save" {
			sawItemText = true
			if op.FontSize != 18 || op.Opacity != 0.72 {
				t.Fatalf("menu item text style op = %+v", op)
			}
		}
	}
	if !sawBar || !sawPanel || !sawBarText || !sawItemText {
		t.Fatalf("missing styled menu ops: bar=%v panel=%v bar_text=%v item_text=%v ops=%+v",
			sawBar, sawPanel, sawBarText, sawItemText, rt.FrameOps())
	}
}

func TestNavigationBarItemLabelUsesStyleSheet(t *testing.T) {
	ClearStylePacks()
	t.Cleanup(ClearStylePacks)
	if !RegisterStylePackSource(`
@pack test.navigation;
tokens {
  color {
    surface: #101820;
    ink: #d8f2ff;
    rule: #708090;
    selected: #2f6bff;
  }
  length { radius: 7; border: 1; }
  material { flat: Flat; }
}
NavigationBar { background: surface; foreground: ink; border: rule; radius: radius; border-width: border; material: flat; }
NavigationBarItem { foreground: ink; font-size: 19; opacity: 0.64; }
NavigationBarItem:selected { background: selected; foreground: ink; border: selected; radius: radius; border-width: border; material: flat; }
`, "Test Navigation", "") || !SetActiveStylePack("test.navigation") {
		t.Fatal("test navigation style did not activate")
	}
	rt := New(AppConfig{Width: 320, Height: 180}).(*runtime)
	items := []NavigationBarItem{{Label: "Home", Route: 44}}

	rt.NavigationBar(NavigationBarProps{
		ViewWidth: 320, ViewHeight: 180,
		Items: items, Count: int32(len(items)),
	})

	for _, op := range rt.FrameOps() {
		if op.Kind == FrameOpText && op.Text == "Home" {
			if op.FontSize != 19 || op.Opacity != 0.64 {
				t.Fatalf("navigation label style op = %+v", op)
			}
			return
		}
	}
	t.Fatalf("missing navigation label op: %+v", rt.FrameOps())
}

func TestNavigationWidgetsResolveClassSelectors(t *testing.T) {
	ClearStylePacks()
	t.Cleanup(ClearStylePacks)
	if !RegisterStylePackSource(`
@pack test.navigation.classes;
tokens {
  color {
    shell: #233142;
    action: #6247aa;
    ink: #f3f8ff;
    rule: #7d8da3;
  }
  length { radius: 6; border: 2; }
  material { flat: Flat; }
}
Toolbar.navkit[role=Bar] { background: shell; foreground: ink; border: rule; radius: radius; border-width: border; material: flat; }
Toolbar.navkit[role=Divider] { border: rule; }
Toolbar.navkit[role=Action] { background: action; foreground: ink; border: action; radius: radius; border-width: border; material: flat; }
NavigationBar.navkit { background: shell; foreground: ink; border: rule; radius: radius; border-width: border; material: flat; }
NavigationBarItem.navkit { foreground: ink; font-size: 18; opacity: 0.71; }
NavigationBarItem.navkit:selected { background: action; foreground: ink; border: action; radius: radius; border-width: border; material: flat; }
TabBar.navkit { background: shell; foreground: ink; border: rule; radius: radius; border-width: border; gap: 0; material: flat; }
Tab.navkit { background: shell; foreground: ink; border: rule; radius: radius; border-width: border; font-size: 17; material: flat; }
Tab.navkit:selected { background: action; foreground: ink; border: action; radius: radius; border-width: border; font-size: 17; material: flat; }
TabClose.navkit { foreground: ink; font-size: 17; material: flat; }
`, "Test Navigation Classes", "") || !SetActiveStylePack("test.navigation.classes") {
		t.Fatal("navigation class style did not activate")
	}
	rt := New(AppConfig{Width: 360, Height: 220}).(*runtime)
	className := StyleClassID("navkit")

	rt.Toolbar(ToolbarProps{
		ID:          90,
		ClassName:   className,
		X:           0,
		Y:           4,
		Width:       180,
		Height:      36,
		Actions:     []ToolbarAction{{IconType: IconSave}},
		ActionCount: 1,
	})
	rt.NavigationBar(NavigationBarProps{
		ViewWidth:  260,
		ViewHeight: 180,
		ClassName:  className,
		Items:      []NavigationBarItem{{Label: "Home", Route: 91, Active: true}},
		Count:      1,
	})
	rt.TabBar(TabBarProps{
		Bounds:        Rectangle{X: 0, Y: 50, Width: 180, Height: 30},
		ClassName:     className,
		Tabs:          []Tab{{Label: "Files", Closeable: true}},
		Count:         1,
		SelectedIndex: 0,
		ID:            92,
	})

	var sawToolbar, sawAction, sawNavLabel, sawTab bool
	for _, op := range rt.FrameOps() {
		switch {
		case op.Kind == FrameOpRect && op.Bounds == (Rectangle{X: 0, Y: 4, Width: 180, Height: 36}):
			sawToolbar = true
			if op.Color != (Color{R: 0x23, G: 0x31, B: 0x42, A: 0xff}) ||
				op.BorderColor != (Color{R: 0x7d, G: 0x8d, B: 0xa3, A: 0xff}) ||
				op.BorderWidth != 2 || op.Radius != 6 {
				t.Fatalf("toolbar class bar op = %+v", op)
			}
		case op.Kind == FrameOpButton && op.ID == 9001:
			sawAction = true
			style := unpackStyle(op.Button.Material.Value)
			if style.Background != (Color{R: 0x62, G: 0x47, B: 0xaa, A: 0xff}) ||
				style.Foreground != (Color{R: 0xf3, G: 0xf8, B: 0xff, A: 0xff}) {
				t.Fatalf("toolbar action class op = %+v", op)
			}
		case op.Kind == FrameOpText && op.ID == 91 && op.Text == "Home":
			sawNavLabel = true
			if op.FontSize != 18 || op.Opacity != 0.71 ||
				op.Color != (Color{R: 0xf3, G: 0xf8, B: 0xff, A: 0xff}) {
				t.Fatalf("navigation class label op = %+v", op)
			}
		case op.Kind == FrameOpButton && op.ID == 92 && op.Text == "Files":
			sawTab = true
			if op.FontSize != 17 ||
				op.Color != (Color{R: 0x62, G: 0x47, B: 0xaa, A: 0xff}) ||
				op.BorderColor != (Color{R: 0x62, G: 0x47, B: 0xaa, A: 0xff}) ||
				op.TextColor != (Color{R: 0xf3, G: 0xf8, B: 0xff, A: 0xff}) {
				t.Fatalf("tab class op = %+v", op)
			}
		}
	}
	if !sawToolbar || !sawAction || !sawNavLabel || !sawTab {
		t.Fatalf("missing classed navigation ops: toolbar=%v action=%v nav=%v tab=%v ops=%+v",
			sawToolbar, sawAction, sawNavLabel, sawTab, rt.FrameOps())
	}
}

func TestMenuResolvesClassSelectors(t *testing.T) {
	ClearStylePacks()
	t.Cleanup(ClearStylePacks)
	if !RegisterStylePackSource(`
@pack test.menu.classes;
tokens {
  color {
    bar: #122032;
    popup: #202636;
    ink: #e8f0ff;
    selected: #6a4bc3;
    rule: #a0a8b5;
  }
  length { radius: 5; border: 2; }
  material { flat: Flat; }
}
Menu.palette[role=Bar] { background: bar; foreground: ink; border: rule; radius: radius; border-width: border; material: flat; }
Menu.palette[role=Popup] { background: popup; foreground: ink; border: rule; radius: radius; border-width: border; material: flat; }
MenuItem.palette { foreground: ink; font-size: 15; material: flat; }
MenuItem.palette:selected { background: selected; foreground: ink; border: selected; radius: radius; border-width: border; font-size: 15; material: flat; }
MenuSeparator.palette { border: rule; }
`, "Test Menu Classes", "") || !SetActiveStylePack("test.menu.classes") {
		t.Fatal("menu class style did not activate")
	}
	rt := New(AppConfig{Width: 320, Height: 180}).(*runtime)
	open := int32(0)
	rt.Menu(MenuProps{
		ID:        93,
		ClassName: StyleClassID("palette"),
		Mode:      MenuModeBar,
		Bounds:    Rectangle{X: 0, Y: 0, Width: 220, Height: 30},
		Menus: []MenuGroup{{
			Label: "File",
			Items: []MenuItem{
				{Kind: MenuCommand, Label: "Open", ID: 1},
				{Kind: MenuSeparator},
				{Kind: MenuCommand, Label: "Save", ID: 2},
			},
		}},
		MenuCount: 1,
		OpenIndex: &open,
	})

	var sawBar, sawPopup, sawItem, sawSeparator bool
	for _, op := range rt.FrameOps() {
		switch {
		case op.Kind == FrameOpRect && op.Bounds == (Rectangle{X: 0, Y: 0, Width: 220, Height: 30}):
			sawBar = true
			if op.Color != (Color{R: 0x12, G: 0x20, B: 0x32, A: 0xff}) ||
				op.BorderColor != (Color{R: 0xa0, G: 0xa8, B: 0xb5, A: 0xff}) ||
				op.Radius != 5 || op.BorderWidth != 2 {
				t.Fatalf("menu bar class op = %+v", op)
			}
		case op.Kind == FrameOpRect && op.Color == (Color{R: 0x20, G: 0x26, B: 0x36, A: 0xff}):
			sawPopup = true
			if op.BorderColor != (Color{R: 0xa0, G: 0xa8, B: 0xb5, A: 0xff}) ||
				op.Radius != 5 || op.BorderWidth != 2 {
				t.Fatalf("menu popup class op = %+v", op)
			}
		case op.Kind == FrameOpText && op.Text == "Open":
			sawItem = true
			if op.FontSize != 15 || op.Color != (Color{R: 0xe8, G: 0xf0, B: 0xff, A: 0xff}) {
				t.Fatalf("menu item class op = %+v", op)
			}
		case op.Kind == FrameOpLine && op.Color == (Color{R: 0xa0, G: 0xa8, B: 0xb5, A: 0xff}):
			sawSeparator = true
		}
	}
	if !sawBar || !sawPopup || !sawItem || !sawSeparator {
		t.Fatalf("missing classed menu ops: bar=%v popup=%v item=%v separator=%v ops=%+v",
			sawBar, sawPopup, sawItem, sawSeparator, rt.FrameOps())
	}
}

func TestProgressAndSeparatorRolesUseStyleSheet(t *testing.T) {
	ClearStylePacks()
	t.Cleanup(ClearStylePacks)
	if !RegisterStylePackSource(`
@pack test.parts;
tokens {
  color {
    track: #18202a;
    fill: #c9a8ff;
    ink: #171022;
    label: #f5f2ff;
    line: #536070;
    bullet: #2f6bff;
  }
  length { radius: 6; border: 2; gap: 10; }
  material { flat: Flat; }
}
Progress[role=Track] { background: track; foreground: label; border: line; radius: radius; border-width: border; material: flat; opacity: 1; }
Progress.primary[role=Track] { background: #223344; }
Progress[role=Fill] { background: fill; foreground: ink; border: fill; radius: radius; border-width: border; material: flat; opacity: 1; }
Progress.primary[role=Fill] { background: #44aa77; }
Progress[role=Label] { foreground: label; font-size: 18; material: flat; opacity: 0.68; }
Progress.primary[role=Label] { foreground: #101820; }
Separator[role=Line] { background: line; foreground: line; gap: gap; opacity: 1; }
Separator.accent[role=Line] { background: #778899; }
Separator[role=Label] { background: line; foreground: label; font-size: 17; gap: gap; opacity: 0.57; }
Separator.accent[role=Label] { foreground: #ccffee; }
Separator[role=Bullet] { background: line; foreground: bullet; gap: gap; opacity: 1; }
`, "Test Parts", "") || !SetActiveStylePack("test.parts") {
		t.Fatal("test parts style did not activate")
	}
	rt := New(AppConfig{Width: 240, Height: 160}).(*runtime)

	rt.Progress(ProgressProps{
		Bounds:    Rectangle{X: 10, Y: 10, Width: 100, Height: 12},
		Min:       0,
		Max:       100,
		Value:     10,
		Label:     "10%",
		ClassName: StyleClassID("primary"),
	})
	rt.Separator(SeparatorProps{
		Bounds:    Rectangle{X: 10, Y: 40, Width: 100, Height: 20},
		Label:     "Section",
		ClassName: StyleClassID("accent"),
	})
	rt.Bullet(Rectangle{X: 10, Y: 70, Width: 12, Height: 12})

	var sawTrack, sawFill, sawProgressLabel, sawSeparatorLabel, sawSeparatorLine, sawBullet bool
	for _, op := range rt.FrameOps() {
		switch {
		case op.Kind == FrameOpRect && op.Bounds == (Rectangle{X: 10, Y: 10, Width: 100, Height: 12}):
			sawTrack = true
			if op.Color != (Color{R: 0x22, G: 0x33, B: 0x44, A: 0xff}) ||
				op.BorderColor != (Color{R: 0x53, G: 0x60, B: 0x70, A: 0xff}) ||
				op.BorderWidth != 2 || op.Radius != 6 {
				t.Fatalf("progress track style op = %+v", op)
			}
		case op.Kind == FrameOpRect && op.Selected:
			sawFill = true
			if op.Color != (Color{R: 0x44, G: 0xaa, B: 0x77, A: 0xff}) ||
				op.Radius != 6 {
				t.Fatalf("progress fill style op = %+v", op)
			}
		case op.Kind == FrameOpText && op.Text == "10%":
			sawProgressLabel = true
			if op.Color != (Color{R: 0x10, G: 0x18, B: 0x20, A: 0xff}) ||
				op.FontSize != 18 || op.Opacity != 0.68 {
				t.Fatalf("progress label on fill style op = %+v", op)
			}
		case op.Kind == FrameOpText && op.Text == "Section":
			sawSeparatorLabel = true
			if op.Color != (Color{R: 0xcc, G: 0xff, B: 0xee, A: 0xff}) ||
				op.FontSize != 17 || op.Opacity != 0.57 {
				t.Fatalf("separator label style op = %+v", op)
			}
		case op.Kind == FrameOpLine && op.Bounds.Y == 50:
			sawSeparatorLine = true
			if op.Color != (Color{R: 0x77, G: 0x88, B: 0x99, A: 0xff}) {
				t.Fatalf("separator line style op = %+v", op)
			}
		case op.Kind == FrameOpRect && op.Bounds.X == 13 && op.Bounds.Y == 73:
			sawBullet = true
			if op.Color != (Color{R: 0x2f, G: 0x6b, B: 0xff, A: 0xff}) {
				t.Fatalf("separator bullet style op = %+v", op)
			}
		}
	}
	if !sawTrack || !sawFill || !sawProgressLabel || !sawSeparatorLabel ||
		!sawSeparatorLine || !sawBullet {
		t.Fatalf("missing role styled ops: track=%v fill=%v progressLabel=%v separatorLabel=%v separatorLine=%v bullet=%v ops=%+v",
			sawTrack, sawFill, sawProgressLabel, sawSeparatorLabel, sawSeparatorLine, sawBullet, rt.FrameOps())
	}
}

func TestCheckboxAndRadioRolesUseStyleSheet(t *testing.T) {
	ClearStylePacks()
	t.Cleanup(ClearStylePacks)
	if !RegisterStylePackSource(`
@pack test.checks;
tokens {
  color {
    box: #18202a;
    mark: #c9a8ff;
    mark-ink: #171022;
    label: #f5f2ff;
    ring: #536070;
  }
  length { border: 2; }
  material { flat: Flat; }
}
Checkbox[role=Box] { background: box; foreground: label; border: ring; border-width: border; material: flat; opacity: 1; }
Checkbox[role=Mark] { background: mark; foreground: mark-ink; border: mark; border-width: border; material: flat; opacity: 1; }
Checkbox.primary[role=Mark] { background: #55bb88; foreground: #081018; border: #55bb88; }
Checkbox[role=Label] { foreground: label; font-size: 18; material: flat; opacity: 0.84; }
Checkbox.primary[role=Label] { foreground: #ddffee; }
Radio[role=Ring] { foreground: label; border: ring; border-width: border; material: flat; opacity: 1; }
Radio[role=Mark] { background: mark; foreground: mark-ink; border: mark; border-width: border; font-size: 21; material: flat; opacity: 0.63; }
Radio.primary[role=Mark] { background: #66cc99; foreground: #081018; border: #66cc99; }
Radio[role=Label] { foreground: label; font-size: 19; material: flat; opacity: 0.76; }
Radio.primary[role=Label] { foreground: #eef8cc; }
`, "Test Checks", "") || !SetActiveStylePack("test.checks") {
		t.Fatal("test check/radio style did not activate")
	}
	rt := New(AppConfig{Width: 240, Height: 160}).(*runtime)
	checked := int32(1)

	rt.Checkbox(CheckboxProps{
		Bounds:    Rectangle{X: 10, Y: 10, Width: 140, Height: 28},
		ID:        41,
		Label:     "Enabled",
		Value:     &checked,
		ClassName: StyleClassID("primary"),
	})
	rt.Radio(RadioProps{
		Bounds:    Rectangle{X: 10, Y: 50, Width: 140, Height: 28},
		ID:        42,
		Label:     "Choice",
		Checked:   true,
		ClassName: StyleClassID("primary"),
	})

	var sawBox, sawCheckMark, sawCheckLabel, sawRadioMark, sawRadioLabel bool
	for _, op := range rt.FrameOps() {
		switch {
		case op.Kind == FrameOpRect && op.ID == 41:
			sawBox = true
			if op.Color != (Color{R: 0x55, G: 0xbb, B: 0x88, A: 0xff}) ||
				op.BorderColor != (Color{R: 0x55, G: 0xbb, B: 0x88, A: 0xff}) {
				t.Fatalf("checkbox box/mark style op = %+v", op)
			}
		case op.Kind == FrameOpLine && op.ID == 41:
			sawCheckMark = true
			if op.Color != (Color{R: 0x08, G: 0x10, B: 0x18, A: 0xff}) {
				t.Fatalf("checkbox mark style op = %+v", op)
			}
		case op.Kind == FrameOpText && op.Text == "Enabled":
			sawCheckLabel = true
			if op.Color != (Color{R: 0xdd, G: 0xff, B: 0xee, A: 0xff}) ||
				op.FontSize != 18 || op.Opacity != 0.84 {
				t.Fatalf("checkbox label style op = %+v", op)
			}
		case op.Kind == FrameOpText && op.Text == "\u25c9":
			sawRadioMark = true
			if op.Color != (Color{R: 0x66, G: 0xcc, B: 0x99, A: 0xff}) ||
				op.FontSize != 21 || op.Opacity != 0.63 {
				t.Fatalf("radio mark style op = %+v", op)
			}
		case op.Kind == FrameOpText && op.Text == "Choice":
			sawRadioLabel = true
			if op.Color != (Color{R: 0xee, G: 0xf8, B: 0xcc, A: 0xff}) ||
				op.FontSize != 19 || op.Opacity != 0.76 {
				t.Fatalf("radio label style op = %+v", op)
			}
		}
	}
	if !sawBox || !sawCheckMark || !sawCheckLabel || !sawRadioMark || !sawRadioLabel {
		t.Fatalf("missing check/radio role styled ops: box=%v checkMark=%v checkLabel=%v radioMark=%v radioLabel=%v ops=%+v",
			sawBox, sawCheckMark, sawCheckLabel, sawRadioMark, sawRadioLabel, rt.FrameOps())
	}
}

func TestListBoxPaintUsesStyleSheet(t *testing.T) {
	ClearStylePacks()
	t.Cleanup(ClearStylePacks)
	if !RegisterStylePackSource(`
@pack test.listbox;
tokens {
  color {
    panel: #18202a;
    selected: #c9a8ff;
    ink: #171022;
    rule: #536070;
  }
  length { radius: 6; border: 2; inset: 11; }
  material { flat: Flat; }
}
ListBox { background: panel; foreground: ink; border: rule; radius: radius; border-width: border; material: flat; }
ListBoxItem:selected { background: selected; foreground: ink; border: selected; radius: radius; border-width: border; padding-x: inset; padding-y: border; font-size: 18; material: flat; opacity: 0.62; }
`, "Test ListBox", "") || !SetActiveStylePack("test.listbox") {
		t.Fatal("test list box style did not activate")
	}
	rt := New(AppConfig{Width: 240, Height: 120}).(*runtime)
	selected := int32(1)
	items := []string{"One", "Two", "Three"}

	rt.ListBox(ListBoxProps{
		Bounds:        Rectangle{X: 8, Y: 8, Width: 120, Height: 72},
		ID:            22,
		Items:         items,
		SelectedIndex: &selected,
		RowHeight:     24,
	})

	var sawPanel, sawSelected, sawSelectedText bool
	for _, op := range rt.FrameOps() {
		if op.Kind == FrameOpRect && op.Bounds == (Rectangle{X: 8, Y: 8, Width: 120, Height: 72}) {
			sawPanel = true
			if op.Color != (Color{R: 0x18, G: 0x20, B: 0x2a, A: 0xff}) || op.BorderWidth != 2 || op.Radius != 6 {
				t.Fatalf("list box panel style op = %+v", op)
			}
		}
		if op.Kind == FrameOpRect && op.Row == 1 && op.Selected {
			sawSelected = true
			if op.Color != (Color{R: 0xc9, G: 0xa8, B: 0xff, A: 0xff}) || op.BorderColor != (Color{R: 0xc9, G: 0xa8, B: 0xff, A: 0xff}) {
				t.Fatalf("list box selected style op = %+v", op)
			}
		}
		if op.Kind == FrameOpText && op.Row == 1 && op.Selected {
			sawSelectedText = true
			if op.FontSize != 18 || op.Opacity != 0.62 || op.Bounds.X != 19 || op.Bounds.Y != 34 {
				t.Fatalf("list box selected text style op = %+v", op)
			}
		}
	}
	if !sawPanel || !sawSelected || !sawSelectedText {
		t.Fatalf("missing styled list box ops: panel=%v selected=%v selectedText=%v ops=%+v", sawPanel, sawSelected, sawSelectedText, rt.FrameOps())
	}
}

func TestTreeViewPaintUsesStyleSheet(t *testing.T) {
	ClearStylePacks()
	t.Cleanup(ClearStylePacks)
	if !RegisterStylePackSource(`
@pack test.tree;
tokens {
  color {
    panel: #18202a;
    selected: #c9a8ff;
    ink: #171022;
    rule: #536070;
  }
  length { radius: 6; border: 2; }
  material { flat: Flat; }
}
TreeView { background: panel; foreground: ink; border: rule; radius: radius; border-width: border; material: flat; }
TreeViewItem:selected { background: selected; foreground: ink; border: selected; radius: radius; border-width: border; font-size: 18; material: flat; opacity: 0.58; }
`, "Test TreeView", "") || !SetActiveStylePack("test.tree") {
		t.Fatal("test tree view style did not activate")
	}
	rt := New(AppConfig{Width: 240, Height: 120}).(*runtime)
	selected := int32(2)
	items := []TreeItem{
		{Label: "Root", ID: 1, Expanded: 1},
		{Label: "Child", Depth: 1, ID: 2, Selectable: 1},
	}

	rt.TreeView(TreeViewProps{
		Bounds:     Rectangle{X: 8, Y: 8, Width: 120, Height: 72},
		ID:         33,
		Items:      items,
		SelectedID: &selected,
		RowHeight:  24,
	})

	var sawPanel, sawSelected, sawLabel bool
	for _, op := range rt.FrameOps() {
		if op.Kind == FrameOpRect && op.Bounds == (Rectangle{X: 8, Y: 8, Width: 120, Height: 72}) {
			sawPanel = true
			if op.Color != (Color{R: 0x18, G: 0x20, B: 0x2a, A: 0xff}) || op.BorderWidth != 2 || op.Radius != 6 {
				t.Fatalf("tree view panel style op = %+v", op)
			}
		}
		if op.Kind == FrameOpRect && op.Row == 1 && op.Selected {
			sawSelected = true
			if op.Color != (Color{R: 0xc9, G: 0xa8, B: 0xff, A: 0xff}) || op.BorderColor != (Color{R: 0xc9, G: 0xa8, B: 0xff, A: 0xff}) {
				t.Fatalf("tree view selected style op = %+v", op)
			}
		}
		if op.Kind == FrameOpText && op.Text == "Child" {
			sawLabel = true
			if op.FontSize != 18 || op.Opacity != 0.58 ||
				op.Color != (Color{R: 0x17, G: 0x10, B: 0x22, A: 0xff}) {
				t.Fatalf("tree view item text style op = %+v", op)
			}
		}
	}
	if !sawPanel || !sawSelected || !sawLabel {
		t.Fatalf("missing styled tree view ops: panel=%v selected=%v label=%v ops=%+v", sawPanel, sawSelected, sawLabel, rt.FrameOps())
	}
}

func TestListBoxMultiPaintUsesStyleSheet(t *testing.T) {
	ClearStylePacks()
	t.Cleanup(ClearStylePacks)
	if !RegisterStylePackSource(`
@pack test.multi;
tokens {
  color {
    panel: #18202a;
    selected: #c9a8ff;
    ink: #171022;
    rule: #536070;
  }
  length { radius: 6; border: 2; }
  material { flat: Flat; }
}
ListBoxMulti { background: panel; foreground: ink; border: rule; radius: radius; border-width: border; material: flat; }
ListBoxMultiItem:selected { background: selected; foreground: ink; border: selected; radius: radius; border-width: border; padding-x: 11; padding-y: 3; font-size: 17; material: flat; opacity: 0.66; }
`, "Test MultiSelect", "") || !SetActiveStylePack("test.multi") {
		t.Fatal("test multi select style did not activate")
	}
	rt := New(AppConfig{Width: 240, Height: 120}).(*runtime)
	selected := []int32{0, 1, 0}
	count := int32(0)
	anchor := int32(1)

	rt.ListBox(ListBoxProps{
		Bounds:        Rectangle{X: 8, Y: 8, Width: 120, Height: 72},
		ID:            44,
		Items:         []string{"One", "Two", "Three"},
		Selected:      selected,
		SelectedCount: &count,
		Anchor:        &anchor,
		RowHeight:     24,
	})

	if count != 1 {
		t.Fatalf("selected count = %d", count)
	}
	var sawPanel, sawSelected bool
	for _, op := range rt.FrameOps() {
		if op.Kind != FrameOpRect {
			continue
		}
		if op.Bounds == (Rectangle{X: 8, Y: 8, Width: 120, Height: 72}) {
			sawPanel = true
			if op.Color != (Color{R: 0x18, G: 0x20, B: 0x2a, A: 0xff}) || op.BorderWidth != 2 || op.Radius != 6 {
				t.Fatalf("multi select panel style op = %+v", op)
			}
		}
		if op.Row == 1 && op.Selected {
			sawSelected = true
			if op.Color != (Color{R: 0xc9, G: 0xa8, B: 0xff, A: 0xff}) || op.BorderColor != (Color{R: 0xc9, G: 0xa8, B: 0xff, A: 0xff}) {
				t.Fatalf("multi select selected style op = %+v", op)
			}
		}
	}
	var sawLabel bool
	for _, op := range rt.FrameOps() {
		if op.Kind == FrameOpText && op.Text == "Two" {
			sawLabel = true
			if op.Bounds.X != 19 || op.Bounds.Y != 35 ||
				op.Bounds.Width != 98 || op.Bounds.Height != 18 ||
				op.Color != (Color{R: 0x17, G: 0x10, B: 0x22, A: 0xff}) ||
				op.FontSize != 17 || op.Opacity != 0.66 {
				t.Fatalf("styled multi item text op = %+v", op)
			}
		}
	}
	if !sawPanel || !sawSelected || !sawLabel {
		t.Fatalf("missing styled multi select ops: panel=%v selected=%v label=%v ops=%+v", sawPanel, sawSelected, sawLabel, rt.FrameOps())
	}
}

func TestGeneratedChoiceWidgetsResolveClassSelectors(t *testing.T) {
	ClearStylePacks()
	t.Cleanup(ClearStylePacks)
	if !RegisterStylePackSource(`
@pack test.choice.classes;
tokens {
  color {
    trigger: #204060;
    panel: #223344;
    ink: #f4fbff;
    row: #6750a4;
  }
  length { radius: 7; border: 2; inset: 13; }
  material { flat: Flat; }
}
Dropdown.choice { background: trigger; foreground: ink; border: panel; radius: radius; border-width: border; font-size: 18; material: flat; }
Segment.choice { background: row; foreground: ink; border: row; radius: radius; border-width: border; font-size: 19; material: flat; }
ListBox.choice { background: panel; foreground: ink; border: trigger; radius: radius; border-width: border; material: flat; }
ListBoxItem.choice:selected { background: row; foreground: ink; padding-x: inset; font-size: 18; opacity: 0.74; material: flat; }
ListBoxMulti.choice { background: panel; foreground: ink; border: trigger; radius: radius; border-width: border; material: flat; }
ListBoxMultiItem.choice:selected { background: row; foreground: ink; padding-x: inset; font-size: 17; opacity: 0.73; material: flat; }
TreeView.choice { background: panel; foreground: ink; border: trigger; radius: radius; border-width: border; material: flat; }
TreeViewItem.choice:selected { background: row; foreground: ink; font-size: 18; opacity: 0.72; material: flat; }
`, "Test Choice Classes", "") || !SetActiveStylePack("test.choice.classes") {
		t.Fatal("choice class style pack did not activate")
	}
	rt := New(AppConfig{Width: 360, Height: 260}).(*runtime)
	className := StyleClassID("choice")
	dropdownSelected := int32(0)
	segmentSelected := int32(0)
	listSelected := int32(1)
	multiSelected := []int32{0, 1}
	multiCount := int32(0)
	treeSelected := int32(2)

	rt.Dropdown(DropdownProps{Bounds: Rectangle{X: 8, Y: 8, Width: 120, Height: 28}, ID: 70, ClassName: className, Options: []string{"Alpha"}, SelectedIndex: &dropdownSelected})
	rt.SegmentedControl(SegmentedControlProps{Bounds: Rectangle{X: 8, Y: 44, Width: 140, Height: 30}, ID: 71, ClassName: className, Options: []SegmentOption{{Label: "One"}}, SelectedIndex: &segmentSelected})
	rt.ListBox(ListBoxProps{Bounds: Rectangle{X: 8, Y: 82, Width: 120, Height: 60}, ID: 72, ClassName: className, Items: []string{"One", "Two"}, SelectedIndex: &listSelected, RowHeight: 24})
	rt.ListBox(ListBoxProps{Bounds: Rectangle{X: 160, Y: 82, Width: 120, Height: 60}, ID: 73, ClassName: className, Items: []string{"One", "Two"}, Selected: multiSelected, SelectedCount: &multiCount, RowHeight: 24})
	rt.TreeView(TreeViewProps{Bounds: Rectangle{X: 8, Y: 152, Width: 120, Height: 60}, ID: 74, ClassName: className, Items: []TreeItem{{Label: "Root", ID: 1}, {Label: "Child", ID: 2, Selectable: 1}}, SelectedID: &treeSelected, RowHeight: 24})

	var sawDropdown, sawSegment, sawListPanel, sawListItem, sawMultiPanel, sawMultiItem, sawTreePanel, sawTreeItem bool
	for _, op := range rt.FrameOps() {
		switch {
		case op.Kind == FrameOpText && op.ID == 70 && op.Row == -1:
			sawDropdown = true
			if op.Color != (Color{R: 0xf4, G: 0xfb, B: 0xff, A: 0xff}) || op.FontSize != 18 {
				t.Fatalf("dropdown class style op = %+v", op)
			}
		case op.Kind == FrameOpButton && op.ID == 71001:
			sawSegment = true
			style := unpackStyle(op.Button.Appearance.Value)
			if op.Button.Font != 19 || style.Background != (Color{R: 0x67, G: 0x50, B: 0xa4, A: 0xff}) || style.Foreground != (Color{R: 0xf4, G: 0xfb, B: 0xff, A: 0xff}) {
				t.Fatalf("segment class style op = %+v", op)
			}
		case op.Kind == FrameOpRect && op.ID == 72 && op.Row == 0 && op.Bounds == (Rectangle{X: 8, Y: 82, Width: 120, Height: 60}):
			sawListPanel = true
			if op.Color != (Color{R: 0x22, G: 0x33, B: 0x44, A: 0xff}) || op.BorderWidth != 2 || op.Radius != 7 {
				t.Fatalf("list panel class style op = %+v", op)
			}
		case op.Kind == FrameOpText && op.ID == 72 && op.Row == 1:
			sawListItem = true
			if op.FontSize != 18 || op.Opacity != 0.74 || op.Bounds.X != 21 || op.Color != (Color{R: 0xf4, G: 0xfb, B: 0xff, A: 0xff}) {
				t.Fatalf("list item class style op = %+v", op)
			}
		case op.Kind == FrameOpRect && op.ID == 73 && op.Row == 0 && op.Bounds == (Rectangle{X: 160, Y: 82, Width: 120, Height: 60}):
			sawMultiPanel = true
			if op.Color != (Color{R: 0x22, G: 0x33, B: 0x44, A: 0xff}) || op.BorderWidth != 2 || op.Radius != 7 {
				t.Fatalf("multi panel class style op = %+v", op)
			}
		case op.Kind == FrameOpText && op.ID == 73 && op.Row == 1:
			sawMultiItem = true
			if op.FontSize != 17 || op.Opacity != 0.73 || op.Bounds.X != 173 || op.Color != (Color{R: 0xf4, G: 0xfb, B: 0xff, A: 0xff}) {
				t.Fatalf("multi item class style op = %+v", op)
			}
		case op.Kind == FrameOpRect && op.ID == 74 && op.Row == 0 && op.Bounds == (Rectangle{X: 8, Y: 152, Width: 120, Height: 60}):
			sawTreePanel = true
			if op.Color != (Color{R: 0x22, G: 0x33, B: 0x44, A: 0xff}) || op.BorderWidth != 2 || op.Radius != 7 {
				t.Fatalf("tree panel class style op = %+v", op)
			}
		case op.Kind == FrameOpText && op.ID == 2 && op.Text == "Child":
			sawTreeItem = true
			if op.FontSize != 18 || op.Opacity != 0.72 || op.Color != (Color{R: 0xf4, G: 0xfb, B: 0xff, A: 0xff}) {
				t.Fatalf("tree item class style op = %+v", op)
			}
		}
	}
	if !sawDropdown || !sawSegment || !sawListPanel || !sawListItem ||
		!sawMultiPanel || !sawMultiItem || !sawTreePanel || !sawTreeItem {
		t.Fatalf("missing class styled ops: dropdown=%v segment=%v listPanel=%v listItem=%v multiPanel=%v multiItem=%v treePanel=%v treeItem=%v ops=%+v",
			sawDropdown, sawSegment, sawListPanel, sawListItem, sawMultiPanel, sawMultiItem, sawTreePanel, sawTreeItem, rt.FrameOps())
	}
}

func TestDragDropTargetPaintUsesStyleSheet(t *testing.T) {
	ClearStylePacks()
	t.Cleanup(ClearStylePacks)
	if !RegisterStylePackSource(`
@pack test.drag;
tokens {
  color {
    target: #536070;
    hot: #c9a8ff;
  }
  length { radius: 6; border: 1; hot-border: 3; }
  material { flat: Flat; }
}
DragDropTarget { background: #00000000; border: target; radius: radius; border-width: border; material: flat; }
DragDropTarget:hover { border: hot; border-width: hot-border; }
`, "Test Drag", "") || !SetActiveStylePack("test.drag") {
		t.Fatal("test drag target style did not activate")
	}
	rt := New(AppConfig{Width: 240, Height: 120}).(*runtime)
	rt.dragDrop = dragDropState{active: true, typeName: "ITEM", data: []byte("item")}
	rt.QueueMouseMove(24, 24)

	rt.DragDrop(DragDropProps{
		Bounds: Rectangle{X: 8, Y: 8, Width: 120, Height: 40},
		ID:     55,
		Role:   DragDropRoleTarget,
		Type:   "ITEM",
	})

	var sawTarget bool
	for _, op := range rt.FrameOps() {
		if op.Kind == FrameOpRect && op.ID == 55 {
			sawTarget = true
			if !op.Hovered || op.BorderWidth != 3 || op.BorderColor != (Color{R: 0xc9, G: 0xa8, B: 0xff, A: 0xff}) {
				t.Fatalf("drag drop target style op = %+v", op)
			}
		}
	}
	if !sawTarget {
		t.Fatalf("missing drag drop target op: %+v", rt.FrameOps())
	}
}

func TestSpinboxPaintUsesStyleSheet(t *testing.T) {
	ClearStylePacks()
	t.Cleanup(ClearStylePacks)
	if !RegisterStylePackSource(`
@pack test.spinbox;
tokens {
  color {
    shell: #18202a;
    value: #c9a8ff;
    ink: #171022;
    rule: #536070;
  }
  length { radius: 6; border: 2; }
  material { flat: Flat; }
}
Spinbox { background: shell; foreground: ink; border: rule; radius: radius; border-width: border; material: flat; }
SpinboxValue { background: value; foreground: ink; border: value; radius: radius; border-width: border; font-size: 19; material: flat; opacity: 0.61; }
`, "Test Spinbox", "") || !SetActiveStylePack("test.spinbox") {
		t.Fatal("test spinbox style did not activate")
	}
	rt := New(AppConfig{Width: 240, Height: 120}).(*runtime)
	value := int32(3)

	rt.Spinbox(SpinboxProps{
		Bounds: Rectangle{X: 8, Y: 8, Width: 120, Height: 30},
		ID:     66,
		Min:    0,
		Max:    10,
		Value:  &value,
	})

	var sawShell, sawValue, sawText bool
	for _, op := range rt.FrameOps() {
		if op.Kind == FrameOpRect && op.ID == 66 && op.Bounds == (Rectangle{X: 8, Y: 8, Width: 120, Height: 30}) {
			sawShell = true
			if op.Color != (Color{R: 0x18, G: 0x20, B: 0x2a, A: 0xff}) || op.BorderWidth != 2 || op.Radius != 6 {
				t.Fatalf("spinbox shell style op = %+v", op)
			}
		}
		if op.Kind == FrameOpRect && op.ID == 66 && op.Bounds == (Rectangle{X: 36, Y: 8, Width: 64, Height: 30}) {
			sawValue = true
			if op.Color != (Color{R: 0xc9, G: 0xa8, B: 0xff, A: 0xff}) || op.BorderColor != (Color{R: 0xc9, G: 0xa8, B: 0xff, A: 0xff}) {
				t.Fatalf("spinbox value style op = %+v", op)
			}
		}
		if op.Kind == FrameOpText && op.ID == 66 && op.Text == "3" {
			sawText = true
			if op.FontSize != 19 || op.Opacity != 0.61 ||
				op.Color != (Color{R: 0x17, G: 0x10, B: 0x22, A: 0xff}) {
				t.Fatalf("spinbox value text style op = %+v", op)
			}
		}
	}
	if !sawShell || !sawValue || !sawText {
		t.Fatalf("missing styled spinbox ops: shell=%v value=%v text=%v ops=%+v", sawShell, sawValue, sawText, rt.FrameOps())
	}
}

func TestColorPickerSwatchPaintUsesStyleSheet(t *testing.T) {
	ClearStylePacks()
	t.Cleanup(ClearStylePacks)
	if !RegisterStylePackSource(`
@pack test.color;
tokens {
  color {
    ink: #171022;
    rule: #c9a8ff;
  }
  length { radius: 6; border: 2; }
  material { flat: Flat; }
}
ColorPickerSwatch { background: #00000000; foreground: ink; border: rule; radius: radius; border-width: border; padding-x: 10; font-size: 18; material: flat; opacity: 0.64; }
`, "Test Color", "") || !SetActiveStylePack("test.color") {
		t.Fatal("test color picker style did not activate")
	}
	rt := New(AppConfig{Width: 240, Height: 220}).(*runtime)
	values := []float32{0.1, 0.2, 0.3, 1}

	rt.ColorPicker(ColorPickerProps{
		Bounds:     Rectangle{X: 8, Y: 8, Width: 120, Height: 160},
		ID:         77,
		Label:      "Preview",
		Values:     values,
		ValueCount: 4,
		Picker:     true,
	})

	var sawSwatch, sawLabel bool
	for _, op := range rt.FrameOps() {
		if op.ID != 77 {
			continue
		}
		if op.Kind == FrameOpRect && op.Bounds == (Rectangle{X: 8, Y: 132, Width: 120, Height: 36}) {
			sawSwatch = true
			if op.Color != (Color{R: 26, G: 51, B: 77, A: 255}) || op.BorderColor != (Color{R: 0xc9, G: 0xa8, B: 0xff, A: 0xff}) || op.BorderWidth != 2 || op.Radius != 6 {
				t.Fatalf("color picker swatch style op = %+v", op)
			}
		}
		if op.Kind == FrameOpText && op.Text == "Preview" {
			sawLabel = true
			if op.Bounds.X != 18 || op.Bounds.Width != 100 ||
				op.Color != (Color{R: 0x17, G: 0x10, B: 0x22, A: 0xff}) ||
				op.FontSize != 18 || op.Opacity != 0.64 {
				t.Fatalf("color picker swatch label op = %+v", op)
			}
		}
	}
	if !sawSwatch || !sawLabel {
		t.Fatalf("missing styled color picker ops: swatch=%v label=%v ops=%+v", sawSwatch, sawLabel, rt.FrameOps())
	}
}

func TestSliderPaintUsesStyleSheet(t *testing.T) {
	ClearStylePacks()
	t.Cleanup(ClearStylePacks)
	if !RegisterStylePackSource(`
@pack test.slider;
tokens {
  color {
    track: #18202a;
    active: #c9a8ff;
    thumb: #f8f5ff;
    thumb-highlight: #ffffff66;
    ink: #171022;
    label: #3b2f55;
    rule: #536070;
  }
  length { radius: 6; border: 2; }
  material { flat: Flat; }
}
Slider[role=Track] { background: track; foreground: ink; border: rule; radius: radius; border-width: border; material: flat; }
Slider.primary[role=Track] { background: #223344; border: #6a7888; }
Slider[role=Fill] { background: active; foreground: ink; border: active; radius: radius; border-width: border; material: flat; }
Slider.primary[role=Fill] { background: #ddeeff; border: #ccddee; }
Slider[role=Label] { foreground: label; font-size: 18; opacity: 0.67; }
Slider.primary[role=Label] { foreground: #102030; font-size: 19; opacity: 0.77; }
`, "Test Slider", "") || !SetActiveStylePack("test.slider") {
		t.Fatal("test slider style did not activate")
	}
	rt := New(AppConfig{Width: 240, Height: 120}).(*runtime)
	values := []float32{0.5}

	rt.Slider(SliderProps{
		Bounds:      Rectangle{X: 8, Y: 28, Width: 120, Height: 30},
		ID:          88,
		Label:       "Level",
		Kind:        NumericFloat,
		FloatValues: values,
		ValueCount:  1,
		Min:         0,
		Max:         1,
		Format:      "%.1f",
		ClassName:   StyleClassID("primary"),
	})

	var sawTrack, sawFill, sawValue, sawLabel bool
	for _, op := range rt.FrameOps() {
		if op.ID != 88 {
			continue
		}
		switch {
		case op.Kind == FrameOpRect && op.Row == 0 && !op.Selected:
			sawTrack = true
			if op.Color != (Color{R: 0x22, G: 0x33, B: 0x44, A: 0xff}) || op.BorderColor != (Color{R: 0x6a, G: 0x78, B: 0x88, A: 0xff}) || op.BorderWidth != 2 || op.Radius != 6 {
				t.Fatalf("slider track style op = %+v", op)
			}
		case op.Kind == FrameOpRect && op.Row == 0 && op.Selected:
			sawFill = true
			if op.Color != (Color{R: 0xdd, G: 0xee, B: 0xff, A: 0xff}) || op.BorderColor != (Color{R: 0xcc, G: 0xdd, B: 0xee, A: 0xff}) {
				t.Fatalf("slider active style op = %+v", op)
			}
		case op.Kind == FrameOpText && op.Row == 0 && op.Text == "0.5":
			sawValue = true
			if op.Color != (Color{R: 0x10, G: 0x20, B: 0x30, A: 0xff}) ||
				op.FontSize != 19 || op.Opacity != 0.77 {
				t.Fatalf("slider value text style op = %+v", op)
			}
		case op.Kind == FrameOpText && op.Text == "Level":
			sawLabel = true
			if op.Color != (Color{R: 0x10, G: 0x20, B: 0x30, A: 0xff}) ||
				op.FontSize != 19 || op.Opacity != 0.77 {
				t.Fatalf("slider label style op = %+v", op)
			}
		}
	}
	if !sawTrack || !sawFill || !sawValue || !sawLabel {
		t.Fatalf("missing styled slider ops: track=%v fill=%v value=%v label=%v ops=%+v", sawTrack, sawFill, sawValue, sawLabel, rt.FrameOps())
	}
}

func TestSliderAtUsesStyleSheet(t *testing.T) {
	ClearStylePacks()
	t.Cleanup(ClearStylePacks)
	if !RegisterStylePackSource(`
@pack test.slider_legacy;
tokens {
  color {
    track: #18202a;
    active: #c9a8ff;
    thumb: #f8f5ff;
    thumb-highlight: #ffffff66;
    ink: #171022;
    label: #3b2f55;
    rule: #536070;
  }
  length { radius: 6; border: 2; }
  material { flat: Flat; }
}
Slider[role=Track] { background: track; foreground: ink; border: rule; radius: radius; border-width: border; material: flat; }
Slider[role=Fill] { background: active; foreground: ink; border: active; radius: radius; border-width: border; material: flat; }
Slider[role=Label] { foreground: label; font-size: 17; opacity: 0.58; }
SliderThumb { background: thumb; foreground: thumb-highlight; border: active; focus: active; radius: radius; border-width: border; material: flat; }
`, "Test Slider Legacy", "") || !SetActiveStylePack("test.slider_legacy") {
		t.Fatal("test legacy slider style did not activate")
	}
	rt := New(AppConfig{Width: 240, Height: 120}).(*runtime)
	value := int32(50)

	rt.sliderAt(188, Rectangle{X: 8, Y: 18, Width: 120, Height: 46}, "Amount", 0, 100, &value, "%")

	var sawTrack, sawFill, sawLabel, sawValue, sawThumb bool
	for _, op := range rt.FrameOps() {
		if op.ID != 188 {
			continue
		}
		switch {
		case op.Kind == FrameOpRect && !op.Selected:
			sawTrack = true
			if op.Color != (Color{R: 0x18, G: 0x20, B: 0x2a, A: 0xff}) || op.BorderColor != (Color{R: 0x53, G: 0x60, B: 0x70, A: 0xff}) || op.BorderWidth != 2 || op.Radius != 6 {
				t.Fatalf("sliderAt track style op = %+v", op)
			}
		case op.Kind == FrameOpRect && op.Selected:
			sawFill = true
			if op.Color != (Color{R: 0xc9, G: 0xa8, B: 0xff, A: 0xff}) || op.BorderColor != (Color{R: 0xc9, G: 0xa8, B: 0xff, A: 0xff}) {
				t.Fatalf("sliderAt active style op = %+v", op)
			}
		case op.Kind == FrameOpText && op.Text == "Amount":
			sawLabel = true
			if op.Color != (Color{R: 0x3b, G: 0x2f, B: 0x55, A: 0xff}) ||
				op.FontSize != 17 || op.Opacity != 0.58 {
				t.Fatalf("sliderAt label style op = %+v", op)
			}
		case op.Kind == FrameOpText && op.Text == "50%":
			sawValue = true
			if op.Color != (Color{R: 0x3b, G: 0x2f, B: 0x55, A: 0xff}) ||
				op.FontSize != 17 || op.Opacity != 0.58 {
				t.Fatalf("sliderAt value style op = %+v", op)
			}
		case op.Kind == FrameOpCircle && op.Color == (Color{R: 0xf8, G: 0xf5, B: 0xff, A: 0xff}):
			sawThumb = true
		}
	}
	if !sawTrack || !sawFill || !sawLabel || !sawValue || !sawThumb {
		t.Fatalf("missing styled sliderAt ops: track=%v fill=%v label=%v value=%v thumb=%v ops=%+v", sawTrack, sawFill, sawLabel, sawValue, sawThumb, rt.FrameOps())
	}
}

func TestTogglePaintUsesStyleSheet(t *testing.T) {
	ClearStylePacks()
	t.Cleanup(ClearStylePacks)
	if !RegisterStylePackSource(`
@pack test.toggle;
tokens {
  color {
    track: #101923;
    active: #54d6a7;
    ink: #182017;
    active-ink: #02130d;
    label: #d2ddd0;
    rule: #405266;
    focus-ring: #ff9f1c;
  }
  length { radius: 7; border: 2; }
  material { flat: Flat; }
}
Toggle[role=Track] { background: track; foreground: ink; border: rule; focus: focus-ring; radius: radius; border-width: border; material: flat; }
Toggle.primary[role=Track] { background: #223344; }
Toggle[role=Fill] { background: active; foreground: active-ink; border: active; focus: focus-ring; radius: radius; border-width: border; material: flat; }
Toggle[role=Label] { foreground: label; font-size: 18; opacity: 0.73; }
`, "Test Toggle", "") || !SetActiveStylePack("test.toggle") {
		t.Fatal("test toggle style did not activate")
	}
	rt := New(AppConfig{Width: 240, Height: 120}).(*runtime)
	value := int32(0)
	rt.SetFocus(89)

	rt.Toggle(ToggleProps{
		Bounds:    Rectangle{X: 8, Y: 28, Width: 120, Height: 34},
		ID:        89,
		ClassName: StyleClassID("primary"),
		Value:     &value,
		OffLabel:  "Off",
		OnLabel:   "On",
	})

	var sawTrack, sawActive, sawOff, sawOn bool
	for _, op := range rt.FrameOps() {
		if op.ID != 89 {
			continue
		}
		switch {
		case op.Kind == FrameOpRect && op.Focused:
			sawTrack = true
			if op.Color != (Color{R: 0x22, G: 0x33, B: 0x44, A: 0xff}) || op.BorderColor != (Color{R: 0xff, G: 0x9f, B: 0x1c, A: 0xff}) || op.FocusColor != (Color{R: 0xff, G: 0x9f, B: 0x1c, A: 0xff}) || op.BorderWidth != 2 || op.Radius != 7 {
				t.Fatalf("toggle track style op = %+v", op)
			}
		case op.Kind == FrameOpRect && op.Selected:
			sawActive = true
			if op.Color != (Color{R: 0x54, G: 0xd6, B: 0xa7, A: 0xff}) || op.BorderColor != (Color{R: 0x54, G: 0xd6, B: 0xa7, A: 0xff}) {
				t.Fatalf("toggle active style op = %+v", op)
			}
		case op.Kind == FrameOpText && op.Text == "Off":
			sawOff = true
			if op.Color != (Color{R: 0x02, G: 0x13, B: 0x0d, A: 0xff}) ||
				op.FontSize != 18 || op.Opacity != 0.73 {
				t.Fatalf("toggle off label style op = %+v", op)
			}
		case op.Kind == FrameOpText && op.Text == "On":
			sawOn = true
			if op.Color != (Color{R: 0xd2, G: 0xdd, B: 0xd0, A: 0xff}) ||
				op.FontSize != 18 || op.Opacity != 0.73 {
				t.Fatalf("toggle on label style op = %+v", op)
			}
		}
	}
	if !sawTrack || !sawActive || !sawOff || !sawOn {
		t.Fatalf("missing styled toggle ops: track=%v active=%v off=%v on=%v ops=%+v", sawTrack, sawActive, sawOff, sawOn, rt.FrameOps())
	}
}

func TestFieldsetPaintUsesStyleSheet(t *testing.T) {
	ClearStylePacks()
	t.Cleanup(ClearStylePacks)
	if !RegisterStylePackSource(`
@pack test.fieldset;
tokens {
  color {
    canvas: #101820;
    text: #d8e4f5;
    rule: #506172;
  }
  length { radius: 9; border: 3; }
  material { flat: Flat; }
}
Fieldset { background: canvas; foreground: text; border: rule; radius: radius; border-width: border; font-size: 18; material: flat; opacity: 0.75; }
`, "Test Fieldset", "") || !SetActiveStylePack("test.fieldset") {
		t.Fatal("test fieldset style did not activate")
	}
	rt := New(AppConfig{Width: 240, Height: 120}).(*runtime)

	rt.Fieldset(FieldsetProps{
		Bounds: Rectangle{X: 12, Y: 24, Width: 140, Height: 70},
		Title:  "Group",
	})

	var sawFrame, sawTitle, sawText bool
	for _, op := range rt.FrameOps() {
		switch {
		case op.Kind == FrameOpRect && op.Bounds == (Rectangle{X: 12, Y: 24, Width: 140, Height: 70}):
			sawFrame = true
			if op.Color != (Color{R: 0x10, G: 0x18, B: 0x20, A: 0xff}) ||
				op.BorderColor != (Color{R: 0x50, G: 0x61, B: 0x72, A: 0xff}) ||
				op.BorderWidth != 3 || op.Radius != 9 ||
				op.Material != MaterialFlat || op.Opacity != 0.75 {
				t.Fatalf("fieldset frame style op = %+v", op)
			}
		case op.Kind == FrameOpRect && op.Bounds.Y == 16:
			sawTitle = true
			if op.Color != (Color{R: 0x10, G: 0x18, B: 0x20, A: 0xff}) {
				t.Fatalf("fieldset title background op = %+v", op)
			}
		case op.Kind == FrameOpText && op.Text == "Group":
			sawText = true
			if op.Color != (Color{R: 0xd8, G: 0xe4, B: 0xf5, A: 0xff}) ||
				op.FontSize != 18 || op.Opacity != 0.75 {
				t.Fatalf("fieldset title text op = %+v", op)
			}
		}
	}
	if !sawFrame || !sawTitle || !sawText {
		t.Fatalf("missing fieldset ops: frame=%v title=%v text=%v ops=%+v", sawFrame, sawTitle, sawText, rt.FrameOps())
	}
}

func TestPanedViewHandleUsesStyleSheet(t *testing.T) {
	ClearStylePacks()
	t.Cleanup(ClearStylePacks)
	if !RegisterStylePackSource(`
@pack test.paned;
tokens {
  color {
    handle: #314150;
    edge: #6a7d90;
  }
  length { radius: 4; border: 2; }
  material { flat: Flat; }
}
PanedView[role=Handle] { background: handle; border: edge; radius: radius; border-width: border; material: flat; opacity: 0.8; }
`, "Test Paned", "") || !SetActiveStylePack("test.paned") {
		t.Fatal("test paned style did not activate")
	}
	rt := New(AppConfig{Width: 180, Height: 140}).(*runtime)
	split := int32(50)

	changed := rt.PanedView(PanedViewProps{
		ID:        77,
		Bounds:    Rectangle{X: 10, Y: 20, Width: 120, Height: 80},
		Split:     &split,
		MinFirst:  20,
		MinSecond: 20,
	})

	if changed != 0 {
		t.Fatalf("paned view unexpectedly changed split: %d", changed)
	}
	ops := rt.FrameOps()
	if len(ops) != 1 || ops[0].Kind != FrameOpRect {
		t.Fatalf("expected one paned handle op, got %+v", ops)
	}
	op := ops[0]
	if op.Bounds != (Rectangle{X: 10, Y: 66, Width: 120, Height: 8}) ||
		op.Color != (Color{R: 0x31, G: 0x41, B: 0x50, A: 0xff}) ||
		op.BorderColor != (Color{R: 0x6a, G: 0x7d, B: 0x90, A: 0xff}) ||
		op.BorderWidth != 2 || op.Radius != 4 ||
		op.Material != MaterialFlat || op.Opacity != 0.8 ||
		op.ID != 77 {
		t.Fatalf("paned handle style op = %+v", op)
	}
}

func TestToastUsesStyleSheet(t *testing.T) {
	ClearStylePacks()
	t.Cleanup(ClearStylePacks)
	if !RegisterStylePackSource(`
@pack test.toast;
tokens {
  color {
    panel: #233040;
    text: #edf3ff;
    rule: #7890aa;
  }
  length { radius: 7; border: 2; }
  material { flat: Flat; }
}
Toast { background: panel; foreground: text; border: rule; radius: radius; border-width: border; material: flat; opacity: 0.85; }
Toast[role=Label] { foreground: text; font-size: 18; opacity: 0.66; }
`, "Test Toast", "") || !SetActiveStylePack("test.toast") {
		t.Fatal("test toast style did not activate")
	}
	rt := New(AppConfig{Width: 220, Height: 120}).(*runtime)

	rt.BeginFrame()
	rt.Toast(ToastProps{Message: "Saved", Seconds: 1})
	rt.EndFrame()

	var sawSurface, sawLabel bool
	for _, op := range rt.FrameOps() {
		switch {
		case op.Kind == FrameOpRect:
			sawSurface = true
			if op.Color != (Color{R: 0x23, G: 0x30, B: 0x40, A: 0xff}) ||
				op.BorderColor != (Color{R: 0x78, G: 0x90, B: 0xaa, A: 0xff}) ||
				op.BorderWidth != 2 || op.Radius != 7 ||
				op.Material != MaterialFlat || op.Opacity != 0.85 {
				t.Fatalf("toast surface style op = %+v", op)
			}
		case op.Kind == FrameOpText && op.Text == "Saved":
			sawLabel = true
			if op.Color != (Color{R: 0xed, G: 0xf3, B: 0xff, A: 0xff}) ||
				op.FontSize != 18 || op.Opacity != 0.66 {
				t.Fatalf("toast label style op = %+v", op)
			}
		}
	}
	if !sawSurface || !sawLabel {
		t.Fatalf("missing toast ops: surface=%v label=%v ops=%+v", sawSurface, sawLabel, rt.FrameOps())
	}
}

func TestChromeWidgetsResolveClassSelectors(t *testing.T) {
	ClearStylePacks()
	t.Cleanup(ClearStylePacks)
	if !RegisterStylePackSource(`
@pack test.chrome.classes;
tokens {
  color {
    panel: #123044;
    ink: #e9f7ff;
    action: #2f6bff;
    toast: #3a2446;
    toast-ink: #ffdfff;
    title: #203820;
    rule: #8fa4b8;
  }
  length { radius: 6; border: 2; }
  material { flat: Flat; }
}
Modal.chrome[role=Scrim] { background: #080a0d; opacity: 0.4; material: flat; }
Modal.chrome[role=Panel] { background: panel; foreground: ink; border: rule; radius: radius; border-width: border; material: flat; }
Modal.chrome[role=Title] { foreground: ink; font-size: 17; opacity: 0.93; }
Modal.chrome[role=Message] { foreground: ink; font-size: 15; opacity: 0.72; }
Modal.chrome[role=Action] { background: action; foreground: ink; border: action; radius: radius; border-width: border; font-size: 16; material: flat; }
TitleBar.chrome[role=Bar] { background: title; foreground: ink; border: rule; radius: radius; border-width: border; material: flat; }
TitleBar.chrome[role=Title] { foreground: ink; font-size: 21; opacity: 0.81; }
TitleBar.chrome[role=Action] { background: action; foreground: ink; border: action; radius: radius; border-width: border; material: flat; }
Toast.chrome { background: toast; foreground: toast-ink; border: rule; radius: radius; border-width: border; material: flat; opacity: 0.88; }
Toast.chrome[role=Label] { foreground: toast-ink; font-size: 18; opacity: 0.67; }
`, "Test Chrome Classes", "") || !SetActiveStylePack("test.chrome.classes") {
		t.Fatal("chrome class style did not activate")
	}
	rt := New(AppConfig{Width: 360, Height: 220}).(*runtime)
	className := StyleClassID("chrome")

	rt.BeginFrame()
	rt.Modal(ModalProps{
		Title:       "Chrome",
		ClassName:   className,
		Message:     "Styled",
		Actions:     []ModalAction{{Label: "OK"}},
		ActionCount: 1,
	})
	rt.TitleBar(TitleBarProps{
		Title:            "Screen",
		ClassName:        className,
		Height:           44,
		HasLeadingAction: true,
	})
	rt.Toast(ToastProps{Message: "Saved", ClassName: className, Seconds: 1})
	rt.EndFrame()

	var sawModalPanel, sawModalTitle, sawModalAction bool
	var sawTitleBar, sawTitle, sawTitleAction bool
	var sawToast, sawToastLabel bool
	for _, op := range rt.FrameOps() {
		switch {
		case op.Kind == FrameOpRect && op.Color == (Color{R: 0x12, G: 0x30, B: 0x44, A: 0xff}):
			sawModalPanel = true
			if op.BorderColor != (Color{R: 0x8f, G: 0xa4, B: 0xb8, A: 0xff}) || op.BorderWidth != 2 || op.Radius != 6 {
				t.Fatalf("modal class panel op = %+v", op)
			}
		case op.Kind == FrameOpText && op.Text == "Chrome":
			sawModalTitle = true
			if op.FontSize != 17 || op.Opacity != 0.93 || op.Color != (Color{R: 0xe9, G: 0xf7, B: 0xff, A: 0xff}) {
				t.Fatalf("modal class title op = %+v", op)
			}
		case op.Kind == FrameOpButton && op.Text == "OK":
			sawModalAction = true
			style := unpackStyle(op.Button.Appearance.Value)
			if style.Background != (Color{R: 0x2f, G: 0x6b, B: 0xff, A: 0xff}) || style.FontSize != 16 {
				t.Fatalf("modal class action op = %+v", op)
			}
		case op.Kind == FrameOpRect && op.Bounds == (Rectangle{X: 0, Y: 0, Width: 360, Height: 44}):
			sawTitleBar = true
			if op.Color != (Color{R: 0x20, G: 0x38, B: 0x20, A: 0xff}) || op.BorderWidth != 2 || op.Radius != 6 {
				t.Fatalf("title bar class surface op = %+v", op)
			}
		case op.Kind == FrameOpText && op.Text == "Screen":
			sawTitle = true
			if op.FontSize != 21 || op.Opacity != 0.81 {
				t.Fatalf("title bar class title op = %+v", op)
			}
		case op.Kind == FrameOpButton && op.Bounds == (Rectangle{X: 12, Y: 2, Width: 40, Height: 40}):
			sawTitleAction = true
			style := unpackStyle(op.Button.Appearance.Value)
			if style.Background != (Color{R: 0x2f, G: 0x6b, B: 0xff, A: 0xff}) {
				t.Fatalf("title bar class action op = %+v", op)
			}
		case op.Kind == FrameOpRect && op.Color == (Color{R: 0x3a, G: 0x24, B: 0x46, A: 0xff}):
			sawToast = true
			if op.Opacity != 0.88 || op.BorderWidth != 2 || op.Radius != 6 {
				t.Fatalf("toast class surface op = %+v", op)
			}
		case op.Kind == FrameOpText && op.Text == "Saved":
			sawToastLabel = true
			if op.FontSize != 18 || op.Opacity != 0.67 || op.Color != (Color{R: 0xff, G: 0xdf, B: 0xff, A: 0xff}) {
				t.Fatalf("toast class label op = %+v", op)
			}
		}
	}
	if !sawModalPanel || !sawModalTitle || !sawModalAction ||
		!sawTitleBar || !sawTitle || !sawTitleAction ||
		!sawToast || !sawToastLabel {
		t.Fatalf("missing chrome class ops: modal(panel=%v title=%v action=%v) titlebar(bar=%v title=%v action=%v) toast(surface=%v label=%v) ops=%+v",
			sawModalPanel, sawModalTitle, sawModalAction,
			sawTitleBar, sawTitle, sawTitleAction,
			sawToast, sawToastLabel, rt.FrameOps())
	}
}

func TestContainerWidgetsResolveClassSelectors(t *testing.T) {
	ClearStylePacks()
	t.Cleanup(ClearStylePacks)
	if !RegisterStylePackSource(`
@pack test.containers.classes;
tokens {
  color {
    panel: #172536;
    header: #284360;
    handle: #5d7890;
    popup: #2f253d;
    ink: #edf6ff;
    rule: #91a7ba;
  }
  length { radius: 5; border: 2; }
  material { flat: Flat; }
}
TableView.shell[role=Panel] { background: panel; border: rule; radius: radius; border-width: border; material: flat; }
TableView.shell[role=Header] { background: header; foreground: ink; border: rule; radius: radius; border-width: border; font-size: 17; material: flat; }
TableView.shell[role=Cell] { foreground: ink; font-size: 15; opacity: 0.7; }
Collapsible.shell[role=Header] { background: header; foreground: ink; border: rule; radius: radius; border-width: border; font-size: 18; material: flat; }
Collapsible.shell[role=Close] { foreground: ink; font-size: 16; opacity: 0.6; }
PanedView.shell[role=Handle] { background: handle; border: rule; radius: radius; border-width: border; material: flat; opacity: 0.75; }
Popup.shell[role=Panel] { background: popup; border: rule; radius: radius; border-width: border; material: flat; }
`, "Test Container Classes", "") || !SetActiveStylePack("test.containers.classes") {
		t.Fatal("container class style did not activate")
	}
	rt := New(AppConfig{Width: 360, Height: 240}).(*runtime)
	className := StyleClassID("shell")
	split := int32(60)
	openSection := true
	visibleSection := true
	openPopup := true

	rt.BeginFrame()
	rt.PanedView(PanedViewProps{
		Bounds:    Rectangle{X: 8, Y: 8, Width: 120, Height: 80},
		ID:        401,
		ClassName: className,
		Split:     &split,
	})
	rt.Collapsible(CollapsibleProps{
		Bounds:    Rectangle{X: 8, Y: 96, Width: 180, Height: 32},
		ClassName: className,
		Label:     "Section",
		Open:      &openSection,
		ID:        402,
		Visible:   &visibleSection,
	})
	rt.TableView(TableViewProps{
		Bounds:       Rectangle{X: 140, Y: 8, Width: 180, Height: 80},
		ID:           403,
		ClassName:    className,
		Columns:      []string{"Name"},
		Rows:         []TableRow{{Cells: []string{"Cell"}}},
		RowHeight:    26,
		ColumnWidths: []int32{120},
	})
	if !rt.BeginPopup(PopupProps{
		Bounds:    Rectangle{X: 30, Y: 140, Width: 120, Height: 60},
		ID:        404,
		ClassName: className,
		Open:      &openPopup,
	}) {
		t.Fatal("open popup returned false")
	}
	rt.EndPopup()
	rt.EndFrame()

	var sawPaned, sawCollapsible, sawClose, sawTable, sawHeader, sawPopup bool
	for _, op := range rt.FrameOps() {
		switch {
		case op.Kind == FrameOpRect && op.ID == 401:
			sawPaned = true
			if op.Color != (Color{R: 0x5d, G: 0x78, B: 0x90, A: 0xff}) ||
				op.BorderColor != (Color{R: 0x91, G: 0xa7, B: 0xba, A: 0xff}) ||
				op.BorderWidth != 2 || op.Radius != 5 || op.Opacity != 0.75 {
				t.Fatalf("paned class op = %+v", op)
			}
		case op.Kind == FrameOpButton && op.ID == 402:
			sawCollapsible = true
			if op.Color != (Color{R: 0x28, G: 0x43, B: 0x60, A: 0xff}) ||
				op.TextColor != (Color{R: 0xed, G: 0xf6, B: 0xff, A: 0xff}) ||
				op.FontSize != 18 {
				t.Fatalf("collapsible class op = %+v", op)
			}
		case op.Kind == FrameOpText && op.Text == "×":
			sawClose = true
			if op.Color != (Color{R: 0xed, G: 0xf6, B: 0xff, A: 0xff}) ||
				op.FontSize != 16 || op.Opacity != 0.6 {
				t.Fatalf("collapsible close class op = %+v", op)
			}
		case op.Kind == FrameOpRect && op.Bounds == (Rectangle{X: 140, Y: 8, Width: 180, Height: 80}):
			sawTable = true
			if op.Color != (Color{R: 0x17, G: 0x25, B: 0x36, A: 0xff}) ||
				op.BorderWidth != 2 || op.Radius != 5 {
				t.Fatalf("table class panel op = %+v", op)
			}
		case op.Kind == FrameOpText && op.Row == -1 && op.Column == 0:
			sawHeader = true
			if op.FontSize != 17 || op.Color != (Color{R: 0xed, G: 0xf6, B: 0xff, A: 0xff}) {
				t.Fatalf("table class header op = %+v", op)
			}
		case op.Kind == FrameOpRect && op.ID == 404:
			sawPopup = true
			if op.Color != (Color{R: 0x2f, G: 0x25, B: 0x3d, A: 0xff}) ||
				op.BorderWidth != 2 || op.Radius != 5 {
				t.Fatalf("popup class op = %+v", op)
			}
		}
	}
	if !sawPaned || !sawCollapsible || !sawClose || !sawTable || !sawHeader || !sawPopup {
		t.Fatalf("missing container class ops: paned=%v collapsible=%v close=%v table=%v header=%v popup=%v ops=%+v",
			sawPaned, sawCollapsible, sawClose, sawTable, sawHeader, sawPopup, rt.FrameOps())
	}
}

func TestDataWidgetsResolveClassSelectors(t *testing.T) {
	ClearStylePacks()
	t.Cleanup(ClearStylePacks)
	if !RegisterStylePackSource(`
@pack test.data.classes;
tokens {
  color {
    panel: #182434;
    mark: #69c6ff;
    ink: #edf7ff;
    field: #263c52;
    drag: #59417a;
    drop: #2f624f;
    rule: #8da4b8;
  }
  length { radius: 5; border: 2; }
  material { flat: Flat; }
}
Fieldset.data { background: field; foreground: ink; border: rule; radius: radius; border-width: border; font-size: 15; material: flat; }
Plot.data { background: panel; foreground: ink; border: rule; border-width: border; font-size: 14; material: flat; }
PlotMark.data:selected { background: mark; border: mark; material: flat; }
Drag.data { foreground: ink; font-size: 16; opacity: 0.73; }
DragValue.data { background: drag; foreground: ink; border: rule; radius: radius; border-width: border; font-size: 17; material: flat; }
DragDropTarget.data { background: drop; border: rule; radius: radius; border-width: border; material: flat; }
`, "Test Data Classes", "") || !SetActiveStylePack("test.data.classes") {
		t.Fatal("data class style did not activate")
	}
	rt := New(AppConfig{Width: 360, Height: 240}).(*runtime)
	className := StyleClassID("data")
	values := []float32{0.2, 0.7}
	dropOutput := make([]byte, 4)
	dropOutputSize := int32(len(dropOutput))
	rt.dragDrop = dragDropState{active: true, typeName: "text/plain", data: []byte("x")}

	rt.BeginFrame()
	rt.Fieldset(FieldsetProps{
		Bounds:    Rectangle{X: 8, Y: 8, Width: 120, Height: 50},
		ClassName: className,
		Title:     "Group",
	})
	rt.Plot(PlotProps{
		Bounds:     Rectangle{X: 140, Y: 8, Width: 120, Height: 60},
		ClassName:  className,
		Label:      "Trend",
		Values:     values,
		ValueCount: int32(len(values)),
	})
	rt.Drag(DragProps{
		Bounds:      Rectangle{X: 8, Y: 96, Width: 120, Height: 30},
		ID:          502,
		ClassName:   className,
		Label:       "Gain",
		FloatValues: []float32{0.5},
		ValueCount:  1,
		Min:         0,
		Max:         1,
	})
	rt.DragDrop(DragDropProps{
		Bounds:       Rectangle{X: 160, Y: 96, Width: 80, Height: 30},
		ID:           503,
		ClassName:    className,
		Role:         DragDropRoleTarget,
		Type:         "text/plain",
		Output:       dropOutput,
		OutputSize:   int32(len(dropOutput)),
		AcceptedSize: &dropOutputSize,
	})
	rt.EndFrame()

	var sawFieldset, sawFieldsetTitle, sawPlot, sawPlotMark, sawDragCell, sawDragLabel, sawDrop bool
	for _, op := range rt.FrameOps() {
		switch {
		case op.Kind == FrameOpRect && op.Bounds == (Rectangle{X: 8, Y: 8, Width: 120, Height: 50}):
			sawFieldset = true
			if op.Color != (Color{R: 0x26, G: 0x3c, B: 0x52, A: 0xff}) ||
				op.BorderColor != (Color{R: 0x8d, G: 0xa4, B: 0xb8, A: 0xff}) ||
				op.BorderWidth != 2 || op.Radius != 5 {
				t.Fatalf("fieldset class op = %+v", op)
			}
		case op.Kind == FrameOpText && op.Text == "Group":
			sawFieldsetTitle = true
			if op.Color != (Color{R: 0xed, G: 0xf7, B: 0xff, A: 0xff}) || op.FontSize != 15 {
				t.Fatalf("fieldset title class op = %+v", op)
			}
		case op.Kind == FrameOpRect && op.Bounds == (Rectangle{X: 140, Y: 8, Width: 120, Height: 60}):
			sawPlot = true
			if op.Color != (Color{R: 0x18, G: 0x24, B: 0x34, A: 0xff}) ||
				op.BorderColor != (Color{R: 0x8d, G: 0xa4, B: 0xb8, A: 0xff}) ||
				op.BorderWidth != 2 {
				t.Fatalf("plot class op = %+v", op)
			}
		case op.Kind == FrameOpLine && op.Color == (Color{R: 0x69, G: 0xc6, B: 0xff, A: 0xff}):
			sawPlotMark = true
		case op.Kind == FrameOpButton && op.ID == 502 && op.Row == 0:
			sawDragCell = true
			if op.Color != (Color{R: 0x59, G: 0x41, B: 0x7a, A: 0xff}) ||
				op.TextColor != (Color{R: 0xed, G: 0xf7, B: 0xff, A: 0xff}) ||
				op.BorderColor != (Color{R: 0x8d, G: 0xa4, B: 0xb8, A: 0xff}) ||
				op.FontSize != 17 || op.BorderWidth != 2 || op.Radius != 5 {
				t.Fatalf("drag value class op = %+v", op)
			}
		case op.Kind == FrameOpText && op.Text == "Gain":
			sawDragLabel = true
			if op.Color != (Color{R: 0xed, G: 0xf7, B: 0xff, A: 0xff}) ||
				op.FontSize != 16 || op.Opacity != 0.73 {
				t.Fatalf("drag label class op = %+v", op)
			}
		case op.Kind == FrameOpRect && op.ID == 503:
			sawDrop = true
			if op.Color != (Color{R: 0x2f, G: 0x62, B: 0x4f, A: 0xff}) ||
				op.BorderColor != (Color{R: 0x8d, G: 0xa4, B: 0xb8, A: 0xff}) ||
				op.BorderWidth != 2 || op.Radius != 5 {
				t.Fatalf("drag-drop class op = %+v", op)
			}
		}
	}
	if !sawFieldset || !sawFieldsetTitle || !sawPlot || !sawPlotMark ||
		!sawDragCell || !sawDragLabel || !sawDrop {
		t.Fatalf("missing data class ops: fieldset=%v title=%v plot=%v mark=%v dragCell=%v dragLabel=%v drop=%v ops=%+v",
			sawFieldset, sawFieldsetTitle, sawPlot, sawPlotMark, sawDragCell, sawDragLabel, sawDrop, rt.FrameOps())
	}
}

func TestPlotTextUsesStyleSheet(t *testing.T) {
	ClearStylePacks()
	t.Cleanup(ClearStylePacks)
	if !RegisterStylePackSource(`
@pack test.plot;
tokens {
  color {
    panel: #101820;
    ink: #d8e4f5;
    rule: #506172;
    mark: #c9a8ff;
  }
  length { radius: 5; border: 2; }
  material { flat: Flat; }
}
Plot { background: panel; foreground: ink; border: rule; radius: radius; border-width: border; font-size: 18; material: flat; opacity: 0.62; }
PlotMark { background: mark; foreground: ink; border: mark; material: flat; }
`, "Test Plot", "") || !SetActiveStylePack("test.plot") {
		t.Fatal("test plot style did not activate")
	}
	rt := New(AppConfig{Width: 240, Height: 120}).(*runtime)
	values := []float32{0.2, 0.7}

	rt.Plot(PlotProps{
		Bounds:     Rectangle{X: 8, Y: 8, Width: 120, Height: 60},
		Values:     values,
		ValueCount: int32(len(values)),
		Label:      "Load",
		Overlay:    "70%",
	})

	var sawLabel, sawOverlay bool
	for _, op := range rt.FrameOps() {
		if op.Kind == FrameOpText && (op.Text == "Load" || op.Text == "70%") {
			if op.FontSize != 18 || op.Opacity != 0.62 ||
				op.Color != (Color{R: 0xd8, G: 0xe4, B: 0xf5, A: 0xff}) {
				t.Fatalf("plot text style op = %+v", op)
			}
			if op.Text == "Load" {
				sawLabel = true
			}
			if op.Text == "70%" {
				sawOverlay = true
			}
		}
	}
	if !sawLabel || !sawOverlay {
		t.Fatalf("missing styled plot text: label=%v overlay=%v ops=%+v", sawLabel, sawOverlay, rt.FrameOps())
	}
}

func TestDragScalarPaintUsesStyleSheet(t *testing.T) {
	ClearStylePacks()
	t.Cleanup(ClearStylePacks)
	if !RegisterStylePackSource(`
@pack test.drag_scalar;
tokens {
  color {
    surface: #101820;
    field: #182231;
    field-focus: #22364f;
    field-disabled: #0b1018;
    ink: #e8f1ff;
    label: #d8e4f5;
    disabled-ink: #7c8797;
    rule: #506172;
    disabled-rule: #2b3340;
    focus-ring: #ff9f1c;
  }
  length { radius: 5; border: 2; }
  material { flat: Flat; }
}
Surface { background: surface; material: flat; }
App { background: surface; }
Text { foreground: label; font-size: 14; }
Drag { foreground: label; font-size: 15; opacity: 0.69; }
DragValue { background: field; foreground: ink; border: rule; focus: focus-ring; radius: radius; border-width: border; font-size: 17; material: flat; }
DragValue:focus { background: field-focus; foreground: ink; border: focus-ring; focus: focus-ring; material: flat; }
DragValue:disabled { background: field-disabled; foreground: disabled-ink; border: disabled-rule; opacity: 0.55; }
`, "Test Drag Scalar", "") || !SetActiveStylePack("test.drag_scalar") {
		t.Fatal("test drag scalar style did not activate")
	}
	rt := New(AppConfig{Width: 240, Height: 120}).(*runtime)
	values := []float32{2.5}
	rt.SetFocus(sliderFocusID(303, 0, false))

	rt.BeginFrame()
	rt.dragFloat(dragFloatProps{
		Bounds:     Rectangle{X: 10, Y: 30, Width: 120, Height: 30},
		ID:         303,
		Label:      "Amount",
		Values:     values,
		ValueCount: 1,
		Speed:      1,
		Min:        0,
		Max:        10,
		Format:     "%.1f",
	})
	rt.EndFrame()

	var sawCell, sawLabel bool
	for _, op := range rt.FrameOps() {
		switch {
		case op.Kind == FrameOpButton && op.ID == 303 && op.Row == 0:
			sawCell = true
			if !op.Focused || op.Color != (Color{R: 0x22, G: 0x36, B: 0x4f, A: 0xff}) ||
				op.BorderColor != (Color{R: 0xff, G: 0x9f, B: 0x1c, A: 0xff}) ||
				op.FocusColor != (Color{R: 0xff, G: 0x9f, B: 0x1c, A: 0xff}) ||
				op.TextColor != (Color{R: 0xe8, G: 0xf1, B: 0xff, A: 0xff}) ||
				op.AmbientColor != (Color{R: 0x10, G: 0x18, B: 0x20, A: 0xff}) ||
				op.BorderWidth != 2 || op.Radius != 5 || op.FontSize != 17 {
				t.Fatalf("drag scalar cell style op = %+v", op)
			}
		case op.Kind == FrameOpText && op.Text == "Amount":
			sawLabel = true
			if op.Color != (Color{R: 0xd8, G: 0xe4, B: 0xf5, A: 0xff}) ||
				op.FontSize != 15 || op.Opacity != 0.69 {
				t.Fatalf("drag scalar label style op = %+v", op)
			}
		}
	}
	if !sawCell || !sawLabel {
		t.Fatalf("missing styled drag scalar ops: cell=%v label=%v ops=%+v", sawCell, sawLabel, rt.FrameOps())
	}

	disabledRT := New(AppConfig{Width: 240, Height: 120}).(*runtime)
	disabledRT.BeginFrame()
	disabledRT.dragFloat(dragFloatProps{
		Bounds:     Rectangle{X: 10, Y: 30, Width: 120, Height: 30},
		ID:         304,
		Values:     values,
		ValueCount: 1,
		Speed:      1,
		Min:        0,
		Max:        10,
		Format:     "%.1f",
		Disabled:   true,
	})
	disabledRT.EndFrame()

	for _, op := range disabledRT.FrameOps() {
		if op.Kind == FrameOpButton && op.ID == 304 && op.Row == 0 {
			if !op.Disabled ||
				op.Color != (Color{R: 0x0b, G: 0x10, B: 0x18, A: 0xff}) ||
				op.BorderColor != (Color{R: 0x2b, G: 0x33, B: 0x40, A: 0xff}) ||
				op.TextColor != (Color{R: 0x7c, G: 0x87, B: 0x97, A: 0xff}) ||
				op.Opacity != 0.55 {
				t.Fatalf("disabled drag scalar cell style op = %+v", op)
			}
			return
		}
	}
	t.Fatalf("missing disabled drag scalar cell op: %+v", disabledRT.FrameOps())
}

func TestIconRenderDrawsTintedPixels(t *testing.T) {
	img := RenderFrame(48, 48, []FrameOp{{
		Kind:     FrameOpIcon,
		Bounds:   Rectangle{X: 8, Y: 8, Width: 24, Height: 24},
		Color:    Color{R: 210, G: 30, B: 40, A: 255},
		IconType: IconWorkbookFillColor,
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
	useMaterialStyleForTest(t)
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
	want := resolveButtonStyleForKind(rt.theme(), rt.effectiveDark(), rt.activeTheme,
		ButtonProps{Disabled: true}, ButtonStateDisabled, StyleSheet_StyleKindButton())
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
	r.Box(NewRectangle(10, 0, 100, 160), RED, BLANK)
	r.BeginScroll(NewRectangle(20, 30, 100, 60), 100, nil)
	r.Box(NewRectangle(0, 0, 160, 160), BLUE, BLANK)
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

func TestScrollWrapperClosesScope(t *testing.T) {
	r := New(AppConfig{}).(*runtime)
	offset := int32(0)
	var content Rectangle
	called := false

	r.QueueMouseMove(30, 30)
	r.QueueMouseWheel(-1)
	r.BeginFrame()
	r.Scroll(ScrollProps{
		Bounds:        NewRectangle(10, 10, 100, 60),
		ContentHeight: 200,
		Offset:        &offset,
	}, func(rect Rectangle) {
		called = true
		content = rect
		r.Box(NewRectangle(10, 0, 100, 160), RED, BLANK)
	})
	r.Box(NewRectangle(10, 80, 100, 28), BLUE, BLANK)
	r.EndFrame()

	if !called {
		t.Fatal("scroll body was not called")
	}
	if offset != 42 || content.Y != -32 || content.Height != 200 {
		t.Fatalf("content=%v offset=%d", content, offset)
	}
	img := RenderFrame(180, 180, r.FrameOps())
	got := color.RGBAModel.Convert(img.At(15, 85)).(color.RGBA)
	if got != (color.RGBA{BLUE.R, BLUE.G, BLUE.B, BLUE.A}) {
		t.Fatalf("post-scroll scope did not restore clipping: %v", got)
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

func TestScrollChromePaintUsesStyleSheet(t *testing.T) {
	ClearStylePacks()
	t.Cleanup(ClearStylePacks)
	if !RegisterStylePackSource(`
@pack test.scroll;
tokens {
  color {
    surface: #121a24;
    thumb: #7ae2ba;
    rule: #506172;
    wrong: #ff00ff;
  }
  length { radius: 4; border: 2; }
  material { flat: Flat; }
}
Surface { background: wrong; border: wrong; radius: 1; border-width: 1; material: flat; }
Slider[tone=Accent] { background: wrong; border: wrong; radius: 1; border-width: 1; material: flat; }
SliderThumb { background: wrong; border: wrong; radius: 1; border-width: 1; material: flat; }
Scroll { background: surface; border: rule; radius: radius; border-width: border; material: flat; }
ScrollThumb { background: thumb; border: thumb; radius: radius; border-width: border; material: flat; }
`, "Test Scroll", "") || !SetActiveStylePack("test.scroll") {
		t.Fatal("test scroll style did not activate")
	}
	r := New(AppConfig{Width: 200, Height: 160}).(*runtime)
	offset := int32(40)

	r.BeginFrame()
	r.BeginScroll(NewRectangle(10, 10, 100, 60), 200, &offset)
	r.EndScroll()
	r.EndFrame()

	var sawTrack, sawThumb bool
	for _, op := range r.FrameOps() {
		switch {
		case op.Kind == FrameOpRect && op.Bounds == (Rectangle{X: 100, Y: 10, Width: 10, Height: 60}):
			sawTrack = true
			if op.Color != (Color{R: 0x12, G: 0x1a, B: 0x24, A: 0xff}) ||
				op.BorderColor != (Color{R: 0x50, G: 0x61, B: 0x72, A: 0xff}) ||
				op.BorderWidth != 2 || op.Radius != 4 {
				t.Fatalf("scroll track style op = %+v", op)
			}
		case op.Kind == FrameOpRect && op.Bounds.X == 102 && op.Bounds.Width == 6:
			sawThumb = true
			if op.Color != (Color{R: 0x7a, G: 0xe2, B: 0xba, A: 0xff}) ||
				op.BorderColor != (Color{R: 0x7a, G: 0xe2, B: 0xba, A: 0xff}) ||
				op.BorderWidth != 2 || op.Radius != 4 {
				t.Fatalf("scroll thumb style op = %+v", op)
			}
		}
	}
	if !sawTrack || !sawThumb {
		t.Fatalf("missing styled scroll chrome: track=%v thumb=%v ops=%+v", sawTrack, sawThumb, r.FrameOps())
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
	useMaterialStyleForTest(t)
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

func TestAppBackgroundUsesActiveStylePack(t *testing.T) {
	ClearStylePacks()
	t.Cleanup(ClearStylePacks)
	if !RegisterStylePackSource(`
@pack test.app_background;
App { background: #123456; }
`, "App Background", "") {
		t.Fatal("style pack did not register")
	}

	rt := New(AppConfig{Width: 320, Height: 240}).(*runtime)
	rt.AppBackground()
	ops := rt.FrameOps()
	if len(ops) != 1 || ops[0].Kind != FrameOpBackground {
		t.Fatalf("missing app background op: %#v", ops)
	}
	if ops[0].Color != (Color{0x12, 0x34, 0x56, 0xff}) {
		t.Fatalf("app background color = %#v", ops[0].Color)
	}
}

func TestPrimitiveAppBackgroundColorFallsBack(t *testing.T) {
	styled := Color{R: 0x12, G: 0x34, B: 0x56, A: 0xff}
	fallback := Color{R: 0xaa, G: 0xbb, B: 0xcc, A: 0xff}
	if got := Primitive_PrimitiveAppBackgroundColor(styled, fallback); got != styled {
		t.Fatalf("styled app background = %#v, want %#v", got, styled)
	}
	if got := Primitive_PrimitiveAppBackgroundColor(Color{}, fallback); got != fallback {
		t.Fatalf("fallback app background = %#v, want %#v", got, fallback)
	}
}

func TestBoxUsesPrimitiveBoundsPolicy(t *testing.T) {
	rt := New(AppConfig{}).(*runtime)
	rt.Box(Rectangle{X: 1.8, Y: 2.2, Width: 3.9, Height: 4.1}, RED, BLUE)
	ops := rt.FrameOps()
	if len(ops) != 1 || ops[0].Kind != FrameOpRect {
		t.Fatalf("box ops = %#v", ops)
	}
	if got, want := ops[0].Bounds, Primitive_PrimitiveRectBounds(1, 2, 3, 4); got != want {
		t.Fatalf("box bounds = %#v, want %#v", got, want)
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
	rt.Link(LinkProps{Text: "Read more", Link: "/more", Bounds: Rectangle{Width: 96, Height: 24}})
	rt.Image(ImageProps{AssetPath: "hero.png", AltText: "Hero", Bounds: Rectangle{Width: 120, Height: 60}, Tint: WHITE})
	rt.End()
	rt.Grid(GridProps{Bounds: Rectangle{X: 10, Y: 140, Width: 200, Height: 80}, Columns: 2, Gap: 4, Padding: 4})
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

	var sawPage, sawHeading, sawLink, sawImage, sawGrid bool
	for _, op := range rt.FrameOps() {
		switch {
		case op.Kind == FrameOpPage && op.Semantic == SemanticPage && op.Bounds.Width == 320:
			sawPage = true
		case op.Kind == FrameOpText && op.Semantic == SemanticHeading && op.Level == 2 && op.Text == "Install":
			sawHeading = true
		case op.Kind == FrameOpText && op.Semantic == SemanticLink && op.Link == "/more":
			sawLink = true
		case op.Kind == FrameOpImage && op.Semantic == SemanticImage && op.AltText == "Hero":
			sawImage = true
		case op.Kind == FrameOpGrid && op.Columns == 2:
			sawGrid = true
		}
	}
	if !sawPage || !sawHeading || !sawLink || !sawImage || !sawGrid {
		t.Fatalf("missing semantic ops: page=%v heading=%v link=%v image=%v grid=%v ops=%#v", sawPage, sawHeading, sawLink, sawImage, sawGrid, rt.FrameOps())
	}
}

func TestPageTextUsesStyleSheetKinds(t *testing.T) {
	ClearStylePacks()
	t.Cleanup(ClearStylePacks)
	if !RegisterStylePackSource(`
@pack test.page_text;
Heading { foreground: #123456; font-size: 30; }
ParagraphText { foreground: #abcdef; font-size: 18; opacity: 0.72; }
Link { foreground: #654321; font-size: 19; opacity: 0.61; }
`, "Page Text", "") || !SetActiveStylePack("test.page_text") {
		t.Fatal("test page text style did not activate")
	}

	rt := New(AppConfig{Width: 320, Height: 240}).(*runtime)
	rt.BeginFrame()
	rt.Heading(HeadingProps{Text: "Styled"})
	rt.ParagraphText(ParagraphTextProps{Text: "Body", Bounds: Rectangle{Width: 200}})
	rt.Link(LinkProps{Text: "More", Link: "/more"})
	rt.EndFrame()

	var sawHeading, sawParagraph, sawLink bool
	for _, op := range rt.FrameOps() {
		switch {
		case op.Kind == FrameOpText && op.Semantic == SemanticHeading:
			sawHeading = true
			if op.Color != (Color{R: 0x12, G: 0x34, B: 0x56, A: 0xff}) || op.FontSize != 30 {
				t.Fatalf("heading style op = %+v", op)
			}
		case op.Kind == FrameOpText && op.Semantic == SemanticParagraph:
			sawParagraph = true
			if op.Color != (Color{R: 0xab, G: 0xcd, B: 0xef, A: 0xff}) ||
				op.FontSize != 18 || op.Opacity != 0.72 {
				t.Fatalf("paragraph style op = %+v", op)
			}
		case op.Kind == FrameOpText && op.Semantic == SemanticLink:
			sawLink = true
			if op.Color != (Color{R: 0x65, G: 0x43, B: 0x21, A: 0xff}) ||
				op.FontSize != 19 || op.Opacity != 0.61 {
				t.Fatalf("link style op = %+v", op)
			}
		}
	}
	if !sawHeading || !sawParagraph || !sawLink {
		t.Fatalf("missing styled page text ops: heading=%v paragraph=%v link=%v ops=%#v",
			sawHeading, sawParagraph, sawLink, rt.FrameOps())
	}
}

func TestTextUsesStyleSheetKind(t *testing.T) {
	ClearStylePacks()
	t.Cleanup(ClearStylePacks)
	if !RegisterStylePackSource(`
@pack test.text_widget;
tokens {
  color { ink: #26384a; }
}
Text { foreground: ink; font-size: 21; opacity: 0.62; }
`, "Test Text Widget", "") || !SetActiveStylePack("test.text_widget") {
		t.Fatal("test text widget style did not activate")
	}
	rt := New(AppConfig{Width: 240, Height: 120}).(*runtime)

	rt.Text(TextProps{
		Bounds: Rectangle{X: 12, Y: 14, Width: 120, Height: 26},
		Text:   "Styled",
	})

	for _, op := range rt.FrameOps() {
		if op.Kind == FrameOpText && op.Text == "Styled" {
			if op.FontSize != 21 || op.Opacity != 0 ||
				op.Color != (Color{R: 0x26, G: 0x38, B: 0x4a, A: 0x9e}) {
				t.Fatalf("text style op = %+v", op)
			}
			return
		}
	}
	t.Fatalf("missing styled text op: %+v", rt.FrameOps())
}

func TestParagraphFallbackColorUsesTextStyleSheet(t *testing.T) {
	ClearStylePacks()
	t.Cleanup(ClearStylePacks)
	if !RegisterStylePackSource(`
@pack test.paragraph_text_fallback;
tokens {
  color { ink: #314253; }
}
Text { foreground: ink; font-size: 18; }
`, "Test Paragraph Text Fallback", "") || !SetActiveStylePack("test.paragraph_text_fallback") {
		t.Fatal("test paragraph fallback style did not activate")
	}
	rt := New(AppConfig{Width: 240, Height: 120}).(*runtime)
	y := int32(10)

	rt.Paragraph(ParagraphSpec{Text: "Body", Width: 120}, 8, &y)

	for _, op := range rt.FrameOps() {
		if op.Kind == FrameOpText && op.Text == "Body" {
			if op.Color != (Color{R: 0x31, G: 0x42, B: 0x53, A: 0xff}) {
				t.Fatalf("paragraph fallback text op = %+v", op)
			}
			return
		}
	}
	t.Fatalf("missing paragraph text op: %+v", rt.FrameOps())
}

func TestSelectableUsesStyleSheetTextAndPadding(t *testing.T) {
	ClearStylePacks()
	t.Cleanup(ClearStylePacks)
	if !RegisterStylePackSource(`
@pack test.selectable;
tokens {
  color {
    selected: #31527a;
    ink: #eef5ff;
  }
}
Selectable {
  background: selected;
  foreground: ink;
  padding-x: 14;
  font-size: 18;
  opacity: 0.72;
}
Selectable.primary {
  background: #224466;
  foreground: #ccffee;
  padding-x: 18;
}
`, "Test Selectable", "") || !SetActiveStylePack("test.selectable") {
		t.Fatal("test selectable style did not activate")
	}
	rt := New(AppConfig{Width: 240, Height: 120}).(*runtime)
	selected := int32(1)
	rt.Selectable(SelectableProps{
		Bounds:    Rectangle{X: 10, Y: 20, Width: 120, Height: 28},
		ID:        72,
		Label:     "Choice",
		Selected:  &selected,
		ClassName: StyleClassID("primary"),
	})

	var sawFill, sawText bool
	for _, op := range rt.FrameOps() {
		switch {
		case op.Kind == FrameOpRect && op.Bounds == (Rectangle{X: 10, Y: 20, Width: 120, Height: 28}):
			sawFill = true
			if op.Color != (Color{R: 0x22, G: 0x44, B: 0x66, A: 0xff}) || op.Opacity != 0.72 {
				t.Fatalf("selectable fill op = %+v", op)
			}
		case op.Kind == FrameOpText && op.Text == "Choice":
			sawText = true
			if op.Bounds.X != 28 || op.Bounds.Width != 84 ||
				op.Color != (Color{R: 0xcc, G: 0xff, B: 0xee, A: 0xff}) ||
				op.FontSize != 18 || op.Opacity != 0.72 {
				t.Fatalf("selectable label op = %+v", op)
			}
		}
	}
	if !sawFill || !sawText {
		t.Fatalf("missing selectable styled ops: fill=%v text=%v ops=%+v",
			sawFill, sawText, rt.FrameOps())
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
	rt.Box(NewRectangle(1, 2, 3, 4), RED, BLANK)
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
		Items:      []TreeItem{{Label: "a", ID: 1, Selectable: 1}, {Label: "b", ID: 2, Selectable: 1}, {Label: "c", ID: 3, Selectable: 1}},
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
	useMaterialStyleForTest(t)
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

func TestTextInputFrameOpsCarryStyleSheetPaint(t *testing.T) {
	ClearStylePacks()
	t.Cleanup(ClearStylePacks)
	if !RegisterStylePackSource(`
@pack test.text_input;
tokens {
  color {
    field: #182231;
    field-focus: #22364f;
    area: #202a36;
    field-ink: #e8f1ff;
    area-ink: #d8e4f5;
    field-rule: #506172;
    area-rule: #60758b;
    focus-ring: #ff9f1c;
  }
  length { field-radius: 5; area-radius: 7; border: 2; }
  material { flat: Flat; }
}
App { background: #101820; }
Surface { background: #101820; material: flat; }
TextField { background: field; foreground: field-ink; border: field-rule; focus: focus-ring; radius: field-radius; border-width: border; font-size: 19; material: flat; }
TextField:focus { background: field-focus; foreground: field-ink; border: focus-ring; focus: focus-ring; material: flat; }
TextArea { background: area; foreground: area-ink; border: area-rule; focus: focus-ring; radius: area-radius; border-width: border; font-size: 21; material: flat; }
`, "Test Text Input", "") || !SetActiveStylePack("test.text_input") {
		t.Fatal("test text input style did not activate")
	}
	rt := New(AppConfig{Width: 240, Height: 160}).(*runtime)
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
	})
	TextArea(TextAreaProps{
		Bounds:         Rectangle{X: 10, Y: 54, Width: 160, Height: 64},
		Text:           text,
		CursorPosition: &cursor,
		Focused:        &areaFocused,
		FocusID:        78,
	})
	EndFrame()

	wantFocused := rt.textInputStyle(FrameOpTextField, true, false)
	wantIdle := rt.textInputStyle(FrameOpTextArea, false, false)
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
		wantFont := int32(19)
		if op.Kind == FrameOpTextArea {
			wantFont = 21
		}
		if got := op.FontSize; got != wantFont {
			t.Fatalf("%s font size = %d, want %d", op.Kind, got, wantFont)
		}
		if got, want := op.SelectionColor, want.Focus; got != want {
			t.Fatalf("selection color = %#v, want %#v", got, want)
		}
		if got, want := op.SelectedTextColor, want.Foreground; got != want {
			t.Fatalf("selected text color = %#v, want %#v", got, want)
		}
		if got, want := op.CursorColor, want.Focus; got != want {
			t.Fatalf("cursor color = %#v, want %#v", got, want)
		}
		if got, want := op.AmbientColor, (Color{R: 0x10, G: 0x18, B: 0x20, A: 0xff}); got != want {
			t.Fatalf("ambient color = %#v, want %#v", got, want)
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

func TestRenderTextAreaCaretUsesLineHeight(t *testing.T) {
	cursor := Color{R: 255, A: 255}
	img := RenderFrame(220, 260, []FrameOp{{
		Kind:          FrameOpTextArea,
		Bounds:        Rectangle{X: 10, Y: 10, Width: 180, Height: 220},
		Text:          "first\nsecond",
		FontSize:      Text14,
		Cursor:        5,
		Focused:       true,
		TextColor:     BLACK,
		CursorColor:   cursor,
		FocusColor:    BLACK,
		Color:         WHITE,
		BorderColor:   BLACK,
		ContentOffset: Vector2{X: 8, Y: 8},
		Gap:           4,
	}})
	pixels := 0
	for y := img.Bounds().Min.Y; y < img.Bounds().Max.Y; y++ {
		for x := img.Bounds().Min.X; x < img.Bounds().Max.X; x++ {
			if got := img.RGBAAt(x, y); got.R == cursor.R && got.G == cursor.G && got.B == cursor.B && got.A == cursor.A {
				pixels++
			}
		}
	}
	if pixels == 0 || pixels > int(textHeight(Text14, 0))*3 {
		t.Fatalf("textarea cursor painted %d pixels, want a line-height caret", pixels)
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

func TestRendererUsesRegisteredTextFontData(t *testing.T) {
	data, err := os.ReadFile("../../fonts/noto/NotoSans-Regular.ttf")
	if err != nil {
		t.Fatalf("read test font: %v", err)
	}
	if !RegisterTextFontData("test-noto", ".ttf", data, nil) {
		t.Fatal("RegisterTextFontData rejected valid TTF")
	}
	UseTextFont("test-noto")

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

func TestTabBarUsesKSSFontByDefault(t *testing.T) {
	ClearStylePacks()
	t.Cleanup(ClearStylePacks)
	if !RegisterStylePackSource(`
@pack test.tab_font;
tokens {
  color {
    surface: #101820;
    tab: #203040;
    text: #ddeeff;
    rule: #506172;
  }
  length { radius: 4; border: 1; }
  material { flat: Flat; }
}
TabBar { background: surface; foreground: text; border: rule; radius: radius; border-width: border; material: flat; }
Tab { background: tab; foreground: text; border: rule; radius: radius; border-width: border; font-size: 21; material: flat; }
`, "Test Tab Font", "") || !SetActiveStylePack("test.tab_font") {
		t.Fatal("test tab style did not activate")
	}
	rt := New(AppConfig{Width: 400, Height: 100}).(*runtime)
	defer SetRuntime(nil)
	SetRuntime(rt)
	tabs := []Tab{{Label: "Alpha"}, {Label: "Beta"}}

	BeginFrame()
	defer EndFrame()
	TabBar(TabBarProps{Bounds: NewRectangle(0, 0, 400, 30), Tabs: tabs,
		Count: int32(len(tabs)), SelectedIndex: 0, ID: 902})

	found := false
	for _, op := range rt.FrameOps() {
		if op.Kind == FrameOpButton && op.Text == "Alpha" {
			found = true
			if op.FontSize != 21 {
				t.Fatalf("tab font = %d, want 21: %+v", op.FontSize, op)
			}
		}
	}
	if !found {
		t.Fatalf("missing tab button op: %+v", rt.FrameOps())
	}
}

func TestSegmentedControlUsesKSSFontByDefault(t *testing.T) {
	ClearStylePacks()
	t.Cleanup(ClearStylePacks)
	if !RegisterStylePackSource(`
@pack test.segment_font;
tokens {
  color {
    surface: #101820;
    segment: #203040;
    text: #ddeeff;
    rule: #506172;
  }
  length { radius: 4; border: 1; }
  material { flat: Flat; }
}
SegmentedControl { background: surface; foreground: text; border: rule; radius: radius; border-width: border; material: flat; }
Segment { background: segment; foreground: text; border: rule; radius: radius; border-width: border; font-size: 22; material: flat; }
`, "Test Segment Font", "") || !SetActiveStylePack("test.segment_font") {
		t.Fatal("test segment style did not activate")
	}
	rt := New(AppConfig{Width: 400, Height: 100}).(*runtime)
	defer SetRuntime(nil)
	SetRuntime(rt)
	options := []SegmentOption{{Label: "Alpha"}, {Label: "Beta"}}
	selected := int32(0)

	BeginFrame()
	defer EndFrame()
	SegmentedControl(SegmentedControlProps{
		Bounds:        NewRectangle(0, 0, 400, 30),
		ID:            912,
		Options:       options,
		OptionCount:   int32(len(options)),
		SelectedIndex: &selected,
	})

	found := false
	for _, op := range rt.FrameOps() {
		if op.Kind == FrameOpButton && op.Text == "Alpha" {
			found = true
			if op.Button.Font != 22 {
				t.Fatalf("segment font = %d, want 22: %+v", op.Button.Font, op)
			}
		}
	}
	if !found {
		t.Fatalf("missing segment button op: %+v", rt.FrameOps())
	}
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
		pressed := rt.Checkbox(CheckboxProps{Bounds: NewRectangle(16, 88, 120, 34), ID: 7, Label: "enable", Value: &v})
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

func TestBeginCanvasUsesStyleSheet(t *testing.T) {
	ClearStylePacks()
	t.Cleanup(ClearStylePacks)
	if !RegisterStylePackSource(`
@pack test.canvas;
tokens {
  color {
    panel: #18222d;
    rule: #667788;
  }
  length { radius: 7; border: 3; }
  material { flat: Flat; }
}
Canvas { background: panel; border: rule; radius: radius; border-width: border; material: flat; }
`, "Test Canvas", "") || !SetActiveStylePack("test.canvas") {
		t.Fatal("test canvas style did not activate")
	}
	rt := New(AppConfig{Width: 320, Height: 200}).(*runtime)

	rt.BeginCanvas(Canvas{Bounds: Rectangle{X: 10, Y: 20, Width: 140, Height: 90}})
	rt.EndCanvas(Canvas{})

	for _, op := range rt.FrameOps() {
		if op.Kind == FrameOpRect && op.Bounds == (Rectangle{X: 10, Y: 20, Width: 140, Height: 90}) {
			if op.Color != (Color{R: 0x18, G: 0x22, B: 0x2d, A: 0xff}) ||
				op.BorderColor != (Color{R: 0x66, G: 0x77, B: 0x88, A: 0xff}) ||
				op.BorderWidth != 3 || op.Radius != 7 {
				t.Fatalf("canvas style op = %+v", op)
			}
			return
		}
	}
	t.Fatalf("missing canvas style op: %+v", rt.FrameOps())
}
