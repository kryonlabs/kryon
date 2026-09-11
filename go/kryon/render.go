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
	frame := img
	for _, op := range ops {
		img = frame
		if op.HasClip {
			clip := image.Rect(int(math.Ceil(float64(op.Clip.X))), int(math.Ceil(float64(op.Clip.Y))),
				int(math.Ceil(float64(op.Clip.X+op.Clip.Width))), int(math.Ceil(float64(op.Clip.Y+op.Clip.Height))))
			img = frame.SubImage(clip.Intersect(frame.Bounds())).(*image.RGBA)
		}
		switch op.Kind {
		case FrameOpBackground:
			fillImage(img, opaque(op.Color, RAYWHITE))
		case FrameOpRect:
			if op.HasPolygon {
				for y := img.Bounds().Min.Y; y < img.Bounds().Max.Y; y++ {
					for x := img.Bounds().Min.X; x < img.Bounds().Max.X; x++ {
						if pointInPolygon(float32(x)+0.5, float32(y)+0.5, op.Polygon[:]) {
							setPixel(img, x, y, op.Color)
						}
					}
				}
			} else if op.SecondaryColor.A != 0 {
				fillGradientH(img, op.Bounds, opaque(op.Color, LIGHTGRAY), opaque(op.SecondaryColor, LIGHTGRAY))
			} else {
				if op.Color.A == 0 && op.BorderColor.A == 0 && op.FocusColor.A == 0 {
					continue
				}
				op.Opacity = 1
				op.BorderWidth = 1
				op.Material = MaterialFlat
				renderMaterial(img, op)
			}
		case FrameOpLine:
			drawLine(img, op.Bounds, opaque(op.Color, BLACK))
		case FrameOpText:
			if op.Rotation != 0 {
				drawRotatedText(img, op)
			} else {
				drawText(img, op.Text, int(op.Bounds.X), int(op.Bounds.Y), op.FontSize, op.Color, op.FontID, op.LetterSpacing)
			}
		case FrameOpButton:
			renderButton(img, op)
		case FrameOpSurface:
			renderMaterial(img, op)
		case FrameOpIcon:
			renderIcon(img, op)
		case FrameOpPicture:
			fillRect(img, op.Bounds, opaque(op.Color, Color{224, 229, 236, 255}))
			strokeRect(img, op.Bounds, Color{136, 146, 160, 255})
		case FrameOpTextField, FrameOpTextArea:
			renderTextInput(img, op)
		case FrameOpColumn, FrameOpRow, FrameOpStack, FrameOpGrid, FrameOpPage, FrameOpSection:
			strokeRect(img, op.Bounds, Color{220, 224, 229, 255})
		case FrameOpScreen:
			// A screen establishes a coordinate/layout scope, not a surface.
			// Its appearance is provided explicitly by Surface or Background.
		}
	}

}

// RenderCurrentFrame paints the active runtime's latest frame operation stream.
func RenderCurrentFrame() *image.RGBA {
	rt := active()
	return RenderFrame(int(rt.GetScreenWidth()), int(rt.GetScreenHeight()), FrameOps())
}

func frameMaterial(op FrameOp) MaterialPaint {
	hover, press, focus := float32(0), float32(0), float32(0)
	if op.Hovered {
		hover = 1
	}
	if op.Pressed {
		press = 1
	}
	if op.Focused {
		focus = 1
	}
	if op.MotionValid {
		hover, press, focus = op.HoverAmount, op.PressAmount, op.FocusAmount
	}
	value := StyleData{Background: packRGBA(op.Color), Border: packRGBA(op.BorderColor),
		Focus: packRGBA(op.FocusColor), Radius: op.Radius, BorderWidth: op.BorderWidth,
		Opacity: op.Opacity, Material: int32(op.Material), BackgroundEnd: packRGBA(op.BackgroundEnd)}
	if op.HasBackgroundEnd {
		value.Fields = uint32(StyleBackgroundEnd)
	}
	return Material_PrepareMaterial(MaterialPaint{Bounds: op.Bounds, Surface: op.SurfaceBounds,
		Value: value, Light: packRGBA(op.BorderColor), Ambient: packRGBA(op.AmbientColor),
		Hover: hover, Press: press, Focus: focus, Disabled: op.Disabled,
		Fill: op.FillStates, FillValid: op.FillStatesValid, Scale: 1})
}

