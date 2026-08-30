package kryon

import (
	"image"
	"image/color"
	"image/draw"
	"math"
	"strings"
	"unicode/utf8"
)

// RenderFrame paints a native Go frame operation stream into an RGBA image.
// It is intentionally dependency-free so Go hosts can render generated Kryon
// frames without cgo or the legacy C bridge.
func RenderFrame(width, height int, ops []FrameOp) *image.RGBA {
	if width <= 0 {
		width = 1
	}
	if height <= 0 {
		height = 1
	}
	img := image.NewRGBA(image.Rect(0, 0, width, height))
	RenderFrameInto(img, ops)
	return img
}

// RenderFrameInto renders into an existing image (cleared first). Hosts that
// present every frame pass the same image back in instead of allocating ~4MB
// per frame; the image is only replaced when the window is resized.
func RenderFrameInto(img *image.RGBA, ops []FrameOp) {
	if img == nil || img.Bounds().Dx() <= 0 || img.Bounds().Dy() <= 0 {
		return
	}
	fillImage(img, RAYWHITE)
	for _, op := range ops {
		switch op.Kind {
		case FrameOpBackground:
			fillImage(img, opaque(op.Color, RAYWHITE))
		case FrameOpRect:
			if op.SecondaryColor.A != 0 {
				fillGradientH(img, op.Bounds, opaque(op.Color, LIGHTGRAY), opaque(op.SecondaryColor, LIGHTGRAY))
			} else {
				fillRect(img, op.Bounds, opaque(op.Color, LIGHTGRAY))
			}
		case FrameOpLine:
			drawLine(img, op.Bounds, opaque(op.Color, BLACK))
		case FrameOpText:
			drawText(img, op.Text, int(round(op.Bounds.X)), int(round(op.Bounds.Y)), op.FontSize, opaque(op.Color, BLACK), op.FontID)
		case FrameOpButton:
			renderButton(img, op)
		case FrameOpIcon:
			renderIcon(img, op)
		case FrameOpPicture:
			fillRect(img, op.Bounds, opaque(op.Color, Color{224, 229, 236, 255}))
			strokeRect(img, op.Bounds, Color{136, 146, 160, 255})
		case FrameOpTextField, FrameOpTextArea:
			renderTextInput(img, op)
		case FrameOpColumn, FrameOpRow, FrameOpStack, FrameOpScreen, FrameOpGrid, FrameOpPage, FrameOpSection:
			strokeRect(img, op.Bounds, Color{220, 224, 229, 255})
		}
	}

}

// RenderCurrentFrame paints the active runtime's latest frame operation stream.
func RenderCurrentFrame() *image.RGBA {
	rt := active()
	return RenderFrame(int(rt.GetScreenWidth()), int(rt.GetScreenHeight()), FrameOps())
}

func renderButton(img *image.RGBA, op FrameOp) {
	fill := Color{236, 240, 245, 255}
	border := Color{136, 146, 160, 255}
	text := Color{28, 36, 48, 255}
	if op.Color.A != 0 {
		fill = op.Color
	}
	if op.BorderColor.A != 0 {
		border = op.BorderColor
	}
	if op.TextColor.A != 0 {
		text = op.TextColor
	}
	if op.Disabled {
		if op.Color.A == 0 {
			fill = Color{226, 228, 232, 255}
		}
		if op.BorderColor.A == 0 {
			border = Color{174, 181, 190, 255}
		}
		if op.TextColor.A == 0 {
			text = Color{126, 134, 146, 255}
		}
	} else if op.Pressed {
		if op.Color.A == 0 {
			fill = Color{198, 217, 246, 255}
		}
		if op.BorderColor.A == 0 {
			border = Color{58, 110, 190, 255}
		}
	}
	fillRect(img, op.Bounds, fill)
	strokeRect(img, op.Bounds, border)
	drawTextInBox(img, op.Text, op.Bounds, op.FontSize, text, op.FontID)
}

