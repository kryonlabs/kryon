package kryon

func (r *runtime) ScrollScope(bounds Rectangle, contentHeight int32, offset *int32) Rectangle {
	clip := r.scrollClip(bounds)
	trackMetricFrame := styleMetricFrame(0, StyleSheet_StyleKindScroll(), StyleSheet_StyleAny())
	thumbMetricFrame := styleMetricFrame(0, StyleSheet_StyleKindScrollThumb(), StyleSheet_StyleAny())
	trackFrame := resolveButtonFrameForKind(r.theme(), true, r.activeTheme,
		ButtonProps{Size: ControlSizeSmall, Pill: true}, ButtonStateNormal,
		false, 0, 0, 0, StyleSheet_StyleKindScroll())
	metrics := Scroll_ScrollMetricsFor(1, trackMetricFrame, thumbMetricFrame)
	value := int32(0)
	if offset != nil {
		value = *offset
	}
	ownsDrag := offset != nil && r.scrollDragOffset == offset
	frame := Scroll_ScrollScopeFrameFor(ScrollScopeInput{
		Bounds: bounds, ContentHeight: contentHeight, HasOffset: offset != nil, Offset: value,
		PointerAllowed: r.pointerCanReach(clip), Disabled: r.contentDisabled(), Mouse: r.mousePos,
		Pressed: r.mousePressed[MouseButtonLeft], Down: r.mouseDown[MouseButtonLeft],
		Released: r.mouseReleased[MouseButtonLeft], Wheel: r.mouseWheel,
		OwnsDrag: ownsDrag, OwnerCaptured: ownsDrag && r.popupInputOwnerCaptures(r.scrollDragOwner),
		Grab: r.scrollDragGrab,
	}, metrics)
	if offset != nil {
		*offset = frame.Offset
	}
	if frame.StartDrag {
		r.scrollDragOffset = offset
		r.scrollDragGrab = frame.Grab
		r.scrollDragOwner = r.currentPopupInputOwner()
		r.consumeTap(frame.Paint.TrackBounds)
	}
	if frame.ClearDrag {
		r.scrollDragOffset = nil
	}
	if frame.ConsumeRelease {
		for i := range r.taps {
			r.taps[i].consumed = true
		}
	}
	if frame.ConsumeWheel {
		r.mouseWheel = 0
	}
	if frame.Scrollbar {
		thumbFrame := resolveButtonFrameForKind(r.theme(), true, r.activeTheme,
			ButtonProps{Tone: ButtonToneAccent, Emphasis: ButtonEmphasisFilled,
				Size: ControlSizeSmall, Pill: true},
			ButtonState(frame.ThumbState), false, 0, 0, 0, StyleSheet_StyleKindScrollThumb())
		r.record(styleFrameRectOp(frame.Paint.TrackBounds, Rectangle{}, trackFrame))
		r.record(styleFrameRectOp(frame.Paint.ThumbBounds, frame.Paint.TrackBounds, thumbFrame))
	}
	r.scrollClips = append(r.scrollClips, r.scrollClip(frame.Clip))
	return frame.Content
}

func (r *runtime) Scroll(props ScrollProps, body func(Rectangle)) {
	content := r.ScrollScope(props.Bounds, props.ContentHeight, props.Offset)
	defer r.ScrollEndScope()
	if body != nil {
		body(content)
	}
}

func (r *runtime) ScrollEndScope() {
	if len(r.scrollClips) > 0 {
		r.scrollClips = r.scrollClips[:len(r.scrollClips)-1]
	}
}

func (r *runtime) scrollClip(bounds Rectangle) Rectangle {
	if len(r.scrollClips) == 0 {
		return bounds
	}
	clip := r.scrollClips[len(r.scrollClips)-1]
	x, y := max(bounds.X, clip.X), max(bounds.Y, clip.Y)
	return Rectangle{X: x, Y: y, Width: max(float32(0), min(bounds.X+bounds.Width, clip.X+clip.Width)-x), Height: max(float32(0), min(bounds.Y+bounds.Height, clip.Y+clip.Height)-y)}
}
