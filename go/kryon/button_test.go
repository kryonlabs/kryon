package kryon

import (
	"reflect"
	"testing"
	"time"
)

func TestButtonEnumBindingsPreservePublicTypesAndValues(t *testing.T) {
	tone := ButtonToneAccent
	emphasis := ButtonEmphasisOutline
	state := ButtonStateHover
	_ = ButtonProps{Tone: tone, Emphasis: emphasis, State: state}
	for index, value := range []ButtonTone{
		ButtonToneNeutral, ButtonToneAccent, ButtonToneDanger, ButtonToneSuccess, ButtonToneWarning,
	} {
		if int32(value) != int32(index) {
			t.Fatalf("tone %d changed public value to %d", index, value)
		}
	}
	for index, value := range []ButtonEmphasis{
		ButtonEmphasisFilled, ButtonEmphasisSoft, ButtonEmphasisOutline, ButtonEmphasisGhost, ButtonEmphasisLink,
	} {
		if int32(value) != int32(index) {
			t.Fatalf("emphasis %d changed public value to %d", index, value)
		}
	}
	for index, value := range []ButtonState{
		ButtonStateAuto, ButtonStateNormal, ButtonStateHover, ButtonStatePressed,
		ButtonStateFocus, ButtonStateDisabled, ButtonStateLoading, ButtonStateSelected,
	} {
		if int32(value) != int32(index) {
			t.Fatalf("state %d changed public value to %d", index, value)
		}
	}
}

func TestButtonMotionIdentitySurvivesContentAndOrderChanges(t *testing.T) {
	for _, ids := range [][2]int32{{9400, 9401}, {-2128831035, -1266624162}} {
		r := New(AppConfig{Width: 768, Height: 1024}).(*runtime)
		var expected InteractionMotion
		metrics := defaultThemeMetrics()
		for frame := 0; frame < 24; frame++ {
			moving := ButtonProps{ID: ids[0], Label: "Run",
				Bounds: Rectangle{X: float32(20 + frame%3), Y: 20, Width: float32(180 + frame%5), Height: 60}}
			other := ButtonProps{ID: ids[1], Label: "Other", Bounds: Rectangle{X: 300, Y: 20, Width: 180, Height: 60}}
			if frame%2 != 0 {
				moving.Label = "Working"
			}
			r.QueueMouseMove(80, 50)
			r.BeginFrame()
			if frame%2 != 0 {
				r.Button(other)
			}
			r.Button(moving)
			if frame%2 == 0 {
				r.Button(other)
			}
			r.EndFrame()
			expected = Surface_AdvanceInteractionMotion(expected, true, false, false,
				true, false, false, false, r.frameDeltaMS, metrics.TransitionNormalMS, metrics.TransitionFastMS)
			if instanceState[ButtonInstance](r, uint64(uint32(ids[0]))).Motion.Hover.Value != expected.Hover.Value || instanceState[ButtonInstance](r, uint64(uint32(ids[1]))).Motion.Hover.Value != 0 {
				t.Fatal("moving and relabeling a stable instance must not reset or transfer its animation")
			}
		}
	}
}

