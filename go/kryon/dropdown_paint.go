package kryon

const (
	dropdownRolePanel     int32 = 2
	dropdownRoleOption    int32 = 26
	dropdownRoleScrollbar int32 = 27
)

func (r *runtime) dropdownRoleFrame(role int32, className int32) StyleFrame {
	props := ButtonProps{Tone: ButtonToneNeutral, Emphasis: ButtonEmphasisSoft, ClassName: className}
	return resolveMinimalControlRoleFrame(props, ButtonStateNormal, false, 0, 0, 0,
		StyleSheet_StyleKindDropdown(), role)
}

// Popup surfaces expose semantic roles; KSS owns the visual result.
func (r *runtime) dropdownStyle(role int32, selected bool, state ButtonState, className ...int32) Style {
	props := ButtonProps{Tone: ButtonToneNeutral, Emphasis: ButtonEmphasisSoft}
	if len(className) > 0 {
		props.ClassName = className[0]
	}
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
	accentProps := ButtonProps{Tone: ButtonToneAccent, Emphasis: ButtonEmphasisFilled, Selected: selected, ClassName: props.ClassName}
	accentState := state
	if selected && state != ButtonStateDisabled {
		accentState = ButtonStateSelected
	}
	accent := resolveButtonStyleForKind(r.theme(), r.effectiveDark(), r.activeTheme,
		accentProps, accentState, StyleSheet_StyleKindDropdown())
	return unpackStyle(Dropdown_Appearance(packStyle(base), packStyle(accent), role, int32(state), selected))
}

func (r *runtime) dropdownSurface(bounds Rectangle, role int32, selected bool, state ButtonState, className ...int32) FrameOp {
	paint := r.dropdownStyle(role, selected, state, className...)
	return FrameOp{Kind: FrameOpSurface, Bounds: bounds,
		Color: paint.Background, BorderColor: paint.Border, TextColor: paint.Foreground,
		FocusColor: paint.Focus, AmbientColor: r.appAmbientColor(),
		Radius: paint.Radius, BorderWidth: paint.BorderWidth, Opacity: paint.Opacity,
		Material: paint.Material, FillStates: styleFill(paint), FillStatesValid: true,
		Hovered: role == 2 && !selected && state == ButtonStateHover}
}

func (r *runtime) dropdownTrigger(id int32, bounds Rectangle, open, focused bool, className ...int32) Color {
	disabled := r.contentDisabled()
	hovered := !disabled && (open || pointInRect(r.mousePos.X, r.mousePos.Y, bounds) && !r.popupCaptures(r.mousePos.X, r.mousePos.Y))
	held := hovered && r.mouseDown[MouseButtonLeft]
	props := ButtonProps{Bounds: bounds, ID: id, Tone: ButtonToneNeutral, Emphasis: ButtonEmphasisSoft, Disabled: disabled}
	if len(className) > 0 {
		props.ClassName = className[0]
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

func (r *runtime) dropdownChevron(id int32, layout DropdownTriggerContent, size int32, open bool, color Color) {
	indicator := Dropdown_DropdownIndicatorFor(layout.IndicatorCenterX, layout.IndicatorCenterY, size, open)
	r.record(FrameOp{Kind: FrameOpLine, ID: id, Color: color,
		Bounds: Rectangle{X: float32(indicator.X1), Y: float32(indicator.Y1), Width: float32(indicator.X2 - indicator.X1), Height: float32(indicator.Y2 - indicator.Y1)}})
	r.record(FrameOp{Kind: FrameOpLine, ID: id, Color: color,
		Bounds: Rectangle{X: float32(indicator.X3), Y: float32(indicator.Y3), Width: float32(indicator.X4 - indicator.X3), Height: float32(indicator.Y4 - indicator.Y3)}})
}
