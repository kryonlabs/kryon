package kryon

// Storage and drawing adaptation stay native; camera selection and coordinates
// are shared with the C renderer through runtime/canvas.kry.
type canvasScopeState struct {
	transform CanvasTransform
	clipDepth int
}

func (r *runtime) CanvasScope(canvas Canvas) CanvasResult {
	var scrollX, scrollY int32
	zoom := float32(1)
	frame := simpleStyleFrameWithClassRole(ButtonToneNeutral, ButtonStateNormal,
		false, false, canvas.ClassName, StyleSheet_StyleKindCanvas(), StyleSheet_StyleAny())
	r.record(styleFrameRectOp(canvas.Bounds, canvas.Bounds, frame))
	if canvas.ScrollX != nil {
		scrollX = *canvas.ScrollX
	}
	if canvas.ScrollY != nil {
		scrollY = *canvas.ScrollY
	}
	if canvas.Zoom != nil {
		zoom = *canvas.Zoom
	}
	policy := Canvas_CanvasBeginResultFor(canvas.Bounds, r.mousePos, scrollX, scrollY, zoom, r.mouseDown[MouseButtonLeft])
	transform := CanvasTransform{Scale: 1}
	if len(r.canvases) > 0 {
		transform = r.canvases[len(r.canvases)-1].transform
	}
	if Canvas_CanvasHasTransform(canvas.ScrollX != nil, canvas.ScrollY != nil, canvas.Zoom != nil, zoom) {
		transform = Canvas_CanvasTransformFor(canvas.Bounds, scrollX, scrollY, zoom)
	}
	r.canvases = append(r.canvases, canvasScopeState{transform: transform, clipDepth: len(r.scrollClips)})
	r.scrollClips = append(r.scrollClips, r.scrollClip(canvas.Bounds))
	return CanvasResult{Active: policy.Active, Dragging: policy.Dragging, World: policy.World}
}

func (r *runtime) CanvasEndScope(canvas Canvas) {
	if len(r.canvases) == 0 {
		return
	}
	scope := r.canvases[len(r.canvases)-1]
	r.canvases = r.canvases[:len(r.canvases)-1]
	r.scrollClips = r.scrollClips[:scope.clipDepth]
	frame := simpleStyleFrameWithClassRole(ButtonToneNeutral, ButtonStateNormal,
		false, false, canvas.ClassName, StyleSheet_StyleKindCanvas(), StyleSheet_StyleAny())
	bounds := canvas.Bounds
	for _, edge := range []Rectangle{
		{X: bounds.X, Y: bounds.Y, Width: bounds.Width},
		{X: bounds.X, Y: bounds.Y + bounds.Height, Width: bounds.Width},
		{X: bounds.X, Y: bounds.Y, Height: bounds.Height},
		{X: bounds.X + bounds.Width, Y: bounds.Y, Height: bounds.Height},
	} {
		r.record(FrameOp{Kind: FrameOpLine, Bounds: edge, Color: unpackRGBA(frame.Value.Border)})
	}
}

func (r *runtime) canvasOperation(op FrameOp) FrameOp {
	if len(r.canvases) == 0 {
		return op
	}
	transform := r.canvases[len(r.canvases)-1].transform
	if transform.Scale == 1 && transform.X == 0 && transform.Y == 0 {
		return op
	}
	scale := transform.Scale
	rect := func(bounds Rectangle) Rectangle { return Canvas_CanvasTransformRect(transform, bounds) }
	op.Bounds = rect(op.Bounds)
	if op.SurfaceBounds.Width != 0 || op.SurfaceBounds.Height != 0 {
		op.SurfaceBounds = rect(op.SurfaceBounds)
	}
	if op.HasClip {
		op.Clip = rect(op.Clip)
	}
	if op.HasPolygon {
		for i := range op.Polygon {
			op.Polygon[i] = Canvas_CanvasTransformPoint(transform, op.Polygon[i])
		}
	}
	op.FontSize = int32(float32(op.FontSize) * scale)
	op.LetterSpacing = int32(float32(op.LetterSpacing) * scale)
	op.Radius *= scale
	op.BorderWidth *= scale
	op.ContentOffset.X *= scale
	op.ContentOffset.Y *= scale
	op.ImageOrigin.X *= scale
	op.ImageOrigin.Y *= scale
	op.Gap *= scale
	op.IconSize *= scale
	op.ScrollY = int32(float32(op.ScrollY) * scale)
	if op.Kind == FrameOpButton {
		op.Button.Props.Bounds = op.Bounds
		op.Button.ContentBounds = rect(op.Button.ContentBounds)
		op.Button.Material.Bounds = op.Bounds
		op.Button.Material.Surface = op.SurfaceBounds
		if op.Button.Material.Scale <= 0 {
			op.Button.Material.Scale = 1
		}
		op.Button.Material.Scale *= scale
		op.Button.Font = int32(float32(op.Button.Font) * scale)
	}
	return op
}
