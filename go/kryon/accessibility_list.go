package kryon

func listItemKey(props ListBoxProps, index int) uint64 {
	if index < len(props.ItemKeys) && props.ItemKeys[index] > 0 {
		return uint64(props.ItemKeys[index])
	}
	return 0
}

func (r *runtime) applyAccessibilityList(props ListBoxProps, count int) (bool, int32) {
	changed, selectedItem := false, int32(-1)
	actions := AccessibilityPolicy_AccessibilityActionsFor(int32(WidgetKindListBox), props.ID,
		props.Disabled || r.contentDisabled(), r.popupKeyboardCaptures(), props.SelectedIndex == nil && props.Selected == nil)
	for i, request := range r.accessibility.active {
		if request.id != props.ID || props.ID <= 0 || request.kind != int32(WidgetKindListBox) ||
			request.action == AccessibilityActionFocus {
			continue
		}
		r.accessibility.active[i].id = 0
		if !AccessibilityPolicy_AccessibilityActionAllowed(actions, request.action) {
			continue
		}
		index := int(request.anchor)
		all := request.action == AccessibilityActionSelectAll || request.action == AccessibilityActionClearSelection
		if !all {
			if request.itemKey != 0 {
				index = -1
				for candidate := 0; candidate < count; candidate++ {
					if listItemKey(props, candidate) == request.itemKey {
						if index >= 0 {
							index = -1
							break
						}
						index = candidate
					}
				}
			} else if index >= 0 && index < count && props.Items[index] != string(request.value) {
				continue
			}
			if index < 0 || index >= count {
				continue
			}
		}
		if props.Selected != nil {
			for row := 0; row < count; row++ {
				if all || row == index {
					value := boolInt(AccessibilityPolicy_AccessibilityItemSelectionFor(props.Selected[row] != 0, int32(row), int32(index), request.action))
					changed = changed || props.Selected[row] != value
					props.Selected[row] = value
				}
			}
			if !all && props.Anchor != nil {
				*props.Anchor = int32(index)
			}
		} else if props.SelectedIndex != nil {
			if request.action == AccessibilityActionSelectAll {
				continue
			}
			value := AccessibilityPolicy_AccessibilitySingleSelectionFor(*props.SelectedIndex, int32(index), request.action)
			changed = changed || value != *props.SelectedIndex
			*props.SelectedIndex = value
		}
		if !all {
			selectedItem = int32(index)
		}
		r.setFocus(props.ID)
	}
	return changed, selectedItem
}

func (r *runtime) recordAccessibilityList(props ListBoxProps, bounds Rectangle, count int, rowHeight int32, scroll int32) {
	owner := 0
	for index := len(r.ops) - 1; index >= 0; index-- {
		if r.ops[index].Role == "listbox" && r.ops[index].ID == props.ID {
			owner = index + 1
			break
		}
	}
	if owner == 0 {
		return
	}
	for index := 0; index < count; index++ {
		selected := props.SelectedIndex != nil && *props.SelectedIndex == int32(index)
		if props.Selected != nil {
			selected = props.Selected[index] != 0
		}
		row := Rectangle{X: bounds.X, Y: bounds.Y + float32(int32(index)*rowHeight-scroll), Width: bounds.Width, Height: float32(rowHeight)}
		r.record(FrameOp{Kind: FrameOpSemantic, Role: "option", Text: props.Items[index], Bounds: row,
			Row: int32(index), Selected: selected, Disabled: props.Disabled,
			accessibilityKey:       listItemKey(props, index),
			accessibilityOffscreen: row.Y+row.Height <= bounds.Y || row.Y >= bounds.Y+bounds.Height})
		r.ops[len(r.ops)-1].accessibilityParent = owner
	}
}