func renderIcon(img *image.RGBA, op FrameOp) {
	tint := BLACK
	if op.Color.A != 0 {
		tint = op.Color
	}
	size := int(round(op.Bounds.Width))
	if op.IconSize > 0 {
		size = int(op.IconSize)
	}
	if size <= 0 {
		size = 16
	}
	rows := iconPattern(op.IconType)
	if len(rows) == 0 {
		rows = iconPattern(UIIconTypeText)
	}
	cell := maxInt(1, size/16)
	x0 := int(round(op.Bounds.X)) + maxInt(0, (size-cell*16)/2)
	y0 := int(round(op.Bounds.Y)) + maxInt(0, (size-cell*16)/2)
	for y, row := range rows {
		for x, on := range row {
			if on == ' ' || on == '.' {
				continue
			}
			fillRectPixels(img, x0+x*cell, y0+y*cell, cell, cell, tint)
		}
	}
}

func iconPattern(iconType int32) []string {
	switch iconType {
	case UIIconTypeSave:
		return []string{
			"................",
			"..############..",
			".##############.",
			".###......#####.",
			".###......#####.",
			".##############.",
			".##############.",
			".####......####.",
			".###........###.",
			".###........###.",
			".###........###.",
			".###........###.",
			".##############.",
			".##############.",
			"................",
			"................",
		}
	case UIIconTypePlus:
		return []string{
			"................",
			"................",
			".......##.......",
			".......##.......",
			".......##.......",
			".......##.......",
			"..############..",
			"..############..",
			".......##.......",
			".......##.......",
			".......##.......",
			".......##.......",
			"................",
			"................",
			"................",
			"................",
		}
	case UIIconTypeTrash:
		return []string{
			"................",
			".....######.....",
			"...##########...",
			"...##......##...",
			"..############..",
			"....########....",
			"....##.##.##....",
			"....##.##.##....",
			"....##.##.##....",
			"....##.##.##....",
			"....##.##.##....",
			"....##.##.##....",
			"....########....",
			".....######.....",
			"................",
			"................",
		}
	case UIIconTypeX, UIIconTypeWorkbookClearFormatting:
		return []string{
			"................",
			"..##........##..",
			"...##......##...",
			"....##....##....",
			".....##..##.....",
			"......####......",
			".......##.......",
			"......####......",
			".....##..##.....",
			"....##....##....",
			"...##......##...",
			"..##........##..",
			"................",
			"................",
			"................",
			"................",
		}
	case UIIconTypeWorkbookFillColor:
		return []string{
			"................",
			".....####.......",
			"....######......",
			"...###..###.....",
			"..###....###....",
			".###......###...",
			"..###....###....",
			"...###..###.....",
			"....######......",
			".....####.......",
			"................",
			"..############..",
			"..############..",
			"..############..",
			"................",
			"................",
		}
	case UIIconTypeWorkbookTextColor, UIIconTypeText:
		return []string{
			"................",
			"......####......",
			".....######.....",
			"....###..###....",
			"....##....##....",
			"...##########...",
			"...##########...",
			"..###......###..",
			"..##........##..",
			".####......####.",
			"................",
			"..############..",
			"..############..",
			"..############..",
			"................",
			"................",
		}
	case UIIconTypeEdit, UIIconTypePencil:
		return []string{
			"............##..",
			"...........####.",
			"..........######",
			".........######.",
			"........######..",
			".......######...",
			"......######....",
			".....######.....",
			"....######......",
			"...######.......",
			"..######........",
			".######.........",
			".####...........",
			"..##............",
			"................",
			"................",
		}
	default:
		return nil
	}
}