func TestButtonInputConsumesOnlyItsOwnActivation(t *testing.T) {
	first := New(AppConfig{Width: 100, Height: 100}).(*runtime)
	second := New(AppConfig{Width: 100, Height: 100}).(*runtime)
	defer first.Close()
	defer second.Close()
	previous := activeRuntime
	SetRuntime(second)
	defer SetRuntime(previous)
	props := ButtonProps{ID: 7, Bounds: Rectangle{Width: 80, Height: 40}}
	first.QueueTap(20, 20)
	first.BeginFrame()
	defer first.EndFrame()
	for _, state := range []ButtonState{ButtonStateDisabled, ButtonStateLoading} {
		props.State = state
		if input := first.Button_ReadButtonInput(props.Bounds, props.ID, int32(props.State),
			props.Disabled, props.Loading, props.Selected); input.Activated {
			t.Fatalf("state %v accepted activation", state)
		}
	}
	props.State = ButtonStateAuto
	input := first.Button_ReadButtonInput(props.Bounds, props.ID, int32(props.State),
		props.Disabled, props.Loading, props.Selected)
	if !input.Activated || !input.Interaction.Pressed {
		t.Fatalf("disabled previews consumed the enabled button's event: %+v", input)
	}
	if next := first.Button_ReadButtonInput(props.Bounds, props.ID, int32(props.State),
		props.Disabled, props.Loading, props.Selected); next.Activated {
		t.Fatal("activation was consumed twice")
	}
	if other := second.Button_ReadButtonInput(props.Bounds, props.ID, int32(props.State),
		props.Disabled, props.Loading, props.Selected); other.Activated {
		t.Fatal("activation leaked to the active runtime")
	}
	// Resolving a retained paint sample cannot poll the host or consume input.
	first.QueueTap(20, 20)
	resolveButtonInputForTest(props, Activation{Hovered: true})
	if !first.Button_ReadButtonInput(props.Bounds, props.ID, int32(props.State),
		props.Disabled, props.Loading, props.Selected).Activated {
		t.Fatal("retained paint resolution consumed an activation")
	}
}

func TestExplicitDisabledButtonOverridesLoadingAppearance(t *testing.T) {
	ClearStylePacks()
	t.Cleanup(ClearStylePacks)
	if !RegisterStylePackSource(`@pack test.button.disabled;
Button.disabled-bg:disabled { background: #01020380; }`, "Button Disabled", "") {
		t.Fatal("button disabled style pack did not register")
	}
	rt := New(AppConfig{Width: 100, Height: 100}).(*runtime)
	frame, activated := rt.surfaceButtonFrame(ButtonProps{
		ID: 1, Bounds: Rectangle{Width: 80, Height: 40}, Label: "Run",
		State: ButtonStateDisabled, Loading: true,
		ClassName: StyleClassID("disabled-bg"),
	}, Rectangle{}, false)
	if activated || !frame.Disabled || !frame.Button.Props.Loading || frame.Hovered || frame.Pressed || frame.Focused {
		t.Fatalf("explicit disabled state lost to loading: %+v, activated=%v", frame, activated)
	}
	if unpackRGBA(frame.Button.Appearance.Value.Background) != (Color{1, 2, 3, 128}) {
		t.Fatalf("disabled appearance lost its explicit style: %+v", unpackRGBA(frame.Button.Appearance.Value.Background))
	}
}

func TestDisabledScopeResolvesButtonStyleBeforeMeasurement(t *testing.T) {
	ClearStylePacks()
	t.Cleanup(ClearStylePacks)
	if !RegisterStylePackSource(`@pack test.button.disabled.measure;
Button.measure-disabled { font-size: 17; padding-x: 8; padding-y: 8; }
Button.measure-disabled:disabled { font-size: 27; padding-x: 19; padding-y: 20; foreground: #11223380; }`, "Button Disabled Measure", "") {
		t.Fatal("button disabled measure style pack did not register")
	}
	var direct FrameOp
	for _, scoped := range []bool{false, true} {
		r := New(AppConfig{Width: 400, Height: 180}).(*runtime)
		r.BeginFrame()
		r.BeginDisabled(scoped)
		r.Button(ButtonProps{Label: "Measured", ID: 951, Disabled: !scoped,
			ClassName: StyleClassID("measure-disabled")})
		r.EndDisabled()
		r.EndFrame()
		op := r.FrameOps()[0]
		if !scoped {
			direct = op
		} else if op.Bounds != direct.Bounds || op.Button.Font != direct.Button.Font ||
			unpackRGBA(op.Button.Appearance.Value.Background) != unpackRGBA(direct.Button.Appearance.Value.Background) || unpackRGBA(op.Button.Appearance.Value.Foreground) != unpackRGBA(direct.Button.Appearance.Value.Foreground) || unpackRGBA(op.Button.Appearance.Value.Border) != unpackRGBA(direct.Button.Appearance.Value.Border) {
			t.Fatalf("scoped disabled style differs from explicit disabled: scoped=%+v direct=%+v", op, direct)
		}
		if scoped && !reflect.DeepEqual(RenderFrame(400, 180, []FrameOp{op}).Pix,
			RenderFrame(400, 180, []FrameOp{direct}).Pix) {
			t.Fatal("scoped and explicit disabled buttons produced different pixels")
		}
	}
}

