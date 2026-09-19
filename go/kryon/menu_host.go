package kryon

func menuItemAt(items []MenuItem, start, direction int) int {
	if len(items) == 0 {
		return -1
	}
	index := start
	for range items {
		index = (index + direction + len(items)) % len(items)
		if items[index].Kind != MenuSeparator && !items[index].Disabled {
			return index
		}
	}
	return -1
}

func menuFirstItem(items []MenuItem) int { return menuItemAt(items, -1, 1) }

func menuLastItem(items []MenuItem) int { return menuItemAt(items, 0, -1) }

func (r *runtime) menuNav(id int32) *menuNavigation {
	state := r.menuNavigation[id]
	if state == nil {
		state = &menuNavigation{}
		r.menuNavigation[id] = state
	}
	return state
}

func resetMenuPath(state *menuNavigation, items []MenuItem) {
	state.Path = state.Path[:0]
	if first := menuFirstItem(items); first >= 0 {
		state.Path = append(state.Path, first)
	}
}

func (r *runtime) menuBar(id int32, className int32, bounds Rectangle, menus []MenuGroup, openIndex *int32) MenuResult {
	result := MenuResult{OpenIndex: -1}
	state := r.menuNav(id)
	barFrame := simpleStyleFrameWithClassRole(ButtonToneNeutral, ButtonStateNormal,
		false, false, className, StyleSheet_StyleKindMenu(), Menu_MenuBarRole())
	panelFrame := simpleStyleFrameWithClassRole(ButtonToneNeutral, ButtonStateNormal,
		false, false, className, StyleSheet_StyleKindMenu(), Menu_MenuPopupRole())
	menuItemBaseFrame := simpleStyleFrameWithClassRole(ButtonToneNeutral, ButtonStateNormal,
		false, false, className, StyleSheet_StyleKindMenuItem(), StyleSheet_StyleAny())
	metrics := Menu_MenuMetricsFor(1, panelFrame, menuItemBaseFrame, barFrame)
	if bounds.Width <= 0 {
		bounds.Width = float32(r.GetScreenWidth()) - bounds.X
	}
	if bounds.Height <= 0 {
		bounds.Height = 30
	}
	if openIndex != nil && *openIndex >= 0 && int(*openIndex) < len(menus) {
		r.openMenus[id] = *openIndex
	}
	open := int32(-1)
	openedByKeyboard := false
	if v, ok := r.openMenus[id]; ok {
		open = v
	}
	if len(menus) > 0 {
		state.Top = clamp32(state.Top, 0, int32(len(menus)-1))
	}
	if !r.contentDisabled() {
		r.registerField(id)
	}
	focused := !r.contentDisabled() && id != 0 && r.focusID == id && !r.popupFocusCaptures(id)
	if focused && len(menus) > 0 {
		keyboardInput := Menu_MenuKeyboardInputFor(false, r.keyDown[KeyDown],
			r.keyDown[KeyHome], r.keyDown[KeyEnd], r.keyDown[KeyLeft],
			r.keyDown[KeyRight], r.keyDown[KeyEnter] || r.keyDown[335],
			r.keyDown[KeySpace], r.keyDown[KeyEscape])
		keyboardDecision := Menu_MenuBarKeyboardDecisionFor(keyboardInput,
			open >= 0, int32(len(state.Path)-1))
		if open < 0 {
			if keyboardDecision.MoveTopDelta != 0 {
				state.Top = Menu_MenuBarMoveTopIndex(state.Top,
					int32(len(menus)), keyboardDecision.MoveTopDelta)
			} else if keyboardDecision.FirstTop {
				state.Top = 0
			} else if keyboardDecision.LastTop {
				state.Top = int32(len(menus) - 1)
			} else if keyboardDecision.OpenTop {
				open = state.Top
				r.openMenus[id] = open
				resetMenuPath(state, limitedMenuItems(menus[open].Items, menus[open].ItemCount))
				openedByKeyboard = true
			}
		} else if keyboardDecision.CloseOpen {
			open = -1
			delete(r.openMenus, id)
			delete(r.openSubmenus, id)
			state.Path = state.Path[:0]
		} else if keyboardDecision.MoveOpenDelta != 0 {
			open = Menu_MenuBarMoveTopIndex(open, int32(len(menus)),
				keyboardDecision.MoveOpenDelta)
			state.Top = open
			r.openMenus[id] = open
			delete(r.openSubmenus, id)
			resetMenuPath(state, limitedMenuItems(menus[open].Items, menus[open].ItemCount))
			openedByKeyboard = true
		} else if keyboardDecision.MoveOpenIfNoSubmenuDelta != 0 {
			selected := -1
			if len(state.Path) == 1 {
				selected = state.Path[0]
			}
			opensSubmenu := selected >= 0 && selected < len(limitedMenuItems(menus[open].Items, menus[open].ItemCount)) &&
				limitedMenuItems(menus[open].Items, menus[open].ItemCount)[selected].Kind == MenuSubmenu &&
				!limitedMenuItems(menus[open].Items, menus[open].ItemCount)[selected].Disabled
			if !opensSubmenu {
				open = Menu_MenuBarMoveTopIndex(open, int32(len(menus)),
					keyboardDecision.MoveOpenIfNoSubmenuDelta)
				state.Top = open
				r.openMenus[id] = open
				delete(r.openSubmenus, id)
				resetMenuPath(state, limitedMenuItems(menus[open].Items, menus[open].ItemCount))
				openedByKeyboard = true
			}
		}
	}
	barStyle := unpackStyle(barFrame.Value)
	r.record(styleFrameRectOp(bounds, Rectangle{}, barFrame))
	r.record(FrameOp{Kind: FrameOpLine, Bounds: Rectangle{X: bounds.X, Y: bounds.Y + bounds.Height - 1, Width: bounds.Width, Height: 0}, Color: barStyle.Border})
	x := Menu_MenuBarFirstItemX(bounds, metrics)
	menuItemBaseStyle := unpackStyle(menuItemBaseFrame.Value)
	font, fontID := styleTextFace(menuItemBaseStyle, Text14)
	for i, menu := range menus {
		w := Menu_MenuGroupItemWidth(int32(runtimeTextWidthWithFont(menu.Label, font, fontID)), metrics)
		item := Menu_MenuGroupItemBounds(x, bounds, w, metrics)
		tapped := !r.contentDisabled() && r.consumeTap(item)
		openID := Menu_MenuBarOpenIdFor(id, open, int32(len(menus)))
		itemID := Menu_MenuBarOpenIdFor(id, int32(i), int32(len(menus)))
		pointerDecision := Menu_MenuGroupPointerDecisionFor(itemID, int32(i),
			openID, tapped, tapped)
		if pointerDecision.SetFocus {
			r.setFocus(id)
		}
		if pointerDecision.ChangedOpen {
			open = Menu_MenuBarOpenIndexFor(id, pointerDecision.NextOpenId,
				int32(len(menus)))
			if open < 0 {
				delete(r.openMenus, id)
			} else {
				r.openMenus[id] = open
			}
		}
		if pointerDecision.ClearSubmenu {
			delete(r.openSubmenus, id)
		}
		if pointerDecision.ResetNavigation {
			state.Top = pointerDecision.NavigationTop
			resetMenuPath(state, limitedMenuItems(menu.Items, menu.ItemCount))
		}
		itemSelected := open == int32(i)
		itemFocused := focused && open < 0 && state.Top == int32(i)
		itemFrame := simpleStyleFrameWithClassRole(ButtonToneNeutral, func() ButtonState {
			if itemSelected {
				return ButtonStateSelected
			}
			if itemFocused {
				return ButtonStateFocus
			}
			return ButtonStateNormal
		}(), false, itemSelected, className, StyleSheet_StyleKindMenuItem(), StyleSheet_StyleAny())
		itemStyle := unpackStyle(itemFrame.Value)
		itemFont, itemFontID := styleTextFace(itemStyle, font)
		if itemSelected || itemFocused {
			op := styleFrameRectOp(item, bounds, itemFrame)
			op.Selected = itemSelected
			op.Focused = itemFocused
			r.record(op)
		}
		labelX := Menu_MenuBarLabelX(item, metrics)
		labelY := Menu_MenuBarLabelY(item, itemFont)
		r.record(FrameOp{Kind: FrameOpText, Bounds: Rectangle{X: float32(labelX), Y: float32(labelY), Width: item.X + item.Width - float32(labelX), Height: item.Height}, Text: menu.Label, Color: itemStyle.Foreground, Opacity: itemStyle.Opacity, FontSize: itemFont, FontID: itemFontID})
		x = Menu_MenuBarNextItemX(x, w, metrics)
	}
	if open >= 0 && int(open) < len(menus) {
		result.OpenIndex = open
		if openIndex != nil {
			*openIndex = open
		}
		menu := menus[open]
		menuItemX := Menu_MenuBarFirstItemX(bounds, metrics)
		menuItemWidth := int32(0)
		for i := 0; i < int(open); i++ {
			w := Menu_MenuGroupItemWidth(int32(runtimeTextWidthWithFont(menus[i].Label, font, fontID)), metrics)
			menuItemX = Menu_MenuBarNextItemX(menuItemX, w, metrics)
		}
		menuItemWidth = Menu_MenuGroupItemWidth(int32(runtimeTextWidthWithFont(menu.Label, font, fontID)), metrics)
		menuItem := Menu_MenuGroupItemBounds(menuItemX, bounds, menuItemWidth, metrics)
		origin := PopupPolicy_PopupMenuBarOrigin(menuItem, bounds)
		items := limitedMenuItems(menu.Items, menu.ItemCount)
		handled := openedByKeyboard
		result.ActivatedID, _ = r.drawPopupMenu(id, className, int32(origin.X), int32(origin.Y), items, id, 0, &handled)
		if result.ActivatedID != 0 {
			delete(r.openMenus, id)
			delete(r.openSubmenus, id)
			open = -1
			state.Path = state.Path[:0]
		}
	}
	if open < 0 && openIndex != nil {
		*openIndex = -1
	}
	return result
}