func renderMaterial(img *image.RGBA, op FrameOp) Rectangle {
	if op.Material == MaterialFlat && op.Radius <= 0 && op.Opacity >= 1 {
		fill := op.Color
		if fill.A == 0 {
			fill = Color{255, 255, 255, 255}
		}
		fillRect(img, op.Bounds, fill)
		if op.BorderWidth > 0 && op.BorderColor.A != 0 {
			strokeRect(img, op.Bounds, op.BorderColor)
		}
		return op.Bounds
	}
	paint := frameMaterial(op)
	for i := int32(0); i < Surface_MaterialLayerCount(int32(op.Material)); i++ {
		renderSurfaceDrawing(img, Material_PaintMaterialLayer(paint, i))
	}
	return Material_MaterialContentBounds(paint)
}

func renderSurfaceDrawing(img *image.RGBA, command SurfaceDrawing) {
	if !command.Visible {
		return
	}
	layer, bounds := command.Layer, command.Bounds
	pixels := clipRect(img, command.Area)
	for y := pixels.Min.Y; y < pixels.Max.Y; y++ {
		shade := Surface_SampleColor(layer, (float32(y)+0.5-bounds.Y)/bounds.Height)
		for x := pixels.Min.X; x < pixels.Max.X; x++ {
			coverage := Surface_SampleCoverage(layer, float32(x)-bounds.X, float32(y)-bounds.Y, command.Scale)
			coverage *= Surface_SegmentCoverage(float32(x)-command.Surface.X,
				command.Segment.X-command.Surface.X, command.Segment.Width, command.Surface.Width)
			blendPixel(img, x, y, unpackRGBA(Surface_Opacity(shade, coverage)))
		}
	}
}

func renderButton(img *image.RGBA, op FrameOp) {
	frame := op.Button
	frame.Props.Bounds = op.Bounds
	frame.Material.Bounds = op.Bounds
	frame.Material.Surface = op.SurfaceBounds
	frame.Material = Material_PrepareMaterial(frame.Material)
	Button_PaintButton(frame, float32(runtimeTextWidthWithFont(frame.Props.Label, frame.Font, op.FontID)),
		op.ElapsedMS, op.Disclosure,
		func(command SurfaceDrawing) { renderSurfaceDrawing(img, command) },
		func(command Drawing) { renderDrawing(img, command, op.FontID) })
}

func renderDrawing(img *image.RGBA, command Drawing, fontID uint32) {
	bounds := command.Bounds
	color := unpackRGBA(command.Color)
	switch command.Kind {
	case DrawingKindDrawingText:
		baseline := fontTextBaseline(command.Text, int(bounds.Y), int(bounds.Height), command.Font, fontID)
		drawText(img, command.Text, int(bounds.X), baseline, command.Font, color, fontID)
	case DrawingKindDrawingIcon:
		renderIcon(img, FrameOp{Bounds: bounds, Color: color, IconType: command.Icon, IconSize: bounds.Width})
	case DrawingKindDrawingChevron:
		pixels := clipRect(img, bounds)
		for y := pixels.Min.Y; y < pixels.Max.Y; y++ {
			for x := pixels.Min.X; x < pixels.Max.X; x++ {
				coverage := Surface_ChevronCoverage(float32(x)-bounds.X, float32(y)-bounds.Y, bounds.Width)
				blendPixel(img, x, y, unpackRGBA(Surface_Opacity(command.Color, coverage)))
			}
		}
	case DrawingKindDrawingRing:
		renderLoadingRing(img, command.Ring.X, command.Ring.Y, command.Ring)
	}
}

