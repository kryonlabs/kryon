package kryon

func (r *runtime) listBoxMultiSelect(props ListBoxProps) int32 {
	count := int(props.ItemCount)
	if count <= 0 || count > len(props.Items) {
		count = len(props.Items)
	}
	if count > len(props.Selected) {
		count = len(props.Selected)
	}
	if count == 0 {
		if props.SelectedCount != nil {
			*props.SelectedCount = 0
		}
		return -1
	}
	bounds := r.layoutRect(props.Bounds)
	disabled := props.Disabled || r.contentDisabled()
	defaultItemFrame := listBoxMultiItemMetricFrame(props.ClassName, disabled)
	rowHeight := ListBoxMulti_ListBoxMultiRowHeight(props.RowHeight, 1, defaultItemFrame)
	if !disabled {
		r.registerField(props.ID)
	}
	clicked := int32(-1)
	rangeAnchor := int32(-1)
	if !disabled {
		for i := 0; i < count; i++ {
			row := ListBoxMulti_ListBoxMultiRowBounds(bounds, int32(i), rowHeight)
			if r.consumeTap(row) {
				clicked = int32(i)
				if props.ID > 0 {
					r.setFocus(props.ID)
				}
				break
			}
		}
	}
	focused := !disabled && props.ID > 0 && r.focusID == props.ID && !r.popupFocusCaptures(props.ID)
	control := r.keyDown[KeyLeftControl] || r.keyDown[KeyRightControl]
	shift := r.keyDown[KeyLeftShift] || r.keyDown[KeyRightShift]
	if focused && clicked < 0 {
		cursor := int32(-1)
		selectedFirst := int32(-1)
		if props.Anchor != nil && *props.Anchor >= 0 && int(*props.Anchor) < count {
			cursor = *props.Anchor
		} else {
			for i := 0; i < count; i++ {
				if props.Selected[i] != 0 {
					selectedFirst = int32(i)
					break
				}
			}
		}
		cursor = ListBoxMulti_ListBoxMultiFocusedRow(cursor, selectedFirst, int32(count))
		input := ListBoxMulti_ListBoxMultiInputFor(
			r.keyDown[KeyHome], r.keyDown[KeyEnd], r.keyDown[KeyUp],
			r.keyDown[KeyDown], r.keyDown[KeySpace], r.keyDown[KeyEnter])
		nav := ListBoxMulti_ListBoxMultiNavigate(int32(count), cursor, control, shift, input)
		clicked = nav.Clicked
		control = nav.Control
		shift = nav.Shift
		rangeAnchor = nav.RangeAnchor
		if nav.AnchorChanged {
			if props.Anchor != nil {
				*props.Anchor = nav.Anchor
			}
		}
	}
	if clicked >= 0 {
		anchor := int32(-1)
		if props.Anchor != nil {
			anchor = *props.Anchor
		}
		for row := 0; row < count; row++ {
			if ListBoxMulti_ListBoxMultiSelectionForRow(int32(row),
				props.Selected[row] != 0, clicked, int32(count), anchor,
				control, shift, rangeAnchor) {
				props.Selected[row] = 1
			} else {
				props.Selected[row] = 0
			}
		}
		if props.Anchor != nil {
			*props.Anchor = ListBoxMulti_ListBoxMultiAnchorAfterClick(anchor,
				clicked, int32(count), control, shift, rangeAnchor)
		}
	}
	selectedCount := int32(0)
	focusRow := int32(-1)
	if focused {
		if props.Anchor != nil && *props.Anchor >= 0 && int(*props.Anchor) < count {
			focusRow = *props.Anchor
		} else {
			for i := 0; i < count; i++ {
				if props.Selected[i] != 0 {
					focusRow = int32(i)
					break
				}
			}
			if focusRow < 0 {
				focusRow = 0
			}
		}
	}
	listFrame := simpleStyleFrameWithClassRole(ButtonToneNeutral, func() ButtonState {
		if disabled {
			return ButtonStateDisabled
		}
		if focused {
			return ButtonStateFocus
		}
		return ButtonStateNormal
	}(), disabled, false, props.ClassName, StyleSheet_StyleKindListBoxMulti(), StyleSheet_StyleAny())
	listOp := styleFrameRectOp(bounds, Rectangle{}, listFrame)
	listOp.ID = props.ID
	listOp.Disabled = disabled
	listOp.Focused = focused
	r.record(listOp)
	for i := 0; i < count; i++ {
		selected := props.Selected[i] != 0
		if selected {
			selectedCount++
		}
		row := ListBoxMulti_ListBoxMultiRowBounds(bounds, int32(i), rowHeight)
		rowFocused := focusRow == int32(i)
		hovered := !disabled && pointInRect(r.mousePos.X, r.mousePos.Y, row)
		pressed := int32(i) == clicked
		itemFrame := simpleStyleFrameWithClassRole(ButtonToneNeutral, func() ButtonState {
			if disabled {
				return ButtonStateDisabled
			}
			if hovered {
				return ButtonStateHover
			}
			if selected {
				return ButtonStateSelected
			}
			if rowFocused {
				return ButtonStateFocus
			}
			return ButtonStateNormal
		}(), disabled, selected, props.ClassName, StyleSheet_StyleKindListBoxMultiItem(), StyleSheet_StyleAny())
		itemStyle := unpackStyle(itemFrame.Value)
		if selected || hovered || disabled || rowFocused {
			op := styleFrameRectOp(row, bounds, itemFrame)
			op.ID = props.ID
			op.Row = int32(i)
			op.Selected = selected
			op.Hovered = hovered
			op.Pressed = pressed
			op.Focused = rowFocused
			op.Disabled = disabled
			if rowFocused {
				op.BorderColor = op.FocusColor
			}
			r.record(op)
		}
		labelX := itemStyle.PaddingX
		if labelX <= 0 {
			labelX = 8
		}
		labelY := itemStyle.PaddingY
		if labelY <= 0 {
			labelY = 4
		}
		font, fontID := styleTextFace(itemStyle, Text14)
		r.record(FrameOp{Kind: FrameOpText, Bounds: Rectangle{X: row.X + labelX, Y: row.Y + labelY, Width: row.Width - labelX*2, Height: row.Height - labelY*2}, Text: props.Items[i], Color: itemStyle.Foreground, Opacity: itemStyle.Opacity, FontSize: font, FontID: fontID, ID: props.ID, Row: int32(i), Selected: selected, Disabled: disabled, Pressed: pressed, Focused: rowFocused})
	}
	if props.SelectedCount != nil {
		*props.SelectedCount = selectedCount
	}
	return clicked
}

