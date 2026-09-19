package kryon

import (
	"reflect"
	"strings"
)

func (r *runtime) Dropdown(args ...any) bool {
	if len(args) == 1 {
		switch p := args[0].(type) {
		case DropdownProps:
			return r.dropdownFromProps(p)
		case *DropdownProps:
			if p == nil {
				return false
			}
			return r.dropdownFromProps(*p)
		}
	}
	if len(args) < 6 {
		return false
	}
	id, ok := anyInt32(args[0])
	if !ok {
		return false
	}
	x, ok := anyInt32(args[1])
	if !ok {
		return false
	}
	y, ok := anyInt32(args[2])
	if !ok {
		return false
	}
	w, ok := anyInt32(args[3])
	if !ok {
		return false
	}
	h, ok := anyInt32(args[4])
	if !ok {
		return false
	}
	options := args[5]
	rest := args[6:]
	labels := labelsOf(options)
	count := int32(len(labels))
	selected := dropdownSelected(rest...)
	if len(rest) > 0 {
		if v, ok := anyInt32(rest[0]); ok && v >= 0 && v < count {
			count = v
		}
	}
	if int(count) < len(labels) {
		labels = labels[:count]
	}
	if !r.contentDisabled() && selected != nil && len(labels) > 0 {
		*selected = clamp32(*selected, 0, int32(len(labels)-1))
	}
	bounds := r.layoutRect(Rectangle{X: float32(x), Y: float32(y), Width: float32(w), Height: float32(h)})
	return r.dropdownAt(id, bounds, labels, selected)
}

func (r *runtime) dropdownFromProps(p DropdownProps) bool {
	p.Bounds = r.layoutRect(p.Bounds)
	if len(p.Items) > 0 {
		count := len(p.Items)
		if p.OptionCount > 0 && int(p.OptionCount) < count {
			count = int(p.OptionCount)
		}
		labels := make([]string, count)
		for i := range labels {
			labels[i] = p.Items[i].Label
		}
		r.DisabledScope(p.Disabled)
		defer r.DisabledEndScope()
		return r.dropdownOptionsAt(p.ID, p.Bounds, labels, p.Items[:count], p.SelectedIndex, p.ClassName)
	}
	n := p.OptionCount
	if n <= 0 || n > int32(len(p.Options)) {
		n = int32(len(p.Options))
	}
	opts := p.Options[:n]
	r.DisabledScope(p.Disabled)
	defer r.DisabledEndScope()
	return r.dropdownAt(p.ID, p.Bounds, opts, p.SelectedIndex, p.ClassName)
}

func (r *runtime) GetSegmentedControlHeight(props SegmentedControlProps) int32 {
	props.Bounds = r.layoutRect(props.Bounds)
	return r.segmentedControlHeight(props)
}