func renderLoadingRing(img *image.RGBA, cx, cy float32, ring Ring) {
	inner, outer := ring.InnerRadius, ring.OuterRadius
	if outer <= 0 || inner >= outer || ring.EndAngle <= ring.StartAngle {
		return
	}
	paintRadius := Surface_LoadingPaintRadius(ring)
	box := clipRect(img, Rectangle{X: cx - paintRadius, Y: cy - paintRadius, Width: 2 * paintRadius, Height: 2 * paintRadius})
	for y := box.Min.Y; y < box.Max.Y; y++ {
		for x := box.Min.X; x < box.Max.X; x++ {
			px, py := float32(x)-cx, float32(y)-cy
			sample := Surface_LoadingSample(ring, px, py)
			blendPixel(img, x, y, unpackRGBA(sample.Glow))
			blendPixel(img, x, y, unpackRGBA(sample.Track))
			blendPixel(img, x, y, unpackRGBA(sample.Arc))
			blendPixel(img, x, y, unpackRGBA(sample.Tip))
		}
	}
}

func renderIcon(img *image.RGBA, op FrameOp) {
	if op.Bounds.Width <= 0 || op.Bounds.Height <= 0 {
		return
	}
	shape := int32(0)
	switch op.IconType {
	case UIIconTypePlus:
		shape = 1
	case UIIconTypePlay:
		shape = 2
	case UIIconTypeTrash:
		shape = 3
	case UIIconTypeSave:
		shape = 4
	}
	if shape != 0 {
		if op.IconSize > 0 {
			op.Bounds.Width = op.IconSize
			op.Bounds.Height = op.IconSize
		}
		pixels := clipRect(img, op.Bounds)
		for y := pixels.Min.Y; y < pixels.Max.Y; y++ {
			for x := pixels.Min.X; x < pixels.Max.X; x++ {
				coverage := Surface_IconCoverage(shape, float32(x)-op.Bounds.X,
					float32(y)-op.Bounds.Y, op.Bounds.Width, op.Bounds.Height)
				blendPixel(img, x, y, unpackRGBA(Surface_Opacity(packRGBA(op.Color), coverage)))
			}
		}
		return
	}
	// Tint has already been resolved by the widget. Transparent is a value,
	// not a request for a fallback color, for both vector and bitmap icons.
	tint := op.Color
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
			for dy := 0; dy < cell; dy++ {
				for dx := 0; dx < cell; dx++ {
					blendPixel(img, x0+x*cell+dx, y0+y*cell+dy, tint)
				}
			}
		}
	}
}

