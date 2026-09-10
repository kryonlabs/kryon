package kryon

// Popup surfaces resolve through Button's palette and material contract. They
// only paint here; dropdownAt remains the sole owner of selection and input.
func (r *runtime) dropdownStyle(role int32, selected bool, state ButtonState) Style {
	base := resolveButtonStyle(r.theme(), r.effectiveDark(), r.activeTheme,
		ButtonProps{Tone: ButtonToneNeutral, Emphasis: ButtonEmphasisSoft}, state)
	emphasis := ButtonEmphasisFilled
	if role == 2 {
		emphasis = ButtonEmphasis(Dropdown_SelectionEmphasis(packRGBA(r.theme().surface)))
	}
	accent := resolveButtonStyle(r.theme(), r.effectiveDark(), r.activeTheme,
		ButtonProps{Tone: ButtonToneAccent, Emphasis: emphasis}, ButtonStateNormal)
	return unpackStyle(Dropdown_Appearance(packStyle(base), packStyle(accent), packRGBA(r.theme().surface), role, int32(state), selected))
}

func (r *runtime) dropdownSurface(bounds Rectangle, role int32, selected bool, state ButtonState) FrameOp {
	paint := r.dropdownStyle(role, selected, state)
	return FrameOp{Kind: FrameOpSurface, Bounds: bounds,
		Color: paint.Background, BorderColor: paint.Border, TextColor: paint.Foreground,
		FocusColor: paint.Focus, AmbientColor: r.theme().surface,
		Radius: paint.Radius, BorderWidth: paint.BorderWidth, Opacity: paint.Opacity,
		Material: paint.Material, FillStates: styleFill(paint), FillStatesValid: true,
		Hovered: role == 2 && !selected && state == ButtonStateHover}
}

func (r *runtime) dropdownTrigger(id int32, bounds Rectangle, open, focused bool) Color {
	disabled := r.contentDisabled()
	hovered := !disabled && (open || pointInRect(r.mousePos.X, r.mousePos.Y, bounds) && !r.popupCaptures(r.mousePos.X, r.mousePos.Y))
	held := hovered && r.mouseDown[MouseButtonLeft]
	props := ButtonProps{ID: id, Tone: ButtonToneNeutral, Emphasis: ButtonEmphasisSoft, Disabled: disabled}
	props.Style = ControlStyle{
		Normal:   r.dropdownStyle(0, false, ButtonStateNormal),
		Hover:    r.dropdownStyle(0, false, ButtonStateHover),
		Pressed:  r.dropdownStyle(0, false, ButtonStatePressed),
		Focused:  r.dropdownStyle(0, false, ButtonStateFocus),
		Disabled: r.dropdownStyle(0, false, ButtonStateDisabled),
	}
	input := Button_ResolveButtonInput(props, Activation{Hovered: hovered, Pressed: held, Focused: focused})
	metrics := r.themeMetrics()
	motion := r.Button_AdvanceButtonMotion(uint64(uint32(id)), props, input,
		Surface_DefaultMotionEnabled(), r.frameDeltaMS,
		metrics.TransitionNormalMS, metrics.TransitionFastMS)
	appearance := resolveButtonFrame(r.theme(), r.effectiveDark(), r.activeTheme, props,
		ButtonState(input.Interaction.State), true, motion.Hover.Value, motion.Press.Value, motion.Focus.Value)
	paint := unpackStyle(appearance.Value)
	frame := r.dropdownSurface(bounds, 0, false, ButtonStateNormal)
	frame.Kind = FrameOpButton
	frame.ID = id
	frame.Color, frame.BorderColor, frame.TextColor = paint.Background, paint.Border, paint.Foreground
	frame.Radius, frame.Opacity = paint.Radius, paint.Opacity
	frame.FillStates = appearance.Fill
	frame.Disabled, frame.Focused, frame.Hovered, frame.Pressed = disabled, focused, hovered, held
	frame.MotionValid = true
	frame.HoverAmount, frame.PressAmount, frame.FocusAmount = motion.Hover.Value, motion.Press.Value, motion.Focus.Value
	r.recordButton(frame)
	return unpackRGBA(Surface_Opacity(packRGBA(paint.Foreground), paint.Opacity))
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
