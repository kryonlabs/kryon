package kryon

import (
	"fmt"
)

func (r *runtime) Radio(props RadioProps) int32 {
	props.Bounds = r.layoutRect(props.Bounds)
	input := r.ReadActivation(props.Bounds, props.ID, !props.Disabled)
	state := checkboxButtonState(input.Hovered, input.Pressed, input.Focused, props.Disabled)
	selectedFrame := radioStyleFrame(ButtonToneAccent, state, props.Disabled, props.Checked, props.ClassName, Radio_RadioMarkRole())
	paint := Radio_RadioPaintFor(RadioSpec{
		Bounds:   props.Bounds,
		Checked:  props.Checked,
		Disabled: props.Disabled,
		SelectedAmount: func() float32 {
			if props.Checked {
				return 1
			}
			return 0
		}(),
		Scale:    1,
		Frame:    radioStyleFrame(ButtonToneNeutral, state, props.Disabled, props.Checked, props.ClassName, Radio_RadioRingRole()),
		Selected: selectedFrame,
	})
	label := radioStyleFrame(ButtonToneNeutral, state, props.Disabled, props.Checked, props.ClassName, Radio_RadioLabelRole())
	paint.LabelColor = label.Value.Foreground
	markStyle := unpackStyle(selectedFrame.Value)
	labelStyle := unpackStyle(label.Value)
	labelFont, labelFontID := styleTextFace(labelStyle, Text16)
	markFont, markFontID := styleTextFace(markStyle, Text16)
	mark := Radio_RadioMarkText(props.Checked)
	markColor := unpackRGBA(paint.RingColor)
	if props.Checked {
		markColor = unpackRGBA(paint.FillColor)
	}
	labelColor := unpackRGBA(paint.LabelColor)
	r.record(FrameOp{Kind: FrameOpText, Bounds: paint.MarkBounds, Text: mark, Color: markColor, Opacity: markStyle.Opacity, FontSize: markFont, FontID: markFontID, ID: props.ID, Pressed: input.Pressed, Disabled: props.Disabled, Selected: props.Checked, Focused: input.Focused})
	r.record(FrameOp{Kind: FrameOpText, Bounds: paint.LabelBounds, Text: props.Label, Color: labelColor, Opacity: labelStyle.Opacity, FontSize: labelFont, FontID: labelFontID, ID: props.ID, Pressed: input.Pressed, Disabled: props.Disabled, Selected: props.Checked, Focused: input.Focused})
	return Radio_RadioActivationFor(props.ID, input.Activated, props.Disabled)
}

func radioStyleFrame(tone ButtonTone, state ButtonState, disabled, selected bool, className int32, role int32) StyleFrame {
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
		StyleSheet_StyleKindRadio(), role)
}

func (r *runtime) Spinbox(p SpinboxProps) bool {
	p.Bounds = r.layoutRect(p.Bounds)
	layout := Spinbox_SpinboxLayoutFor(p.Bounds, Spinbox_SpinboxDefaultButtonWidth(1.0))
	l := layout.Left
	rr := layout.Right
	disabled := p.Disabled || r.contentDisabled()
	frame := simpleStyleFrameWithClassRole(ButtonToneNeutral, func() ButtonState {
		if disabled {
			return ButtonStateDisabled
		}
		return ButtonStateNormal
	}(), disabled, false, p.ClassName, StyleSheet_StyleKindSpinbox(), StyleSheet_StyleAny())
	op := styleFrameRectOp(p.Bounds, Rectangle{}, frame)
	op.ID = p.ID
	op.Disabled = disabled
	r.record(op)
	minus := r.buttonAt(ButtonProps{Bounds: l, Label: "-",
		ID: Spinbox_SpinboxDecrementIdFor(p.ID), ClassName: p.ClassName, Disabled: disabled})
	plus := r.buttonAt(ButtonProps{Bounds: rr, Label: "+",
		ID: Spinbox_SpinboxIncrementIdFor(p.ID), ClassName: p.ClassName, Disabled: disabled})
	changed := false
	if p.Value != nil {
		result := Spinbox_SpinboxStepButtonsValue(*p.Value, p.Min, p.Max,
			p.Step, minus, plus, p.Wrap)
		changed = result.Changed
		*p.Value = result.Value
	}
	center := layout.Text
	txt := p.ValueText
	if txt == "" {
		v := int32(0)
		if p.Value != nil {
			v = *p.Value
		}
		txt = fmt.Sprint(v)
	}
	valueFrame := simpleStyleFrameWithClassRole(ButtonToneNeutral, func() ButtonState {
		if disabled {
			return ButtonStateDisabled
		}
		return ButtonStateNormal
	}(), disabled, false, p.ClassName, StyleSheet_StyleKindSpinboxValue(), StyleSheet_StyleAny())
	valueStyle := unpackStyle(valueFrame.Value)
	valueOp := styleFrameRectOp(center, p.Bounds, valueFrame)
	valueOp.ID = p.ID
	valueOp.Disabled = disabled
	r.record(valueOp)
	valueFont, valueFontID := styleTextFace(valueStyle, Text16)
	r.record(FrameOp{Kind: FrameOpText, Bounds: center, Text: txt, Color: valueStyle.Foreground, Opacity: valueStyle.Opacity, FontSize: valueFont, FontID: valueFontID, ID: p.ID, Disabled: disabled})
	return changed
}

