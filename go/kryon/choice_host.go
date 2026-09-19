package kryon

import (
	"time"
)

func (r *runtime) Selectable(props SelectableProps) bool {
	props.Bounds = r.layoutRect(props.Bounds)
	selected := props.Selected != nil && *props.Selected != 0
	pressed, focused := r.focusablePress(props.Bounds, props.ID, props.Disabled)
	toggle := Selectable_SelectableToggleFor(selected, pressed, props.Selected != nil)
	selected = toggle.Selected
	if toggle.Changed && props.Selected != nil {
		if toggle.Selected {
			*props.Selected = 1
		} else {
			*props.Selected = 0
		}
	}
	state := ButtonStateNormal
	if props.Disabled {
		state = ButtonStateDisabled
	} else if pressed {
		state = ButtonStatePressed
	} else if selected {
		state = ButtonStateSelected
	}
	face := simpleStyleFrameWithClassRole(ButtonToneNeutral, state,
		props.Disabled, selected, props.ClassName,
		StyleSheet_StyleKindSelectable(), StyleSheet_StyleAny())
	style := unpackStyle(face.Value)
	labelInset := style.PaddingX
	if labelInset <= 0 {
		labelInset = 8
	}
	font, fontID := styleTextFace(style, Text14)
	paint := Selectable_SelectablePaintFor(SelectableSpec{
		Bounds:     props.Bounds,
		Selected:   selected,
		Pressed:    pressed,
		Disabled:   props.Disabled,
		Face:       face,
		LabelInset: labelInset,
	})
	if paint.DrawFill {
		r.record(FrameOp{Kind: FrameOpRect, Bounds: paint.Bounds, Color: unpackRGBA(paint.FillColor), Opacity: style.Opacity, Disabled: props.Disabled})
	}
	r.record(FrameOp{Kind: FrameOpText, Bounds: Rectangle{X: paint.LabelX, Y: props.Bounds.Y + 6, Width: props.Bounds.Width - labelInset*2, Height: props.Bounds.Height}, Text: props.Label, Color: unpackRGBA(paint.TextColor), Opacity: style.Opacity, FontSize: font, FontID: fontID, ID: props.ID, Disabled: props.Disabled, Pressed: pressed, Selected: selected, Focused: focused})
	return pressed
}

func (r *runtime) Checkbox(props CheckboxProps) bool {
	props.Bounds = r.layoutRect(props.Bounds)
	disabled := props.Disabled || (props.Value == nil && props.Flags == nil)
	r.prepareAccessibility(props.ID, int32(WidgetKindCheckbox), !disabled)
	input := r.ReadActivation(props.Bounds, props.ID, !disabled)
	checked := false
	changed := false
	if props.Flags != nil {
		state := Checkbox_CheckboxFlagApply(uint32(*props.Flags), uint32(props.FlagsValue), input.Activated)
		checked = state.Checked
		changed = state.Changed
		if state.Changed {
			*props.Flags = int32(state.Flags)
		}
	} else if props.Value != nil {
		state := Checkbox_CheckboxValueApply(*props.Value != 0, input.Activated, true)
		checked = state.Checked
		changed = state.Changed
		if state.Changed {
			if state.Checked {
				*props.Value = 1
			} else {
				*props.Value = 0
			}
		}
	}
	state := checkboxButtonState(input.Hovered, input.Pressed, input.Focused, disabled)
	box := checkboxStyleFrame(ButtonToneNeutral, state, disabled, checked, props.ClassName, Checkbox_CheckboxBoxRoleForTone(ButtonToneNeutral))
	active := checkboxStyleFrame(ButtonToneAccent, state, disabled, checked, props.ClassName, Checkbox_CheckboxBoxRoleForTone(ButtonToneAccent))
	label := checkboxStyleFrame(ButtonToneNeutral, state, disabled, checked, props.ClassName, Checkbox_CheckboxLabelRole())
	paint := Checkbox_CheckboxPaintFor(CheckboxSpec{
		Bounds:  props.Bounds,
		Checked: checked,
		Enabled: !disabled,
		Hovered: input.Hovered,
		Pressed: input.Pressed,
		Focused: input.Focused,
		Scale:   1,
		Box:     box,
		Active:  active,
		Label:   label,
	})
	labelStyle := unpackStyle(label.Value)
	labelFont, labelFontID := styleTextFace(labelStyle, Text14)
	fill := unpackRGBA(box.Value.Background)
	if paint.ShowFill {
		fill = unpackRGBA(paint.FillColor)
	}
	r.record(FrameOp{Kind: FrameOpRect, Bounds: paint.BoxBounds, Color: fill, BorderColor: unpackRGBA(paint.BorderColor), ID: props.ID, Disabled: disabled, Pressed: input.Pressed, Selected: checked, Focused: input.Focused,
		Role: "checkbox", AccessibleLabel: props.Label, AccessibleBounds: props.Bounds,
		accessibilityKind: int32(WidgetKindCheckbox)})
	if paint.ShowMark {
		r.record(FrameOp{Kind: FrameOpLine, Bounds: Rectangle{X: paint.CheckStart.X, Y: paint.CheckStart.Y, Width: paint.CheckMiddle.X - paint.CheckStart.X, Height: paint.CheckMiddle.Y - paint.CheckStart.Y}, Color: unpackRGBA(paint.MarkColor), ID: props.ID})
		r.record(FrameOp{Kind: FrameOpLine, Bounds: Rectangle{X: paint.CheckMiddle.X, Y: paint.CheckMiddle.Y, Width: paint.CheckEnd.X - paint.CheckMiddle.X, Height: paint.CheckEnd.Y - paint.CheckMiddle.Y}, Color: unpackRGBA(paint.MarkColor), ID: props.ID})
	}
	labelX := Checkbox_CheckboxLabelXFor(paint.SlotBounds, 1, label)
	labelY := Checkbox_CheckboxLabelYFor(props.Bounds, float32(labelFont))
	r.record(FrameOp{Kind: FrameOpText, Bounds: Rectangle{X: labelX, Y: labelY, Width: props.Bounds.Width - (labelX - props.Bounds.X), Height: props.Bounds.Height}, Text: props.Label, Color: unpackRGBA(paint.LabelColor), Opacity: labelStyle.Opacity, FontSize: labelFont, FontID: labelFontID, Disabled: disabled, Role: "presentation"})
	return changed
}