func TestButtonBlockingStateClearsAndRestartsMotion(t *testing.T) {
	for _, mode := range []string{"disabled", "scope", "disabled-state", "loading"} {
		t.Run(mode, func(t *testing.T) {
			now := time.Unix(1, 0)
			r := New(AppConfig{Width: 200, Height: 100, FrameClock: func() time.Time { return now }}).(*runtime)
			r.SetFocus(975)
			r.QueueMouseMove(30, 30)
			draw := func(blocked bool) (FrameOp, bool) {
				now = now.Add(35 * time.Millisecond)
				r.BeginFrame()
				r.BeginDisabled(blocked && mode == "scope")
				props := ButtonProps{Bounds: Rectangle{X: 20, Y: 20, Width: 120, Height: 40}, Label: "Run", ID: 975,
					Disabled: blocked && mode == "disabled", Loading: blocked && mode == "loading"}
				if blocked && mode == "disabled-state" {
					props.State = ButtonStateDisabled
				}
				clicked := r.Button(props)
				r.EndDisabled()
				r.EndFrame()
				return r.FrameOps()[0], clicked
			}
			draw(false)
			r.QueueKey(KeySpace)
			active, clicked := draw(false)
			if !clicked || active.Button.Material.Hover <= 0 || active.Button.Material.Press <= 0 || active.Button.Material.Focus <= 0 {
				t.Fatalf("test did not establish live interaction: %+v", active)
			}
			r.QueueKey(KeySpace)
			blocked, clicked := draw(true)
			if clicked || blocked.Button.Material.Hover != 0 || blocked.Button.Material.Press != 0 || blocked.Button.Material.Focus != 0 {
				t.Fatalf("blocking state retained interaction or activated: %+v", blocked)
			}
			resumed, clicked := draw(false)
			if clicked || resumed.Button.Material.Hover <= 0 || resumed.Button.Material.Hover >= 1 || resumed.Button.Material.Press != 0 {
				t.Fatalf("unblocking did not restart hover cleanly: %+v", resumed)
			}
		})
	}
}

func TestExplicitButtonPreviewDoesNotAnimateActivation(t *testing.T) {
	for _, state := range []ButtonState{ButtonStateNormal, ButtonStateHover, ButtonStatePressed,
		ButtonStateFocus, ButtonStateDisabled, ButtonStateLoading, ButtonStateSelected} {
		r := New(AppConfig{Width: 200, Height: 100}).(*runtime)
		r.SetFocus(976)
		r.QueueMouseMove(30, 30)
		r.QueueKey(KeySpace)
		r.BeginFrame()
		clicked := r.Button(ButtonProps{Bounds: Rectangle{X: 20, Y: 20, Width: 120, Height: 40}, Label: "Run", ID: 976, State: state})
		r.EndFrame()
		op := r.FrameOps()[0]
		wantClick := state != ButtonStateDisabled && state != ButtonStateLoading
		wantHover, wantPress, wantFocus := float32(0), float32(0), float32(0)
		if state == ButtonStateHover {
			wantHover = 1
		}
		if state == ButtonStatePressed {
			wantPress = 1
		}
		if state == ButtonStateFocus {
			wantFocus = 1
		}
		if clicked != wantClick || op.Button.Material.Hover != wantHover || op.Button.Material.Press != wantPress || op.Button.Material.Focus != wantFocus {
			t.Errorf("state %v: clicked %v motion %g/%g/%g, want %v %g/%g/%g", state, clicked,
				op.Button.Material.Hover, op.Button.Material.Press, op.Button.Material.Focus, wantClick, wantHover, wantPress, wantFocus)
		}
	}
}