func (r *runtime) SegmentedControl(props SegmentedControlProps) SegmentedControlResult {
	props.Bounds = r.layoutRect(props.Bounds)
	selected := int32(-1)
	if props.SelectedIndex != nil {
		selected = *props.SelectedIndex
	}
	result := SegmentedControlResult{
		SelectedIndex: selected,
		ClickedIndex:  -1,
		Height:        r.segmentedControlHeight(props),
	}
	count := r.segmentedControlCount(props)
	metrics := r.segmentedControlMetrics(props)
	if count <= 0 || props.Bounds.Width <= 0 || metrics.RowHeight <= 0 {
		return result
	}
	font, fontID := r.segmentedControlTextFace(props.ClassName)
	rowStart := 0
	rowWidth := int32(0)
	rowCount := 0
	y := int32(props.Bounds.Y)
	for i := 0; i <= count; i++ {
		endRow := i == count
		itemWidth := int32(0)
		if !endRow {
			itemWidth = r.segmentedOptionWidth(props.Options[i], font, fontID, metrics)
		}
		nextWidth := SegmentedControl_SegmentedNextRowWidth(rowWidth, itemWidth, metrics.Gap)
		if !endRow && !SegmentedControl_SegmentedShouldWrap(props.Wrap, rowWidth, nextWidth, int32(props.Bounds.Width)) {
			rowWidth = nextWidth
			rowCount++
			continue
		}
		if rowCount > 0 {
			row := SegmentedControl_SegmentedRowFor(props.Bounds.X, props.Bounds.Width, y,
				int32(rowStart), int32(rowCount), rowWidth, props.Wrap, metrics)
			for j := 0; j < rowCount; j++ {
				index := rowStart + j
				option := props.Options[index]
				bounds := Rectangle{
					X:      float32(row.X + int32(j)*(row.ButtonWidth+metrics.Gap)),
					Y:      float32(row.Y),
					Width:  float32(row.ButtonWidth),
					Height: float32(metrics.RowHeight),
				}
				pressed := r.segmentedButtonAt(ButtonProps{
					Bounds:    bounds,
					ID:        SegmentedControl_SegmentedFocusIdFor(props.ID, int32(index)),
					Label:     option.Label,
					Pill:      true,
					Selected:  int32(index) == selected,
					Disabled:  option.Disabled || r.contentDisabled(),
					ClassName: props.ClassName,
				}, font)
				if pressed {
					selection := SegmentedControl_SegmentedSelectionFor(
						selected, int32(index), props.SelectedIndex != nil)
					result.ClickedIndex = int32(index)
					result.SelectedIndex = selection.SelectedIndex
					if selection.Changed {
						result.Changed = 1
					}
					if selection.Changed && props.SelectedIndex != nil {
						*props.SelectedIndex = selection.SelectedIndex
					}
					selected = selection.SelectedIndex
				}
			}
			y += metrics.RowHeight + metrics.Gap
		}
		rowStart = i
		rowWidth = itemWidth
		rowCount = 1
	}
	return result
}

func (r *runtime) segmentedButtonAt(props ButtonProps, font int32) bool {
	frame, pressed := r.surfaceButtonFrameForKind(props, Rectangle{}, false,
		StyleSheet_StyleKindSegment())
	if font > 0 {
		frame.Button.Font = font
		frame.Button.Appearance.Value.FontSize = float32(font)
	} else if frame.Button.Appearance.Value.FontSize > 0 {
		frame.Button.Font = int32(frame.Button.Appearance.Value.FontSize + 0.5)
	}
	r.record(frame)
	return pressed
}

func (r *runtime) segmentedControlHeight(props SegmentedControlProps) int32 {
	count := r.segmentedControlCount(props)
	metrics := r.segmentedControlMetrics(props)
	if count <= 0 || metrics.RowHeight <= 0 {
		return 0
	}
	if props.Bounds.Width <= 0 || !props.Wrap {
		return metrics.RowHeight
	}
	font, fontID := r.segmentedControlTextFace(props.ClassName)
	rows := int32(1)
	rowWidth := int32(0)
	for i := 0; i < count; i++ {
		itemWidth := r.segmentedOptionWidth(props.Options[i], font, fontID, metrics)
		nextWidth := SegmentedControl_SegmentedNextRowWidth(rowWidth, itemWidth, metrics.Gap)
		if SegmentedControl_SegmentedShouldWrap(props.Wrap, rowWidth, nextWidth, int32(props.Bounds.Width)) {
			rows++
			rowWidth = itemWidth
		} else {
			rowWidth = nextWidth
		}
	}
	return SegmentedControl_SegmentedHeightForRows(rows, metrics.RowHeight, metrics.Gap)
}

func (r *runtime) segmentedControlCount(props SegmentedControlProps) int {
	count := len(props.Options)
	if props.OptionCount > 0 && int(props.OptionCount) < count {
		count = int(props.OptionCount)
	}
	return count
}

