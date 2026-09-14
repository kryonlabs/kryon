package kryon

func (r *runtime) dropdownPanel(button Rectangle, count int, className int32) Rectangle {
	panel := StyleFrame{Value: packStyle(r.dropdownStyle(Dropdown_DropdownPanelRole(), false,
		ButtonStateNormal, className))}
	return Dropdown_PopupBounds(button,
		Rectangle{Width: float32(r.config.Width), Height: float32(r.config.Height)}, int32(count), 1, panel)
}
