package kryon

// Size in whole rows where possible and choose the side with room, matching
// the native C dropdown's gap, padding and screen-edge margin. Input capture
// and paint must use this same rectangle, including when the menu flips up.
func (r *runtime) dropdownPanel(button Rectangle, count int) Rectangle {
	belowY := max(float32(0), button.Y+button.Height+4)
	below := max(float32(0), float32(r.config.Height)-belowY-16)
	above := max(float32(0), button.Y-16)
	available := max(above, below)
	height := button.Height*float32(count) + 8
	if height > available && button.Height > 0 {
		rows := max(1, int((available-8)/button.Height))
		height = float32(rows)*button.Height + 8
	}
	y := belowY
	if height > below && above > below {
		y = button.Y - 4 - height
	}
	// A tiny window may not fit even one row. Keep its clip on-screen.
	height = min(height, float32(r.config.Height))
	y = max(float32(0), min(y, float32(r.config.Height)-height))
	width := max(float32(0), min(button.Width, float32(r.config.Width)))
	x := max(float32(0), min(button.X, float32(r.config.Width)-width))
	return Rectangle{X: x, Y: y, Width: width, Height: height}
}