func (r *runtime) segmentedControlMetrics(props SegmentedControlProps) SegmentedMetrics {
	control := r.segmentedControlFrame(props.ClassName)
	segment := r.segmentFrame(props.ClassName)
	return SegmentedControl_SegmentedDefaultMetrics(props.Height,
		props.MinItemWidth, props.MaxItemWidth, 1, control, segment)
}

func (r *runtime) segmentedControlFont(className ...int32) int32 {
	size, _ := r.segmentedControlTextFace(className...)
	return size
}

func (r *runtime) segmentedControlTextFace(className ...int32) (int32, uint32) {
	styleClass := int32(0)
	if len(className) > 0 {
		styleClass = className[0]
	}
	style := unpackStyle(r.segmentFrame(styleClass).Value)
	return styleTextFace(style, Text14)
}

func (r *runtime) segmentedControlFrame(className int32) StyleFrame {
	props := ButtonProps{ClassName: int32(0)}
	props.ClassName = className
	return simpleStyleFrameWithClassRole(ButtonToneNeutral, ButtonStateNormal,
		false, false, props.ClassName, StyleSheet_StyleKindSegmentedControl(),
		StyleSheet_StyleAny())
}

func (r *runtime) segmentFrame(className int32) StyleFrame {
	props := ButtonProps{ClassName: int32(0)}
	props.ClassName = className
	return simpleStyleFrameWithClassRole(ButtonToneNeutral, ButtonStateNormal,
		false, false, props.ClassName, StyleSheet_StyleKindSegment(),
		StyleSheet_StyleAny())
}

func (r *runtime) segmentedOptionWidth(option SegmentOption, font int32, fontID uint32, metrics SegmentedMetrics) int32 {
	return SegmentedControl_SegmentedItemWidth(int32(runtimeTextWidthWithFont(option.Label, font, fontID)), metrics)
}

func (r *runtime) dropdownKeyboardAvailable(id int32) bool {
	if _, openPopup := r.popupPanels[id]; openPopup {
		return !r.popupKeyboardCapturesOwner(id, true)
	}
	return !r.popupFocusCaptures(id)
}

func (r *runtime) dropdownAt(id int32, bounds Rectangle, labels []string, selected *int32, className ...int32) bool {
	return r.dropdownOptionsAt(id, bounds, labels, nil, selected, className...)
}

