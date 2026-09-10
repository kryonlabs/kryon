package kryon

// Popup surfaces resolve through Button's palette and material contract. They
// only paint here; dropdownAt remains the sole owner of selection and input.
func (r *runtime) dropdownSurface(bounds Rectangle, selected bool, state ButtonState) FrameOp {
	props := ButtonProps{Tone: ButtonToneNeutral, Emphasis: ButtonEmphasisSoft}
	if selected {
		props.Tone = ButtonToneAccent
		state = ButtonStateSelected
	}
	paint := resolveButtonStyle(r.theme(), r.effectiveDark(), r.activeTheme, props, state)
	return FrameOp{Kind: FrameOpSurface, Bounds: bounds,
		Color: paint.Background, BorderColor: paint.Border, TextColor: paint.Foreground,
		FocusColor: paint.Focus, AmbientColor: r.theme().surface,
		Radius: paint.Radius, BorderWidth: paint.BorderWidth, Opacity: paint.Opacity,
		Material: paint.Material, FillStates: styleFill(paint), FillStatesValid: true}
}

func (r *runtime) dropdownTrigger(id int32, bounds Rectangle, open, focused bool) Color {
	disabled := r.contentDisabled()
	hovered := !disabled && (open || pointInRect(r.mousePos.X, r.mousePos.Y, bounds) && !r.popupCaptures(r.mousePos.X, r.mousePos.Y))
	held := hovered && r.mouseDown[MouseButtonLeft]
	props := ButtonProps{ID: id, Tone: ButtonToneNeutral, Emphasis: ButtonEmphasisSoft, Disabled: disabled}
	input := Button_ResolveButtonInput(props, Activation{Hovered: hovered, Pressed: held, Focused: focused})
	metrics := r.themeMetrics()
	motion := r.Button_AdvanceButtonMotion(uint64(uint32(id)), props, input,
		Surface_DefaultMotionEnabled(), r.frameDeltaMS,
		metrics.TransitionNormalMS, metrics.TransitionFastMS)
	appearance := resolveButtonFrame(r.theme(), r.effectiveDark(), r.activeTheme, props,
		ButtonState(input.Interaction.State), true, motion.Hover.Value, motion.Press.Value, motion.Focus.Value)
	paint := unpackStyle(appearance.Value)
	frame := r.dropdownSurface(bounds, false, ButtonStateNormal)
	frame.Kind = FrameOpButton
	frame.ID = id
	frame.Color, frame.BorderColor, frame.TextColor = paint.Background, paint.Border, paint.Foreground
	frame.Radius, frame.Opacity = paint.Radius, paint.Opacity
	frame.FillStates = appearance.Fill
	frame.Disabled, frame.Focused, frame.Hovered, frame.Pressed = disabled, focused, hovered, held
	frame.MotionValid = true
	frame.HoverAmount, frame.PressAmount, frame.FocusAmount = motion.Hover.Value, motion.Press.Value, motion.Focus.Value
	r.record(frame)
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
