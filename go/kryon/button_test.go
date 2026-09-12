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

func TestDisabledBordersSoftenColorWithoutErasingNeutralEdges(t *testing.T) {
	colors := []uint32{0x183858ff, 0x006cffff, 0xff0020ff, 0x00ccaaff, 0xffaa00ff}
	for _, surface := range []uint32{0x092039ff, 0xffffffff} {
		for tone, base := range colors {
			for _, emphasis := range []ButtonEmphasis{ButtonEmphasisFilled, ButtonEmphasisSoft, ButtonEmphasisOutline, ButtonEmphasisGhost, ButtonEmphasisLink} {
				color := base
				if surface == 0xffffffff && emphasis == ButtonEmphasisFilled && tone != int(ButtonToneAccent) {
					color = Button_MixColor(surface, color, 28)
				}
				amount := uint32(25)
				if tone == int(ButtonToneNeutral) {
					amount = 45
				}
				want := Button_MixColor(surface, color, amount)
				if emphasis == ButtonEmphasisGhost || emphasis == ButtonEmphasisLink {
					want = 0
				}
				got := Button_ButtonBorder(int32(tone), int32(emphasis), int32(ButtonStateDisabled),
					surface, colors[1], colors[0], colors[2], colors[3], colors[4])
				if got != want {
					t.Fatalf("surface=%08x tone=%d emphasis=%d: border=%08x want=%08x", surface, tone, emphasis, got, want)
				}
			}
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

func TestLightDangerPressedBodyAndOverride(t *testing.T) {
	palette := Theme_DefaultPalette(false)
	resolve := func(state ButtonState, styles StyleStates) StyleData {
		return Button_ResolveAppearance(int32(ButtonToneDanger), int32(ButtonEmphasisFilled),
			int32(state), 0, false, false, false, false, false, palette, Theme_DefaultMetrics(), styles)
	}
	normal := resolve(ButtonStateNormal, StyleStates{})
	pressed := resolve(ButtonStatePressed, StyleStates{})
	if pressed.Background != Button_MixColor(normal.Background, palette.Surface, 20) {
		t.Fatal("pressed danger material must soften its saturated body")
	}
	if pressed.Foreground != normal.Foreground {
		t.Fatal("pressed body tint must not change the semantic label")
	}
	custom := uint32(0x12345600)
	styles := StyleStates{Pressed: StyleData{Fields: uint32(StyleBackground), Background: custom}}
	if resolve(ButtonStatePressed, styles).Background != custom {
		t.Fatal("custom pressed background must override the default tint")
	}
}

func TestLightWarningBodyAndDisabledTint(t *testing.T) {
	palette := Theme_DefaultPalette(false)
	resolve := func(state ButtonState, styles StyleStates) StyleData {
		return Button_ResolveAppearance(int32(ButtonToneWarning), int32(ButtonEmphasisFilled),
			int32(state), 0, false, false, false, false, false, palette, Theme_DefaultMetrics(), styles)
	}
	warning := Surface_ChromaColor(palette.Warning, 255)
	want := Button_MixColor(palette.Surface, warning, 20)
	for _, state := range []ButtonState{ButtonStateNormal, ButtonStateFocus} {
		if resolve(state, StyleStates{}).Background != want {
			t.Fatal("available warning face must retain its chromatic body")
		}
	}
	disabled := Button_MixColor(palette.Surface, Button_MixColor(palette.Surface, warning, 18), 45)
	if resolve(ButtonStateDisabled, StyleStates{}).Background != disabled {
		t.Fatal("stronger available tint must not deepen disabled warning material")
	}
	custom := uint32(0x12345600)
	if resolve(ButtonStateNormal, StyleStates{Normal: StyleData{Fields: uint32(StyleBackground), Background: custom}}).Background != custom {
		t.Fatal("custom transparent background must override the warning material")
	}
}

func TestLightOutlineBodyPreservesAlphaAndOverrides(t *testing.T) {
	for _, alpha := range []uint32{0, 128, 255} {
		palette := Theme_DefaultPalette(false)
		palette.Surface = palette.Surface&0xffffff00 | alpha
		for _, state := range []ButtonState{ButtonStateNormal, ButtonStateFocus} {
			resolve := func(styles StyleStates) StyleData {
				return Button_ResolveAppearance(int32(ButtonToneAccent), int32(ButtonEmphasisOutline),
					int32(state), 0, false, false, false, false, false, palette, Theme_DefaultMetrics(), styles)
			}
			want := Button_MixColor(palette.Surface, palette.Accent, 4)&0xffffff00 | alpha
			if got := resolve(StyleStates{}); got.Background != want {
				t.Fatalf("state %v: outline tint must retain the surface alpha: %08x, want %08x", state, got.Background, want)
			}
			custom := uint32(0x12345600) | alpha
			if got := resolve(StyleStates{Normal: StyleData{Fields: uint32(StyleBackground), Background: custom}}); got.Background != custom {
				t.Fatal("application background must override the default outline tint")
			}
		}
	}
}

func TestDarkPressedOutlineKeepsRestrainedTintAndOverrides(t *testing.T) {
	palette := Theme_DefaultPalette(true)
	resolve := func(styles StyleStates) StyleData {
		return Button_ResolveAppearance(int32(ButtonToneAccent), int32(ButtonEmphasisOutline),
			int32(ButtonStatePressed), 0, false, false, false, false, false,
			palette, Theme_DefaultMetrics(), styles)
	}
	if got := resolve(StyleStates{}).Background; got != Button_MixColor(palette.Surface, palette.Accent, 12) {
		t.Fatalf("pressed outline has excessive body tint: %08x", got)
	}
	for _, custom := range []uint32{0x12345600, 0x12345680, 0x123456ff} {
		value := StyleData{Fields: uint32(StyleBackground), Background: custom}
		for _, styles := range []StyleStates{{Normal: value}, {Pressed: value}} {
			if resolve(styles).Background != custom {
				t.Fatal("outline tint replaced an explicit background or its alpha")
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
	rt := New(AppConfig{Width: 100, Height: 100}).(*runtime)
	frame, activated := rt.surfaceButtonFrame(ButtonProps{
		ID: 1, Bounds: Rectangle{Width: 80, Height: 40}, Label: "Run",
		State: ButtonStateDisabled, Loading: true,
		Style: ControlStyle{Disabled: Style{Fields: StyleBackground, Background: Color{1, 2, 3, 128}}},
	}, Rectangle{}, false)
	if activated || !frame.Disabled || !frame.Button.Props.Loading || frame.Hovered || frame.Pressed || frame.Focused {
		t.Fatalf("explicit disabled state lost to loading: %+v, activated=%v", frame, activated)
	}
	if unpackRGBA(frame.Button.Appearance.Value.Background) != (Color{1, 2, 3, 128}) {
		t.Fatalf("disabled appearance lost its explicit style: %+v", unpackRGBA(frame.Button.Appearance.Value.Background))
	}
}

func TestLightGhostHoverBodyPreservesAlphaAndOverrides(t *testing.T) {
	for _, alpha := range []uint32{0, 128, 255} {
		palette := Theme_DefaultPalette(false)
		palette.Surface = palette.Surface&0xffffff00 | alpha
		resolve := func(state ButtonState, styles StyleStates) StyleData {
			return Button_ResolveAppearance(int32(ButtonToneAccent), int32(ButtonEmphasisGhost),
				int32(state), 0, false, false, false, false, false, palette, Theme_DefaultMetrics(), styles)
		}
		want := Button_MixColor(palette.Surface, palette.Accent, 5)&0xffffff00 | alpha
		hover := resolve(ButtonStateHover, StyleStates{})
		if hover.Background != want || hover.Border != 0 {
			t.Fatal("hovering ghost must keep a borderless tinted face with the surface alpha")
		}
		for _, state := range []ButtonState{ButtonStateNormal, ButtonStateFocus} {
			if resolve(state, StyleStates{}).Background != palette.Surface {
				t.Fatal("hover tint must not change resting or focused ghost material")
			}
		}
		custom := uint32(0x12345600) | alpha
		styles := StyleStates{Hover: StyleData{Fields: uint32(StyleBackground), Background: custom}}
		if resolve(ButtonStateHover, styles).Background != custom {
			t.Fatal("explicit hover background must override the ghost tint")
		}
	}
}

func TestFocusLinkTintPreservesSurfaceAlpha(t *testing.T) {
	for _, dark := range []bool{false, true} {
		for _, alpha := range []uint32{0, 128, 255} {
			palette := Theme_DefaultPalette(dark)
			palette.Surface = palette.Surface&0xffffff00 | alpha
			metrics := Theme_DefaultMetrics()
			appearance := func(emphasis ButtonEmphasis, styles StyleStates) StyleData {
				return Button_ResolveAppearance(int32(ButtonToneAccent), int32(emphasis),
					int32(ButtonStateFocus), 0, false, false, false, false, false, palette, metrics, styles)
			}
			link := appearance(ButtonEmphasisLink, StyleStates{})
			want := Button_MixColor(palette.Surface, palette.Accent, 8)&0xffffff00 | alpha
			if link.Background != want || link.Border != 0 {
				t.Fatalf("focused link must retain a borderless, alpha-preserving tinted face: %+v", link)
			}
			if ghost := appearance(ButtonEmphasisGhost, StyleStates{}); ghost.Background != palette.Surface {
				t.Fatal("link tint must not change the ghost face")
			}
			custom := uint32(0x12345600) | alpha
			styles := StyleStates{Focused: StyleData{Fields: uint32(StyleBackground), Background: custom}}
			if got := appearance(ButtonEmphasisLink, styles); got.Background != custom {
				t.Fatal("explicit focused background must override the default link tint")
			}
		}
	}
}

func TestPaleAccentFocusTintAndOverrides(t *testing.T) {
	for _, alpha := range []uint32{0, 128, 255} {
		palette := Theme_DefaultPalette(false)
		palette.Surface = palette.Surface&0xffffff00 | alpha
		palette.Accent = palette.Accent&0xffffff00 | alpha
		appearance := func(styles StyleStates) StyleData {
			return Button_ResolveAppearance(int32(ButtonToneAccent), int32(ButtonEmphasisFilled),
				int32(ButtonStateFocus), 0, false, false, false, false, false,
				palette, Theme_DefaultMetrics(), styles)
		}
		focus := appearance(StyleStates{})
		if focus.Background != Button_MixColor(palette.Surface, palette.Accent, 10) || focus.Background&255 != alpha {
			t.Fatal("pale accent focus must retain a quiet, alpha-preserving tint")
		}
		custom := uint32(0x12345600) | alpha
		for _, styles := range []StyleStates{
			{Normal: StyleData{Fields: uint32(StyleBackground), Background: custom}},
			{Focused: StyleData{Fields: uint32(StyleBackground), Background: custom}},
		} {
			if appearance(styles).Background != custom {
				t.Fatal("explicit background must override the pale focus default")
			}
		}
	}
}

func TestNeutralFocusBodyLightPreservesAlphaAndOverrides(t *testing.T) {
	for _, alpha := range []uint32{0, 128, 255} {
		palette := Theme_DefaultPalette(true)
		palette.Surface = palette.Surface&0xffffff00 | alpha
		palette.SurfaceRaised = palette.SurfaceRaised&0xffffff00 | alpha
		metrics := Theme_DefaultMetrics()
		appearance := func(state ButtonState, styles StyleStates) StyleData {
			return Button_ResolveAppearance(int32(ButtonToneNeutral), int32(ButtonEmphasisSoft),
				int32(state), 0, false, false, false, false, false, palette, metrics, styles)
		}
		normal := appearance(ButtonStateNormal, StyleStates{})
		focus := appearance(ButtonStateFocus, StyleStates{})
		white := uint32(0xffffff00) | alpha
		if focus.Background != Button_MixColor(normal.Background, white, 5) || focus.Background&255 != alpha {
			t.Fatal("neutral focus body light must preserve the material's alpha")
		}
		custom := uint32(0x12345600) | alpha
		styles := StyleStates{Normal: StyleData{Fields: uint32(StyleBackground), Background: custom}}
		if got := appearance(ButtonStateFocus, styles); got.Background != custom {
			t.Fatal("explicit Style background must override diffuse focus light")
		}
	}
}

func TestSharedButtonFrameTransitionEligibility(t *testing.T) {
	for _, dark := range []bool{false, true} {
		palette, metrics := Theme_DefaultPalette(dark), Theme_DefaultMetrics()
		styles := packStyleStates(ControlStyle{
			Normal: Style{Fields: StyleForeground | StyleBackgroundEnd, Foreground: Color{20, 40, 60, 0}, BackgroundEnd: Color{80, 90, 100, 255}},
			Hover:  Style{Fields: StyleForeground | StyleFontSize, Foreground: Color{100, 120, 140, 255}, FontSize: 27},
		})
		for flags := 0; flags < 16; flags++ {
			automatic, disabled, loading, selected := flags&1 != 0, flags&2 != 0, flags&4 != 0, flags&8 != 0
			resolved := Button_ResolveAppearance(1, 0, int32(ButtonStateHover), 0, false, false,
				disabled, loading, selected, palette, metrics, styles)
			frame := Button_ResolveFrame(1, 0, int32(ButtonStateHover), 0, false, false,
				disabled, loading, selected, palette, metrics, styles, automatic, 0.5, 0, 0)
			if automatic && !disabled && !loading && !selected {
				normal := Button_ResolveAppearance(1, 0, int32(ButtonStateNormal), 0, false, false,
					false, false, false, palette, metrics, styles)
				if frame.Value.Foreground != Surface_InteractionColor(normal.Foreground, resolved.Foreground, normal.Foreground, normal.Foreground, 0.5, 0, 0) ||
					frame.Value.FontSize != 27 {
					t.Fatal("automatic appearance must blend color while retaining the resolved layout font")
				}
			} else if frame.Value != resolved || frame.Fill != Surface_FillState(resolved.Fields, resolved.Background, resolved.BackgroundEnd) {
				t.Fatalf("flags %d: fixed, disabled, loading or selected style was animated", flags)
			}
		}
	}
}

func TestDarkPrimaryHoverKeepsDepthInTheSurface(t *testing.T) {
	for _, test := range []struct {
		state   ButtonState
		surface uint32
		want    uint32
	}{
		{ButtonStateHover, 0x092039ff, 0x0570ffff},
		{ButtonStateNormal, 0x092039ff, 0x006cffff},
		{ButtonStateHover, 0xffffffff, 0x87bdffff},
	} {
		got := Button_ButtonBackground(int32(ButtonToneAccent), int32(ButtonEmphasisFilled),
			int32(test.state), test.surface, 0x006cffff, 0x2184ffff, 0, 0, 0, 0, 0)
		if got != test.want {
			t.Fatalf("state=%d surface=%08x: background=%08x, want %08x", test.state, test.surface, got, test.want)
		}
	}
}

func TestLoadingEdgeLeavesActivityToSpinner(t *testing.T) {
	for _, test := range []struct {
		state   ButtonState
		surface uint32
		want    uint32
	}{
		{ButtonStateLoading, 0x092039ff, 0x082c59ff},
		{ButtonStateNormal, 0x092039ff, 0x006cffff},
		{ButtonStateLoading, 0xffffffff, 0xe0edffff},
		{ButtonStateNormal, 0xffffffff, 0x006cffff},
	} {
		got := Button_ButtonBorder(int32(ButtonToneAccent), int32(ButtonEmphasisFilled),
			int32(test.state), test.surface, 0x006cffff, 0, 0, 0, 0)
		if got != test.want {
			t.Fatalf("state=%d surface=%08x: border=%08x, want %08x", test.state, test.surface, got, test.want)
		}
	}
	spinner := Button_ButtonForeground(int32(ButtonToneAccent), int32(ButtonEmphasisFilled),
		int32(ButtonStateLoading), 0x092039ff, 0xffffffff, 0xffffffff,
		0, 0, 0, 0, 0, 0, 0, 0, 0x006cffff)
	if spinner != 0x006cffff {
		t.Fatalf("loading spinner lost its accent: %08x", spinner)
	}
	outline := Button_ButtonBorder(int32(ButtonToneAccent), int32(ButtonEmphasisOutline),
		int32(ButtonStateLoading), 0xffffffff, 0x006cffff, 0, 0, 0, 0)
	if outline != 0xbfdaffff {
		t.Fatalf("light loading outline lost its extra definition: %08x", outline)
	}
	for _, tone := range []ButtonTone{ButtonToneNeutral, ButtonToneAccent, ButtonToneDanger, ButtonToneSuccess, ButtonToneWarning} {
		for _, emphasis := range []ButtonEmphasis{ButtonEmphasisFilled, ButtonEmphasisSoft, ButtonEmphasisOutline} {
			border := Button_ButtonBorder(int32(tone), int32(emphasis), int32(ButtonStateLoading),
				0x092039ff, 0x006cffff, 0x183858ff, 0xff0020ff, 0x00ccaaff, 0xffaa00ff)
			wantAlpha := uint32(255)
			if tone == ButtonToneNeutral || emphasis == ButtonEmphasisOutline {
				wantAlpha = 0
			}
			if border&255 != wantAlpha {
				t.Fatalf("loading tone %v emphasis %v: border alpha %d, want %d", tone, emphasis, border&255, wantAlpha)
			}
		}
	}
	r := New(AppConfig{}).(*runtime)
	r.SetTheme(ThemeDefaultDark())
	for _, alpha := range []uint8{0, 128, 255} {
		props := ButtonProps{Emphasis: ButtonEmphasisOutline, Style: ControlStyle{
			Loading: Style{Fields: StyleBorder, Border: Color{17, 34, 51, alpha}}}}
		resolved := resolveButtonStyle(r.theme(), false, r.activeTheme, props, ButtonStateLoading)
		if resolved.Border != props.Style.Loading.Border {
			t.Fatal("loading material discarded a custom border")
		}
	}
}

func TestDisabledScopeResolvesButtonStyleBeforeMeasurement(t *testing.T) {
	var direct FrameOp
	for _, scoped := range []bool{false, true} {
		r := New(AppConfig{Width: 400, Height: 180}).(*runtime)
		r.BeginFrame()
		r.BeginDisabled(scoped)
		r.Button(ButtonProps{Label: "Measured", ID: 951, Disabled: !scoped,
			Style: ControlStyle{
				Normal: Style{Fields: StyleFontSize | StylePaddingX | StylePaddingY,
					FontSize: 17, PaddingX: 8, PaddingY: 8},
				Disabled: Style{Fields: StyleFontSize | StylePaddingX | StylePaddingY | StyleForeground,
					FontSize: 27, PaddingX: 19, PaddingY: 20, Foreground: Color{17, 34, 51, 128}},
			}})
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
		brightness := func(c Color) int { return int(c.R) + int(c.G) + int(c.B) }
		if theme.Mode == ThemeModeDark {
			if brightness(unpackRGBA(op.Button.Appearance.Value.Foreground)) <= brightness(theme.Colors.DisabledText) {
				t.Fatal("dark disabled labels must retain contrast against the muted face")
			}
		} else if brightness(unpackRGBA(op.Button.Appearance.Value.Foreground)) >= brightness(theme.Colors.DisabledText) {
			t.Fatal("light disabled labels must not wash out against the pale face")
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

	rt.BeginButton(props)
	rt.End()
	shorthand := append([]FrameOp(nil), rt.ops...)
	rt.ops = nil
	props.Label = ""
	rt.BeginButton(props)
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
	transparent := Color{}
	hoverText := Color{R: 103, G: 232, B: 249, A: 255}
	r.Button(ButtonProps{
		Bounds: Rectangle{X: 10, Y: 10, Width: 140, Height: 40},
		Label:  "Custom", State: ButtonStateHover,
		Style: ControlStyle{
			Normal: Style{Fields: StyleBackground | StyleRadius,
				Background: transparent, Radius: 0},
			Hover: Style{Fields: StyleForeground | StyleContentOffset,
				Foreground: hoverText, ContentOffset: Vector2{X: 2, Y: 1}},
		},
	})
	op := r.ops[len(r.ops)-1]
	if unpackRGBA(op.Button.Appearance.Value.Background) != transparent || unpackRGBA(op.Button.Appearance.Value.Foreground) != hoverText {
		t.Fatalf("custom colors = %#v/%#v", unpackRGBA(op.Button.Appearance.Value.Background), unpackRGBA(op.Button.Appearance.Value.Foreground))
	}
	if op.Button.Appearance.Value.Radius != 0 || (Vector2{X: op.Button.Appearance.Value.OffsetX, Y: op.Button.Appearance.Value.OffsetY}) != (Vector2{X: 2, Y: 1}) {
		t.Fatalf("custom geometry = radius %v offset %#v", op.Button.Appearance.Value.Radius, (Vector2{X: op.Button.Appearance.Value.OffsetX, Y: op.Button.Appearance.Value.OffsetY}))
	}
}

func TestOutlineRadiusUsesThemeAndPreservesOverrides(t *testing.T) {
	r := New(AppConfig{}).(*runtime)
	theme := ThemeDefaultLight()
	theme.Metrics.RadiusMedium = 6
	theme.Metrics.RadiusLarge = 14
	theme.Metrics.RadiusPill = 999
	r.SetTheme(theme)
	props := ButtonProps{Emphasis: ButtonEmphasisOutline}
	resolve := func(state ButtonState) Style {
		return resolveButtonStyle(r.theme(), false, r.activeTheme, props, state)
	}
	for _, state := range []ButtonState{ButtonStateNormal, ButtonStateHover} {
		if resolve(state).Radius != 14 {
			t.Fatal("resting outline did not use the theme's large radius")
		}
	}
	for _, state := range []ButtonState{ButtonStateFocus, ButtonStatePressed, ButtonStateDisabled} {
		if resolve(state).Radius != 6 {
			t.Fatal("compact outline did not use the theme's medium radius")
		}
	}
	props.Pill = true
	if resolve(ButtonStateNormal).Radius != 999 {
		t.Fatal("outline geometry replaced explicit pill shape")
	}
	props.Style.Normal = Style{Fields: StyleRadius, Radius: 0}
	if resolve(ButtonStateNormal).Radius != 0 {
		t.Fatal("explicit zero radius was lost")
	}
}

func TestRestingButtonRadiusUsesThemeMetrics(t *testing.T) {
	r := New(AppConfig{}).(*runtime)
	theme := ThemeDefaultDark()
	theme.Metrics.RadiusMedium = 6
	theme.Metrics.RadiusLarge = 14
	r.SetTheme(theme)
	for _, size := range []ControlSize{ControlSizeSmall, ControlSizeMedium, ControlSizeLarge} {
		for _, state := range []ButtonState{ButtonStateNormal, ButtonStateHover, ButtonStatePressed,
			ButtonStateFocus, ButtonStateDisabled, ButtonStateLoading, ButtonStateSelected} {
			props := ButtonProps{Size: size}
			want := float32(6)
			if size == ControlSizeMedium && (state == ButtonStateNormal || state == ButtonStateHover) {
				want = 10
			}
			actual := resolveButtonStyle(r.theme(), false, r.activeTheme, props, state)
			if actual.Radius != want {
				t.Fatalf("size %v state %v: radius %g, want %g", size, state, actual.Radius, want)
			}
			props.Style.Normal = Style{Fields: StyleRadius}
			if resolveButtonStyle(r.theme(), false, r.activeTheme, props, state).Radius != 0 {
				t.Fatal("default rounding replaced explicit zero radius")
			}
		}
	}
}

func TestOutlineRadiusTransitionsThroughRealInput(t *testing.T) {
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
						expected = 12
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
			frame(0, 12)
			r.SetFocus(props.ID)
			// At 35/140 ms, cubic ease-out is 0.578125.
			frame(35*time.Millisecond, 9.6875)
			r.SetFocus(0)
			frame(0, 9.6875) // Retargeting must not jump.
			frame(35*time.Millisecond, 12-4*0.578125*0.421875)
			frame(140*time.Millisecond, 12)
			r.SetFocus(props.ID)
			frame(140*time.Millisecond, 8)
			r.QueueMouseMove(50, 40)
			frame(140*time.Millisecond, 12)
			r.QueueMouseButtonDown(MouseButtonLeft, 50, 40)
			frame(40*time.Millisecond, 8.5) // Half of the 80 ms press duration.
			r.QueueMouseButtonUp(MouseButtonLeft, 50, 40)
			frame(140*time.Millisecond, 12)
			props.Style.Normal = Style{Fields: StyleRadius, Radius: 0}
			frame(0, 0)
			r.QueueMouseMove(-100, -100)
			frame(70*time.Millisecond, 0)
		})
	}
}

func TestLightButtonInteractionPreservesMaterialEmphasis(t *testing.T) {
	r := New(AppConfig{}).(*runtime)
	r.SetTheme(ThemeDefaultLight())
	resolve := func(tone ButtonTone, emphasis ButtonEmphasis, state ButtonState) Style {
		return resolveButtonStyle(r.theme(), false, r.activeTheme,
			ButtonProps{Tone: tone, Emphasis: emphasis}, state)
	}
	for _, tone := range []ButtonTone{ButtonToneDanger, ButtonToneSuccess, ButtonToneWarning} {
		normal := resolve(tone, ButtonEmphasisFilled, ButtonStateNormal)
		hover := resolve(tone, ButtonEmphasisFilled, ButtonStateHover)
		if hover.Background.R < normal.Background.R || hover.Background.G < normal.Background.G ||
			hover.Background.B < normal.Background.B || hover.Background == normal.Background {
			t.Fatalf("light semantic hover must illuminate its pastel face: tone=%d normal=%v hover=%v",
				tone, normal.Background, hover.Background)
		}
		if hover.Background.A != normal.Background.A || hover.Foreground != normal.Foreground {
			t.Fatal("hover illumination must preserve face opacity and readable label color")
		}
	}
	for _, emphasis := range []ButtonEmphasis{ButtonEmphasisOutline, ButtonEmphasisLink} {
		pressed := resolve(ButtonToneAccent, emphasis, ButtonStatePressed)
		filled := resolve(ButtonToneAccent, ButtonEmphasisFilled, ButtonStatePressed)
		if pressed.Background.R <= filled.Background.R || pressed.Background.G <= filled.Background.G {
			t.Fatalf("press turned low-emphasis material into a filled face: emphasis=%d", emphasis)
		}
	}
	normal := resolve(ButtonToneNeutral, ButtonEmphasisSoft, ButtonStateNormal)
	pressed := resolve(ButtonToneNeutral, ButtonEmphasisSoft, ButtonStatePressed)
	if pressed.Background != normal.Background {
		t.Fatal("neutral press should sink the existing surface, not replace it with white")
	}
}

func TestGeneratedButtonThemePolicy(t *testing.T) {
	if ButtonStateHover != 2 || int32(ButtonStateHover) != 2 ||
		ButtonToneAccent != 1 || int32(ButtonToneAccent) != 1 ||
		ButtonEmphasisFilled != 0 || int32(ButtonEmphasisFilled) != 0 {
		t.Fatalf("public/generated enum values diverged: state=%d/%d tone=%d/%d emphasis=%d/%d",
			ButtonStateHover, int32(ButtonStateHover), ButtonToneAccent, int32(ButtonToneAccent),
			ButtonEmphasisFilled, int32(ButtonEmphasisFilled))
	}
	surface := Color{16, 24, 40, 255}
	accent := Color{37, 99, 235, 255}
	hover := Color{59, 130, 246, 255}
	pressed := Color{29, 78, 216, 255}
	neutral := Color{51, 65, 85, 255}
	danger := Color{220, 38, 38, 255}
	success := Color{5, 150, 105, 255}
	warning := Color{217, 119, 6, 255}
	neutralHover := Button_ButtonBackground(
		int32(ButtonToneNeutral), int32(ButtonEmphasisSoft), int32(ButtonStateHover),
		packRGBA(surface), packRGBA(accent), packRGBA(hover), packRGBA(pressed),
		packRGBA(neutral), packRGBA(danger), packRGBA(success), packRGBA(warning))
	neutralBody := Button_MixColor(packRGBA(surface), packRGBA(neutral), 85)
	if neutralHover != Button_MixColor(neutralBody, packRGBA(hover), 12) {
		t.Fatal("neutral hover must retain its body color beneath the cool reflection")
	}

	got := unpackRGBA(Button_ButtonBackground(
		int32(ButtonToneAccent), int32(ButtonEmphasisFilled), int32(ButtonStateHover),
		packRGBA(surface), packRGBA(accent), packRGBA(hover), packRGBA(pressed),
		packRGBA(neutral), packRGBA(danger), packRGBA(success), packRGBA(warning)))
	// The body uses a 15% lift; surface lighting supplies the remaining depth.
	wantHover := Color{40, 104, 237, 255}
	if got != wantHover {
		t.Fatalf("accent hover = %#v, want %#v", got, wantHover)
	}

	got = unpackRGBA(Button_ButtonBackground(
		int32(ButtonToneDanger), int32(ButtonEmphasisOutline), int32(ButtonStateNormal),
		packRGBA(surface), packRGBA(accent), packRGBA(hover), packRGBA(pressed),
		packRGBA(neutral), packRGBA(danger), packRGBA(success), packRGBA(warning)))
	if got != surface {
		t.Fatalf("outlined danger background = %#v, want surface %#v", got, surface)
	}

	// Dark semantic faces brighten on hover and sink on press, while the
	// saturated tone remains available independently for the border and glow.
	for _, tone := range []ButtonTone{ButtonToneDanger, ButtonToneSuccess, ButtonToneWarning} {
		colors := make([]Color, 3)
		for index, state := range []ButtonState{ButtonStateNormal, ButtonStateHover, ButtonStatePressed} {
			colors[index] = unpackRGBA(Button_ButtonBackground(
				int32(tone), int32(ButtonEmphasisFilled), int32(state),
				packRGBA(surface), packRGBA(accent), packRGBA(hover), packRGBA(pressed),
				packRGBA(neutral), packRGBA(danger), packRGBA(success), packRGBA(warning)))
		}
		brightness := func(color Color) int { return int(color.R) + int(color.G) + int(color.B) }
		if brightness(colors[1]) <= brightness(colors[0]) || brightness(colors[2]) >= brightness(colors[0]) {
			t.Fatalf("semantic tone %d lacks hover/pressed depth: %#v", tone, colors)
		}
	}

	border := unpackRGBA(Button_ButtonBorder(
		int32(ButtonToneNeutral), int32(ButtonEmphasisGhost), int32(ButtonStateNormal),
		packRGBA(surface), packRGBA(accent), packRGBA(neutral), packRGBA(danger),
		packRGBA(success), packRGBA(warning)))
	if border != (Color{}) {
		t.Fatalf("ghost border = %#v, want transparent", border)
	}
}
