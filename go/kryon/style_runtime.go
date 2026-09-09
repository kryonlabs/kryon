package kryon

// Host adapters only; field presence and state selection live in style.kry.
func packStyle(value Style) StyleData {
	return StyleData{
		Fields:        value.Fields,
		Material:      int32(value.Material),
		Typeface:      value.Typeface,
		Background:    packRGBA(value.Background),
		BackgroundEnd: packRGBA(value.BackgroundEnd),
		Foreground:    packRGBA(value.Foreground),
		Border:        packRGBA(value.Border),
		Focus:         packRGBA(value.Focus),
		Radius:        value.Radius,
		BorderWidth:   value.BorderWidth,
		Opacity:       value.Opacity,
		PaddingX:      value.PaddingX,
		PaddingY:      value.PaddingY,
		Gap:           value.Gap,
		FontSize:      value.FontSize,
		IconSize:      value.IconSize,
		OffsetX:       value.ContentOffset.X,
		OffsetY:       value.ContentOffset.Y,
	}
}

func unpackStyle(value StyleData) Style {
	return Style{
		Material:      MaterialKind(value.Material),
		Typeface:      value.Typeface,
		Fields:        value.Fields,
		Background:    unpackRGBA(value.Background),
		BackgroundEnd: unpackRGBA(value.BackgroundEnd),
		Foreground:    unpackRGBA(value.Foreground),
		Border:        unpackRGBA(value.Border),
		Focus:         unpackRGBA(value.Focus),
		Radius:        value.Radius,
		BorderWidth:   value.BorderWidth,
		Opacity:       value.Opacity,
		PaddingX:      value.PaddingX,
		PaddingY:      value.PaddingY,
		Gap:           value.Gap,
		FontSize:      value.FontSize,
		IconSize:      value.IconSize,
		ContentOffset: Vector2{X: value.OffsetX, Y: value.OffsetY},
	}
}

func mergeStyle(base, override Style) Style {
	return unpackStyle(Style_MergeValues(packStyle(base), packStyle(override)))
}

func styleFill(value Style) FillStates {
	return Surface_FillState(value.Fields, packRGBA(value.Background), packRGBA(value.BackgroundEnd))
}

func packStyleStates(control ControlStyle) StyleStates {
	return StyleStates{
		Normal: packStyle(control.Normal), Hover: packStyle(control.Hover),
		Pressed: packStyle(control.Pressed), Focused: packStyle(control.Focused),
		Disabled: packStyle(control.Disabled), Loading: packStyle(control.Loading),
		Selected: packStyle(control.Selected),
	}
}

func resolveControlStyle(base Style, control ControlStyle, state ButtonState) Style {
	return unpackStyle(Style_ResolveValues(packStyle(base), packStyleStates(control), int32(state)))
}
