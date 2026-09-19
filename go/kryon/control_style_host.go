package kryon

func styleLength(value float32) int32 {
	if value <= 0 {
		return 0
	}
	return int32(value + 0.5)
}

func styleForClassKind(className int32, kind int32) Style {
	facts := StyleSheet_StyleDefaultFacts(kind)
	facts.ClassName = className
	facts.State = int32(ButtonStateNormal)
	return unpackStyle(ResolveActiveStyle(packStyle(Style{Fields: uint32(StyleGap | StylePaddingX | StylePaddingY)}),
		facts, int32(ButtonStateNormal)))
}

func defaultStyleFrame(styleKind int32) StyleFrame {
	value := ResolveActiveStyle(StyleData{}, StyleSheet_StyleDefaultFacts(styleKind), int32(ButtonStateNormal))
	value.Opacity = Style_StyleOpacityValue(value.Fields, value.Opacity)
	return StyleFrame{Value: value, Fill: Surface_FillState(value.Fields, value.Background, value.BackgroundEnd)}
}

func styleMetricFrame(className int32, styleKind int32, role int32) StyleFrame {
	facts := StyleSheet_StyleDefaultFacts(styleKind)
	facts.ClassName = className
	facts.Role = role
	facts.State = int32(ButtonStateNormal)
	value := ResolveActiveStyle(StyleData{}, facts, int32(ButtonStateNormal))
	return StyleFrame{Value: value, Fill: Surface_FillState(value.Fields, value.Background, value.BackgroundEnd)}
}

func defaultTextStyle(font int32) Style {
	value := ResolveActiveStyle(packStyle(Style{Fields: uint32(StyleFontSize | StyleOpacity), FontSize: float32(font), Opacity: 1}),
		StyleSheet_StyleTextFacts(0, 0, StyleSheet_StyleKindText(), int32(ButtonStateNormal)),
		int32(ButtonStateNormal))
	return unpackStyle(value)
}

func defaultTextStyleForKind(font int32, kind int32) Style {
	value := ResolveActiveStyle(packStyle(Style{Fields: uint32(StyleFontSize | StyleOpacity), FontSize: float32(font), Opacity: 1}),
		StyleSheet_StyleDefaultFacts(kind),
		int32(ButtonStateNormal))
	return unpackStyle(value)
}

func defaultTextStyleForClassKind(font int32, className int32, kind int32) Style {
	facts := StyleSheet_StyleDefaultFacts(kind)
	facts.ClassName = className
	facts.State = int32(ButtonStateNormal)
	value := ResolveActiveStyle(packStyle(Style{Fields: uint32(StyleFontSize | StyleOpacity), FontSize: float32(font), Opacity: 1}),
		facts,
		int32(ButtonStateNormal))
	return unpackStyle(value)
}

func styleFont(style Style, fallback int32) int32 {
	font := int32(style.FontSize)
	if font > 0 {
		return font
	}
	return fallback
}

func styleFontID(style Style) uint32 {
	if style.Fields&StyleTypeface == 0 {
		return 0
	}
	return registeredTypeface(style.Typeface)
}

func styleTextFace(style Style, fallback int32) (int32, uint32) {
	return styleFont(style, fallback), styleFontID(style)
}

func styleTextFaceWithFallback(style Style, fallbackFont int32, fallbackID uint32) (int32, uint32) {
	font := styleFont(style, fallbackFont)
	fontID := fallbackID
	if style.Fields&StyleTypeface != 0 {
		fontID = styleFontID(style)
	}
	return font, fontID
}

func simpleStyleFrameWithRole(tone ButtonTone, state ButtonState, disabled, selected bool, styleKind int32, role int32) StyleFrame {
	return simpleStyleFrameWithClassRole(tone, state, disabled, selected, 0,
		styleKind, role)
}

func simpleStyleFrameWithClassRole(tone ButtonTone, state ButtonState, disabled, selected bool, className int32, styleKind int32, role int32) StyleFrame {
	props := ButtonProps{
		ClassName: className,
		Tone:      tone,
		Emphasis:  ButtonEmphasisSoft,
		Size:      ControlSizeMedium,
		Pill:      true,
		Disabled:  disabled,
		Selected:  selected,
	}
	if tone == ButtonToneAccent {
		props.Emphasis = ButtonEmphasisFilled
	}
	return resolveMinimalControlRoleFrame(props, state, false, 0, 0, 0, styleKind, role)
}

func styleFrameRectOp(bounds, surface Rectangle, frame StyleFrame) FrameOp {
	style := unpackStyle(frame.Value)
	return FrameOp{
		Kind:             FrameOpRect,
		Bounds:           bounds,
		SurfaceBounds:    surface,
		Color:            style.Background,
		BackgroundEnd:    style.BackgroundEnd,
		HasBackgroundEnd: frame.Value.Fields&uint32(StyleBackgroundEnd) != 0,
		FillStates:       frame.Fill,
		FillStatesValid:  true,
		BorderColor:      style.Border,
		FocusColor:       style.Focus,
		TextColor:        style.Foreground,
		Radius:           style.Radius,
		BorderWidth:      style.BorderWidth,
		Opacity:          style.Opacity,
		Material:         style.Material,
		Fields:           frame.Value.Fields,
	}
}