func checkboxStyleFrame(tone ButtonTone, state ButtonState, disabled, selected bool, className int32, role int32) StyleFrame {
	props := ButtonProps{
		ClassName: className,
		Tone:      tone,
		Emphasis:  ButtonEmphasisOutline,
		Size:      ControlSizeMedium,
		Disabled:  disabled,
		Selected:  selected,
	}
	if tone == ButtonToneAccent {
		props.Emphasis = ButtonEmphasisFilled
	}
	return resolveMinimalControlRoleFrame(props, state, false, 0, 0, 0,
		StyleSheet_StyleKindCheckbox(), role)
}

func checkboxButtonState(hovered, pressed, focused, disabled bool) ButtonState {
	switch {
	case disabled:
		return ButtonStateDisabled
	case pressed:
		return ButtonStatePressed
	case focused:
		return ButtonStateFocus
	case hovered:
		return ButtonStateHover
	default:
		return ButtonStateNormal
	}
}

func (r *runtime) Bullet(bounds Rectangle) {
	bounds = r.layoutRect(bounds)
	frame := simpleStyleFrameWithRole(ButtonToneNeutral, ButtonStateNormal, false, false,
		StyleSheet_StyleKindSeparator(), Separator_SeparatorBulletRole())
	paint := Separator_BulletPaintFor(bounds, frame)
	r.record(FrameOp{Kind: FrameOpRect, Bounds: paint.Bounds, Color: unpackRGBA(paint.Color)})
}

func (r *runtime) Separator(props SeparatorProps) {
	props.Bounds = r.layoutRect(props.Bounds)
	state := ButtonStateNormal
	if props.Disabled {
		state = ButtonStateDisabled
	}
	frame := simpleStyleFrameWithClassRole(ButtonToneNeutral, state,
		props.Disabled, false, props.ClassName, StyleSheet_StyleKindSeparator(), Separator_SeparatorLabelRole())
	if props.Label == "" {
		lineFrame := simpleStyleFrameWithClassRole(ButtonToneNeutral, state,
			props.Disabled, false, props.ClassName, StyleSheet_StyleKindSeparator(), Separator_SeparatorLineRole())
		paint := Separator_SeparatorLineFor(props.Bounds, props.Vertical, lineFrame)
		r.record(FrameOp{Kind: FrameOpLine, Bounds: paint.Line, Color: unpackRGBA(paint.Color)})
		return
	}
	labelStyle := unpackStyle(frame.Value)
	font, fontID := styleTextFace(labelStyle, Text14)
	if font <= 0 {
		font = Text14
	}
	labelWidth := float32(runtimeTextWidthWithFont(props.Label, font, fontID))
	paint := Separator_SeparatorLabelPaintFor(props.Bounds, labelWidth, props.Label != "", font, 1, frame)
	lineFrame := simpleStyleFrameWithClassRole(ButtonToneNeutral, state,
		props.Disabled, false, props.ClassName, StyleSheet_StyleKindSeparator(), Separator_SeparatorLineRole())
	paint.LineColor = lineFrame.Value.Background
	if paint.ShowText {
		r.record(FrameOp{Kind: FrameOpText, Bounds: paint.Text, Text: props.Label, Color: unpackRGBA(paint.TextColor), Opacity: labelStyle.Opacity, FontSize: font, FontID: fontID, Disabled: props.Disabled})
	}
	if paint.ShowLine {
		r.record(FrameOp{Kind: FrameOpLine, Bounds: paint.Line, Color: unpackRGBA(paint.LineColor), Disabled: props.Disabled})
	}
}