func renderTextInput(img *image.RGBA, op FrameOp) {
	fill := WHITE
	if op.Color.A != 0 {
		fill = op.Color
	}
	fillRect(img, op.Bounds, fill)
	border := Color{144, 152, 164, 255}
	if op.BorderColor.A != 0 {
		border = op.BorderColor
	}
	if op.Focused {
		if op.BorderColor.A == 0 {
			border = Color{29, 96, 196, 255}
		}
	}
	strokeRect(img, op.Bounds, border)
	x := int(round(op.Bounds.X)) + 8
	y := int(round(op.Bounds.Y)) + maxInt(3, (int(round(op.Bounds.Height))-int(textHeight(op.FontSize, op.FontID)))/2)
	text := BLACK
	if op.TextColor.A != 0 {
		text = op.TextColor
	}
	if op.SelectionStart != op.SelectionEnd {
		start, end := orderedInt32(op.SelectionStart, op.SelectionEnd)
		sx := x + textAdvance(op.Text, start, op.FontSize, op.FontID)
		ex := x + textAdvance(op.Text, end, op.FontSize, op.FontID)
		selection := Color{58, 110, 190, 255}
		if op.SelectionColor.A != 0 {
			selection = op.SelectionColor
		}
		fillRectPixels(img, sx, int(round(op.Bounds.Y))+3, maxInt(1, ex-sx), maxInt(1, int(round(op.Bounds.Height))-6), selection)
	}
	drawText(img, op.Text, x, y, op.FontSize, text, op.FontID)
	if op.SelectionStart != op.SelectionEnd {
		start, end := orderedInt32(op.SelectionStart, op.SelectionEnd)
		sx := x + textAdvance(op.Text, start, op.FontSize, op.FontID)
		selected := text
		if op.SelectedTextColor.A != 0 {
			selected = op.SelectedTextColor
		}
		drawText(img, sliceTextByByteCursor(op.Text, start, end), sx, y, op.FontSize, selected, op.FontID)
	}
	if op.Focused {
		cursorX := x + textAdvance(op.Text, op.Cursor, op.FontSize, op.FontID)
		top := int(round(op.Bounds.Y)) + 5
		bottom := int(round(op.Bounds.Y+op.Bounds.Height)) - 5
		cursor := border
		if op.CursorColor.A != 0 {
			cursor = op.CursorColor
		}
		drawVertical(img, cursorX, top, bottom, cursor)
	}
}

func fillImage(img *image.RGBA, c Color) {
	draw.Draw(img, img.Bounds(), &image.Uniform{C: rgba(c)}, image.Point{}, draw.Src)
}

func fillRect(img *image.RGBA, r Rectangle, c Color) {
	draw.Draw(img, clipRect(img, r), &image.Uniform{C: rgba(c)}, image.Point{}, draw.Src)
}

func fillGradientH(img *image.RGBA, r Rectangle, left, right Color) {
	rect := clipRect(img, r)
	if rect.Empty() {
		return
	}
	denom := maxInt(1, rect.Dx()-1)
	for x := rect.Min.X; x < rect.Max.X; x++ {
		t := float32(x-rect.Min.X) / float32(denom)
		c := lerpColor(left, right, t)
		for y := rect.Min.Y; y < rect.Max.Y; y++ {
			img.SetRGBA(x, y, rgba(c))
		}
	}
}

func strokeRect(img *image.RGBA, r Rectangle, c Color) {
	rect := clipRect(img, r)
	if rect.Empty() {
		return
	}
	for x := rect.Min.X; x < rect.Max.X; x++ {
		setPixel(img, x, rect.Min.Y, c)
		setPixel(img, x, rect.Max.Y-1, c)
	}
	for y := rect.Min.Y; y < rect.Max.Y; y++ {
		setPixel(img, rect.Min.X, y, c)
		setPixel(img, rect.Max.X-1, y, c)
	}
}

func drawLine(img *image.RGBA, r Rectangle, c Color) {
	x0 := int(round(r.X))
	y0 := int(round(r.Y))
	x1 := int(round(r.X + r.Width))
	y1 := int(round(r.Y + r.Height))
	dx := absInt(x1 - x0)
	sx := -1
	if x0 < x1 {
		sx = 1
	}
	dy := -absInt(y1 - y0)
	sy := -1
	if y0 < y1 {
		sy = 1
	}
	err := dx + dy
	for {
		setPixel(img, x0, y0, c)
		if x0 == x1 && y0 == y1 {
			return
		}
		e2 := 2 * err
		if e2 >= dy {
			err += dy
			x0 += sx
		}
		if e2 <= dx {
			err += dx
			y0 += sy
		}
	}
}

func drawVertical(img *image.RGBA, x, y0, y1 int, c Color) {
	if y1 < y0 {
		y0, y1 = y1, y0
	}
	for y := y0; y <= y1; y++ {
		setPixel(img, x, y, c)
	}
}

func drawTextInBox(img *image.RGBA, text string, bounds Rectangle, fontSize int32, c Color, fontID uint32) {
	x := int(round(bounds.X)) + 8
	y := int(round(bounds.Y)) + maxInt(3, (int(round(bounds.Height))-int(textHeight(fontSize, fontID)))/2)
	drawText(img, text, x, y, fontSize, c, fontID)
}

