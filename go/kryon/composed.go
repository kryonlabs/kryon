package kryon

type CarouselControlsProps struct {
	Bounds, Indicators    Rectangle
	Count, Selected, Move int32
	Disabled              bool
	ID                    int32
}

func CarouselControls(p CarouselControlsProps) int32 {
	if p.Count <= 0 {
		return -1
	}
	s := (p.Selected%p.Count + p.Count) % p.Count
	if p.Count == 1 {
		return s
	}
	if !p.Disabled {
		s = (s + p.Move%p.Count + p.Count) % p.Count
	}
	hit, inset, icon := ScaleUIPx(48), ScaleUIPx(12), ScaleUIPx(10)
	if p.Bounds.Width >= float32(hit*2) && p.Bounds.Height >= float32(hit) {
		for i := int32(0); i < 2; i++ {
			x := p.Bounds.X + float32(inset)
			if i == 1 {
				x = p.Bounds.X + p.Bounds.Width - float32(inset+hit)
			}
			b := Rectangle{X: x, Y: p.Bounds.Y + (p.Bounds.Height-float32(hit))/2, Width: float32(hit), Height: float32(hit)}
			cx, cy := int32(b.X)+hit/2, int32(b.Y)+hit/2
			fg := Color{255, 255, 255, 255}
			if p.Disabled {
				fg.A = 89
			}
			DrawCircleV(Vector2{X: float32(cx), Y: float32(cy)}, hit/2, Color{0, 0, 0, 96})
			d := int32(-1)
			if i == 1 {
				d = 1
			}
			Line(cx-d*icon/2, cy-icon, cx+d*icon/2, cy, fg)
			Line(cx+d*icon/2, cy, cx-d*icon/2, cy+icon, fg)
			if InvisibleButton(InvisibleButtonProps{Bounds: b, ID: p.ID + i, Disabled: p.Disabled}) {
				s = (s + d + p.Count) % p.Count
			}
		}
	}
	if p.Indicators.Width >= float32(hit*p.Count) && p.Indicators.Height >= float32(hit) {
		x := p.Indicators.X + (p.Indicators.Width-float32(hit*p.Count))/2
		y := p.Indicators.Y + (p.Indicators.Height-float32(hit))/2
		for i := int32(0); i < p.Count; i++ {
			b := Rectangle{X: x + float32(i*hit), Y: y, Width: float32(hit), Height: float32(hit)}
			if InvisibleButton(InvisibleButtonProps{Bounds: b, ID: p.ID + 2 + i, Disabled: p.Disabled}) {
				s = i
			}
			c := GetThemeText()
			r := int32(3)
			c.A = 71
			if i == s {
				r = 4
				c.A = 255
			}
			if p.Disabled {
				c.A = 51
			}
			DrawCircleV(Vector2{X: b.X + float32(hit)/2, Y: b.Y + float32(hit)/2}, ScaleUIPx(r), c)
		}
	}
	return s
}