func (r *runtime) DragDrop(props DragDropProps) bool {
	if props.Role == DragDropRoleTarget {
		if props.AcceptedSize != nil {
			*props.AcceptedSize = 0
		}
		bounds := r.layoutRect(props.Bounds)
		matches := DragDrop_DragDropTargetMatches(r.dragDrop.active, props.Type != "", r.dragDrop.typeName == props.Type)
		disabled := props.Disabled || r.contentDisabled()
		hot := DragDrop_DragDropTargetHot(props.Disabled, r.contentDisabled(), r.pointerCanReach(bounds))
		if matches {
			frame := simpleStyleFrameWithClassRole(ButtonToneNeutral, func() ButtonState {
				if disabled {
					return ButtonStateDisabled
				}
				if hot {
					return ButtonStateHover
				}
				return ButtonStateNormal
			}(), disabled, hot, props.ClassName, StyleSheet_StyleKindDragDropTarget(), StyleSheet_StyleAny())
			op := styleFrameRectOp(bounds, Rectangle{}, frame)
			op.ID = props.ID
			op.Disabled = disabled
			op.Hovered = hot
			op.Selected = hot
			r.record(op)
		}
		if !DragDrop_DragDropTargetAccepts(props.Disabled, r.contentDisabled(), matches, hot, r.mouseReleased[MouseButtonLeft]) {
			return false
		}
		size := int(props.OutputSize)
		if size <= 0 || size > len(props.Output) {
			size = len(props.Output)
		}
		if size > len(r.dragDrop.data) {
			size = len(r.dragDrop.data)
		}
		size = int(DragDrop_DragDropCopySize(int32(len(r.dragDrop.data)), int32(size)))
		copy(props.Output[:size], r.dragDrop.data[:size])
		if props.AcceptedSize != nil {
			*props.AcceptedSize = int32(size)
		}
		r.dragDrop = dragDropState{}
		r.mouseReleased[MouseButtonLeft] = false
		return true
	}
	if DragDrop_DragDropShouldClearSource(r.dragDrop.active, r.dragDrop.sourceID, props.ID, r.mouseDown[MouseButtonLeft], r.mouseReleased[MouseButtonLeft]) {
		r.dragDrop = dragDropState{}
	}
	size := int(props.DataSize)
	if size <= 0 || size > len(props.Data) {
		size = len(props.Data)
	}
	valid := DragDrop_DragDropSourceValid(props.Disabled, r.contentDisabled(), props.Type != "", int32(size), int32(len(props.Data)), len(props.Data) > 0)
	if !valid {
		return false
	}
	bounds := r.layoutRect(props.Bounds)
	if DragDrop_DragDropSourceStarts(valid, r.pointerCanReach(bounds), r.mousePressed[MouseButtonLeft]) {
		r.dragDrop = dragDropState{active: true, sourceID: props.ID, typeName: props.Type, data: append([]byte(nil), props.Data[:size]...)}
	}
	return DragDrop_DragDropSourceReturnsActive(r.dragDrop.active, r.dragDrop.sourceID, props.ID, r.mouseDown[MouseButtonLeft], r.mouseReleased[MouseButtonLeft])
}

func (r *runtime) colorEdit(props ColorPickerProps, channels int) bool {
	if len(props.Values) < channels || int(props.ValueCount) > 0 && int(props.ValueCount) < channels {
		return false
	}
	return r.sliderFloat(sliderFloatProps{Bounds: props.Bounds, ID: props.ID, ClassName: props.ClassName, Label: props.Label, Values: props.Values[:channels], ValueCount: int32(channels), Min: 0, Max: 1, Format: "%.3f", Disabled: props.Disabled}, false)
}

