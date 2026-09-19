package kryon

type iconActionProps struct {
	Bounds      Rectangle
	Icon        Texture2D
	IconType    int32
	IconSize    int32
	IconPadding int32
	FocusID     int32
	Disabled    bool
	StyleKind   int32
	Role        int32
	ClassName   int32
}

func (r *runtime) iconAction(props iconActionProps) bool {
	props.Bounds = r.layoutRect(props.Bounds)
	padding := props.IconPadding
	if padding <= 0 {
		padding = 4
	}
	size := props.IconSize
	if size <= 0 {
		availableW := int32(props.Bounds.Width) - padding*2
		availableH := int32(props.Bounds.Height) - padding*2
		size = availableW
		if availableH < size {
			size = availableH
		}
		if size < 1 {
			size = 1
		}
	}
	styleKind := props.StyleKind
	if styleKind == 0 {
		styleKind = StyleSheet_StyleKindButton()
	}
	role := props.Role
	if role == 0 {
		role = StyleSheet_StyleAny()
	}
	frame, pressed := r.surfaceButtonFrameForRoleKind(ButtonProps{
		Bounds:    props.Bounds,
		ID:        props.FocusID,
		Disabled:  props.Disabled,
		Tone:      ButtonToneNeutral,
		Emphasis:  ButtonEmphasisSoft,
		Size:      ControlSizeMedium,
		ClassName: props.ClassName,
	}, Rectangle{}, false, styleKind, role)
	r.record(frame)
	iconX := int32(props.Bounds.X) + (int32(props.Bounds.Width)-size)/2
	iconY := int32(props.Bounds.Y) + (int32(props.Bounds.Height)-size)/2
	iconType := props.IconType
	if iconType == 0 && props.Icon.ID != 0 {
		iconType = int32(props.Icon.ID)
	}
	r.Icon(props.FocusID, iconX, iconY, size, iconType, unpackRGBA(frame.Button.Foreground))
	return pressed
}

