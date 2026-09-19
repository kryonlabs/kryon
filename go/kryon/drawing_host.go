package kryon

import (
	"hash/fnv"
)

func (r *runtime) Scale(px int32) int32 { return px }

func (r *runtime) GetScreenWidth() int32 { return int32(r.config.Width) }

func (r *runtime) GetScreenHeight() int32 { return int32(r.config.Height) }

func (r *runtime) FancyEffectsEnabled() int32 { return 1 }

func (r *runtime) NewVector2(x, y any) Vector2 { return NewVector2(number32(x), number32(y)) }

func (r *runtime) Circle(centerX, centerY, radius int32, color Color) {
	r.record(FrameOp{Kind: FrameOpCircle, Bounds: Primitive_PrimitiveCircleBounds(centerX, centerY, radius), Color: color})
}

func (r *runtime) Ring(centerX, centerY, innerRadius, outerRadius int32, color Color) {
	r.record(FrameOp{Kind: FrameOpRing, Bounds: Primitive_PrimitiveRingBounds(centerX, centerY, outerRadius), Radius: float32(innerRadius), Color: color})
}

func (r *runtime) Box(bounds Rectangle, fill Color, border Color) {
	r.record(FrameOp{Kind: FrameOpRect,
		Bounds:      Primitive_PrimitiveRectBounds(int32(bounds.X), int32(bounds.Y), int32(bounds.Width), int32(bounds.Height)),
		Color:       fill,
		BorderColor: border})
}

func (r *runtime) Surface(bounds Rectangle, style Style) {
	style = mergeStyle(unpackStyle(defaultStyleFrame(StyleSheet_StyleKindSurface()).Value), style)
	r.record(FrameOp{Kind: FrameOpSurface, Bounds: bounds,
		Material: style.Material, FocusColor: style.Focus, AmbientColor: r.appAmbientColor(),
		BackgroundEnd: style.BackgroundEnd, HasBackgroundEnd: style.Fields&StyleBackgroundEnd != 0,
		Color: style.Background, BorderColor: style.Border, Radius: style.Radius,
		BorderWidth: style.BorderWidth, Opacity: style.Opacity, Fields: style.Fields})
}

func (r *runtime) RectGradientH(x, y, w, h int32, left, right Color) {
	r.record(FrameOp{
		Kind:           FrameOpRect,
		Bounds:         Primitive_PrimitiveRectBounds(x, y, w, h),
		Color:          left,
		SecondaryColor: right,
	})
}

func (r *runtime) Line(x1, y1, x2, y2 int32, color Color) {
	line := Primitive_PrimitiveLineFor(x1, y1, x2, y2)
	r.record(FrameOp{
		Kind:   FrameOpLine,
		Bounds: Rectangle{X: float32(line.X1), Y: float32(line.Y1), Width: float32(line.X2 - line.X1), Height: float32(line.Y2 - line.Y1)},
		Color:  color,
	})
}

func (r *runtime) Key(text string) KeyID { return Key(text) }

func (r *runtime) Fade(c Color, alpha float32) Color {
	if alpha < 0 {
		alpha = 0
	}
	if alpha > 1 {
		alpha = 1
	}
	c.A = uint8(float32(c.A) * alpha)
	return c
}

func (r *runtime) Bevel(x, y, w, h int32, light, dark Color) {
	lines := Bevel_BevelLinesFor(x, y, w, h)
	r.record(FrameOp{Kind: FrameOpLine, Bounds: lines.Top, Color: light})
	r.record(FrameOp{Kind: FrameOpLine, Bounds: lines.Left, Color: light})
	r.record(FrameOp{Kind: FrameOpLine, Bounds: lines.Bottom, Color: dark})
	r.record(FrameOp{Kind: FrameOpLine, Bounds: lines.Right, Color: dark})
}

func (r *runtime) Icon(id, x, y, size int32, iconType int32, tint Color) {
	layout := Icon_IconLayoutFor(x, y, size)
	if !layout.Drawable {
		return
	}
	r.record(FrameOp{
		Kind:     FrameOpIcon,
		Bounds:   layout.Bounds,
		Color:    tint,
		ID:       id,
		IconType: iconType,
		IconSize: float32(layout.Size),
	})
}

func (r *runtime) CanvasGrid(bounds Rectangle, step int32, color Color) {
	bounds = r.layoutRect(bounds)
	spacing := CanvasGrid_CanvasGridSpacing(r.Scale(step), 4)
	packed := packRGBA(color)
	for i, count := int32(0), CanvasGrid_CanvasGridLineCount(bounds.Width, spacing); i < count; i++ {
		line := CanvasGrid_CanvasGridVerticalLine(bounds, i, spacing, packed)
		r.record(FrameOp{Kind: FrameOpLine, Bounds: line.Bounds, Color: unpackRGBA(line.Color)})
	}
	for i, count := int32(0), CanvasGrid_CanvasGridLineCount(bounds.Height, spacing); i < count; i++ {
		line := CanvasGrid_CanvasGridHorizontalLine(bounds, i, spacing, packed)
		r.record(FrameOp{Kind: FrameOpLine, Bounds: line.Bounds, Color: unpackRGBA(line.Color)})
	}
}

func Key(text string) KeyID {
	h := fnv.New64a()
	_, _ = h.Write([]byte(text))
	return KeyID(h.Sum64())
}
