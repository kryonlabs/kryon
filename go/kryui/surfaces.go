package kryui

/*
#cgo CFLAGS: -I${SRCDIR}/../../include
#include <ui_core.h>
*/
import "C"

// UIPanelProps describes a passive themed surface.
type UIPanelProps struct {
	Bounds      Rectangle
	Fill        Color
	Border      Color
	Radius      float32
	BorderWidth float32
}

// UICardProps describes a clickable themed surface. Zero colors use the
// active Kryon material scheme, which itself follows the current theme source.
type UICardProps struct {
	Bounds      Rectangle
	Fill        Color
	HoverFill   Color
	Border      Color
	Radius      float32
	BorderWidth float32
	Disabled    bool
	Selected    bool
}

// UIBadgeProps describes a compact label badge.
type UIBadgeProps struct {
	Bounds     Rectangle
	Label      string
	Accent     Color
	Background Color
	Text       Color
	Font       int32
	PaddingX   int32
	PaddingY   int32
	Radius     float32
}

func colorZero(c Color) bool { return c.A == 0 && c.R == 0 && c.G == 0 && c.B == 0 }

func uiRoundness(bounds Rectangle, radius float32) float32 {
	if radius <= 0 || bounds.Width <= 0 || bounds.Height <= 0 {
		return 0
	}
	minSide := bounds.Width
	if bounds.Height < minSide {
		minSide = bounds.Height
	}
	r := float32(ScaleUIPx(int32(radius))) / minSide
	if r > 0.5 {
		return 0.5
	}
	return r
}

func drawUIRoundedFill(bounds Rectangle, radius float32, fill Color) {
	if radius > 0 {
		DrawRectangleRounded(bounds, uiRoundness(bounds, radius), 12, fill)
		return
	}
	DrawRectangleRec(bounds, fill)
}

func drawUIRoundedBorder(bounds Rectangle, radius, width float32, border Color) {
	if width <= 0 {
		width = 1
	}
	if radius > 0 {
		DrawRectangleRoundedLinesEx(bounds, uiRoundness(bounds, radius), 12, width, border)
		return
	}
	DrawRectangleLinesEx(bounds, width, border)
}

// DrawUIPanel draws a passive surface using the active Kryon theme by default.
func DrawUIPanel(props UIPanelProps) {
	scheme := GetUIMaterialScheme()
	tokens := GetUIStyleTokens()
	fill := props.Fill
	if colorZero(fill) {
		fill = scheme.SurfaceContainer
	}
	border := props.Border
	if colorZero(border) {
		border = scheme.Outline
	}
	radius := props.Radius
	if radius == 0 {
		radius = tokens.PanelRadius
	}
	drawUIRoundedFill(props.Bounds, radius, fill)
	drawUIRoundedBorder(props.Bounds, radius, props.BorderWidth, border)
}

// DrawUICard draws a themed card and returns hover/click state.
func DrawUICard(props UICardProps) (hovered, clicked bool) {
	scheme := GetUIMaterialScheme()
	tokens := GetUIStyleTokens()
	fill := props.Fill
	if colorZero(fill) {
		fill = scheme.Surface
	}
	hoverFill := props.HoverFill
	if colorZero(hoverFill) {
		hoverFill = scheme.SurfaceVariant
	}
	border := props.Border
	if colorZero(border) {
		border = scheme.Outline
	}
	disabled := C.int(0)
	if props.Disabled {
		disabled = 1
	}
	var hover C.int
	clicked = C.UIHandleClick(props.Bounds.toC(), disabled, &hover) != 0
	hovered = hover != 0

	drawFill := fill
	if hovered && !props.Disabled {
		drawFill = hoverFill
	}
	radius := props.Radius
	if radius == 0 {
		radius = tokens.PanelRadius
	}
	drawUIRoundedFill(props.Bounds, radius, drawFill)
	width := props.BorderWidth
	if props.Selected {
		border = scheme.Primary
		width = 2
	}
	drawUIRoundedBorder(props.Bounds, radius, width, border)
	return hovered, clicked
}

// MeasureUIBadge returns the bounds needed for a badge label.
func MeasureUIBadge(props UIBadgeProps) Rectangle {
	font := props.Font
	if font == 0 {
		font = UIText12
	}
	padX := props.PaddingX
	if padX == 0 {
		padX = ScaleUIPx(6)
	}
	padY := props.PaddingY
	if padY == 0 {
		padY = ScaleUIPx(4)
	}
	w := float32(MeasureUIText(props.Label, font) + padX*2)
	h := float32(font + padY*2)
	bounds := props.Bounds
	if bounds.Width == 0 {
		bounds.Width = w
	}
	if bounds.Height == 0 {
		bounds.Height = h
	}
	return bounds
}

// DrawUIBadge draws a compact themed label and returns the drawn bounds.
func DrawUIBadge(props UIBadgeProps) Rectangle {
	scheme := GetUIMaterialScheme()
	tokens := GetUIStyleTokens()
	accent := props.Accent
	if colorZero(accent) {
		accent = scheme.Secondary
	}
	background := props.Background
	if colorZero(background) {
		background = ColorLerp(scheme.SurfaceContainer, accent, 0.16)
	}
	text := props.Text
	if colorZero(text) {
		text = accent
	}
	font := props.Font
	if font == 0 {
		font = UIText12
	}
	padX := props.PaddingX
	if padX == 0 {
		padX = ScaleUIPx(6)
	}
	padY := props.PaddingY
	if padY == 0 {
		padY = ScaleUIPx(4)
	}
	radius := props.Radius
	if radius == 0 {
		radius = tokens.ControlRadius
	}
	bounds := MeasureUIBadge(props)
	drawUIRoundedFill(bounds, radius, background)
	DrawUIText(props.Label, int32(bounds.X)+padX, int32(bounds.Y)+padY, font, text)
	return bounds
}
