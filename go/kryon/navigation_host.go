package kryon

func (r *runtime) TitleBar(props TitleBarProps) int32 {
	height := props.Height
	if height <= 0 {
		height = 44
	}
	surfaceFrame := simpleStyleFrameWithClassRole(ButtonToneNeutral, ButtonStateNormal, false, false,
		props.ClassName, StyleSheet_StyleKindTitleBar(), TitleBar_TitleBarBarRole())
	titleFrame := simpleStyleFrameWithClassRole(ButtonToneNeutral, ButtonStateNormal, false, false,
		props.ClassName, StyleSheet_StyleKindTitleBar(), TitleBar_TitleBarTitleRole())
	actionFrame := simpleStyleFrameWithClassRole(ButtonToneNeutral, ButtonStateNormal, false, false,
		props.ClassName, StyleSheet_StyleKindTitleBar(), TitleBar_TitleBarActionRole())
	metrics := TitleBar_TitleBarMetricsFor(1, surfaceFrame, titleFrame, actionFrame)
	layout := TitleBar_TitleBarLayoutFor(r.GetScreenWidth(), height,
		props.HasLeadingAction, props.HasDropdown, props.Dropdown.Height,
		props.Dropdown.MinWidth, metrics)
	r.record(styleFrameRectOp(layout.Bounds, Rectangle{}, surfaceFrame))
	clicked := int32(0)
	if props.HasLeadingAction {
		iconType := int32(IconLeft)
		if props.LeadingIcon.ID != 0 {
			iconType = int32(IconNone)
		}
		button, pressed := r.surfaceButtonFrameForRoleKind(ButtonProps{
			Icon:      props.LeadingIcon,
			IconType:  iconType,
			IconOnly:  true,
			Bounds:    layout.LeadingBounds,
			Disabled:  r.contentDisabled(),
			ClassName: props.ClassName,
			Tone:      ButtonToneNeutral,
			Emphasis:  ButtonEmphasisSoft,
			Size:      ControlSizeMedium,
		}, layout.Bounds, false, StyleSheet_StyleKindTitleBar(), TitleBar_TitleBarActionRole())
		if pressed {
			clicked = 1
		}
		button.IconSize = float32(metrics.LeadingIconSize)
		r.record(button)
	}
	if props.HasDropdown {
		if !props.Dropdown.Disabled {
			selected := props.Dropdown.SelectedIndex
			count := props.Dropdown.OptionCount
			if count <= 0 || count > int32(len(props.Dropdown.Options)) {
				count = int32(len(props.Dropdown.Options))
			}
			changed := int32(0)
			if r.Dropdown(DropdownProps{
				ID:            props.Dropdown.ID,
				Bounds:        layout.DropdownBounds,
				Options:       props.Dropdown.Options[:count],
				OptionCount:   count,
				SelectedIndex: selected,
			}) {
				changed = 1
			}
			return clicked | changed
		}
		return clicked
	}
	titleStyle := unpackStyle(titleFrame.Value)
	titleFont, titleFontID := styleTextFace(titleStyle, Text20)
	titleW := runtimeTextWidthWithFont(props.Title, titleFont, titleFontID)
	titlePaint := TitleBar_TitleBarTitlePaintFor(layout, int32(titleW), titleFont)
	r.record(FrameOp{Kind: FrameOpText, Bounds: Rectangle{
		X: float32(titlePaint.X), Y: float32(titlePaint.Y),
		Width: float32(titleW), Height: float32(titleFont + 4),
	}, Text: props.Title, Color: titleStyle.Foreground, Opacity: titleStyle.Opacity, FontSize: titleFont, FontID: titleFontID})
	return clicked
}