func (r *runtime) Fieldset(p FieldsetProps) {
	p.Bounds = r.layoutRect(p.Bounds)
	frame := simpleStyleFrameWithClassRole(ButtonToneNeutral, ButtonStateNormal,
		false, false, p.ClassName, StyleSheet_StyleKindFieldset(), StyleSheet_StyleAny())
	style := unpackStyle(frame.Value)
	font, fontID := styleTextFace(style, Text14)
	w := float32(0)
	if p.Title != "" {
		w = float32(runtimeTextWidthWithFont(p.Title, font, fontID))
	}
	paint := Fieldset_FieldsetPaintFor(p.Bounds, w, p.Title != "", 1, frame)
	r.record(styleFrameRectOp(paint.Frame, Rectangle{}, paint.Face))
	if paint.ShowTitle {
		r.record(FrameOp{Kind: FrameOpRect, Bounds: paint.TitleBackground, Color: unpackRGBA(paint.BackgroundColor)})
		r.record(FrameOp{Kind: FrameOpText, Bounds: paint.TitleText, Text: p.Title, Color: unpackRGBA(paint.TextColor), Opacity: style.Opacity, FontSize: font, FontID: fontID})
	}
}

func (r *runtime) PanedView(p PanedViewProps) int32 {
	if p.Split == nil {
		return 0
	}
	p.Bounds = r.layoutRect(p.Bounds)
	split := *p.Split
	normalFrame := StyleFrame{Value: ResolveActiveStyle(StyleData{},
		PanedView_PanedViewHandleFactsFor(p.ClassName, int32(ButtonStateNormal)),
		int32(ButtonStateNormal))}
	metrics := PanedView_PanedViewMetricsFor(1, normalFrame)
	limit := PanedView_PanedViewLimit(PanedView_PanedViewSize(p.Bounds, p.Vertical), p.MinFirst, p.MinSecond)
	split = PanedView_PanedViewClampSplit(split, p.MinFirst, limit)
	h := PanedView_PanedViewHandleFor(p.Bounds, p.Vertical, split, metrics)
	changed := int32(0)
	if r.drag.active && r.popupInputOwnerCaptures(r.drag.owner) {
		r.drag = scalarDrag{}
	}
	if r.contentDisabled() && r.drag.active && r.drag.token == p.ID {
		r.drag = scalarDrag{}
	}
	if !r.contentDisabled() && r.mousePressed[MouseButtonLeft] && r.consumeTap(h) {
		r.drag = scalarDrag{active: true, token: p.ID, owner: r.currentPopupInputOwner()}
	}
	if r.drag.active && r.drag.token == p.ID && r.mouseDown[MouseButtonLeft] {
		n := PanedView_PanedViewPointerSplit(p.Bounds, p.Vertical, r.mousePos.X, r.mousePos.Y)
		n = PanedView_PanedViewClampSplit(n, p.MinFirst, limit)
		if n != *p.Split {
			*p.Split = n
			split = n
			changed = 1
		}
	}
	h = PanedView_PanedViewHandleFor(p.Bounds, p.Vertical, split, metrics)
	if !r.mouseDown[MouseButtonLeft] && r.drag.token == p.ID {
		r.drag = scalarDrag{}
	}
	if *p.Split != split {
		*p.Split = split
		changed = 1
	}
	state := ButtonStateNormal
	if changed != 0 {
		state = ButtonStatePressed
	}
	frame := StyleFrame{Value: ResolveActiveStyle(StyleData{},
		PanedView_PanedViewHandleFactsFor(p.ClassName, int32(state)),
		int32(state))}
	op := styleFrameRectOp(h, p.Bounds, frame)
	op.ID = p.ID
	op.Pressed = changed != 0
	r.record(op)
	return changed
}