func colorFromFloats(values []float32, channels int) Color {
	component := [4]float32{0, 0, 0, 1}
	for i := 0; i < channels && i < len(values); i++ {
		component[i] = values[i]
	}
	return ColorPicker_ColorPickerColorFor(component[0], component[1], component[2], component[3], int32(channels))
}

func (r *runtime) colorPickerFloat(props ColorPickerProps, channels int) bool {
	if len(props.Values) < channels || int(props.ValueCount) > 0 && int(props.ValueCount) < channels {
		return false
	}
	props.Bounds = r.layoutRect(props.Bounds)
	disabled := props.Disabled || r.contentDisabled()
	pickerFrame := simpleStyleFrameWithClassRole(ButtonToneNeutral, func() ButtonState {
		if disabled {
			return ButtonStateDisabled
		}
		return ButtonStateNormal
	}(), disabled, false, props.ClassName, StyleSheet_StyleKindColorPicker(), StyleSheet_StyleAny())
	layout := ColorPicker_ColorPickerLayoutFor(props.Bounds, int32(channels), 1, pickerFrame)
	changed := false
	for i := 0; i < channels; i++ {
		row := ColorPicker_ColorPickerChannelBounds(props.Bounds, int32(i), int32(channels), 1, pickerFrame)
		changed = r.sliderFloat(sliderFloatProps{Bounds: row, ID: ColorPicker_ColorPickerChannelIdFor(props.ID, int32(i)), ClassName: props.ClassName, Values: props.Values[i : i+1], ValueCount: 1, Min: 0, Max: 1, Format: "%.3f", Disabled: props.Disabled}, false) || changed
	}
	frame := simpleStyleFrameWithClassRole(ButtonToneNeutral, func() ButtonState {
		if disabled {
			return ButtonStateDisabled
		}
		return ButtonStateNormal
	}(), disabled, false, props.ClassName, StyleSheet_StyleKindColorPickerSwatch(), StyleSheet_StyleAny())
	style := unpackStyle(frame.Value)
	op := styleFrameRectOp(layout.SwatchBounds, props.Bounds, frame)
	op.ID = props.ID
	op.Color = colorFromFloats(props.Values, channels)
	op.Disabled = disabled
	r.record(op)
	if props.Label != "" {
		labelInset := style.PaddingX
		if labelInset <= 0 {
			labelInset = 6
		}
		font, fontID := styleTextFace(style, Text14)
		r.record(FrameOp{Kind: FrameOpText, Bounds: Rectangle{X: layout.SwatchBounds.X + labelInset, Y: layout.SwatchBounds.Y + (layout.SwatchBounds.Height-float32(font))/2, Width: layout.SwatchBounds.Width - labelInset*2, Height: float32(font)}, Text: props.Label, Color: style.Foreground, Opacity: style.Opacity, FontSize: font, FontID: fontID, ID: props.ID, Disabled: disabled})
	}
	return changed
}

func (r *runtime) ColorPicker(props ColorPickerProps) bool {
	channels := 3
	if props.ValueCount >= 4 {
		channels = 4
	}
	if props.Picker {
		return r.colorPickerFloat(props, channels)
	}
	return r.colorEdit(props, channels)
}