func TestDisabledLabelsRemainReadableWithoutBecomingInteractive(t *testing.T) {
	useMaterialStyleForTest(t)
	for _, theme := range []Theme{ThemeDefaultDark(), ThemeDefaultLight()} {
		r := New(AppConfig{Width: 120, Height: 60}).(*runtime)
		r.SetTheme(theme)
		r.BeginFrame()
		r.QueueTap(30, 30)
		props := ButtonProps{Bounds: Rectangle{X: 10, Y: 10, Width: 100, Height: 40},
			Label: "Unavailable", ID: 901, Disabled: true, Tone: ButtonToneNeutral}
		if r.Button(props) {
			t.Fatal("improved label contrast must not enable a disabled button")
		}
		ops := r.FrameOps()
		op := ops[len(ops)-1]
		if !op.Disabled {
			t.Fatal("disabled state was lost")
		}
		foreground := unpackRGBA(op.Button.Appearance.Value.Foreground)
		if foreground.A == 0 || op.Button.Appearance.Value.Opacity >= 1 {
			t.Fatal("disabled labels must come from the attached style without becoming interactive")
		}
		r.EndFrame()
	}
}

func TestFaceChromaPreservesThemeHueAndOpacity(t *testing.T) {
	for _, test := range []struct{ color, peak, want uint32 }{
		{0x8080807f, 255, 0x8080807f},
		{0x00856a80, 200, 0x00c89f80},
		{0x80402000, 255, 0xff550000},
		{0xff0000ff, 300, 0xff0000ff},
	} {
		if got := Surface_ChromaColor(test.color, test.peak); got != test.want {
			t.Fatalf("chroma %#x at %d: got %#x want %#x", test.color, test.peak, got, test.want)
		}
	}
}

func TestButtonSplitLayoutPolicy(t *testing.T) {
	for _, test := range []struct {
		width, height, resolvedWidth, actionWidth float32
	}{
		{120, 40, 120, 80},
		{24, 40, 80, 40},
		{80, 40, 80, 40},
		{100.5, 27.25, 100.5, 73.25},
	} {
		layout := Button_ButtonResolveSplitLayout(test.width, test.height)
		want := ButtonSplitLayout{Width: test.resolvedWidth, ActionWidth: test.actionWidth,
			MenuOffset: test.actionWidth, MenuWidth: test.height, DividerInset: 8}
		if layout != want || layout.MenuOffset+layout.MenuWidth != layout.Width {
			t.Fatalf("split layout for %+v: got %+v, want %+v", test, layout, want)
		}
	}
}

func TestAutomaticButtonSplitSegmentsHaveIndependentIDs(t *testing.T) {
	r := New(AppConfig{Width: 640, Height: 200}).(*runtime)
	for frame := 0; frame < 2; frame++ {
		r.BeginFrame()
		for index := 0; index < 2; index++ {
			r.Button(ButtonProps{
				Bounds: Rectangle{X: float32(10 + index*150), Y: 10, Width: 120, Height: 40},
				Label:  "Add",
				Split:  true,
			})
		}
		r.Button(ButtonProps{Bounds: Rectangle{X: 310, Y: 10, Width: 120, Height: 40}, ID: 1, Label: "Explicit"})
		r.Button(ButtonProps{Bounds: Rectangle{X: 460, Y: 10, Width: 120, Height: 40}, Label: "Automatic"})
		want := []int32{0x40000000, 0x40000001, 0x40000002, 0x40000003, 1, 0x40000004}
		index := 0
		for _, op := range r.FrameOps() {
			if op.Kind != FrameOpButton {
				continue
			}
			if index >= len(want) || op.ID != want[index] {
				t.Fatalf("frame=%d segment=%d: id=%d expected sequence=%v", frame, index, op.ID, want)
			}
			index++
		}
		if index != len(want) {
			t.Fatalf("got %d button segments, want %d", index, len(want))
		}
		r.EndFrame()
	}
}