func drawText(img *image.RGBA, text string, x, y int, fontSize int32, c Color, fontID uint32) {
	if drawFontText(img, text, x, y, fontSize, c, fontID) {
		return
	}
	scale := glyphScale(fontSize)
	cursor := x
	for _, r := range text {
		if r == '\n' {
			cursor = x
			y += 8 * scale
			continue
		}
		pattern, ok := glyphPattern(r)
		if !ok {
			cursor += 6 * scale
			continue
		}
		for gy, row := range pattern {
			for gx, on := range row {
				if on != '1' {
					continue
				}
				fillRectPixels(img, cursor+gx*scale, y+gy*scale, scale, scale, c)
			}
		}
		cursor += 6 * scale
	}
}

func textAdvance(text string, cursor int32, fontSize int32, fontID uint32) int {
	if advance, ok := fontTextAdvance(text, cursor, fontSize, fontID); ok {
		return advance
	}
	if cursor < 0 {
		cursor = 0
	}
	return byteCursorRuneCount(text, int(cursor)) * 6 * glyphScale(fontSize)
}

func orderedInt32(a, b int32) (int32, int32) {
	if a < b {
		return a, b
	}
	return b, a
}

func sliceTextByByteCursor(text string, start, end int32) string {
	s := clampByteCursor(text, int(start))
	e := clampByteCursor(text, int(end))
	if s > e {
		s, e = e, s
	}
	return text[s:e]
}

func byteCursorRuneCount(text string, cursor int) int {
	cursor = clampByteCursor(text, cursor)
	return len([]rune(text[:cursor]))
}

func clampByteCursor(text string, cursor int) int {
	if cursor < 0 {
		return 0
	}
	if cursor > len(text) {
		return len(text)
	}
	for cursor > 0 && cursor < len(text) && !utf8.RuneStart(text[cursor]) {
		cursor--
	}
	return cursor
}

func textHeight(fontSize int32, fontID uint32) int32 {
	if height, ok := fontTextHeight(fontSize, fontID); ok {
		return height
	}
	return int32(7 * glyphScale(fontSize))
}

func glyphScale(fontSize int32) int {
	if fontSize <= 0 {
		fontSize = Text16
	}
	return maxInt(1, int(fontSize)/8)
}

func glyphPattern(r rune) ([7]string, bool) {
	if r >= 'a' && r <= 'z' {
		r -= 'a' - 'A'
	}
	p, ok := glyphs[r]
	return p, ok
}

func fillRectPixels(img *image.RGBA, x, y, w, h int, c Color) {
	if w <= 0 || h <= 0 {
		return
	}
	draw.Draw(img, image.Rect(x, y, x+w, y+h).Intersect(img.Bounds()), &image.Uniform{C: rgba(c)}, image.Point{}, draw.Src)
}

func clipRect(img *image.RGBA, r Rectangle) image.Rectangle {
	x0 := int(math.Floor(float64(r.X)))
	y0 := int(math.Floor(float64(r.Y)))
	x1 := int(math.Ceil(float64(r.X + r.Width)))
	y1 := int(math.Ceil(float64(r.Y + r.Height)))
	return image.Rect(x0, y0, x1, y1).Intersect(img.Bounds())
}

func setPixel(img *image.RGBA, x, y int, c Color) {
	if image.Pt(x, y).In(img.Bounds()) {
		img.SetRGBA(x, y, rgba(c))
	}
}

func rgba(c Color) color.RGBA {
	return color.RGBA{R: c.R, G: c.G, B: c.B, A: c.A}
}

func opaque(c, fallback Color) Color {
	if c.A == 0 {
		return fallback
	}
	return c
}

func lerpColor(a, b Color, t float32) Color {
	return Color{
		R: uint8(float32(a.R) + (float32(b.R)-float32(a.R))*t),
		G: uint8(float32(a.G) + (float32(b.G)-float32(a.G))*t),
		B: uint8(float32(a.B) + (float32(b.B)-float32(a.B))*t),
		A: uint8(float32(a.A) + (float32(b.A)-float32(a.A))*t),
	}
}

func round(v float32) float32 {
	if v < 0 {
		return float32(math.Ceil(float64(v - 0.5)))
	}
	return float32(math.Floor(float64(v + 0.5)))
}

func absInt(v int) int {
	if v < 0 {
		return -v
	}
	return v
}

func normalizeGlyph(rows ...string) [7]string {
	var out [7]string
	for i := 0; i < len(out) && i < len(rows); i++ {
		out[i] = strings.ReplaceAll(rows[i], "#", "1")
	}
	return out
}