func (r *runtime) Toggle(props ToggleProps) bool {
	r.prepareAccessibility(props.ID, int32(WidgetKindToggle), !props.Disabled && props.Value != nil)
	if props.Value == nil {
		return false
	}
	bounds := props.Bounds
	hasLabels := props.OffLabel != "" || props.OnLabel != ""
	disabled := props.Disabled || r.contentDisabled()
	labelStyle := unpackStyle(simpleStyleFrameWithClassRole(ButtonToneNeutral, ButtonStateNormal,
		disabled, false, props.ClassName,
		StyleSheet_StyleKindToggle(), Toggle_ToggleLabelRole()).Value)
	labelFont, labelFontID := styleTextFace(labelStyle, Text16)
	offWidth := int32(runtimeTextWidthWithFont(props.OffLabel, labelFont, labelFontID))
	onWidth := int32(runtimeTextWidthWithFont(props.OnLabel, labelFont, labelFontID))
	checkedForMetrics := *props.Value != 0
	trackToneForMetrics := ButtonToneNeutral
	trackRoleForMetrics := Toggle_ToggleTrackRoleFor(checkedForMetrics, hasLabels)
	if checkedForMetrics && !hasLabels {
		trackToneForMetrics = ButtonToneAccent
	}
	trackFrameForMetrics := simpleStyleFrameWithClassRole(trackToneForMetrics, ButtonStateNormal,
		disabled, checkedForMetrics, props.ClassName, StyleSheet_StyleKindToggle(),
		trackRoleForMetrics)
	activeFrameForMetrics := simpleStyleFrameWithClassRole(ButtonToneAccent, ButtonStateNormal,
		disabled, checkedForMetrics, props.ClassName, StyleSheet_StyleKindToggle(), Toggle_ToggleFillRole())
	labelFrameForMetrics := simpleStyleFrameWithClassRole(ButtonToneNeutral, ButtonStateNormal,
		disabled, false, props.ClassName, StyleSheet_StyleKindToggle(), Toggle_ToggleLabelRole())
	thumbFrameForMetrics := simpleStyleFrameWithClassRole(trackToneForMetrics, ButtonStateNormal,
		disabled, checkedForMetrics, props.ClassName, StyleSheet_StyleKindToggleThumb(),
		StyleSheet_StyleAny())
	if minW := float32(Toggle_ToggleMinimumWidthForStyle(hasLabels, offWidth, onWidth,
		1, trackFrameForMetrics, activeFrameForMetrics, labelFrameForMetrics)); bounds.Width < minW {
		bounds.Width = minW
	}
	if minH := float32(Toggle_ToggleMinimumHeightForStyle(1, trackFrameForMetrics,
		thumbFrameForMetrics)); bounds.Height < minH {
		bounds.Height = minH
	}
	bounds = r.layoutRect(bounds)
	input := r.ReadActivation(bounds, props.ID, !disabled)
	toggle := Toggle_ToggleValueFor(*props.Value != 0, input.Activated,
		!disabled, props.Value != nil)
	if toggle.Changed {
		if toggle.Value {
			*props.Value = 1
		} else {
			*props.Value = 0
		}
	}
	checked := *props.Value != 0
	state := ButtonStateNormal
	if disabled {
		state = ButtonStateDisabled
	} else if input.Pressed {
		state = ButtonStatePressed
	} else if input.Focused {
		state = ButtonStateFocus
	} else if input.Hovered {
		state = ButtonStateHover
	}
	trackTone := ButtonToneNeutral
	trackRole := Toggle_ToggleTrackRoleFor(checked, hasLabels)
	if checked && !hasLabels {
		trackTone = ButtonToneAccent
	}
	labelFrame := simpleStyleFrameWithClassRole(ButtonToneNeutral, state, disabled,
		false, props.ClassName, StyleSheet_StyleKindToggle(), Toggle_ToggleLabelRole())
	paint := Toggle_TogglePaintFor(ToggleSpec{
		Bounds:    bounds,
		Checked:   checked,
		Enabled:   !disabled,
		Hovered:   input.Hovered,
		Pressed:   input.Pressed,
		Focused:   input.Focused,
		HasLabels: hasLabels,
		OffWidth:  offWidth,
		OnWidth:   onWidth,
		Font:      labelFont,
		Scale:     1,
		Track: simpleStyleFrameWithClassRole(trackTone, state, disabled, checked,
			props.ClassName, StyleSheet_StyleKindToggle(), trackRole),
		Active: simpleStyleFrameWithClassRole(ButtonToneAccent, state, disabled,
			checked, props.ClassName, StyleSheet_StyleKindToggle(), Toggle_ToggleFillRole()),
		Label: labelFrame,
		Thumb: simpleStyleFrameWithClassRole(trackTone, state, disabled, checked,
			props.ClassName, StyleSheet_StyleKindToggleThumb(), StyleSheet_StyleAny()),
	})
	if paint.HasLabels {
		labelStyle = unpackStyle(labelFrame.Value)
		labelFont, labelFontID = styleTextFace(labelStyle, Text16)
		labelColor := packRGBA(labelStyle.Foreground)
		if checked {
			paint.OffLabelColor = labelColor
		} else {
			paint.OnLabelColor = labelColor
		}
	}
	trackOp := styleFrameRectOp(paint.TrackBounds, Rectangle{}, paint.Track)
	trackOp.ID = props.ID
	trackOp.Role = "checkbox"
	trackOp.accessibilityKind = int32(WidgetKindToggle)
	trackOp.AccessibleBounds = bounds
	trackOp.AccessibleLabel = props.OffLabel
	if checked {
		trackOp.AccessibleLabel = props.OnLabel
	}
	trackOp.Focused = input.Focused
	trackOp.Hovered = input.Hovered
	trackOp.Pressed = input.Pressed
	trackOp.Selected = checked
	trackOp.Disabled = disabled
	if input.Focused {
		trackOp.BorderColor = trackOp.FocusColor
	}
	r.record(trackOp)
	if paint.HasLabels {
		activeOp := styleFrameRectOp(paint.ActiveBounds, paint.TrackBounds, paint.Active)
		activeOp.ID = props.ID
		activeOp.Pressed = input.Pressed
		activeOp.Selected = true
		activeOp.Disabled = disabled
		r.record(activeOp)
		r.record(FrameOp{Kind: FrameOpText, Bounds: paint.OffLabelBounds, Text: props.OffLabel, Color: unpackRGBA(paint.OffLabelColor), Opacity: labelStyle.Opacity, FontSize: labelFont, FontID: labelFontID, ID: props.ID, Disabled: disabled, Role: "presentation"})
		r.record(FrameOp{Kind: FrameOpText, Bounds: paint.OnLabelBounds, Text: props.OnLabel, Color: unpackRGBA(paint.OnLabelColor), Opacity: labelStyle.Opacity, FontSize: labelFont, FontID: labelFontID, ID: props.ID, Disabled: disabled, Role: "presentation"})
	} else {
		if input.Hovered && !disabled {
			r.record(FrameOp{Kind: FrameOpCircle, Bounds: circleBounds(paint.ThumbX, paint.ThumbY, paint.ThumbRadius+5), Color: unpackRGBA(paint.ThumbGlowColor), ID: props.ID, Hovered: true})
		}
		r.record(FrameOp{Kind: FrameOpCircle, Bounds: circleBounds(paint.ThumbX, paint.ThumbY+2, paint.ThumbRadius+1), Color: unpackRGBA(paint.ThumbShadowColor), ID: props.ID, Disabled: disabled})
		r.record(FrameOp{Kind: FrameOpCircle, Bounds: circleBounds(paint.ThumbX, paint.ThumbY, paint.ThumbRadius), Color: unpackRGBA(paint.ThumbFillColor), ID: props.ID, Selected: checked, Disabled: disabled})
		r.record(FrameOp{Kind: FrameOpCircle, Bounds: circleBounds(paint.ThumbX-3, paint.ThumbY-4, paint.ThumbRadius*0.45), Color: unpackRGBA(paint.ThumbHighlightColor), ID: props.ID, Disabled: disabled})
		edgeRadius := paint.ThumbRadius - 1
		if edgeRadius < 0 {
			edgeRadius = 0
		}
		r.record(FrameOp{Kind: FrameOpRing, Bounds: circleBounds(paint.ThumbX, paint.ThumbY, paint.ThumbRadius), Radius: edgeRadius, Color: unpackRGBA(paint.ThumbEdgeColor), ID: props.ID, Selected: checked, Disabled: disabled})
	}
	return input.Activated
}

func circleBounds(x, y, radius float32) Rectangle {
	return Rectangle{X: x - radius, Y: y - radius, Width: radius * 2, Height: radius * 2}
}
