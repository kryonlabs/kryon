package kryon

func (r *runtime) dropdownPanel(button Rectangle, count int) Rectangle {
	return Dropdown_PopupBounds(button,
		Rectangle{Width: float32(r.config.Width), Height: float32(r.config.Height)}, int32(count), 1)
}