func TestButtonSplitSharedSurfaceAndIndependentActions(t *testing.T) {
	for _, test := range []struct {
		name                           string
		x                              float32
		disabled, loading, click, open bool
	}{
		{"action", 30, false, false, true, false},
		{"menu", 105, false, false, false, true},
		{"disabled", 105, true, false, false, false},
		{"loading", 30, false, true, false, false},
	} {
		t.Run(test.name, func(t *testing.T) {
			r := New(AppConfig{Width: 240, Height: 120}).(*runtime)
			r.BeginFrame()
			r.QueueTap(test.x, 30)
			open := int32(0)
			bounds := Rectangle{X: 10, Y: 10, Width: 120, Height: 40}
			activated := int32(0)
			clicked := r.Button(ButtonProps{
				Bounds: bounds, Label: "Add", ID: 20,
				Disabled: test.disabled, Loading: test.loading,
				Split: true, Open: &open, MenuID: 80, ActivatedID: &activated,
			})
			if clicked != test.click || (open != 0) != test.open {
				t.Fatalf("activation: clicked=%v open=%v", clicked, open)
			}
			buttons := 0
			for _, op := range r.FrameOps() {
				if op.Kind != FrameOpButton {
					continue
				}
				buttons++
				if op.Disclosure != (buttons == 2) {
					t.Fatal("only the menu segment should carry a disclosure chevron")
				}
				if op.ID != int32(19+buttons) {
					t.Fatal("explicit split-button IDs changed")
				}
				if op.Button.Material.Surface != bounds {
					t.Fatalf("segment does not share outer surface: %+v", op.Button.Material.Surface)
				}
			}
			if buttons != 2 {
				t.Fatalf("got %d button segments", buttons)
			}
			r.EndFrame()
		})
	}
}

func TestButtonLabelAndTextChildUseSameTextPath(t *testing.T) {
	rt := New(AppConfig{Width: 320, Height: 160}).(*runtime)
	props := ButtonProps{
		Bounds: Rectangle{X: 20, Y: 20, Width: 120, Height: 40},
		Label:  "Save", ID: 991,
	}

	rt.ButtonScope(props)
	rt.End()
	shorthand := append([]FrameOp(nil), rt.ops...)
	rt.ops = nil
	props.Label = ""
	rt.ButtonScope(props)
	rt.Text(TextProps{Text: "Save", Wrap: TextWrapNone})
	rt.End()
	composed := append([]FrameOp(nil), rt.ops...)

	if !reflect.DeepEqual(shorthand, composed) {
		t.Fatalf("label shorthand and Text child differ:\nshort: %#v\nchild: %#v",
			shorthand, composed)
	}
}

func TestGeneratedButtonGeometryAndStatePolicy(t *testing.T) {
	if got := Button_ShapeWidth(120, 48, 96, 300, false, false, true); got != 48 {
		t.Fatalf("circle width = %v, want 48", got)
	}
	if got := Button_ShapeRadius(0.16, 0.5, false, true); got != 0.5 {
		t.Fatalf("circle radius = %v, want 0.5", got)
	}
	if got := Style_ResolveState(int32(ButtonStateAuto), false, true,
		true, true, true, true); got != int32(ButtonStateLoading) {
		t.Fatalf("loading precedence = %v, want %v", got, ButtonStateLoading)
	}
	if got := Style_ResolveState(int32(ButtonStateAuto), true, false,
		true, true, true, true); got != int32(ButtonStateDisabled) {
		t.Fatalf("disabled precedence = %v, want %v", got, ButtonStateDisabled)
	}
}