// TabBar is the canonical tab implementation. It owns sizing, scrolling,
// focus, keyboard navigation, closing and pointer interaction for every tab
// surface; narrower helpers only adapt their props to this function.
func (r *runtime) TabBar(props TabBarProps) int32 {
	resetIndex := func(value *int32) {
		if value != nil {
			*value = -1
		}
	}
	resetIndex(props.ClosedIndex)
	resetIndex(props.DoubleClickedIndex)
	resetIndex(props.ReorderedFromIndex)
	resetIndex(props.ReorderedToIndex)
	resetIndex(props.MiddleClickedIndex)
	if props.SelectedTabBounds != nil {
		*props.SelectedTabBounds = Rectangle{}
	}
	count := int(props.Count)
	if count <= 0 || count > len(props.Tabs) {
		count = len(props.Tabs)
	}
	if count == 0 {
		return -1
	}
	bounds := r.layoutRect(props.Bounds)
	if bounds.Width <= 0 || bounds.Height <= 0 {
		return -1
	}
	disabled := props.Disabled || r.contentDisabled()
	if !disabled && props.ID > 0 {
		r.registerField(props.ID)
	}
	focused := !disabled && props.ID > 0 && r.focusID == props.ID && !r.popupFocusCaptures(props.ID)
	barFrame := simpleStyleFrameWithClassRole(ButtonToneNeutral, func() ButtonState {
		if disabled {
			return ButtonStateDisabled
		}
		return ButtonStateNormal
	}(), disabled, false, props.ClassName, StyleSheet_StyleKindTabBar(), StyleSheet_StyleAny())
	var tabGap int32
	selected := props.SelectedIndex
	if selected < 0 || int(selected) >= count {
		selected = 0
	}
	nextEnabled := func(from, direction int32) int32 {
		for step := 1; step <= count; step++ {
			i := (from + direction*int32(step)) % int32(count)
			if i < 0 {
				i += int32(count)
			}
			if !props.Tabs[i].Disabled {
				return i
			}
		}
		return from
	}
	clicked := int32(-1)
	if focused {
		remaining := r.inputEvents[:0]
		for _, event := range r.inputEvents {
			handled := false
			if !event.shortcut {
				switch event.key {
				case KeyLeft, KeyUp:
					clicked, handled = nextEnabled(selected, -1), true
				case KeyRight, KeyDown:
					clicked, handled = nextEnabled(selected, 1), true
				case KeyHome:
					clicked, handled = nextEnabled(-1, 1), true
				case KeyEnd:
					clicked, handled = nextEnabled(0, -1), true
				case KeyDelete, KeyBackspace:
					if props.Tabs[selected].Closeable && props.ClosedIndex != nil {
						*props.ClosedIndex, handled = selected, true
					}
				}
			}
			if handled && clicked >= 0 {
				selected = clicked
			}
			if !handled {
				remaining = append(remaining, event)
			}
		}
		r.inputEvents = remaining
	}
	tabFrame := simpleStyleFrameWithClassRole(ButtonToneNeutral,
		func() ButtonState {
			if disabled {
				return ButtonStateDisabled
			}
			return ButtonStateNormal
		}(), disabled, false, props.ClassName, StyleSheet_StyleKindTab(), StyleSheet_StyleAny())
	metricCloseFrame := simpleStyleFrameWithClassRole(ButtonToneNeutral,
		func() ButtonState {
			if disabled {
				return ButtonStateDisabled
			}
			return ButtonStateNormal
		}(), disabled, false, props.ClassName, StyleSheet_StyleKindTabClose(), StyleSheet_StyleAny())
	tabStyle := unpackStyle(tabFrame.Value)
	font, fontID := styleTextFace(tabStyle, Text12)
	metrics := TabBar_TabBarDefaultMetrics(props.MinTabWidth, props.MaxTabWidth,
		1, barFrame, tabFrame, metricCloseFrame)
	tabGap = metrics.Gap
	widths := make([]float32, count)
	totalWidth := int32(0)
	for i := 0; i < count; i++ {
		labelWidth := int32(runtimeTextWidthWithFont(props.Tabs[i].Label, font, fontID))
		w := TabBar_TabBarTabWidth(labelWidth, props.Tabs[i].Label != "", false, props.Tabs[i].Closeable, metrics)
		widths[i] = float32(w)
		totalWidth += w
	}
	totalWidth = TabBar_TabBarTotalWidth(totalWidth, int32(count), tabGap)
	scrollState := TabBar_TabBarScrollFor(bounds.Width, totalWidth, 0)
	equalTabs := scrollState.EqualTabs
	if equalTabs {
		for i := range widths {
			widths[i] = bounds.Width / float32(count)
		}
		totalWidth = int32(bounds.Width)
	}
	localScroll := int32(0)
	scroll := props.ScrollOffset
	if scroll == nil {
		if props.ID > 0 {
			if r.tabScroll == nil {
				r.tabScroll = make(map[int32]int32)
			}
			if r.tabBarsSeen == nil {
				r.tabBarsSeen = make(map[int32]bool)
			}
			localScroll = r.tabScroll[props.ID]
			r.tabBarsSeen[props.ID] = true
		}
		scroll = &localScroll
	}
	scrollState = TabBar_TabBarScrollFor(bounds.Width, totalWidth, *scroll)
	maxScroll := scrollState.MaxScroll
	*scroll = scrollState.Scroll
	if !disabled && !equalTabs && r.pointerCanReach(bounds) && r.mouseWheel != 0 {
		*scroll = min(maxScroll, max(0, *scroll-int32(r.mouseWheel*42)))
		r.mouseWheel = 0
	}
	if equalTabs {
		*scroll = 0
	}
	if props.FocusSelected && !equalTabs {
		x := bounds.X - float32(*scroll)
		for i := int32(0); i < selected; i++ {
			x += widths[i] + float32(tabGap)
		}
		*scroll = TabBar_TabBarRevealScroll(x, widths[selected], bounds, *scroll, maxScroll)
	}
	barPaint := TabBar_TabBarPaintFor(barFrame, barFrame, barFrame, 1)
	r.record(FrameOp{Kind: FrameOpRect, Bounds: bounds, Color: unpackRGBA(barPaint.BarColor), BorderColor: unpackRGBA(barPaint.BarBorderColor), BorderWidth: barFrame.Value.BorderWidth, Radius: barFrame.Value.Radius})
	x := bounds.X - float32(*scroll)
	for i := 0; i < count; i++ {
		item := props.Tabs[i]
		tab := Rectangle{X: x, Y: bounds.Y, Width: widths[i], Height: bounds.Height}
		x += widths[i] + float32(tabGap)
		itemDisabled := disabled || item.Disabled
		isSelected := int32(i) == selected
		closeWidth := float32(0)
		closeBounds := Rectangle{}
		closed := false
		if item.Closeable {
			closeWidth = min(24, tab.Width)
			closeBounds = Rectangle{X: tab.X + tab.Width - closeWidth, Y: tab.Y, Width: closeWidth, Height: tab.Height}
			closed = !itemDisabled && r.consumeTap(intersectRectangles(closeBounds, bounds))
			if closed && props.ClosedIndex != nil {
				*props.ClosedIndex = int32(i)
			}
		}
		body := Rectangle{X: tab.X, Y: tab.Y, Width: max(0, tab.Width-closeWidth), Height: tab.Height}
		pressed := !itemDisabled && !closed && r.consumeTap(intersectRectangles(body, bounds))
		if pressed {
			selected, clicked = int32(i), int32(i)
			if props.ID > 0 {
				r.setFocus(props.ID)
				focused = true
			}
			now := time.Now()
			if props.DoubleClickedIndex != nil && r.lastTabClick.id == props.ID && r.lastTabClick.bounds == bounds &&
				r.lastTabClick.index == int32(i) && now.Sub(r.lastTabClick.when) <= 450*time.Millisecond {
				*props.DoubleClickedIndex = int32(i)
			}
			r.lastTabClick = tabClick{id: props.ID, index: int32(i), when: now, bounds: bounds}
		}
		isSelected = int32(i) == selected
		if isSelected && props.SelectedTabBounds != nil {
			*props.SelectedTabBounds = tab
		}
		if !itemDisabled {
			if _, middle := r.consumeMouseButtonPoint(MouseButtonMiddle, intersectRectangles(tab, bounds)); middle && props.MiddleClickedIndex != nil {
				*props.MiddleClickedIndex = int32(i)
			}
		}
		hovered := !itemDisabled && r.pointerCanReach(intersectRectangles(tab, bounds))
		tabState := ButtonStateNormal
		if itemDisabled {
			tabState = ButtonStateDisabled
		} else if pressed {
			tabState = ButtonStatePressed
		} else if hovered {
			tabState = ButtonStateHover
		} else if isSelected {
			tabState = ButtonStateSelected
		}
		tabFrame := simpleStyleFrameWithClassRole(func() ButtonTone {
			if isSelected {
				return ButtonToneAccent
			}
			return ButtonToneNeutral
		}(), tabState, itemDisabled, isSelected, props.ClassName, StyleSheet_StyleKindTab(), StyleSheet_StyleAny())
		closeState := ButtonStateNormal
		if itemDisabled {
			closeState = ButtonStateDisabled
		}
		if item.Closeable && !itemDisabled && r.pointerCanReach(closeBounds) {
			closeState = ButtonStateHover
		}
		closeFrame := simpleStyleFrameWithClassRole(ButtonToneNeutral, closeState, itemDisabled,
			false, props.ClassName, StyleSheet_StyleKindTabClose(), StyleSheet_StyleAny())
		paint := TabBar_TabBarPaintFor(barFrame, tabFrame, closeFrame, 1)
		r.recordButton(FrameOp{Kind: FrameOpButton, Button: ButtonFrame{Props: ButtonProps{ClassName: props.ClassName}}, Opacity: 1,
			BorderWidth: paint.BorderWidth, Radius: paint.Radius,
			AmbientColor: unpackRGBA(paint.BarColor), FocusColor: unpackRGBA(paint.FocusColor), Bounds: tab, Clip: bounds, HasClip: true,
			Text: fitTabLabelWithFont(item.Label, tab.Width-closeWidth-12, font, fontID), Color: unpackRGBA(paint.TabColor),
			BorderColor: unpackRGBA(paint.TabBorderColor), TextColor: unpackRGBA(paint.TextColor), FontSize: font, FontID: fontID, ID: props.ID,
			Disabled: itemDisabled, Pressed: isSelected, Focused: focused && isSelected, Row: int32(i)})
		if item.Closeable {
			r.record(FrameOp{Kind: FrameOpText, Bounds: closeBounds, Clip: bounds, HasClip: true, Text: "×", Color: unpackRGBA(paint.CloseColor),
				FontSize: font, FontID: fontID, Disabled: itemDisabled, Pressed: closed, Row: int32(i)})
		}
	}
	if !disabled && props.ReorderedFromIndex != nil && props.ReorderedToIndex != nil {
		token := props.ID
		if r.mousePressed[MouseButtonLeft] && pointInRect(r.mousePos.X, r.mousePos.Y, bounds) {
			x = bounds.X - float32(*scroll)
			for i, width := range widths {
				if r.mousePos.X >= x && r.mousePos.X < x+width {
					r.tabDrag = tabDrag{active: true, id: token, from: int32(i), bounds: bounds}
					break
				}
				x += width + float32(tabGap)
			}
		}
		if r.tabDrag.active && r.tabDrag.id == token && r.tabDrag.bounds == bounds && r.mouseReleased[MouseButtonLeft] {
			x = bounds.X - float32(*scroll)
			to := r.tabDrag.from
			for i, width := range widths {
				if r.mousePos.X < x+width/2 {
					to = int32(i)
					break
				}
				to = int32(i)
				x += width + float32(tabGap)
			}
			if to != r.tabDrag.from {
				*props.ReorderedFromIndex, *props.ReorderedToIndex = r.tabDrag.from, to
				clicked = -1
			}
			r.tabDrag = tabDrag{}
		}
	}
	if props.ScrollOffset == nil && props.ID > 0 {
		r.tabScroll[props.ID] = *scroll
	}
	return clicked
}

