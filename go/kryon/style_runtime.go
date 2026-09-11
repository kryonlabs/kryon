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

func (r *runtime) MergeStyle(base, override Style) Style {
	_ = r
	return mergeStyle(base, override)
}

func (r *runtime) LightenColor(c Color, amount int32) Color {
	_ = r
	if amount < 0 {
		amount = 0
	}
	return adjustColorLightness(c, int(amount))
}

func (r *runtime) DarkenColor(c Color, amount int32) Color {
	_ = r
	if amount < 0 {
		amount = 0
	}
	return adjustColorLightness(c, -int(amount))
}

func adjustColorLightness(c Color, amount int) Color {
	h, s, l := colorToHSL(c)
	l = clampFloat(l+float64(amount)/255.0, 0, 1)
	return colorFromHSL(h, s, l, c.A)
}

func colorToHSL(c Color) (float64, float64, float64) {
	r := float64(c.R) / 255.0
	g := float64(c.G) / 255.0
	b := float64(c.B) / 255.0
	max := r
	if g > max {
		max = g
	}
	if b > max {
		max = b
	}
	min := r
	if g < min {
		min = g
	}
	if b < min {
		min = b
	}
	chroma := max - min
	l := (max + min) * 0.5
	if chroma <= 0 {
		return 0, 0, l
	}
	denom := 1.0 - (2.0*l - 1.0)
	if denom > 1.0 {
		denom = 2.0 - denom
	}
	s := chroma / denom
	var h float64
	switch max {
	case r:
		h = (g - b) / chroma
		if g < b {
			h += 6.0
		}
	case g:
		h = (b-r)/chroma + 2.0
	default:
		h = (r-g)/chroma + 4.0
	}
	return h / 6.0, s, l
}

func colorFromHSL(h, s, l float64, alpha uint8) Color {
	h = clampFloat(h, 0, 1)
	s = clampFloat(s, 0, 1)
	l = clampFloat(l, 0, 1)
	if s <= 0 {
		gray := unitToByte(l)
		return Color{gray, gray, gray, alpha}
	}
	var q float64
	if l < 0.5 {
		q = l * (1.0 + s)
	} else {
		q = l + s - l*s
	}
	p := 2.0*l - q
	return Color{
		unitToByte(hueToRGB(p, q, h+1.0/3.0)),
		unitToByte(hueToRGB(p, q, h)),
		unitToByte(hueToRGB(p, q, h-1.0/3.0)),
		alpha,
	}
}

func hueToRGB(p, q, t float64) float64 {
	if t < 0 {
		t += 1
	}
	if t > 1 {
		t -= 1
	}
	if t < 1.0/6.0 {
		return p + (q-p)*6.0*t
	}
	if t < 1.0/2.0 {
		return q
	}
	if t < 2.0/3.0 {
		return p + (q-p)*(2.0/3.0-t)*6.0
	}
	return p
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