func (r *runtime) dropdownOptionsAt(id int32, bounds Rectangle, labels []string, items []DropdownOption, selected *int32, className ...int32) bool {
	styleClass := int32(0)
	if len(className) > 0 {
		styleClass = className[0]
	}
	triggerFrame := StyleFrame{Value: packStyle(r.dropdownStyle(0, false,
		ButtonStateNormal, styleClass))}
	contentMetrics := Dropdown_Content(triggerFrame.Value, 1)
	triggerMetrics := Dropdown_DropdownTriggerMetricsFor(1, triggerFrame)
	disabledRow := func(index int32) bool {
		return index >= 0 && int(index) < len(items) && items[index].Disabled
	}
	if r.dropdownsSeen == nil {
		r.dropdownsSeen = make(map[int32]bool)
	}
	r.dropdownsSeen[id] = true
	if r.contentDisabled() {
		r.closeDropdown(id)
	}
	pointerActivate := r.consumeTap(bounds)
	if !r.contentDisabled() && id > 0 {
		r.registerField(id)
		if pointerActivate {
			r.setFocus(id)
		}
	}
	previousOpen := r.openDropdowns[id]
	keyboardAvailable := r.dropdownKeyboardAvailable(id)
	panel := r.dropdownPanel(bounds, len(labels), styleClass)
	outside := r.mousePressed[MouseButtonLeft] && !pointInRect(r.mousePos.X, r.mousePos.Y, bounds) && !pointInRect(r.mousePos.X, r.mousePos.Y, panel)
	for _, tap := range r.taps {
		outside = outside || (!pointInRect(tap.x, tap.y, bounds) && !pointInRect(tap.x, tap.y, panel))
	}
	triggerInput := Dropdown_DropdownTriggerInputFor(r.keyDown[KeyEnter],
		r.keyDown[335], r.keyDown[KeySpace], r.keyDown[KeyDown])
	openDecision := Dropdown_DropdownOpenDecisionFor(previousOpen,
		r.contentDisabled(), int32(len(labels)), id > 0 && r.focusID == id,
		!r.popupKeyboardCaptures(), pointerActivate, triggerInput,
		bounds.Height, keyboardAvailable && r.keyDown[KeyEscape], false, outside)
	open := openDecision.Open
	pressed := openDecision.Changed
	if openDecision.Opened {
		for other := range r.openDropdowns {
			if other != id {
				r.closeDropdown(other)
			}
		}
		if r.dropdownHighlight == nil {
			r.dropdownHighlight = make(map[int32]int32)
		}
		chosen := int32(0)
		if selected != nil {
			chosen = *selected
		}
		r.dropdownHighlight[id] = Dropdown_ClampIndex(chosen, int32(len(labels)))
		if offset := r.dropdownOffsets[id]; offset != nil {
			*offset = 0
		}
		delete(r.dropdownGestures, id)
	}
	r.openDropdowns[id] = open
	changed := false
	navigating := !pressed && keyboardAvailable && (r.keyDown[KeyUp] || r.keyDown[KeyDown] || r.keyDown[KeyHome] || r.keyDown[KeyEnd])
	if open && (pressed || navigating) {
		nav := Dropdown_StartNavigation(r.dropdownHighlight[id], int32(len(labels)),
			navigating && r.keyDown[KeyUp], navigating && r.keyDown[KeyDown],
			navigating && r.keyDown[KeyHome], navigating && r.keyDown[KeyEnd])
		for nav.Searching {
			nav = Dropdown_ScanNavigation(nav, !disabledRow(nav.Index))
		}
		if r.dropdownHighlight == nil {
			r.dropdownHighlight = make(map[int32]int32)
		}
		r.dropdownHighlight[id] = nav.Result
	}
	highlight := r.dropdownHighlight[id]
	if open && Dropdown_CanCommit(highlight >= 0 && int(highlight) < len(labels) && !disabledRow(highlight),
		pressed, keyboardAvailable, r.keyDown[KeyEnter] || r.keyDown[335], false, false, false, false) {
		if selected != nil {
			changed = *selected != highlight
			*selected = highlight
		}
		openDecision := Dropdown_DropdownOpenAfterCommit(open, true)
		open = openDecision.Open
	}
	if !open {
		r.closeDropdown(id)
	}
	focused := !r.contentDisabled() && id > 0 && r.focusID == id
	foreground := r.dropdownTrigger(id, bounds, open, focused, styleClass)
	hasIcon := selected != nil && *selected >= 0 && int(*selected) < len(items) && items[*selected].IconType != IconNone
	triggerContent := Dropdown_DropdownTriggerContentFor(bounds, contentMetrics, triggerMetrics, hasIcon)
	if hasIcon {
		r.record(FrameOp{Kind: FrameOpIcon, Bounds: triggerContent.IconBounds, IconType: items[*selected].IconType, Color: foreground, ID: id})
	}
	r.record(FrameOp{Kind: FrameOpText, Clip: triggerContent.ClipBounds, HasClip: true, Bounds: triggerContent.TextBounds, Text: selectedLabel(labels, selected), Color: foreground, FontSize: int32(contentMetrics.Font), ID: id, Row: -1})
	r.dropdownChevron(id, triggerContent, triggerMetrics.IndicatorSize, open, foreground)
	if !open {
		return changed
	}
	itemH := bounds.Height
	layer := r.beginPaintLayer(id)
	input := r.beginPopupInput(id, panel)
	defer func() {
		r.endPopupInput(input)
		r.endPaintLayer(layer)
	}()
	surface := r.dropdownSurface(panel, Dropdown_DropdownPanelRole(), false, ButtonStateNormal, styleClass)
	surface.ID = id
	r.record(surface)
	menuMetrics := Dropdown_DropdownMenuMetricsFor(1,
		r.dropdownRoleFrame(Dropdown_DropdownPanelRole(), styleClass),
		r.dropdownRoleFrame(Dropdown_DropdownOptionRole(), styleClass),
		r.dropdownRoleFrame(Dropdown_DropdownScrollbarRole(), styleClass))
	menuLayout := Dropdown_MenuLayoutFor(panel, int32(len(labels)), int32(itemH),
		menuMetrics.PaddingTop, menuMetrics.PaddingBottom,
		menuMetrics.ScrollbarWidth, menuMetrics.ScrollbarGap)
	if r.dropdownOffsets == nil {
		r.dropdownOffsets = make(map[int32]*int32)
	}
	offset := r.dropdownOffsets[id]
	if offset == nil {
		offset = new(int32)
		r.dropdownOffsets[id] = offset
	}
	viewport := menuLayout.ContentBounds
	contentHeight := menuLayout.ContentHeight
	maximum := menuLayout.MaxScroll
	if pressed || navigating {
		*offset = Dropdown_RevealRow(*offset, r.dropdownHighlight[id], itemH, viewport.Height, maximum)
	}
	if r.dropdownGestures == nil {
		r.dropdownGestures = make(map[int32]PopupGesture)
	}
	track := Dropdown_ScrollbarTrackBounds(menuLayout.ScrollbarBounds, menuMetrics.ScrollbarTrackInset)
	if maximum > 0 && !r.contentDisabled() && r.mousePressed[MouseButtonLeft] && r.consumeTap(track) {
		thumbH := min(track.Height, max(float32(menuMetrics.ScrollbarWidth), track.Height*track.Height/float32(contentHeight)))
		thumbY := track.Y
		travel := track.Height - thumbH
		if travel > 0 {
			thumbY += travel * float32(*offset) / float32(maximum)
		}
		r.scrollDragOffset = offset
		r.scrollDragOwner = r.currentPopupInputOwner()
		r.scrollDragGrab = thumbH / 2
		if r.mousePos.Y >= thumbY && r.mousePos.Y < thumbY+thumbH {
			r.scrollDragGrab = r.mousePos.Y - thumbY
		}
	}
	scrollbar := r.scrollDragOffset == offset || maximum > 0 && pointInRect(r.mousePos.X, r.mousePos.Y, track)
	gesture := Dropdown_PopupDragGesture(r.dropdownGestures[id], *offset, r.mouseDown[MouseButtonLeft],
		pointInRect(r.mousePos.X, r.mousePos.Y, panel) || pointInRect(r.mousePos.X, r.mousePos.Y, bounds),
		scrollbar, r.mousePos.Y, maximum, float32(menuMetrics.DragThreshold))
	*offset = gesture.Offset
	r.dropdownGestures[id] = gesture
	if r.pointerCanReach(viewport) && r.mouseWheel != 0 {
		*offset = Dropdown_WheelOffset(*offset, r.mouseWheel, itemH, maximum)
		r.mouseWheel = 0
	}
	// The generic scroll host owns clipping and its scrollbar. Dropdown's
	// wheel, drag, reveal, and visible-row decisions come from shared policy.
	content := r.ScrollScope(viewport, contentHeight, offset)
	defer r.ScrollEndScope()
	rows := Dropdown_Rows(int32(len(labels)), *offset, viewport.Height, itemH)
	for i := int(rows.First); i < int(rows.End); i++ {

		label := labels[i]
		optionPaint := Dropdown_OptionPaintFor(panel, menuLayout.OptionWidth,
			int32(i), int32(itemH), *offset, menuMetrics.PaddingTop,
			menuMetrics.PaddingBottom, menuMetrics.HighlightInsetX,
			menuMetrics.HighlightInsetY)
		row := Rectangle{X: content.X, Y: float32(optionPaint.OptionY),
			Width: content.Width, Height: itemH}
		visibleRow := optionPaint.VisibleBounds
		selectedRow := selected != nil && int32(i) == *selected
		highlighted := !disabledRow(int32(i)) && (r.dropdownHighlight[id] == int32(i) || pointInRect(r.mousePos.X, r.mousePos.Y, visibleRow))
		state := ButtonStateNormal
		if highlighted {
			state = ButtonStateHover
		}
		if disabledRow(int32(i)) {
			state = ButtonStateDisabled
		}
		paint := r.dropdownSurface(optionPaint.HighlightBounds, Dropdown_DropdownOptionRole(), selectedRow, state, styleClass)
		paint.ID, paint.Row, paint.Selected, paint.Focused = id, int32(i), selectedRow, highlighted
		if selectedRow || highlighted {
			r.record(paint)
		}
		hasIcon := i < len(items) && items[i].IconType != IconNone
		rowContent := Dropdown_DropdownOptionContentFor(row, visibleRow,
			contentMetrics, menuMetrics, hasIcon)
		if selected != nil && Dropdown_CanCommit(!disabledRow(int32(i)), pressed, false, false,
			r.consumeTap(row), scrollbar, gesture.Dragging, *offset == gesture.OriginOffset) {
			next := int32(i)
			if *selected != next {
				*selected = next
				changed = true
			}
			openDecision := Dropdown_DropdownOpenAfterCommit(open, true)
			open = openDecision.Open
			if openDecision.Closed {
				r.closeDropdown(id)
			}
			selectedRow = true
		}
		paint.TextColor = unpackRGBA(Surface_Opacity(packRGBA(paint.TextColor), paint.Opacity))
		if i < len(items) {
			item := items[i]
			if item.SeparatorBefore {
				r.record(FrameOp{Kind: FrameOpLine, Bounds: rowContent.SeparatorBounds, Color: r.Fade(paint.TextColor, 0.18), ID: id})
			}
			if item.IconType != IconNone {
				r.record(FrameOp{Kind: FrameOpIcon, Bounds: rowContent.IconBounds, IconType: item.IconType, Color: paint.TextColor, ID: id})
			}
		}
		r.record(FrameOp{Kind: FrameOpText, Clip: rowContent.ClipBounds, HasClip: true, Bounds: rowContent.TextBounds, Text: label, Color: paint.TextColor, FontSize: int32(contentMetrics.Font), ID: id, Row: int32(i), Selected: selectedRow})
		if selectedRow {
			r.record(FrameOp{Kind: FrameOpIcon, ID: id, Color: paint.TextColor, IconType: IconCheck,
				Bounds: rowContent.CheckBounds, Row: int32(i), Selected: true})
		}
	}
	return changed
}

func labelsOf(v any) []string {
	switch l := v.(type) {
	case string:
		if l == "" {
			return nil
		}
		return strings.Split(l, ";")
	case []string:
		return l
	}
	rv := reflect.ValueOf(v)
	if rv.Kind() == reflect.Array && rv.Type().Elem().Kind() == reflect.String {
		out := make([]string, rv.Len())
		for i := range out {
			out[i] = rv.Index(i).String()
		}
		return out
	}
	return nil
}

func anyInt32(v any) (int32, bool) {
	switch n := v.(type) {
	case int:
		return int32(n), true
	case int32:
		return n, true
	case int64:
		return int32(n), true
	case uint:
		return int32(n), true
	case uint32:
		return int32(n), true
	}
	return 0, false
}

func dropdownSelected(rest ...any) *int32 {
	for _, arg := range rest {
		if p, ok := arg.(*int32); ok {
			return p
		}
	}
	return nil
}

func selectedLabel(labels []string, selected *int32) string {
	if len(labels) == 0 {
		return ""
	}
	index := int32(0)
	if selected != nil {
		index = clamp32(*selected, 0, int32(len(labels)-1))
	}
	return labels[index]
}
