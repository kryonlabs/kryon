package kryon

import "testing"

func TestComposedTooltipOwnsArbitraryPaintWithoutInputCapture(t *testing.T) {
	r := New(AppConfig{Width: 240, Height: 180}).(*runtime)
	r.QueueMouseMove(30, 25)
	r.BeginFrame()
	if !r.BeginPopup(PopupProps{
		Bounds: NewRectangle(80, 50, 130, 70), ID: 29300,
		Trigger: NewRectangle(20, 20, 80, 30), Flags: PopupTooltip,
	}) {
		t.Fatal("hovered tooltip popup returned false")
	}
	r.Column(ColumnProps{Bounds: NewRectangle(88, 58, 114, 54), Gap: 4})
	r.Text(TextProps{Text: "arbitrary tooltip", Font: Text14, Color: BLACK})
	r.Button(ButtonProps{Bounds: NewRectangle(0, 0, 90, 24), Label: "detail", ID: 29301})
	r.End()
	r.EndPopup()
	r.EndFrame()

	panel, text, child := -1, -1, -1
	for i, op := range r.ops {
		if op.ID == 29300 {
			panel = i
		}
		if op.Text == "arbitrary tooltip" {
			text = i
		}
		if op.ID == 29301 {
			child = i
		}
	}
	if panel < 0 || text <= panel || child <= text {
		t.Fatalf("tooltip layer order panel=%d text=%d child=%d", panel, text, child)
	}
	if len(r.popupPanels) != 0 {
		t.Fatalf("tooltip captured popup input: %+v", r.popupPanels)
	}

	r.QueueMouseMove(230, 170)
	r.BeginFrame()
	if r.BeginPopup(PopupProps{
		Bounds: NewRectangle(80, 50, 130, 70), ID: 29300,
		Trigger: NewRectangle(20, 20, 80, 30), Flags: PopupTooltip,
	}) {
		t.Fatal("tooltip remained visible outside its trigger")
	}
	r.EndFrame()
}

func TestComposedModalOwnsArbitraryContentAndFullViewInput(t *testing.T) {
	r := New(AppConfig{Width: 240, Height: 180}).(*runtime)
	open := true
	r.QueueTap(190, 150)
	r.BeginFrame()
	if !r.BeginPopup(PopupProps{
		Bounds: NewRectangle(40, 30, 120, 90), ID: 29400,
		Open: &open, Flags: PopupModal,
	}) {
		t.Fatal("open modal popup returned false")
	}
	r.Column(ColumnProps{Bounds: NewRectangle(48, 38, 104, 70), Gap: 4})
	r.Text(TextProps{Text: "arbitrary modal", Font: Text14, Color: BLACK})
	r.Button(ButtonProps{Bounds: NewRectangle(0, 0, 90, 24), Label: "confirm", ID: 29401})
	r.End()
	r.EndPopup()
	if r.Button(ButtonProps{Bounds: NewRectangle(170, 130, 60, 40), Label: "Behind", ID: 29402}) {
		t.Fatal("modal leaked an outside tap to the background")
	}
	r.EndFrame()
	if !open {
		t.Fatal("outside tap dismissed modal")
	}

	backdrop, panel, text, child := -1, -1, -1, -1
	for i, op := range r.ops {
		if op.Kind == FrameOpRect && op.Bounds == (Rectangle{Width: 240, Height: 180}) {
			backdrop = i
		}
		if op.ID == 29400 {
			panel = i
		}
		if op.Text == "arbitrary modal" {
			text = i
		}
		if op.ID == 29401 {
			child = i
		}
	}
	if backdrop < 0 || panel <= backdrop || text <= panel || child <= text {
		t.Fatalf("modal layer order backdrop=%d panel=%d text=%d child=%d", backdrop, panel, text, child)
	}
	if r.ops[backdrop].HasClip {
		t.Fatal("modal backdrop was clipped to the popup panel")
	}

	r.QueueKey(KeyEscape)
	r.BeginFrame()
	if r.BeginPopup(PopupProps{
		Bounds: NewRectangle(40, 30, 120, 90), ID: 29400,
		Open: &open, Flags: PopupModal,
	}) {
		t.Fatal("Escape left modal popup open")
	}
	r.EndFrame()
	if open {
		t.Fatal("Escape did not update modal caller state")
	}
}