func (r *runtime) NavigationBar(props NavigationBarProps) {
	count := int(props.Count)
	if count <= 0 || count > len(props.Items) {
		count = len(props.Items)
	}
	if count == 0 {
		return
	}
	w := props.ViewWidth
	if w <= 0 {
		w = r.GetScreenWidth()
	}
	viewH := props.ViewHeight
	if viewH <= 0 {
		viewH = r.GetScreenHeight()
	}
	itemBaseFrame := StyleFrame{Value: ResolveActiveStyle(StyleData{},
		NavigationBar_NavigationBarItemFactsFor(props.ClassName,
			int32(ButtonToneNeutral), int32(ButtonEmphasisSoft),
			int32(ButtonStateNormal)), int32(ButtonStateNormal))}
	barFrame := StyleFrame{Value: ResolveActiveStyle(StyleData{},
		NavigationBar_NavigationBarFactsFor(props.ClassName,
			int32(ButtonStateNormal)), int32(ButtonStateNormal))}
	paint := NavigationBar_NavigationBarPaintFor(NavigationBarSpec{
		ViewWidth:    w,
		ViewHeight:   viewH,
		Count:        int32(count),
		Height:       props.Height,
		SideMargin:   -1,
		BottomMargin: -1,
		IconSize:     -1,
		Scale:        1,
		Bar:          barFrame,
		Item:         itemBaseFrame,
	})
	bar := unpackStyle(paint.Bar.Value)
	r.record(FrameOp{Kind: FrameOpRect, Bounds: paint.BarBounds, Color: bar.Background,
		BorderColor: bar.Border, BorderWidth: bar.BorderWidth, Radius: bar.Radius,
		Material: bar.Material})
	for i := 0; i < count; i++ {
		item := props.Items[i]
		itemState := checkboxButtonState(false, false, false, item.Disabled)
		if item.Active && !item.Disabled {
			itemState = ButtonStateSelected
		}
		baseFrame := StyleFrame{Value: ResolveActiveStyle(StyleData{},
			NavigationBar_NavigationBarItemFactsFor(props.ClassName,
				int32(ButtonToneNeutral), int32(ButtonEmphasisSoft),
				int32(checkboxButtonState(false, false, false, item.Disabled))),
			int32(checkboxButtonState(false, false, false, item.Disabled)))}
		baseStyle := unpackStyle(baseFrame.Value)
		labelFont, labelFontID := styleTextFace(baseStyle, Text14)
		faceTone := ButtonToneNeutral
		faceEmphasis := ButtonEmphasisSoft
		if item.Active {
			faceTone = ButtonToneAccent
			faceEmphasis = ButtonEmphasisFilled
		}
		faceFrame := StyleFrame{Value: ResolveActiveStyle(StyleData{},
			NavigationBar_NavigationBarItemFactsFor(props.ClassName,
				int32(faceTone), int32(faceEmphasis), int32(itemState)),
			int32(itemState))}
		itemPaint := NavigationBar_NavigationBarItemPaintFor(NavigationBarItemSpec{
			Bar:         paint,
			Index:       int32(i),
			Active:      item.Active,
			Disabled:    item.Disabled,
			Hovered:     false,
			LabelHeight: labelFont + 4,
			Base:        baseFrame,
			Face:        faceFrame,
		})
		pressed := !item.Disabled && r.consumeTap(itemPaint.Bounds)
		textStyle := baseStyle
		if itemPaint.DrawFace {
			face := unpackStyle(itemPaint.Face.Value)
			textStyle = face
			r.record(FrameOp{Kind: FrameOpRect, Bounds: itemPaint.StateBounds, Color: face.Background,
				BorderColor: face.Border, BorderWidth: face.BorderWidth, Radius: face.Radius,
				Material: face.Material, Selected: item.Active, Disabled: item.Disabled})
		}
		textFont, textFontID := styleTextFaceWithFallback(textStyle, labelFont, labelFontID)
		if item.Icon.ID != 0 {
			tint := unpackRGBA(itemPaint.IconColor)
			if item.Disabled {
				tint.A = uint8(uint32(tint.A) * uint32(itemPaint.IconAlpha) / 255)
			}
			r.Icon(item.Route, int32(itemPaint.IconBounds.X), int32(itemPaint.IconBounds.Y),
				int32(itemPaint.IconBounds.Width), int32(item.Icon.ID), tint)
		}
		r.record(FrameOp{Kind: FrameOpText, Bounds: itemPaint.LabelBounds, Text: item.Label,
			Color: unpackRGBA(itemPaint.TextColor), Opacity: textStyle.Opacity,
			FontSize: textFont, FontID: textFontID, ID: item.Route,
			Pressed: pressed, Selected: item.Active, Disabled: item.Disabled})
	}
}

func (r *runtime) Toolbar(props ToolbarProps) ToolbarResult {
	result := ToolbarResult{SelectedMenuItem: -1, ClickedAction: -1}
	if props.Width <= 0 {
		props.Width = r.GetScreenWidth() - props.X
	}
	if props.Height <= 0 {
		props.Height = 44
	}
	actionCount := int32(props.ActionCount)
	if actionCount <= 0 || int(actionCount) > len(props.Actions) {
		actionCount = int32(len(props.Actions))
	}
	barFrame := simpleStyleFrameWithClassRole(ButtonToneNeutral, ButtonStateNormal, false, false,
		props.ClassName, StyleSheet_StyleKindToolbar(), Toolbar_ToolbarBarRole())
	actionFrame := simpleStyleFrameWithClassRole(ButtonToneNeutral, ButtonStateNormal, false, false,
		props.ClassName, StyleSheet_StyleKindToolbar(), Toolbar_ToolbarActionRole())
	layout := Toolbar_ToolbarLayoutFor(ToolbarSpec{
		X:                 props.X,
		Y:                 props.Y,
		Width:             props.Width,
		Height:            props.Height,
		ActionCount:       actionCount,
		ActionIconSize:    -1,
		ActionIconPadding: -1,
		ActionGap:         -1,
		SidePadding:       -1,
		DropdownMinWidth:  props.DropdownMinWidth,
		DropdownMaxWidth:  props.DropdownMaxWidth,
		DropdownHeight:    props.DropdownHeight,
		Scale:             1,
		Bar:               barFrame,
		Action:            actionFrame,
	})
	bounds := layout.Bounds
	r.record(styleFrameRectOp(bounds, Rectangle{}, barFrame))
	dividerStyle := unpackStyle(simpleStyleFrameWithClassRole(ButtonToneNeutral, ButtonStateNormal, false, false,
		props.ClassName, StyleSheet_StyleKindToolbar(), Toolbar_ToolbarDividerRole()).Value)
	r.record(FrameOp{Kind: FrameOpLine, Bounds: Rectangle{X: bounds.X, Y: bounds.Y + bounds.Height - 1, Width: bounds.Width, Height: 0}, Color: dividerStyle.Border})
	for i := int32(0); i < actionCount; i++ {
		action := props.Actions[i]
		if r.iconAction(iconActionProps{
			Bounds:      Toolbar_ToolbarActionBoundsFor(layout, i, actionCount),
			Icon:        action.Icon,
			IconType:    action.IconType,
			IconSize:    layout.ActionIconSize,
			IconPadding: layout.ActionIconPadding,
			FocusID:     Toolbar_ToolbarActionIdFor(props.ID, int32(i)),
			Disabled:    action.Disabled,
			StyleKind:   StyleSheet_StyleKindToolbar(),
			Role:        Toolbar_ToolbarActionRole(),
			ClassName:   props.ClassName,
		}) {
			result.ClickedAction = int32(i)
		}
	}
	return result
}
