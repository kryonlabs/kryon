package kryon

func (r *runtime) Image(props ImageProps) {
	props.Bounds = r.layoutRect(props.Bounds)
	frame := simpleStyleFrameWithClassRole(ButtonToneNeutral, ButtonStateNormal,
		false, false, props.ClassName, StyleSheet_StyleKindImage(), StyleSheet_StyleAny())
	style := unpackStyle(frame.Value)
	if style.Fields&uint32(StyleBackground|StyleBorder|StyleRadius|StyleBorderWidth|StyleMaterial|StyleOpacity) != 0 {
		r.record(styleFrameRectOp(props.Bounds, Rectangle{}, StyleFrame{
			Value: packStyle(style),
			Fill:  styleFill(style),
		}))
	}
	tintStyle := unpackStyle(ResolveActiveStyle(StyleData{},
		StyleSheet_StyleDefaultFacts(StyleSheet_StyleKindImage()),
		int32(ButtonStateNormal)))
	if props.ClassName != 0 {
		facts := StyleSheet_StyleDefaultFacts(StyleSheet_StyleKindImage())
		facts.ClassName = props.ClassName
		tintStyle = unpackStyle(ResolveActiveStyle(StyleData{},
			facts, int32(ButtonStateNormal)))
	}
	tint := White
	if tintStyle.Fields&uint32(StyleForeground) != 0 {
		tint = tintStyle.Foreground
	}
	if tintStyle.Fields&uint32(StyleOpacity) != 0 && tintStyle.Opacity < 1 {
		tint = unpackRGBA(Surface_Opacity(packRGBA(tint), tintStyle.Opacity))
	}
	op := imageOperation(props, tint)
	op.Radius = style.Radius
	if props.AltText != "" {
		op.Semantic = SemanticImage
		op.Role = "img"
		op.AltText = props.AltText
	}
	r.record(op)
}