func limitedMenus(menus []MenuGroup, count int32) []MenuGroup {
	if count <= 0 || int(count) > len(menus) {
		return menus
	}
	return menus[:count]
}

func limitedMenuItems(items []MenuItem, count int32) []MenuItem {
	if count <= 0 || int(count) > len(items) {
		return items
	}
	return items[:count]
}

func (r *runtime) drawPopupMenu(id, className, x, y int32, items []MenuItem, focusID int32, depth int, handled *bool) (int32, Rectangle) {
	panelFrame := simpleStyleFrameWithClassRole(ButtonToneNeutral, ButtonStateNormal,
		false, false, className, StyleSheet_StyleKindMenu(), Menu_MenuPopupRole())
	barFrame := simpleStyleFrameWithClassRole(ButtonToneNeutral, ButtonStateNormal,
		false, false, className, StyleSheet_StyleKindMenu(), Menu_MenuBarRole())
	baseFrame := simpleStyleFrameWithClassRole(ButtonToneNeutral, ButtonStateNormal,
		false, false, className, StyleSheet_StyleKindMenuItem(), StyleSheet_StyleAny())
	baseStyle := unpackStyle(baseFrame.Value)
	font, fontID := styleTextFace(baseStyle, Text14)
	metrics := Menu_MenuMetricsFor(1, panelFrame, baseFrame, barFrame)
	width := metrics.PanelMinWidth
	for _, item := range items {
		accelWidth := int32(0)
		if item.Accelerator != "" {
			accelWidth = int32(runtimeTextWidthWithFont(item.Accelerator, font, fontID))
		}
		width = Menu_MenuPanelWidthStep(width,
			int32(runtimeTextWidthWithFont(item.Label, font, fontID)), accelWidth,
			item.Accelerator != "", metrics)
	}
	panel := Menu_MenuPanelBounds(x, y, width, int32(len(items)), metrics)
	if len(items) == 0 {
		return 0, panel
	}
	state := r.menuNav(focusID)
	keyboard := !r.contentDisabled() && focusID != 0 && r.focusID == focusID && !r.popupFocusCaptures(focusID)
	if keyboard {
		for len(state.Path) <= depth {
			state.Path = append(state.Path, menuFirstItem(items))
		}
		selected := state.Path[depth]
		if selected < 0 || selected >= len(items) || items[selected].Kind == MenuSeparator || items[selected].Disabled {
			selected = menuFirstItem(items)
			state.Path[depth] = selected
		}
		if !*handled && depth == len(state.Path)-1 && selected >= 0 {
			keyboardInput := Menu_MenuKeyboardInputFor(r.keyDown[KeyUp],
				r.keyDown[KeyDown], r.keyDown[KeyHome], r.keyDown[KeyEnd],
				r.keyDown[KeyLeft], r.keyDown[KeyRight],
				r.keyDown[KeyEnter] || r.keyDown[335], r.keyDown[KeySpace],
				false)
			keyboardDecision := Menu_MenuKeyboardDecisionFor(keyboardInput,
				int32(depth), int32(selected))
			if keyboardDecision.MoveDelta != 0 {
				state.Path[depth] = menuItemAt(items, selected,
					int(keyboardDecision.MoveDelta))
				*handled = true
			} else if keyboardDecision.First {
				state.Path[depth] = menuFirstItem(items)
				*handled = true
			} else if keyboardDecision.Last {
				state.Path[depth] = menuLastItem(items)
				*handled = true
			} else if keyboardDecision.CloseParent {
				state.Path = state.Path[:depth]
				*handled = true
			} else if keyboardDecision.OpenOrActivate {
				item := items[selected]
				opensSubmenu := Menu_MenuItemCanOpenSubmenu(int32(item.Kind),
					item.Disabled, item.Submenu != nil,
					int32(len(limitedMenuItems(item.Submenu, item.SubmenuCount))),
					int32(depth), menuMaxDepth)
				if opensSubmenu {
					r.openSubmenus[id] = item.ID
					state.Path = append(state.Path, menuFirstItem(limitedMenuItems(item.Submenu, item.SubmenuCount)))
					*handled = true
				} else if Menu_MenuItemKeyboardActivates(int32(item.Kind),
					item.Disabled, opensSubmenu) {
					*handled = true
					return item.ID, panel
				}
			}
		}
	}
	separatorStyle := unpackStyle(simpleStyleFrameWithClassRole(ButtonToneNeutral, ButtonStateNormal,
		false, false, className, StyleSheet_StyleKindMenuSeparator(), StyleSheet_StyleAny()).Value)
	r.record(styleFrameRectOp(panel, Rectangle{}, panelFrame))
	for i, item := range items {
		row := Menu_MenuRowBounds(panel, int32(i), metrics)
		if item.Kind == MenuSeparator {
			line := Menu_MenuSeparatorLineFor(row, metrics)
			r.record(FrameOp{Kind: FrameOpLine, Bounds: Rectangle{X: float32(line.X1), Y: float32(line.Y1), Width: float32(line.X2 - line.X1)}, Color: separatorStyle.Border})
			continue
		}
		hovered := !r.contentDisabled() && pointInRect(r.mousePos.X, r.mousePos.Y, row)
		selected := keyboard && len(state.Path) > depth && state.Path[depth] == i
		itemFrame := simpleStyleFrameWithClassRole(ButtonToneNeutral, func() ButtonState {
			if item.Disabled {
				return ButtonStateDisabled
			}
			if hovered {
				return ButtonStateHover
			}
			if selected {
				return ButtonStateSelected
			}
			return ButtonStateNormal
		}(), item.Disabled, selected, className, StyleSheet_StyleKindMenuItem(), StyleSheet_StyleAny())
		itemStyle := unpackStyle(itemFrame.Value)
		itemFont, itemFontID := styleTextFace(itemStyle, font)
		if hovered && !item.Disabled {
			if len(state.Path) > depth {
				state.Path[depth] = i
				state.Path = state.Path[:depth+1]
			}
			op := styleFrameRectOp(row, panel, itemFrame)
			op.Hovered = true
			r.record(op)
			if item.Kind == MenuSubmenu {
				r.openSubmenus[id] = item.ID
			}
		}
		if selected && !hovered {
			op := styleFrameRectOp(row, panel, itemFrame)
			op.Selected = true
			r.record(op)
		}
		tapped := !item.Disabled && r.consumeTap(row)
		pointerDecision := Menu_MenuItemPointerDecisionFor(hovered, tapped,
			r.focusID == focusID, int32(item.Kind), item.Disabled, item.ID,
			int32(i), int32(depth), menuMaxDepth)
		if pointerDecision.ResetNavigation {
			resetMenuPath(state, items)
		}
		if pointerDecision.SetNavigationPath {
			for len(state.Path) <= int(pointerDecision.NavigationDepth) {
				state.Path = append(state.Path, menuFirstItem(items))
			}
			state.Path[pointerDecision.NavigationDepth] =
				int(pointerDecision.NavigationIndex)
			if pointerDecision.ClearChildNavigation {
				state.Path = state.Path[:int(pointerDecision.NavigationDepth)+1]
			}
		}
		if pointerDecision.SetFocus {
			r.setFocus(focusID)
		}
		if pointerDecision.SetSubmenu {
			r.openSubmenus[id] = pointerDecision.SubmenuId
		}
		if pointerDecision.Activate {
			return pointerDecision.ActivatedID, panel
		}
		textColor := itemStyle.Foreground
		label := item.Label
		if (item.Kind == MenuCheck || item.Kind == MenuRadio) && item.Checked {
			label = "✓ " + label
		}
		labelX := Menu_MenuLabelX(row, metrics)
		textY := Menu_MenuTextY(row, itemFont)
		r.record(FrameOp{Kind: FrameOpText, Bounds: Rectangle{X: float32(labelX), Y: float32(textY), Width: row.X + row.Width - float32(labelX), Height: row.Height}, Text: label, Color: textColor, Opacity: itemStyle.Opacity, FontSize: itemFont, FontID: itemFontID, Disabled: item.Disabled})
		if item.Accelerator != "" {
			accelWidth := runtimeTextWidthWithFont(item.Accelerator, itemFont, itemFontID)
			accelX := Menu_MenuAcceleratorX(row, int32(accelWidth), metrics)
			r.record(FrameOp{Kind: FrameOpText, Bounds: Rectangle{X: float32(accelX), Y: float32(textY), Width: float32(accelWidth), Height: row.Height}, Text: item.Accelerator, Color: textColor, Opacity: itemStyle.Opacity, FontSize: itemFont, FontID: itemFontID, Disabled: item.Disabled})
		}
		if item.Kind == MenuSubmenu {
			indicatorX := Menu_MenuSubmenuIndicatorX(row, metrics)
			r.record(FrameOp{Kind: FrameOpText, Bounds: Rectangle{X: float32(indicatorX), Y: float32(textY), Width: 12, Height: row.Height}, Text: ">", Color: textColor, Opacity: itemStyle.Opacity, FontSize: itemFont, FontID: itemFontID})
			submenuOpen := r.openSubmenus[id] == item.ID
			if keyboard {
				submenuOpen = selected && len(state.Path) > depth+1
			}
			if submenuOpen {
				subitems := limitedMenuItems(item.Submenu, item.SubmenuCount)
				origin := Menu_MenuSubmenuOrigin(row)
				activated, _ := r.drawPopupMenu(item.ID, className, int32(origin.X), int32(origin.Y), subitems, focusID, depth+1, handled)
				if activated != 0 {
					return activated, panel
				}
			}
		}
	}
	return 0, panel
}

