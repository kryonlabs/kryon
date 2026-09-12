package kryon

// Popup surfaces expose semantic roles; KSS owns the visual result.
func (r *runtime) dropdownStyle(role int32, selected bool, state ButtonState) Style {
	props := ButtonProps{Tone: ButtonToneNeutral, Emphasis: ButtonEmphasisSoft}
	if role == 1 {
		props.Emphasis = ButtonEmphasisFilled
	}
	if role == 2 && selected && state != ButtonStateDisabled {
		props.Tone = ButtonToneAccent
		props.Emphasis = ButtonEmphasisFilled
		props.Selected = true
		state = ButtonStateSelected
	}
	base := resolveButtonStyleForKind(r.theme(), r.effectiveDark(), r.activeTheme,
		props, state, StyleSheet_StyleKindDropdown())
	if role != 2 {
		return base
	}
	accentProps := ButtonProps{Tone: ButtonToneAccent, Emphasis: ButtonEmphasisFilled, Selected: selected}
	accentState := state
	if selected && state != ButtonStateDisabled {
		accentState = ButtonStateSelected
	}
	accent := resolveButtonStyleForKind(r.theme(), r.effectiveDark(), r.activeTheme,
		accentProps, accentState, StyleSheet_StyleKindDropdown())
	return unpackStyle(Dropdown_Appearance(packStyle(base), packStyle(accent), role, int32(state), selected))
}

func (r *runtime) dropdownSurface(bounds Rectangle, role int32, selected bool, state ButtonState) FrameOp {
	paint := r.dropdownStyle(role, selected, state)
	return FrameOp{Kind: FrameOpSurface, Bounds: bounds,
		Color: paint.Background, BorderColor: paint.Border, TextColor: paint.Foreground,
		FocusColor: paint.Focus, AmbientColor: r.appAmbientColor(),
		Radius: paint.Radius, BorderWidth: paint.BorderWidth, Opacity: paint.Opacity,
		Material: paint.Material, FillStates: styleFill(paint), FillStatesValid: true,
		Hovered: role == 2 && !selected && state == ButtonStateHover}
}

func (r *runtime) dropdownTrigger(id int32, bounds Rectangle, open, focused bool) Color {
	disabled := r.contentDisabled()
	hovered := !disabled && (open || pointInRect(r.mousePos.X, r.mousePos.Y, bounds) && !r.popupCaptures(r.mousePos.X, r.mousePos.Y))
	held := hovered && r.mouseDown[MouseButtonLeft]
	props := ButtonProps{Bounds: bounds, ID: id, Tone: ButtonToneNeutral, Emphasis: ButtonEmphasisSoft, Disabled: disabled}
	props.Style = ControlStyle{
		Normal:   r.dropdownStyle(0, false, ButtonStateNormal),
		Hover:    r.dropdownStyle(0, false, ButtonStateHover),
		Pressed:  r.dropdownStyle(0, false, ButtonStatePressed),
		Focused:  r.dropdownStyle(0, false, ButtonStateFocus),
		Disabled: r.dropdownStyle(0, false, ButtonStateDisabled),
	}
	input := Button_ResolveButtonInput(int32(props.State), props.Disabled, props.Loading, props.Selected,
		Activation{Hovered: hovered, Pressed: held, Focused: focused})
	metrics := r.themeMetrics()
	motion := r.Button_AdvanceButtonMotion(uint64(uint32(id)), int32(props.State), input,
		Surface_DefaultMotionEnabled(), r.frameDeltaMS,
		metrics.TransitionNormalMS, metrics.TransitionFastMS)
	appearance := resolveButtonFrameForKind(r.theme(), r.effectiveDark(), r.activeTheme, props,
		ButtonState(input.Interaction.State), true, motion.Hover.Value, motion.Press.Value,
		motion.Focus.Value, StyleSheet_StyleKindDropdown())
	resolved := Button_BuildFrame(props, input, appearance, motion, Rectangle{},
		packRGBA(r.appAmbientColor()), 1, int32(appearance.Value.FontSize), Text16)
	r.record(FrameOp{Kind: FrameOpButton, Button: resolved, Bounds: bounds, ID: id,
		Disabled: disabled, Focused: focused, Hovered: hovered, Pressed: held})
	return unpackRGBA(resolved.Foreground)
}

func (r *runtime) dropdownChevron(id int32, bounds Rectangle, open bool, color Color) {
	x, y := bounds.X+bounds.Width-24, bounds.Y+bounds.Height/2
	dy := float32(4)
	if open {
		dy = -dy
	}
	r.record(FrameOp{Kind: FrameOpLine, ID: id, Color: color,
		Bounds: Rectangle{X: x - 5, Y: y - dy/2, Width: 5, Height: dy}})
	r.record(FrameOp{Kind: FrameOpLine, ID: id, Color: color,
		Bounds: Rectangle{X: x, Y: y + dy/2, Width: 5, Height: -dy}})
}