func iconPattern(iconType int32) []string {
	switch iconType {
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

type textAreaRenderLine struct {
	text       string
	start, end int
}

func renderTextAreaLines(text string, wrapWidth int, fontSize int32, fontID uint32, wrap bool) []textAreaRenderLine {
	var lines []textAreaRenderLine
	lineStart := 0
	for i := 0; i <= len(text); i++ {
		if i < len(text) && text[i] != '\n' {
			continue
		}
		paragraphStart, paragraphEnd := lineStart, i
		paragraph := text[paragraphStart:paragraphEnd]
		if !wrap || wrapWidth <= 0 || paragraph == "" {
			lines = append(lines, textAreaRenderLine{text: paragraph, start: paragraphStart, end: paragraphEnd})
		} else {
			chunkStart := paragraphStart
			lineText := ""
			lineEnd := paragraphStart
			wordStart := -1
			flushWord := func(wordEnd int) {
				if wordStart < 0 {
					return
				}
				word := text[wordStart:wordEnd]
				candidate := word
				if lineText != "" {
					candidate = lineText + " " + word
				}
				if lineText != "" && textAdvance(candidate, int32(len(candidate)), fontSize, fontID) > wrapWidth {
					lines = append(lines, textAreaRenderLine{text: lineText, start: chunkStart, end: lineEnd})
					chunkStart = wordStart
					lineText = word
				} else {
					lineText = candidate
				}
				lineEnd = wordEnd
				wordStart = -1
			}
			for at := paragraphStart; at <= paragraphEnd; at++ {
				if at < paragraphEnd && text[at] != ' ' && text[at] != '\t' {
					if wordStart < 0 {
						wordStart = at
					}
					continue
				}
				flushWord(at)
			}
			if lineText != "" {
				lines = append(lines, textAreaRenderLine{text: lineText, start: chunkStart, end: lineEnd})
			}
		}
		lineStart = i + 1
	}
	if len(lines) == 0 {
		lines = append(lines, textAreaRenderLine{})
	}
	return lines
}

func renderTextArea(img *image.RGBA, op FrameOp) {
	paintOp := op
	if paintOp.Opacity == 0 {
		paintOp.Opacity = 1
	}
	if paintOp.BorderWidth == 0 {
		paintOp.BorderWidth = 1
	}
	if paintOp.FocusColor.A == 0 {
		paintOp.FocusColor = paintOp.CursorColor
	}
	paintOp.Kind = FrameOpSurface
	renderMaterial(img, paintOp)

	paddingX := int(round(op.ContentOffset.X))
	if paddingX <= 0 {
		paddingX = 8
	}
	paddingY := int(round(op.ContentOffset.Y))
	if paddingY <= 0 {
		paddingY = 8
	}
	lineGap := int(round(op.Gap))
	if lineGap < 0 {
		lineGap = 0
	}
	fontHeight := int(textHeight(op.FontSize, op.FontID))
	lineHeight := maxInt(1, fontHeight+lineGap)
	x := int(round(op.Bounds.X)) + paddingX
	y0 := int(round(op.Bounds.Y)) + paddingY - int(op.ScrollY)
	clipTop := int(round(op.Bounds.Y)) + paddingY
	clipBottom := int(round(op.Bounds.Y+op.Bounds.Height)) - paddingY
	wrapWidth := int(round(op.Bounds.Width)) - paddingX*2
	if wrapWidth < 24 {
		wrapWidth = 0
	}
	text := BLACK
	if op.TextColor.A != 0 {
		text = op.TextColor
	}
	selection := Color{58, 110, 190, 255}
	if op.SelectionColor.A != 0 {
		selection = op.SelectionColor
	}
	selected := text
	if op.SelectedTextColor.A != 0 {
		selected = op.SelectedTextColor
	}
	cursorColor := op.BorderColor
	if cursorColor.A == 0 {
		cursorColor = Color{144, 152, 164, 255}
	}
	if op.CursorColor.A != 0 {
		cursorColor = op.CursorColor
	}
	lines := renderTextAreaLines(op.Text, wrapWidth, op.FontSize, op.FontID, op.Wrap)
	for i, line := range lines {
		y := y0 + i*lineHeight
		if y+lineHeight < clipTop || y > clipBottom {
			continue
		}
		if op.SelectionStart != op.SelectionEnd && line.end >= line.start {
			selStart, selEnd := orderedInt32(op.SelectionStart, op.SelectionEnd)
			start := maxInt(line.start, int(selStart))
			end := minInt(line.end, int(selEnd))
			if end > start {
				prefix := op.Text[line.start:start]
				part := op.Text[start:end]
				sx := x + textAdvance(prefix, int32(len(prefix)), op.FontSize, op.FontID)
				sw := maxInt(1, textAdvance(part, int32(len(part)), op.FontSize, op.FontID))
				fillRectPixels(img, sx, y, sw, fontHeight, selection)
			}
		}
		drawText(img, line.text, x, y, op.FontSize, text, op.FontID)
		if op.SelectionStart != op.SelectionEnd && line.end >= line.start {
			selStart, selEnd := orderedInt32(op.SelectionStart, op.SelectionEnd)
			start := maxInt(line.start, int(selStart))
			end := minInt(line.end, int(selEnd))
			if end > start {
				prefix := op.Text[line.start:start]
				sx := x + textAdvance(prefix, int32(len(prefix)), op.FontSize, op.FontID)
				drawText(img, op.Text[start:end], sx, y, op.FontSize, selected, op.FontID)
			}
		}
		if op.CompositionStart != op.CompositionEnd && line.end >= line.start {
			compStart, compEnd := orderedInt32(op.CompositionStart, op.CompositionEnd)
			start := maxInt(line.start, int(compStart))
			end := minInt(line.end, int(compEnd))
			if end > start {
				prefix := op.Text[line.start:start]
				part := op.Text[start:end]
				sx := x + textAdvance(prefix, int32(len(prefix)), op.FontSize, op.FontID)
				sw := maxInt(2, textAdvance(part, int32(len(part)), op.FontSize, op.FontID))
				fillRectPixels(img, sx, y+fontHeight-2, sw, 2, cursorColor)
			}
		}
		cursor := clampCursor(op.Text, int(op.Cursor))
		if op.Focused && !op.ReadOnly && cursor >= line.start && cursor <= line.end {
			prefix := op.Text[line.start:cursor]
			cursorX := x + textAdvance(prefix, int32(len(prefix)), op.FontSize, op.FontID)
			drawVertical(img, cursorX, y, y+fontHeight, cursorColor)
		}
	}
}

func renderTextInput(img *image.RGBA, op FrameOp) {
	if op.Kind == FrameOpTextArea {
		renderTextArea(img, op)
		return
	}
	if op.Material == 0 && op.Opacity == 0 && op.Radius == 0 && op.BorderWidth == 0 {
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
	} else {
		if op.Opacity == 0 {
			op.Opacity = 1
		}
		if op.BorderWidth == 0 {
			op.BorderWidth = 1
		}
		if op.FocusColor.A == 0 {
			op.FocusColor = op.CursorColor
		}
		op.Kind = FrameOpSurface
		renderMaterial(img, op)
	}
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
	if op.CompositionStart != op.CompositionEnd {
		start, end := orderedInt32(op.CompositionStart, op.CompositionEnd)
		sx := x + textAdvance(op.Text, start, op.FontSize, op.FontID)
		ex := x + textAdvance(op.Text, end, op.FontSize, op.FontID)
		composition := op.BorderColor
		if composition.A == 0 {
			composition = Color{144, 152, 164, 255}
		}
		if op.CursorColor.A != 0 {
			composition = op.CursorColor
		}
		fillRectPixels(img, sx, y+int(textHeight(op.FontSize, op.FontID))-2,
			maxInt(2, ex-sx), 2, composition)
	}
	if op.Focused && !op.ReadOnly {
		cursorX := x + textAdvance(op.Text, op.Cursor, op.FontSize, op.FontID)
		top := int(round(op.Bounds.Y)) + 5
		bottom := int(round(op.Bounds.Y+op.Bounds.Height)) - 5
		cursor := op.BorderColor
		if cursor.A == 0 {
			cursor = Color{144, 152, 164, 255}
		}
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

func roundedContains(x, y float32, r Rectangle, radius float32) bool {
	if r.Width <= 0 || r.Height <= 0 || x < r.X || x >= r.X+r.Width || y < r.Y || y >= r.Y+r.Height {
		return false
	}
	if radius <= 0 {
		return x >= r.X && x < r.X+r.Width && y >= r.Y && y < r.Y+r.Height
	}
	radius = min(radius, min(r.Width, r.Height)/2)
	cx := min(max(x, r.X+radius), r.X+r.Width-radius)
	cy := min(max(y, r.Y+radius), r.Y+r.Height-radius)
	dx, dy := x-cx, y-cy
	return dx*dx+dy*dy <= radius*radius
}

func fillRoundedRect(img *image.RGBA, r Rectangle, radius float32, c Color) {
	rect := clipRect(img, r)
	for y := rect.Min.Y; y < rect.Max.Y; y++ {
		for x := rect.Min.X; x < rect.Max.X; x++ {
			if roundedContains(float32(x)+0.5, float32(y)+0.5, r, radius) {
				blendPixel(img, x, y, c)
			}
		}
	}
}

func strokeRoundedRect(img *image.RGBA, r Rectangle, radius float32, width int, c Color) {
	if width <= 0 || c.A == 0 {
		return
	}
	inner := Rectangle{X: r.X + float32(width), Y: r.Y + float32(width),
		Width: r.Width - float32(width*2), Height: r.Height - float32(width*2)}
	rect := clipRect(img, r)
	for y := rect.Min.Y; y < rect.Max.Y; y++ {
		for x := rect.Min.X; x < rect.Max.X; x++ {
			px, py := float32(x)+0.5, float32(y)+0.5
			if roundedContains(px, py, r, radius) &&
				!roundedContains(px, py, inner, max(float32(0), radius-float32(width))) {
				blendPixel(img, x, y, c)
			}
		}
	}
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

func pointInPolygon(x, y float32, points []Vector2) bool {
	inside := false
	j := len(points) - 1
	for i, p := range points {
		q := points[j]
		if (p.Y > y) != (q.Y > y) && x < (q.X-p.X)*(y-p.Y)/(q.Y-p.Y)+p.X {
			inside = !inside
		}
		j = i
	}
	return inside
}

func drawRotatedText(img *image.RGBA, op FrameOp) {
	angle := float64(op.Rotation) * math.Pi / 180
	if math.IsNaN(angle) || math.IsInf(angle, 0) || img.Bounds().Empty() {
		return
	}
	s, c := math.Sincos(angle)
	font := max32(1, op.FontSize)
	w := min(runtimeTextWidth(op.Text, font)+8, img.Bounds().Dx()+img.Bounds().Dy()+int(font)*2)
	source := image.NewRGBA(image.Rect(0, 0, max(1, w), int(font)*2+8))
	drawText(source, op.Text, 0, 0, font, op.Color, op.FontID)
	for y := img.Bounds().Min.Y; y < img.Bounds().Max.Y; y++ {
		for x := img.Bounds().Min.X; x < img.Bounds().Max.X; x++ {
			if op.HasPolygon && !pointInPolygon(float32(x)+0.5, float32(y)+0.5, op.Polygon[:]) {
				continue
			}
			dx, dy := float64(x)+0.5-float64(op.Bounds.X), float64(y)+0.5-float64(op.Bounds.Y)
			sx, sy := int(math.Floor(c*dx+s*dy)), int(math.Floor(-s*dx+c*dy))
			if !image.Pt(sx, sy).In(source.Bounds()) {
				continue
			}
			pixel := color.NRGBAModel.Convert(source.RGBAAt(sx, sy)).(color.NRGBA)
			if pixel.A != 0 {
				blendPixel(img, x, y, Color{pixel.R, pixel.G, pixel.B, pixel.A})
			}
		}
	}
}

func drawText(img *image.RGBA, text string, x, y int, fontSize int32, c Color, fontID uint32, letterSpacing ...int32) {
	if c.A == 0 {
		return
	}
	if drawFontText(img, text, x, y, fontSize, c, fontID, letterSpacing...) {
		return
	}
	spacing := 0
	if len(letterSpacing) > 0 {
		spacing = int(max(letterSpacing[0], 0))
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
			cursor += 6*scale + spacing
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
		cursor += 6*scale + spacing
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
	draw.Draw(img, image.Rect(x, y, x+w, y+h).Intersect(img.Bounds()),
		image.NewUniform(color.NRGBA{R: c.R, G: c.G, B: c.B, A: c.A}), image.Point{}, draw.Over)
}

func clipRect(img *image.RGBA, r Rectangle) image.Rectangle {
	x0 := int(math.Floor(float64(r.X)))
	y0 := int(math.Floor(float64(r.Y)))
	x1 := int(math.Ceil(float64(r.X + r.Width)))
	y1 := int(math.Ceil(float64(r.Y + r.Height)))
	return image.Rect(x0, y0, x1, y1).Intersect(img.Bounds())
}

// Surface colors are straight alpha; image.RGBA stores premultiplied channels.
func blendPixel(img *image.RGBA, x, y int, c Color) {
	if !image.Pt(x, y).In(img.Bounds()) || c.A == 0 {
		return
	}
	dst := img.RGBAAt(x, y)
	a := uint32(c.A)
	remaining := 255 - a
	img.SetRGBA(x, y, color.RGBA{
		R: uint8((uint32(c.R)*a + uint32(dst.R)*remaining + 127) / 255),
		G: uint8((uint32(c.G)*a + uint32(dst.G)*remaining + 127) / 255),
		B: uint8((uint32(c.B)*a + uint32(dst.B)*remaining + 127) / 255),
		A: uint8(a + (uint32(dst.A)*remaining+127)/255),
	})
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