func (r *runtime) popupMenu(id, className, x, y int32, items []MenuItem, itemCount int32) int32 {
	items = limitedMenuItems(items, itemCount)
	if !r.contentDisabled() {
		r.registerField(id)
	}
	state := r.menuNav(id)
	if Menu_MenuEscapeShouldClose(!r.contentDisabled() && r.focusID == id,
		r.popupFocusCaptures(id), r.keyDown[KeyEscape]) {
		state.Path = state.Path[:0]
		r.setFocus(0)
		return 0
	}
	handled := false
	selected, _ := r.drawPopupMenu(id, className, x, y, items, id, 0, &handled)
	return selected
}

func (r *runtime) contextMenu(props MenuProps) int32 {
	if props.ID == 0 {
		return 0
	}
	if props.Open != nil && *props.Open == 0 {
		delete(r.contextMenus, props.ID)
		delete(r.openSubmenus, props.ID)
		if state := r.menuNavigation[props.ID]; state != nil {
			state.Path = state.Path[:0]
		}
	}
	if props.Open != nil && *props.Open != 0 {
		pos := r.mousePos
		if props.X != nil {
			pos.X = float32(*props.X)
		}
		if props.Y != nil {
			pos.Y = float32(*props.Y)
		}
		r.contextMenus[props.ID] = pos
	}
	activation := PopupPolicy_PopupContextActivationFor(
		PopupPolicy_PopupDecisionFor(uint32(PopupContext), false), r.scrollClip(props.Trigger),
		r.mousePos, r.contentDisabled(), r.popupCaptures(r.mousePos.X, r.mousePos.Y),
		r.mouseReleased[MouseButtonRight])
	if activation.Open {
		r.contextMenus[props.ID] = activation.Origin
		r.setFocus(props.ID)
		resetMenuPath(r.menuNav(props.ID), limitedMenuItems(props.Items, props.ItemCount))
		if props.Open != nil {
			openResult := Menu_MenuContextOpenFor(*props.Open != 0, true, false, props.Open != nil)
			*props.Open = boolInt(openResult.Open)
		}
		if props.X != nil {
			*props.X = int32(activation.Origin.X)
		}
		if props.Y != nil {
			*props.Y = int32(activation.Origin.Y)
		}
	}
	pos, open := r.contextMenus[props.ID]
	if !open {
		return 0
	}
	if !r.contentDisabled() {
		r.registerField(props.ID)
	}
	if Menu_MenuEscapeShouldClose(!r.contentDisabled() && r.focusID == props.ID,
		r.popupFocusCaptures(props.ID), r.keyDown[KeyEscape]) {
		delete(r.contextMenus, props.ID)
		delete(r.openSubmenus, props.ID)
		r.menuNav(props.ID).Path = r.menuNav(props.ID).Path[:0]
		if props.Open != nil {
			openResult := Menu_MenuContextOpenFor(*props.Open != 0, false, true, props.Open != nil)
			*props.Open = boolInt(openResult.Open)
		}
		return 0
	}
	handled := false
	selected, panel := r.drawPopupMenu(props.ID, props.ClassName, int32(pos.X), int32(pos.Y), limitedMenuItems(props.Items, props.ItemCount), props.ID, 0, &handled)
	closeMenu := selected != 0
	if !closeMenu {
		for i := range r.taps {
			tap := r.taps[i]
			suppress := r.contentDisabled() || r.popupCaptures(tap.x, tap.y) ||
				Menu_MenuContextShouldSuppressClose(activation.Open,
					pointInRect(tap.x, tap.y, props.Trigger), true)
			decision := Menu_MenuContextOutsideCloseDecisionFor(props.ID, props.ID,
				suppress, !tap.consumed, true, pointInRect(tap.x, tap.y, panel))
			if decision.ConsumeRelease {
				r.taps[i].consumed = true
			}
			if decision.CloseOpen {
				closeMenu = true
				break
			}
		}
	}
	if closeMenu {
		delete(r.contextMenus, props.ID)
		delete(r.openSubmenus, props.ID)
		r.menuNav(props.ID).Path = r.menuNav(props.ID).Path[:0]
		if props.Open != nil {
			openResult := Menu_MenuContextOpenFor(*props.Open != 0, false, true, props.Open != nil)
			*props.Open = boolInt(openResult.Open)
		}
	}
	return selected
}

func (r *runtime) Menu(props MenuProps) MenuResult {
	result := MenuResult{OpenIndex: -1}
	switch props.Mode {
	case MenuModeBar:
		return r.menuBar(props.ID, props.ClassName, props.Bounds,
			limitedMenus(props.Menus, props.MenuCount), props.OpenIndex)
	case MenuModePopup:
		result.ActivatedID = r.popupMenu(props.ID, props.ClassName, int32(props.Bounds.X),
			int32(props.Bounds.Y), props.Items, props.ItemCount)
		return result
	default:
		result.ActivatedID = r.contextMenu(props)
		return result
	}
}