func (r *runtime) ListBox(props ListBoxProps) int32 {
	if props.Selected != nil {
		return r.listBoxMultiSelect(props)
	}
	props = normalizeListBoxProps(props)
	props.Bounds = r.layoutRect(props.Bounds)
	props.Disabled = props.Disabled || r.contentDisabled()
	defaultItemFrame := listBoxItemMetricFrame(props.ClassName, props.Disabled)
	rowH := ListBox_ListBoxRowHeight(props.RowHeight, 1, defaultItemFrame)
	maxScroll := ListBox_ListBoxMaxScroll(props.Bounds.Height, int32(len(props.Items)), rowH, 0, 1, defaultItemFrame)
	if props.ScrollOffset != nil {
		*props.ScrollOffset = ListBox_ListBoxClampScroll(*props.ScrollOffset, maxScroll)
	}
	changed := int32(0)
	if !props.Disabled && r.pointerCanReach(props.Bounds) && props.ScrollOffset != nil && r.mouseWheel != 0 {
		*props.ScrollOffset = ListBox_ListBoxClampScroll(*props.ScrollOffset-int32(r.mouseWheel)*rowH*3, maxScroll)
		changed = 1
	}
	if !props.Disabled && props.ID != 0 {
		r.registerField(props.ID)
	}
	if !props.Disabled && props.ID != 0 && r.focusID == props.ID &&
		!r.popupFocusCaptures(props.ID) && props.SelectedIndex != nil && len(props.Items) > 0 {
		key := int32(0)
		switch {
		case r.keyDown[KeyHome]:
			key = 1
		case r.keyDown[KeyEnd]:
			key = 2
		case r.keyDown[KeyUp]:
			key = 3
		case r.keyDown[KeyDown]:
			key = 4
		}
		if key != 0 {
			scroll := int32(0)
			if props.ScrollOffset != nil {
				scroll = *props.ScrollOffset
			}
			nav := ListBox_ListBoxNavigate(*props.SelectedIndex, int32(len(props.Items)), key, scroll, rowH, props.Bounds.Height, maxScroll, 1, defaultItemFrame)
			if nav.Changed {
				*props.SelectedIndex = nav.Selected
				changed = 1
			}
			if props.ScrollOffset != nil {
				*props.ScrollOffset = nav.Scroll
			}
		}
	}
	changed |= r.recordListBoxOps(props, rowH)
	return changed
}

