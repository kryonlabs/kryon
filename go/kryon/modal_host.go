package kryon

func (r *runtime) Modal(props ModalProps) int32 {
	count := int(props.ActionCount)
	if count <= 0 || count > len(props.Actions) {
		count = len(props.Actions)
	}
	actions := make([]ModalAction, 0, count)
	for i := 0; i < count; i++ {
		actions = append(actions, props.Actions[i])
	}
	fieldHeight := float32(0)
	if props.Text != nil && props.CursorPosition != nil && props.Focused != nil {
		fieldHeight = 38
	}
	result, field := r.drawActionModal(props.Title, props.Message, actions, fieldHeight, props.ClassName)
	commit := false
	if fieldHeight > 0 {
		focusID := props.FocusID
		if focusID == 0 {
			focusID = 7301
		}
		maxCodepoints := int32(len(props.Text) - 1)
		if props.TextSize > 0 && props.TextSize <= int32(len(props.Text)) {
			maxCodepoints = props.TextSize - 1
		}
		r.editText(field, props.Text, props.CursorPosition, props.Focused, &commit, focusID, textEditOptions{maxCodepoints: maxCodepoints})
		focused := r.focusID == focusID || props.Focused != nil && *props.Focused
		font := r.textInputDefaultFont(FrameOpTextField, focused, r.contentDisabled(), props.ClassName, Text16)
		r.recordTextInput(FrameOpTextField, field, props.Text, props.CursorPosition, props.Focused, focusID, font, false, false,
			textInputRecordOptions{className: props.ClassName})
	}
	if result == 0 && commit {
		if count > 1 {
			result = 2
		} else {
			result = 1
		}
	}
	return result
}

func modalActionLabel(action ModalAction, index, count int) string {
	if action.Label != "" {
		return action.Label
	}
	if count == 1 {
		return "OK"
	}
	if index == 0 {
		return "Cancel"
	}
	return "OK"
}

func (r *runtime) drawActionModal(title, message string, actions []ModalAction, fieldHeight float32, className int32) (int32, Rectangle) {
	panelFrame := simpleStyleFrameWithClassRole(ButtonToneNeutral, ButtonStateNormal, false, false,
		className, StyleSheet_StyleKindModal(), Modal_ModalPanelRole())
	titleFrame := simpleStyleFrameWithClassRole(ButtonToneNeutral, ButtonStateNormal, false, false,
		className, StyleSheet_StyleKindModal(), Modal_ModalTitleRole())
	messageFrame := simpleStyleFrameWithClassRole(ButtonToneNeutral, ButtonStateNormal, false, false,
		className, StyleSheet_StyleKindModal(), Modal_ModalMessageRole())
	actionFrame := simpleStyleFrameWithClassRole(ButtonToneNeutral, ButtonStateNormal, false, false,
		className, StyleSheet_StyleKindModal(), Modal_ModalActionRole())
	closeFrame := simpleStyleFrameWithClassRole(ButtonToneNeutral, ButtonStateNormal, false, false,
		className, StyleSheet_StyleKindModal(), Modal_ModalCloseRole())
	metrics := Modal_ModalMetricsFor(1, panelFrame, titleFrame, messageFrame, actionFrame, closeFrame)
	messageHeight := int32(0)
	if message != "" {
		messageHeight = 24
	}
	buttonRows := int32(0)
	if len(actions) > 0 {
		buttonRows = 1
	}
	layout := Modal_ModalLayoutFor(r.GetScreenWidth(), r.GetScreenHeight(), 0, messageHeight, buttonRows, fieldHeight > 0, metrics)
	panel := layout.Panel
	panelStyle := unpackStyle(panelFrame.Value)
	titleStyle := unpackStyle(titleFrame.Value)
	messageStyle := unpackStyle(messageFrame.Value)
	scrimStyle := unpackStyle(simpleStyleFrameWithClassRole(ButtonToneNeutral, ButtonStateNormal, false, false,
		className, StyleSheet_StyleKindModal(), Modal_ModalScrimRole()).Value)
	r.record(FrameOp{Kind: FrameOpRect, Bounds: Rectangle{Width: float32(r.GetScreenWidth()), Height: float32(r.GetScreenHeight())}, Color: unpackRGBA(Surface_Opacity(packRGBA(scrimStyle.Background), scrimStyle.Opacity)), Opacity: scrimStyle.Opacity})
	r.record(styleFrameRectOp(panel, Rectangle{}, panelFrame))
	titleFont, titleFontID := styleTextFace(titleStyle, Text16)
	r.record(FrameOp{Kind: FrameOpText, Bounds: Rectangle{X: panel.X + float32(metrics.PaddingX), Y: panel.Y + float32(metrics.FrameTitleY), Width: float32(layout.ContentWidth), Height: 30}, Text: title, Color: titleStyle.Foreground, Opacity: titleStyle.Opacity, FontSize: titleFont, FontID: titleFontID})
	if message != "" {
		messageFont, messageFontID := styleTextFace(messageStyle, Text16)
		r.record(FrameOp{Kind: FrameOpText, Bounds: Rectangle{X: float32(layout.MessageX), Y: float32(layout.MessageY), Width: float32(layout.ContentWidth), Height: float32(messageHeight)}, Text: message, Color: messageStyle.Foreground, Opacity: messageStyle.Opacity, FontSize: messageFont, FontID: messageFontID})
	}

	result := int32(0)
	buttonW := float32(metrics.ActionMinWidth)
	gap := float32(metrics.ButtonGap)
	buttonY := float32(layout.ButtonY)
	buttonX := panel.X + panel.Width - float32(metrics.PaddingX) - float32(len(actions))*buttonW - float32(maxInt(0, len(actions)-1))*gap
	for i, action := range actions {
		label := modalActionLabel(action, i, len(actions))
		bounds := Rectangle{X: buttonX + float32(i)*(buttonW+gap), Y: buttonY, Width: buttonW, Height: float32(metrics.ButtonHeight)}
		tone := action.Tone
		emphasis := action.Emphasis
		if emphasis == 0 {
			emphasis = ButtonEmphasisSoft
		}
		if tone == 0 && i == len(actions)-1 {
			tone = ButtonToneAccent
		}
		button, pressed := r.surfaceButtonFrameForRoleKind(ButtonProps{Bounds: bounds, Label: label,
			ClassName: className, Tone: tone, Emphasis: emphasis, Disabled: action.Disabled},
			panel, false, StyleSheet_StyleKindModal(), Modal_ModalActionRole())
		button.AmbientColor = panelStyle.Background
		r.record(button)
		if pressed {
			result = int32(i + 1)
		}
	}
	if result == 0 {
		for i := range r.taps {
			if !r.taps[i].consumed && !pointInRect(r.taps[i].x, r.taps[i].y, panel) {
				r.taps[i].consumed = true
				result = -1
				break
			}
		}
	}
	field := Rectangle{X: float32(layout.MessageX), Y: float32(layout.PromptY), Width: float32(layout.ContentWidth), Height: float32(layout.PromptHeight)}
	return result, field
}