func TestComposedContextPopupOpensOnRightRelease(t *testing.T) {
	r := New(AppConfig{Width: 240, Height: 180}).(*runtime)
	open := false
	r.QueueMouseButton(MouseButtonRight, 30, 25)
	r.BeginFrame()
	if !r.BeginPopup(PopupProps{
		Bounds: NewRectangle(80, 50, 130, 70), ID: 29500, Open: &open,
		Trigger: NewRectangle(20, 20, 80, 30), Flags: PopupContext,
	}) {
		t.Fatal("right release in trigger did not open context popup")
	}
	r.Text(TextProps{Bounds: NewRectangle(88, 58, 0, 0), Text: "context child", Font: Text14, Color: BLACK, Wrap: TextWrapNone})
	r.EndPopup()
	r.EndFrame()
	if !open {
		t.Fatal("context popup did not update caller-owned open state")
	}
	found := false
	for _, op := range r.FrameOps() {
		if op.Text == "context child" {
			found = true
		}
	}
	if !found {
		t.Fatal("context popup did not retain an ordinary child")
	}

	r.QueueTap(220, 160)
	r.BeginFrame()
	if r.BeginPopup(PopupProps{
		Bounds: NewRectangle(80, 50, 130, 70), ID: 29500, Open: &open,
		Trigger: NewRectangle(20, 20, 80, 30), Flags: PopupContext,
	}) {
		t.Fatal("outside tap did not dismiss context popup")
	}
	r.EndFrame()
	if open {
		t.Fatal("context-popup dismissal did not update open state")
	}

	r.QueueMouseButton(MouseButtonRight, 30, 25)
	r.BeginFrame()
	if r.BeginPopup(PopupProps{
		Bounds: NewRectangle(80, 50, 130, 70), ID: 29500, Open: &open,
		Trigger: NewRectangle(20, 20, 80, 30), Flags: PopupContext, Disabled: true,
	}) {
		t.Fatal("disabled context popup opened")
	}
	r.EndFrame()
}

func TestComposedPopupOwnsOrdinaryChildrenAndPaintOrder(t *testing.T) {
	r := New(AppConfig{Width: 240, Height: 180}).(*runtime)
	open := true
	r.BeginFrame()
	if !r.BeginPopup(PopupProps{Bounds: NewRectangle(20, 30, 140, 100), ID: 29000, Open: &open}) {
		t.Fatal("open popup returned false")
	}
	r.Column(ColumnProps{Bounds: NewRectangle(28, 40, 120, 70), Gap: 4})
	r.Button(ButtonProps{Bounds: NewRectangle(0, 0, 100, 28), Label: "Apply", ID: 29001})
	r.End()
	r.EndPopup()
	r.Button(ButtonProps{Bounds: NewRectangle(20, 30, 140, 100), Label: "Later", ID: 29002})
	r.EndFrame()

	background, popupPanel, child := -1, -1, -1
	for i, op := range r.ops {
		switch op.ID {
		case 29000:
			popupPanel = i
		case 29001:
			child = i
		case 29002:
			background = i
		}
	}
	if background < 0 || popupPanel <= background || child <= popupPanel {
		t.Fatalf("popup layer order background=%d panel=%d child=%d", background, popupPanel, child)
	}
	if got := r.ops[child].Bounds; got.X != 28 || got.Y != 40 {
		t.Fatalf("popup child layout = %+v", got)
	}
}

func TestComposedPopupDismissalConsumesOutsideTap(t *testing.T) {
	r := New(AppConfig{Width: 240, Height: 180}).(*runtime)
	open := true
	r.BeginFrame()
	r.BeginPopup(PopupProps{Bounds: NewRectangle(20, 20, 100, 80), ID: 29100, Open: &open})
	r.EndPopup()
	r.EndFrame()

	r.QueueTap(160, 40)
	r.BeginFrame()
	if r.BeginPopup(PopupProps{Bounds: NewRectangle(20, 20, 100, 80), ID: 29100, Open: &open}) {
		t.Fatal("outside tap left popup open")
	}
	if r.Button(ButtonProps{Bounds: NewRectangle(140, 20, 80, 40), Label: "Behind", ID: 29101}) {
		t.Fatal("outside dismissal leaked into background button")
	}
	r.EndFrame()
	if open {
		t.Fatal("outside dismissal did not update caller state")
	}
}

func TestComposedPopupCloseAndMissingOwner(t *testing.T) {
	r := New(AppConfig{Width: 240, Height: 180}).(*runtime)
	open := true
	r.BeginFrame()
	r.BeginPopup(PopupProps{Bounds: NewRectangle(20, 20, 100, 80), ID: 29200, Open: &open})
	r.Text(TextProps{Bounds: NewRectangle(24, 24, 0, 0), Text: "hidden", Font: Text14, Color: BLACK, Wrap: TextWrapNone})
	r.ClosePopup()
	r.EndPopup()
	r.EndFrame()
	if open {
		t.Fatal("ClosePopup did not update caller state")
	}
	for _, op := range r.ops {
		if op.ID == 29200 || op.Text == "hidden" {
			t.Fatal("same-frame close retained popup paint")
		}
	}

	open = true
	r.BeginFrame()
	r.BeginPopup(PopupProps{Bounds: NewRectangle(20, 20, 100, 80), ID: 29200, Open: &open})
	r.EndPopup()
	r.EndFrame()
	r.BeginFrame()
	r.EndFrame()
	if open {
		t.Fatal("missing popup owner did not close caller state")
	}
}

func TestComposedPopupScopeBalance(t *testing.T) {
	r := New(AppConfig{}).(*runtime)
	defer func() {
		if recover() == nil {
			t.Fatal("EndPopup without BeginPopup was accepted")
		}
	}()
	r.EndPopup()
}