func TestButtonControlStyleLayersTransparentAndStateValues(t *testing.T) {
	r := New(AppConfig{Width: 240, Height: 120}).(*runtime)
	ClearStylePacks()
	t.Cleanup(ClearStylePacks)
	if !RegisterStylePackSource(`@pack test.button.states;
Button.custom { background: #00000000; radius: 0; }
Button.custom:hover { foreground: #67e8f9; offset-x: 2; offset-y: 1; }`, "Button States", "") {
		t.Fatal("button state style pack did not register")
	}
	transparent := Color{}
	hoverText := Color{R: 103, G: 232, B: 249, A: 255}
	r.Button(ButtonProps{
		Bounds: Rectangle{X: 10, Y: 10, Width: 140, Height: 40},
		Label:  "Custom", State: ButtonStateHover,
		ClassName: StyleClassID("custom"),
	})
	op := r.ops[len(r.ops)-1]
	if unpackRGBA(op.Button.Appearance.Value.Background) != transparent || unpackRGBA(op.Button.Appearance.Value.Foreground) != hoverText {
		t.Fatalf("custom colors = %#v/%#v", unpackRGBA(op.Button.Appearance.Value.Background), unpackRGBA(op.Button.Appearance.Value.Foreground))
	}
	if op.Button.Appearance.Value.Radius != 0 || (Vector2{X: op.Button.Appearance.Value.OffsetX, Y: op.Button.Appearance.Value.OffsetY}) != (Vector2{X: 2, Y: 1}) {
		t.Fatalf("custom geometry = radius %v offset %#v", op.Button.Appearance.Value.Radius, (Vector2{X: op.Button.Appearance.Value.OffsetX, Y: op.Button.Appearance.Value.OffsetY}))
	}
}

func TestOutlineRadiusTransitionsThroughRealInput(t *testing.T) {
	useMaterialStyleForTest(t)
	if !RegisterStylePackSource(`@pack test.button.radius;
Button { radius: 8; }
Button.zero-radius { radius: 0; }`, "Button Radius", "") {
		t.Fatal("button radius style pack did not register")
	}
	if !SetActiveStylePack("test.button.radius") {
		t.Fatal("button radius style pack did not activate")
	}
	for _, theme := range []Theme{ThemeDefaultLight(), ThemeDefaultDark()} {
		t.Run(theme.Name, func(t *testing.T) {
			now := time.Unix(1, 0)
			r := New(AppConfig{FrameClock: func() time.Time { return now }}).(*runtime)
			r.SetTheme(theme)
			props := ButtonProps{ID: 11, Bounds: Rectangle{X: 20, Y: 20, Width: 72, Height: 40},
				Label: "Run", Emphasis: ButtonEmphasisOutline}
			neighbor := props
			neighbor.ID = 12
			neighbor.Bounds.X = 120
			r.QueueMouseMove(-100, -100)
			frame := func(delta time.Duration, want float32) {
				t.Helper()
				now = now.Add(delta)
				r.BeginFrame()
				r.Button(props)
				r.Button(neighbor)
				r.EndFrame()
				seen := 0
				for _, op := range r.FrameOps() {
					if op.Kind != FrameOpButton {
						continue
					}
					expected := want
					bounds := props.Bounds
					if op.ID == neighbor.ID {
						expected = 8
						bounds = neighbor.Bounds
					}
					if op.Button.Appearance.Value.Radius != expected || op.Bounds != bounds {
						t.Fatalf("button %d after %v: radius=%g, want %g; bounds=%+v",
							op.ID, delta, op.Button.Appearance.Value.Radius, expected, op.Bounds)
					}
					seen++
				}
				if seen != 2 {
					t.Fatalf("recorded %d buttons, want both independent instances", seen)
				}
			}
			frame(0, 8)
			r.SetFocus(props.ID)
			frame(35*time.Millisecond, 8)
			r.SetFocus(0)
			frame(0, 8)
			frame(35*time.Millisecond, 8)
			frame(140*time.Millisecond, 8)
			r.SetFocus(props.ID)
			frame(140*time.Millisecond, 8)
			r.QueueMouseMove(50, 40)
			frame(140*time.Millisecond, 8)
			r.QueueMouseButtonDown(MouseButtonLeft, 50, 40)
			frame(40*time.Millisecond, 8)
			r.QueueMouseButtonUp(MouseButtonLeft, 50, 40)
			frame(140*time.Millisecond, 8)
			props.ClassName = StyleClassID("zero-radius")
			frame(0, 0)
			r.QueueMouseMove(-100, -100)
			frame(70*time.Millisecond, 0)
		})
	}
}