type treeHeaderNav struct{ id, depth int32 }

func (r *runtime) treeHeaderTarget(p CollapsibleProps, key int32) int32 {
	for i, node := range r.prevTreeHeaders {
		if node.id != p.ID {
			continue
		}
		scan := Focus_TreeFocusBegin(int32(i), int32(len(r.prevTreeHeaders)), node.depth,
			key == KeyDown, key == KeyUp, key == KeyRight, key == KeyLeft)
		for !scan.Done {
			scan = Focus_TreeFocusAdvance(scan, r.prevTreeHeaders[scan.Index].depth)
		}
		return r.prevTreeHeaders[scan.Index].id
	}
	return p.ID
}

func (r *runtime) Collapsible(p CollapsibleProps) int32 {
	if p.Visible != nil && !*p.Visible {
		return 0
	}
	defaultState := ButtonStateNormal
	if p.Disabled || r.contentDisabled() {
		defaultState = ButtonStateDisabled
	} else if p.Selected {
		defaultState = ButtonStateSelected
	}
	headerRole := Collapsible_CollapsibleHeaderRoleFor(p.Tree)
	defaultHeaderFrame := simpleStyleFrameWithClassRole(ButtonToneNeutral,
		defaultState, p.Disabled || r.contentDisabled(), p.Selected,
		p.ClassName, StyleSheet_StyleKindCollapsible(), headerRole)
	treeHeaderFrame := simpleStyleFrameWithClassRole(ButtonToneNeutral,
		defaultState, p.Disabled || r.contentDisabled(), p.Selected,
		p.ClassName, StyleSheet_StyleKindCollapsible(), Collapsible_CollapsibleTreeHeaderRole())
	closeDefaultFrame := simpleStyleFrameWithClassRole(ButtonToneNeutral,
		ButtonStateNormal, p.Disabled || r.contentDisabled(), false,
		p.ClassName, StyleSheet_StyleKindCollapsible(), Collapsible_CollapsibleCloseRole())
	metrics := Collapsible_CollapsibleMetricsFor(1, defaultHeaderFrame,
		treeHeaderFrame, closeDefaultFrame)
	layoutBounds := p.Bounds
	layoutBounds.Height = float32(metrics.HeaderHeight)
	p.Bounds = r.layoutRect(layoutBounds)
	layout := Collapsible_CollapsibleLayoutFor(p.Bounds, p.Tree, p.Depth, p.Visible != nil, metrics)
	enabled := !p.Disabled && !r.contentDisabled()
	if enabled {
		r.registerField(p.ID)
		if p.Tree && p.ID > 0 {
			r.treeHeaders = append(r.treeHeaders, treeHeaderNav{p.ID, max(p.Depth, 0)})
		}
	}
	header := layout.Header
	closeBounds := layout.CloseBounds
	body := layout.Body
	closed := false
	if p.Visible != nil {
		closed = enabled && r.consumeTap(closeBounds)
		if closed {
			*p.Visible = false
		}
	}
	tapped := enabled && !closed && r.consumeTap(body)
	if tapped && p.ID != 0 {
		r.setFocus(p.ID)
	}
	pressed := tapped && !p.Leaf && p.Open != nil
	if pressed {
		openResult := Collapsible_CollapsibleOpenApply(
			*p.Open, true, false, false, true)
		*p.Open = openResult.Open
		pressed = openResult.Changed
	}
	if enabled && p.ID != 0 && r.focusID == p.ID && !r.popupFocusCaptures(p.ID) {
		remaining := r.inputEvents[:0]
		for _, event := range r.inputEvents {
			handled := false
			if !event.shortcut && r.focusID == p.ID {
				if event.key == KeyTab {
					r.setFocus(r.nextFocus(p.ID, event.shift))
					handled = true
				} else {
					key := Collapsible_CollapsibleKeyFor(event.key == KeyDown,
						event.key == KeyUp, event.key == KeyRight, event.key == KeyLeft)
					open := p.Open != nil && *p.Open
					decision := Collapsible_CollapsibleKeyboardDecisionFor(true, false,
						false, p.Tree, key, open, p.Leaf, p.Open != nil,
						event.key == KeyEnter || event.key == KeySpace)
					if decision.MoveFocus {
						r.setFocus(r.treeHeaderTarget(p, event.key))
					} else if p.Open != nil {
						result := Collapsible_CollapsibleOpenApply(*p.Open,
							decision.ToggleOpen, decision.SetOpen, decision.Open, true)
						*p.Open = result.Open
						pressed = pressed || result.Changed
					}
					handled = decision.Handled
				}
			}
			if !handled {
				remaining = append(remaining, event)
			}
		}
		r.inputEvents = remaining
	}
	marker := Collapsible_CollapsibleMarkerFor(p.Open != nil && *p.Open, p.Leaf)
	mark := Collapsible_CollapsibleMarkerText(marker)
	state := ButtonStateNormal
	if !enabled {
		state = ButtonStateDisabled
	} else if pressed {
		state = ButtonStatePressed
	} else if enabled && p.ID != 0 && r.focusID == p.ID {
		state = ButtonStateFocus
	} else if p.Selected {
		state = ButtonStateSelected
	}
	buttonProps := ButtonProps{
		Bounds:    header,
		ID:        p.ID,
		ClassName: p.ClassName,
		Tone:      ButtonToneNeutral,
		Emphasis:  ButtonEmphasisSoft,
		Size:      ControlSizeMedium,
		Selected:  p.Selected,
		Disabled:  !enabled,
	}
	frame := simpleStyleFrameWithClassRole(ButtonToneNeutral, state, !enabled, p.Selected,
		p.ClassName, StyleSheet_StyleKindCollapsible(), headerRole)
	headerStyle := unpackStyle(frame.Value)
	headerFont, headerFontID := styleTextFace(headerStyle, Text16)
	label := elideTextWithFont(mark+"  "+p.Label, body.Width-12, headerFont, headerFontID)
	buttonProps.Label = label
	button := Button_BuildFrame(buttonProps, ButtonInput{}, frame, InteractionMotion{},
		Rectangle{}, packRGBA(r.appAmbientColor()), 1,
		headerFont, Text16)
	fg := unpackRGBA(button.Foreground)
	r.recordButton(FrameOp{Kind: FrameOpButton, Button: button, Opacity: button.Appearance.Value.Opacity,
		Bounds: header, Text: label, Color: unpackRGBA(button.Appearance.Value.Background),
		BorderColor: unpackRGBA(button.Appearance.Value.Border), TextColor: fg,
		BorderWidth: button.Appearance.Value.BorderWidth, Radius: button.Appearance.Value.Radius,
		Material: MaterialKind(button.Appearance.Value.Material), FontSize: headerFont, FontID: headerFontID,
		Pressed: pressed, Selected: p.Selected, ID: p.ID,
		Focused: enabled && p.ID != 0 && r.focusID == p.ID, Disabled: !enabled})
	if p.Visible != nil {
		closeState := ButtonStateNormal
		if !enabled {
			closeState = ButtonStateDisabled
		} else if closed {
			closeState = ButtonStatePressed
		}
		closeStyle := unpackStyle(simpleStyleFrameWithClassRole(ButtonToneNeutral, closeState, !enabled,
			false, p.ClassName, StyleSheet_StyleKindCollapsible(), Collapsible_CollapsibleCloseRole()).Value)
		closeFont, closeFontID := styleTextFace(closeStyle, Text16)
		r.record(FrameOp{Kind: FrameOpText, Bounds: closeBounds, Text: "×", Color: closeStyle.Foreground, Opacity: closeStyle.Opacity, FontSize: closeFont, FontID: closeFontID, Pressed: closed, Disabled: !enabled})
	}
	if pressed || closed {
		return 1
	}
	return 0
}