func (r *runtime) recordListBoxOps(props ListBoxProps, rowH int32) int32 {
	defaultItemFrame := listBoxItemMetricFrame(props.ClassName, props.Disabled)
	rowH = ListBox_ListBoxRowHeight(rowH, 1, defaultItemFrame)
	changed := int32(0)
	focused := !props.Disabled && props.ID != 0 && r.focusID == props.ID && !r.popupFocusCaptures(props.ID)
	listFrame := simpleStyleFrameWithClassRole(ButtonToneNeutral, func() ButtonState {
		if props.Disabled {
			return ButtonStateDisabled
		}
		if focused {
			return ButtonStateFocus
		}
		return ButtonStateNormal
	}(), props.Disabled, false, props.ClassName, StyleSheet_StyleKindListBox(), StyleSheet_StyleAny())
	listOp := styleFrameRectOp(props.Bounds, Rectangle{}, listFrame)
	listOp.ID = props.ID
	listOp.Disabled = props.Disabled
	listOp.Focused = focused
	r.record(listOp)
	scroll := int32(0)
	if props.ScrollOffset != nil {
		scroll = *props.ScrollOffset
	}
	layout := ListBox_ListBoxLayoutFor(props.Bounds, int32(len(props.Items)), rowH, 0, scroll, 1, defaultItemFrame)
	first := layout.FirstRow
	visible := layout.VisibleRows
	for i := int32(0); i <= visible && first+i < int32(len(props.Items)); i++ {
		index := first + i
		row := ListBox_ListBoxRowBounds(props.Bounds, i, layout)
		if !props.Disabled && props.SelectedIndex != nil && r.consumeTap(row) {
			if *props.SelectedIndex != index {
				*props.SelectedIndex = index
				changed = 1
			}
			if props.ID != 0 {
				r.setFocus(props.ID)
			}
		}
		selected := props.SelectedIndex != nil && *props.SelectedIndex == index
		hovered := !props.Disabled && pointInRect(r.mousePos.X, r.mousePos.Y, row)
		itemFrame := simpleStyleFrameWithClassRole(ButtonToneNeutral, func() ButtonState {
			if props.Disabled {
				return ButtonStateDisabled
			}
			if hovered {
				return ButtonStateHover
			}
			if selected {
				return ButtonStateSelected
			}
			return ButtonStateNormal
		}(), props.Disabled, selected, props.ClassName, StyleSheet_StyleKindListBoxItem(), StyleSheet_StyleAny())
		itemStyle := unpackStyle(itemFrame.Value)
		if selected || hovered || props.Disabled {
			op := styleFrameRectOp(row, props.Bounds, itemFrame)
			op.ID = props.ID
			op.Row = index
			op.Selected = selected
			op.Hovered = hovered
			op.Disabled = props.Disabled
			r.record(op)
		}
		labelX := itemStyle.PaddingX
		if labelX <= 0 {
			labelX = 8
		}
		labelY := itemStyle.PaddingY
		if labelY <= 0 {
			labelY = 4
		}
		font, fontID := styleTextFace(itemStyle, Text16)
		r.record(FrameOp{Kind: FrameOpText, Bounds: Rectangle{X: row.X + labelX, Y: row.Y + labelY, Width: row.Width - labelX*2, Height: row.Height - labelY*2}, Text: elideTextWithFont(props.Items[index], row.Width-labelX*2, font, fontID), Color: itemStyle.Foreground, Opacity: itemStyle.Opacity, FontSize: font, FontID: fontID, ID: props.ID, Row: index, Selected: selected, Disabled: props.Disabled})
	}
	return changed
}

func listBoxItemMetricFrame(className int32, disabled bool) StyleFrame {
	state := ButtonStateNormal
	if disabled {
		state = ButtonStateDisabled
	}
	return simpleStyleFrameWithClassRole(ButtonToneNeutral, state, disabled, false,
		className, StyleSheet_StyleKindListBoxItem(), StyleSheet_StyleAny())
}

func listBoxMultiItemMetricFrame(className int32, disabled bool) StyleFrame {
	state := ButtonStateNormal
	if disabled {
		state = ButtonStateDisabled
	}
	return simpleStyleFrameWithClassRole(ButtonToneNeutral, state, disabled, false,
		className, StyleSheet_StyleKindListBoxMultiItem(), StyleSheet_StyleAny())
}

func normalizeListBoxProps(props ListBoxProps) ListBoxProps {
	if props.ItemCount > 0 && int(props.ItemCount) < len(props.Items) {
		props.Items = props.Items[:props.ItemCount]
	}
	return props
}