// fitTabLabel truncates with an ellipsis until the label measures within
// maxWidth (rune-safe; measurement falls back to a width estimate when no
// font face is loaded, e.g. headless tests).
func fitTabLabel(label string, maxWidth float32, fontSize int32) string {
	return fitTabLabelWithFont(label, maxWidth, fontSize, 0)
}

func fitTabLabelWithFont(label string, maxWidth float32, fontSize int32, fontID uint32) string {
	if maxWidth <= 8 {
		return ""
	}
	runes := []rune(label)
	for len(runes) > 1 {
		s := string(runes)
		if w, ok := measureFontText(s, fontSize, fontID); ok {
			if w.X <= maxWidth {
				return s
			}
		} else if float32(len(runes))*float32(fontSize)*0.6 <= maxWidth {
			return s
		}
		runes = runes[:len(runes)-1]
		if w, ok := measureFontText(string(runes)+"\u2026", fontSize, fontID); ok && w.X <= maxWidth {
			return string(runes) + "\u2026"
		}
	}
	return string(runes)
}

func (r *runtime) Progress(props ProgressProps) {
	bounds := r.layoutRect(props.Bounds)
	labelFrame := simpleStyleFrameWithClassRole(ButtonToneNeutral, ButtonStateNormal,
		false, false, props.ClassName, StyleSheet_StyleKindProgress(), Progress_ProgressLabelRole())
	labelStyle := unpackStyle(labelFrame.Value)
	font, fontID := styleTextFace(labelStyle, Text14)
	labelW := float32(runtimeTextWidthWithFont(props.Label, font, fontID))
	labelLineHeight := float32(font)
	paint := Progress_ProgressPaintFor(bounds, props.Min, props.Max, props.Value,
		labelW, labelLineHeight, 1,
		simpleStyleFrameWithClassRole(ButtonToneNeutral, ButtonStateNormal,
			false, false, props.ClassName, StyleSheet_StyleKindProgress(), Progress_ProgressTrackRole()),
		simpleStyleFrameWithClassRole(ButtonToneAccent, ButtonStateNormal,
			false, true, props.ClassName, StyleSheet_StyleKindProgress(), Progress_ProgressFillRole()),
		labelFrame)
	r.record(FrameOp{Kind: FrameOpRect, Bounds: bounds, Color: unpackRGBA(paint.TrackColor), BorderColor: unpackRGBA(paint.BorderColor), BorderWidth: paint.BorderWidth, Radius: paint.Radius})
	if bounds.Width > 0 && bounds.Height > 0 && paint.Layout.Ratio > 0 {
		r.record(FrameOp{Kind: FrameOpRect, Bounds: paint.Layout.FillBounds, Color: unpackRGBA(paint.FillColor), Radius: paint.Radius, Selected: true})
	}
	if props.Label != "" {
		textColor := unpackRGBA(paint.LabelColor)
		if paint.Layout.LabelOnFill {
			textColor = unpackRGBA(paint.FilledLabelColor)
		}
		r.record(FrameOp{Kind: FrameOpText, Bounds: Rectangle{X: paint.Layout.LabelX, Y: paint.Layout.LabelY, Width: labelW, Height: labelLineHeight}, Text: props.Label, Color: textColor, Opacity: labelStyle.Opacity, FontSize: font, FontID: fontID})
	}
}