var glyphs = map[rune][7]string{
	' ':  normalizeGlyph("00000", "00000", "00000", "00000", "00000", "00000", "00000"),
	'!':  normalizeGlyph("00100", "00100", "00100", "00100", "00100", "00000", "00100"),
	'"':  normalizeGlyph("01010", "01010", "01010", "00000", "00000", "00000", "00000"),
	'#':  normalizeGlyph("01010", "01010", "11111", "01010", "11111", "01010", "01010"),
	'$':  normalizeGlyph("00100", "01111", "10100", "01110", "00101", "11110", "00100"),
	'%':  normalizeGlyph("11001", "11010", "00010", "00100", "01000", "01011", "10011"),
	'&':  normalizeGlyph("01100", "10010", "10100", "01000", "10101", "10010", "01101"),
	'\'': normalizeGlyph("00100", "00100", "01000", "00000", "00000", "00000", "00000"),
	'(':  normalizeGlyph("00010", "00100", "01000", "01000", "01000", "00100", "00010"),
	')':  normalizeGlyph("01000", "00100", "00010", "00010", "00010", "00100", "01000"),
	'*':  normalizeGlyph("00000", "00100", "10101", "01110", "10101", "00100", "00000"),
	'+':  normalizeGlyph("00000", "00100", "00100", "11111", "00100", "00100", "00000"),
	',':  normalizeGlyph("00000", "00000", "00000", "00000", "00100", "00100", "01000"),
	'-':  normalizeGlyph("00000", "00000", "00000", "11111", "00000", "00000", "00000"),
	'.':  normalizeGlyph("00000", "00000", "00000", "00000", "00000", "00100", "00100"),
	'/':  normalizeGlyph("00001", "00010", "00010", "00100", "01000", "01000", "10000"),
	'0':  normalizeGlyph("01110", "10001", "10011", "10101", "11001", "10001", "01110"),
	'1':  normalizeGlyph("00100", "01100", "00100", "00100", "00100", "00100", "01110"),
	'2':  normalizeGlyph("01110", "10001", "00001", "00010", "00100", "01000", "11111"),
	'3':  normalizeGlyph("11110", "00001", "00001", "01110", "00001", "00001", "11110"),
	'4':  normalizeGlyph("00010", "00110", "01010", "10010", "11111", "00010", "00010"),
	'5':  normalizeGlyph("11111", "10000", "10000", "11110", "00001", "00001", "11110"),
	'6':  normalizeGlyph("00110", "01000", "10000", "11110", "10001", "10001", "01110"),
	'7':  normalizeGlyph("11111", "00001", "00010", "00100", "01000", "01000", "01000"),
	'8':  normalizeGlyph("01110", "10001", "10001", "01110", "10001", "10001", "01110"),
	'9':  normalizeGlyph("01110", "10001", "10001", "01111", "00001", "00010", "11100"),
	':':  normalizeGlyph("00000", "00100", "00100", "00000", "00100", "00100", "00000"),
	';':  normalizeGlyph("00000", "00100", "00100", "00000", "00100", "00100", "01000"),
	'<':  normalizeGlyph("00010", "00100", "01000", "10000", "01000", "00100", "00010"),
	'=':  normalizeGlyph("00000", "00000", "11111", "00000", "11111", "00000", "00000"),
	'>':  normalizeGlyph("01000", "00100", "00010", "00001", "00010", "00100", "01000"),
	'?':  normalizeGlyph("01110", "10001", "00001", "00010", "00100", "00000", "00100"),
	'@':  normalizeGlyph("01110", "10001", "10111", "10101", "10111", "10000", "01110"),
	'A':  normalizeGlyph("01110", "10001", "10001", "11111", "10001", "10001", "10001"),
	'B':  normalizeGlyph("11110", "10001", "10001", "11110", "10001", "10001", "11110"),
	'C':  normalizeGlyph("01110", "10001", "10000", "10000", "10000", "10001", "01110"),
	'D':  normalizeGlyph("11110", "10001", "10001", "10001", "10001", "10001", "11110"),
	'E':  normalizeGlyph("11111", "10000", "10000", "11110", "10000", "10000", "11111"),
	'F':  normalizeGlyph("11111", "10000", "10000", "11110", "10000", "10000", "10000"),
	'G':  normalizeGlyph("01110", "10001", "10000", "10111", "10001", "10001", "01110"),
	'H':  normalizeGlyph("10001", "10001", "10001", "11111", "10001", "10001", "10001"),
	'I':  normalizeGlyph("01110", "00100", "00100", "00100", "00100", "00100", "01110"),
	'J':  normalizeGlyph("00111", "00010", "00010", "00010", "10010", "10010", "01100"),
	'K':  normalizeGlyph("10001", "10010", "10100", "11000", "10100", "10010", "10001"),
	'L':  normalizeGlyph("10000", "10000", "10000", "10000", "10000", "10000", "11111"),
	'M':  normalizeGlyph("10001", "11011", "10101", "10101", "10001", "10001", "10001"),
	'N':  normalizeGlyph("10001", "11001", "10101", "10011", "10001", "10001", "10001"),
	'O':  normalizeGlyph("01110", "10001", "10001", "10001", "10001", "10001", "01110"),
	'P':  normalizeGlyph("11110", "10001", "10001", "11110", "10000", "10000", "10000"),
	'Q':  normalizeGlyph("01110", "10001", "10001", "10001", "10101", "10010", "01101"),
	'R':  normalizeGlyph("11110", "10001", "10001", "11110", "10100", "10010", "10001"),
	'S':  normalizeGlyph("01111", "10000", "10000", "01110", "00001", "00001", "11110"),
	'T':  normalizeGlyph("11111", "00100", "00100", "00100", "00100", "00100", "00100"),
	'U':  normalizeGlyph("10001", "10001", "10001", "10001", "10001", "10001", "01110"),
	'V':  normalizeGlyph("10001", "10001", "10001", "10001", "10001", "01010", "00100"),
	'W':  normalizeGlyph("10001", "10001", "10001", "10101", "10101", "10101", "01010"),
	'X':  normalizeGlyph("10001", "10001", "01010", "00100", "01010", "10001", "10001"),
	'Y':  normalizeGlyph("10001", "10001", "01010", "00100", "00100", "00100", "00100"),
	'Z':  normalizeGlyph("11111", "00001", "00010", "00100", "01000", "10000", "11111"),
	'[':  normalizeGlyph("01110", "01000", "01000", "01000", "01000", "01000", "01110"),
	'\\': normalizeGlyph("10000", "01000", "01000", "00100", "00010", "00010", "00001"),
	']':  normalizeGlyph("01110", "00010", "00010", "00010", "00010", "00010", "01110"),
	'^':  normalizeGlyph("00100", "01010", "10001", "00000", "00000", "00000", "00000"),
	'_':  normalizeGlyph("00000", "00000", "00000", "00000", "00000", "00000", "11111"),
	'`':  normalizeGlyph("01000", "00100", "00010", "00000", "00000", "00000", "00000"),
	'{':  normalizeGlyph("00010", "00100", "00100", "01000", "00100", "00100", "00010"),
	'|':  normalizeGlyph("00100", "00100", "00100", "00100", "00100", "00100", "00100"),
	'}':  normalizeGlyph("01000", "00100", "00100", "00010", "00100", "00100", "01000"),
	'~':  normalizeGlyph("00000", "00000", "01000", "10101", "00010", "00000", "00000"),
	'…':  normalizeGlyph("00000", "00000", "00000", "00000", "00000", "10101", "10101"),
	'ƒ':  normalizeGlyph("00110", "01001", "01000", "11100", "01000", "01000", "10000"),
	'Δ':  normalizeGlyph("00100", "01010", "01010", "10001", "10001", "11111", "00000"),
	'€':  normalizeGlyph("00111", "01000", "11110", "01000", "11110", "01000", "00111"),
	'£':  normalizeGlyph("00110", "01001", "01000", "11100", "01000", "01001", "11110"),
	'¥':  normalizeGlyph("10001", "01010", "00100", "11111", "00100", "11111", "00100"),
	'₿':  normalizeGlyph("11110", "10101", "10101", "11110", "10101", "10101", "11110"),
	'↑':  normalizeGlyph("00100", "01110", "10101", "00100", "00100", "00100", "00100"),
	'↓':  normalizeGlyph("00100", "00100", "00100", "00100", "10101", "01110", "00100"),
	'←':  normalizeGlyph("00000", "00100", "01000", "11111", "01000", "00100", "00000"),
	'→':  normalizeGlyph("00000", "00100", "00010", "11111", "00010", "00100", "00000"),
}