func (r *runtime) TreeView(props TreeViewProps) int32 {
	props.Bounds = r.layoutRect(props.Bounds)
	props.Disabled = props.Disabled || r.contentDisabled()
	count := props.ItemCount
	if count <= 0 || count > int32(len(props.Items)) {
		count = int32(len(props.Items))
	}
	panelFrame := simpleStyleFrameWithClassRole(ButtonToneNeutral, func() ButtonState {
		if props.Disabled {
			return ButtonStateDisabled
		}
		return ButtonStateNormal
	}(), props.Disabled, false, props.ClassName, StyleSheet_StyleKindTreeView(), StyleSheet_StyleAny())
	defaultItemFrame := simpleStyleFrameWithClassRole(ButtonToneNeutral, func() ButtonState {
		if props.Disabled {
			return ButtonStateDisabled
		}
		return ButtonStateNormal
	}(), props.Disabled, false, props.ClassName, StyleSheet_StyleKindTreeViewItem(), StyleSheet_StyleAny())
	metrics := TreeView_TreeViewMetricsFor(1, panelFrame, defaultItemFrame)
	rowH := TreeView_TreeViewRowHeight(props.RowHeight, 1, metrics)
	contentHeight := TreeView_TreeViewContentHeight(count, rowH)
	maxScroll := TreeView_TreeViewMaxScroll(int32(props.Bounds.Height), contentHeight)
	if props.ScrollOffset != nil {
		*props.ScrollOffset = clamp32(*props.ScrollOffset, 0, maxScroll)
		if !props.Disabled && r.pointerCanReach(props.Bounds) && r.mouseWheel != 0 {
			*props.ScrollOffset = clamp32(*props.ScrollOffset-int32(r.mouseWheel)*rowH*3, 0, maxScroll)
		}
	}
	scroll := int32(0)
	if props.ScrollOffset != nil {
		scroll = *props.ScrollOffset
	}
	scrollLayout := TreeView_TreeViewScrollFor(scroll, rowH)
	visible := TreeView_TreeViewVisibleRows(int32(props.Bounds.Height), rowH)
	panelOp := styleFrameRectOp(props.Bounds, Rectangle{}, panelFrame)
	panelOp.ID = props.ID
	panelOp.Disabled = props.Disabled
	r.record(panelOp)
	changed := int32(0)
	first := scrollLayout.First
	yOffset := scrollLayout.YOffset
	for visibleIndex := int32(0); visibleIndex < visible && first+visibleIndex < count; visibleIndex++ {
		index := first + visibleIndex
		item := props.Items[index]
		row := TreeView_TreeViewRowBounds(props.Bounds, visibleIndex, rowH, yOffset)
		markerBounds := TreeView_TreeViewMarkerBounds(row, item.Depth, metrics)
		textBounds := TreeView_TreeViewTextBounds(row, item.Depth, metrics)
		selected := props.SelectedID != nil && *props.SelectedID == item.ID
		pressed := !props.Disabled && item.Selectable != 0 && r.consumeTap(row)
		decision := TreeView_TreeViewRowDecisionFor(!props.Disabled, pressed, item.Selectable != 0, props.SelectedID != nil, item.ID)
		if decision.Select {
			*props.SelectedID = decision.SelectedID
			selected = true
			changed = 1
		}
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
		}(), props.Disabled, selected, props.ClassName, StyleSheet_StyleKindTreeViewItem(), StyleSheet_StyleAny())
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
		mark := TreeView_TreeViewMarkerText(item.Expanded != 0)
		font, fontID := styleTextFace(itemStyle, Text16)
		textPaint := TreeView_TreeViewTextPaintFor(markerBounds, textBounds, font)
		markerBounds.X = float32(textPaint.MarkerX)
		markerBounds.Y = float32(textPaint.MarkerY)
		markerBounds.Height = float32(font)
		textBounds.X = float32(textPaint.TextX)
		textBounds.Y = float32(textPaint.TextY)
		textBounds.Height = float32(font)
		r.record(FrameOp{Kind: FrameOpText, Bounds: markerBounds, Text: mark, Color: itemStyle.Foreground, Opacity: itemStyle.Opacity, FontSize: font, FontID: fontID, ID: item.ID, Row: index, Disabled: props.Disabled})
		r.record(FrameOp{Kind: FrameOpText, Bounds: textBounds, Text: item.Label, Color: itemStyle.Foreground, Opacity: itemStyle.Opacity, FontSize: font, FontID: fontID, ID: item.ID, Row: index, Pressed: pressed, Selected: selected, Disabled: props.Disabled})
	}
	return changed
}