func (r *runtime) Plot(props PlotProps) {
	props.Bounds = r.layoutRect(props.Bounds)
	count := int(props.ValueCount)
	if count <= 0 || count > len(props.Values) {
		count = len(props.Values)
	}
	plotFrame := simpleStyleFrameWithClassRole(ButtonToneNeutral, ButtonStateNormal,
		false, false, props.ClassName, StyleSheet_StyleKindPlot(), StyleSheet_StyleAny())
	markFrame := simpleStyleFrameWithClassRole(ButtonToneAccent, ButtonStateSelected,
		false, true, props.ClassName, StyleSheet_StyleKindPlotMark(), StyleSheet_StyleAny())
	plotStyle := unpackStyle(plotFrame.Value)
	r.record(FrameOp{Kind: FrameOpRect, Bounds: props.Bounds, Color: plotStyle.Background, BorderColor: plotStyle.Border, BorderWidth: plotStyle.BorderWidth})
	if count == 0 {
		return
	}
	offset := int(props.Offset) % count
	offset = int(Plot_PlotOffset(int32(count), int32(offset)))
	minValue, maxValue := props.ScaleMin, props.ScaleMax
	if minValue >= maxValue {
		minValue, maxValue = props.Values[offset], props.Values[offset]
		for i := 1; i < count; i++ {
			v := props.Values[(offset+i)%count]
			if v < minValue {
				minValue = v
			}
			if v > maxValue {
				maxValue = v
			}
		}
		if minValue == maxValue {
			minValue -= 0.5
			maxValue += 0.5
		}
	}
	plotRange := Plot_PlotRangeFor(props.ScaleMin, props.ScaleMax, minValue, maxValue)
	if props.Mode == PlotBars {
		for i := 0; i < count; i++ {
			bar := Plot_PlotHistogramBar(props.Bounds, int32(i), int32(count), props.Values[(offset+i)%count], plotRange, markFrame)
			r.record(FrameOp{Kind: FrameOpRect, Bounds: bar.Bounds, Color: unpackRGBA(bar.Color), Row: int32(i)})
		}
	} else if count == 1 {
		line := Plot_PlotSingleLine(props.Bounds, props.Values[offset], plotRange, markFrame)
		r.record(FrameOp{Kind: FrameOpLine, Bounds: line.Bounds, Color: unpackRGBA(line.Color)})
	} else {
		for i := 1; i < count; i++ {
			line := Plot_PlotLineSegment(props.Bounds, int32(i), int32(count), props.Values[(offset+i-1)%count], props.Values[(offset+i)%count], plotRange, markFrame)
			r.record(FrameOp{Kind: FrameOpLine, Bounds: line.Bounds, Color: unpackRGBA(line.Color), Row: int32(i - 1)})
		}
	}
	labelWidth := float32(0)
	font, fontID := styleTextFace(plotStyle, Text14)
	if props.Label != "" {
		labelWidth = float32(runtimeTextWidthWithFont(props.Label, font, fontID))
	}
	overlayWidth := float32(0)
	if props.Overlay != "" {
		overlayWidth = float32(runtimeTextWidthWithFont(props.Overlay, font, fontID))
	}
	text := Plot_PlotTextPaintFor(props.Bounds, labelWidth, overlayWidth, 1, plotFrame, props.Label != "", props.Overlay != "")
	if props.Label != "" {
		r.record(FrameOp{Kind: FrameOpText, Bounds: text.LabelBounds, Text: props.Label, Color: unpackRGBA(text.TextColor), Opacity: plotStyle.Opacity, FontSize: font, FontID: fontID})
	}
	if props.Overlay != "" {
		r.record(FrameOp{Kind: FrameOpText, Bounds: text.OverlayBounds, Text: props.Overlay, Color: unpackRGBA(text.TextColor), Opacity: plotStyle.Opacity, FontSize: font, FontID: fontID})
	}
}
