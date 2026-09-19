package kryon

import (
	"strings"
	"unicode/utf8"
)

func (r *runtime) TextArea(props TextAreaProps) bool {
	r.prepareAccessibility(props.FocusID, int32(WidgetKindTextArea), true)
	props.Bounds = r.layoutRect(props.Bounds)
	changed := r.editText(props.Bounds, props.Text, props.CursorPosition, props.Focused, nil, props.FocusID, textEditOptions{
		maxCodepoints: props.MaxCodepoints,
		pageRows:      r.textAreaPageRows(props),
		area:          &props,
		readOnly:      props.ReadOnly,
		multiline:     true,
	})
	r.recordTextArea(props)
	return changed
}

func (r *runtime) textAreaPageRows(props TextAreaProps) int {
	style := r.textInputStyle(FrameOpTextArea, false, r.contentDisabled(), props.ClassName)
	defaultFont := r.textInputDefaultFont(FrameOpTextArea, false, r.contentDisabled(), props.ClassName, Text16)
	metrics := TextInput_TextInputMetricsFor(style.Fields, 0, int32(style.PaddingX), int32(style.PaddingY), styleGapLength(style), defaultFont, 10, 8, 0)
	return int(TextInput_TextAreaPageRows(props.Bounds.Height, metrics.Font, metrics.LineGap, metrics.PaddingY))
}

func (r *runtime) TextField(props TextFieldProps) {
	r.prepareAccessibility(props.FocusID, int32(WidgetKindTextField), true)
	props.Bounds = r.layoutRect(props.Bounds)
	focused := r.focusID == props.FocusID || props.Focused != nil && *props.Focused
	defaultFont := r.textInputDefaultFont(FrameOpTextField, focused, r.contentDisabled(), props.ClassName, Text16)
	style := r.textInputStyle(FrameOpTextField, focused, r.contentDisabled(), props.ClassName)
	metrics := TextInput_TextInputMetricsFor(style.Fields, 0, int32(style.PaddingX), int32(style.PaddingY), 0, defaultFont, 10, 8, 0)
	r.editText(props.Bounds, props.Text, props.CursorPosition, props.Focused, props.CommitPressed, props.FocusID, textEditOptions{
		maxCodepoints: props.MaxCodepoints,
		secure:        props.Secure,
		readOnly:      props.ReadOnly,
	})
	r.recordTextInput(FrameOpTextField, props.Bounds, props.Text, props.CursorPosition, props.Focused, props.FocusID, metrics.Font, props.Secure, props.ReadOnly, textInputRecordOptions{
		className: props.ClassName,
		paddingX:  metrics.PaddingX,
		paddingY:  metrics.PaddingY,
	})
}

func (r *runtime) recordTextInput(kind FrameOpKind, bounds Rectangle, buf []byte, cursor *int32, focused *bool, focusID, font int32, secure, readOnly bool, options ...textInputRecordOptions) {
	text := string(buf[:zeroIndex(buf)])
	value := text
	pos := len(text)
	if cursor != nil {
		pos = clampCursor(text, int(*cursor))
	}
	selectionStart, selectionEnd := pos, pos
	if sel, ok := r.selection[focusID]; ok {
		selectionStart, selectionEnd = selectionRange(sel)
	}
	accessibleSelection := r.normalizedSelection(focusID, value, pos)
	if secure {
		accessibleSelection = selection{}
	}
	compositionStart, compositionEnd := 0, 0
	if preedit, ok := r.preedit[focusID]; ok && r.focusID == focusID && !secure {
		if view, visible := makeTextCompositionView(text, selectionStart, selectionEnd, preedit); visible {
			text = view.text
			pos = view.cursor
			selectionStart = view.selectionStart
			selectionEnd = view.selectionEnd
			compositionStart = view.compositionStart
			compositionEnd = view.compositionEnd
		}
	}
	if secure {
		value = ""
		text = strings.Repeat("*", utf8.RuneCountInString(text))
	}
	fieldFocused := r.focusID == focusID || focused != nil && *focused
	disabled := r.contentDisabled()
	var opt textInputRecordOptions
	if len(options) > 0 {
		opt = options[0]
	}
	paint := r.textInputStyle(kind, fieldFocused, disabled, opt.className)
	op := FrameOp{
		Kind:              kind,
		Bounds:            bounds,
		Text:              text,
		AccessibleValue:   value,
		AccessibleLabel:   opt.label,
		Color:             paint.Background,
		BorderColor:       paint.Border,
		FocusColor:        paint.Focus,
		AmbientColor:      r.appAmbientColor(),
		TextColor:         paint.Foreground,
		SelectionColor:    paint.Focus,
		SelectedTextColor: paint.Foreground,
		CursorColor:       paint.Focus,
		Radius:            paint.Radius,
		BorderWidth:       paint.BorderWidth,
		Opacity:           paint.Opacity,
		Material:          paint.Material,
		Fields:            paint.Fields,
		FillStates:        styleFill(paint),
		FillStatesValid:   true,
		FontSize:          font,
		FontID:            styleFontID(paint),
		Gap:               float32(opt.lineGap),
		ContentOffset:     Vector2{X: float32(opt.paddingX), Y: float32(opt.paddingY)},
		ScrollY:           opt.scrollY,
		Wrap:              opt.wrap,
		FocusID:           focusID,
		Cursor:            int32(pos),
		SelectionStart:    int32(selectionStart),
		SelectionEnd:      int32(selectionEnd),
		CompositionStart:  int32(compositionStart),
		CompositionEnd:    int32(compositionEnd),
		Focused:           r.focusID == focusID,
		Disabled:          disabled,
		Secure:            secure,
		ReadOnly:          readOnly,

		accessibilityAnchor: int32(accessibleSelection.Anchor),
		accessibilityCursor: int32(accessibleSelection.Cursor),
	}
	if focused != nil {
		op.Focused = *focused
	}
	r.record(op)
}

func (r *runtime) recordTextArea(props TextAreaProps) {
	focused := r.focusID == props.FocusID || props.Focused != nil && *props.Focused
	defaultFont := r.textInputDefaultFont(FrameOpTextArea, focused, r.contentDisabled(), props.ClassName, Text16)
	style := r.textInputStyle(FrameOpTextArea, focused, r.contentDisabled(), props.ClassName)
	metrics := TextInput_TextInputMetricsFor(style.Fields, 0, int32(style.PaddingX), int32(style.PaddingY), styleGapLength(style), defaultFont, 10, 8, 0)
	scrollY := int32(0)
	if props.ScrollY != nil {
		scrollY = *props.ScrollY
	}
	r.recordTextInput(FrameOpTextArea, props.Bounds, props.Text, props.CursorPosition, props.Focused, props.FocusID, metrics.Font, false, props.ReadOnly, textInputRecordOptions{
		label:     props.Placeholder,
		className: props.ClassName,
		lineGap:   metrics.LineGap,
		paddingX:  metrics.PaddingX,
		paddingY:  metrics.PaddingY,
		scrollY:   scrollY,
		wrap:      props.Wrap,
	})
}

type textInputRecordOptions struct {
	label     string
	className int32
	lineGap   int32
	paddingX  int32
	paddingY  int32
	scrollY   int32
	wrap      bool
}

func (r *runtime) textInputStyle(kind FrameOpKind, focused, disabled bool, className int32) Style {
	state := ButtonStateNormal
	if focused {
		state = ButtonStateFocus
	}
	if disabled {
		state = ButtonStateDisabled
	}
	styleKind := StyleSheet_StyleKindTextField()
	if kind == FrameOpTextArea {
		styleKind = StyleSheet_StyleKindTextArea()
	}
	return resolveButtonStyleForKind(r.theme(), r.effectiveDark(), r.activeTheme,
		ButtonProps{ClassName: className, Tone: ButtonToneNeutral, Emphasis: ButtonEmphasisSoft, Disabled: disabled}, state, styleKind)
}

func (r *runtime) textInputDefaultFont(kind FrameOpKind, focused, disabled bool, className int32, fallback int32) int32 {
	style := r.textInputStyle(kind, focused, disabled, className)
	return styleFont(style, fallback)
}

func styleGapLength(style Style) int32 {
	if style.Fields&StyleGap == 0 {
		return -1
	}
	return styleLength(style.Gap)
}

type textEditOptions struct {
	area          *TextAreaProps
	maxCodepoints int32
	pageRows      int
	secure        bool
	readOnly      bool
	multiline     bool
}

func (r *runtime) editText(bounds Rectangle, buf []byte, cursor *int32, focused *bool, commit *bool, focusID int32, options textEditOptions) bool {
	if options.readOnly {
		delete(r.preedit, focusID)
	}
	if len(buf) == 0 {
		return false
	}
	r.registerField(focusID)
	if focused != nil {
		r.focusRefs[focusID] = focused
	}
	if commit != nil {
		*commit = false
	}
	if r.contentDisabled() || r.popupKeyboardCaptures() {
		delete(r.preedit, focusID)
		return false
	}
	changed := r.applyAccessibilityText(focusID, buf, cursor, options)
	tapPoint, tapped := r.consumeTapPosition(bounds)
	if focusID != 0 && tapped {
		r.setFocus(focusID)
	}
	if focused != nil && *focused {
		r.setFocus(focusID)
	}
	if r.focusID != focusID {
		delete(r.preedit, focusID)
		if focused != nil {
			*focused = false
		}
		return false
	}
	if focused != nil {
		*focused = true
	}
	if cursor == nil {
		return false
	}
	text := string(buf[:zeroIndex(buf)])
	pos := clampCursor(text, int(*cursor))
	sel := r.normalizedSelection(focusID, text, pos)
	if tapped {
		if options.area != nil {
			pos = r.textAreaCursorAtPoint(text, *options.area, tapPoint)
		} else {
			pos = cursorAtTap(text, bounds, tapPoint.X)
		}
		sel = collapsedSelection(pos)
	}
	for len(r.inputEvents) > 0 && r.focusID == focusID {
		event := r.inputEvents[0]
		r.inputEvents = r.inputEvents[1:]
		if event.text != "" {
			if options.readOnly {
				continue
			}
			var inserted bool
			text, pos, inserted = insertText(text, pos, sel, event.text, textLimit(buf, options.maxCodepoints))
			if inserted {
				changed = true
				sel = collapsedSelection(pos)
			}
			continue
		}
		textSelection := event.shift || r.keyDown[KeyLeftShift] || r.keyDown[KeyRightShift]
		if event.shortcut {
			shortcuts := TextInput_TextShortcutInputFor(event.shortcut,
				event.key == KeyA, event.key == KeyC, event.key == KeyX, event.key == KeyV)
			command := int32(0)
			switch {
			case shortcuts.SelectAll:
				command = TextInput_TextContextCommandSelectAll()
			case shortcuts.Copy:
				command = TextInput_TextContextCommandCopy()
			case shortcuts.Cut:
				command = TextInput_TextContextCommandCut()
			case shortcuts.Paste:
				command = TextInput_TextContextCommandPaste()
			}
			if command != 0 {
				decision := TextInput_TextEditCommandDecisionFor(command,
					sel.Anchor != sel.Cursor, false, false, len(text) > 0,
					!options.secure && (!shortcuts.Cut || !options.readOnly),
					!options.readOnly, !options.readOnly)
				if decision.SelectAll {
					sel = selectAllSelection(len(text))
					pos = sel.Cursor
				}
				if decision.CopySelection {
					start, end := selectionRange(sel)
					r.clipboard = text[start:end]
				}
				if decision.DeleteSelection && !decision.Paste {
					start, end := selectionRange(sel)
					text = text[:start] + text[end:]
					pos = start
					sel = collapsedSelection(pos)
					changed = true
				}
				if decision.Paste {
					var inserted bool
					text, pos, inserted = insertText(text, pos, sel, r.clipboard, textLimit(buf, options.maxCodepoints))
					if inserted {
						changed = true
						sel = collapsedSelection(pos)
					}
				}
				continue
			}
		}
		key := TextInput_TextNavigationKeyFor(options.multiline,
			event.key == KeyLeft, event.key == KeyRight,
			event.key == KeyHome, event.key == KeyEnd,
			event.key == KeyUp, event.key == KeyDown,
			event.key == KeyPageUp, event.key == KeyPageDown)
		navigation := TextInput_TextNavigationDecisionFor(key, options.multiline,
			textSelection, event.shortcut, options.secure, sel.Anchor != sel.Cursor)
		if navigation.Consumed {
			target := pos
			switch {
			case navigation.CollapseSelectionStart:
				target, _ = selectionRange(sel)
			case navigation.CollapseSelectionEnd:
				_, target = selectionRange(sel)
			case navigation.DocumentEdge < 0:
				target = 0
			case navigation.DocumentEdge > 0:
				target = len(text)
			case navigation.LineEdge < 0:
				target = textLineStart(text, pos)
			case navigation.LineEdge > 0:
				target = textLineEnd(text, pos)
			case navigation.WordDirection < 0:
				target = textWordLeft(text, pos)
			case navigation.WordDirection > 0:
				target = textWordRight(text, pos)
			case navigation.CharDirection < 0:
				target = previousGrapheme(text, pos)
			case navigation.CharDirection > 0:
				target = nextGrapheme(text, pos)
			case navigation.VerticalDirection != 0:
				target = textMoveVertical(text, pos, int(navigation.VerticalDirection), 1)
			case navigation.PageDirection != 0:
				target = textMoveVertical(text, pos, int(navigation.PageDirection), options.pageRows)
			}
			pos, sel = textMoveSelection(sel, pos, target, navigation.ExtendSelection)
			continue
		}
		switch event.key {
		case KeyTab:
			if !event.shortcut {
				r.setFocus(r.nextFocus(focusID, event.shift))
				if r.focusID != focusID {
					r.textInputHandoff = r.focusID
					r.ClearTextComposition()
				}
				sel = collapsedSelection(pos)
			}
		case KeyEscape:
			if TextInput_TextEscapeShouldBlur(r.focusID == focusID, true, true) {
				r.setFocus(0)
			}
		case KeyBackspace, KeyDelete:
			if TextInput_TextDeleteShortcutShouldRun(options.readOnly,
				event.key == KeyBackspace, event.key == KeyDelete, 0) {
				var deleted bool
				text, pos, sel, deleted = textDeleteKey(text, pos, sel, event.key,
					event.shortcut, options.secure)
				changed = changed || deleted
			}
		case KeyEnter:
			if !TextInput_TextNativeEditShouldRun(options.readOnly, event.shortcut) {
				continue
			}
			if options.multiline {
				var inserted bool
				text, pos, inserted = insertText(text, pos, sel, "\n", textLimit(buf, options.maxCodepoints))
				changed = changed || inserted
				sel = collapsedSelection(pos)
			} else if commit != nil {
				*commit = true
			}
		}
	}
	var composed bool
	composition := TextInput_TextCompositionInputDecisionFor(r.focusID == focusID, options.readOnly)
	if composition.Cancel {
		delete(r.preedit, focusID)
	}
	if composition.DrainEvents {
		r.ClearTextComposition()
	}
	if composition.AcceptEvents {
		text, pos, sel, composed = r.editComposition(focusID, text, pos, sel, textLimit(buf, options.maxCodepoints))
	}
	changed = changed || composed
	if !options.readOnly {
		clear(buf)
		copy(buf, text)
	}
	*cursor = int32(pos)
	if sel.Anchor == sel.Cursor {
		delete(r.selection, focusID)
	} else {
		r.selection[focusID] = sel
	}
	if focused != nil {
		*focused = r.focusID == focusID
	}
	return changed
}

func cursorAtTap(text string, bounds Rectangle, x float32) int {
	const padding = float32(10)
	const charWidth = float32(8)

	rel := x - bounds.X - padding
	if rel <= 0 {
		return 0
	}
	target := int(rel / charWidth)
	pos := 0
	for i := 0; i < target && pos < len(text); i++ {
		pos = nextGrapheme(text, pos)
	}
	return pos
}

func (r *runtime) normalizedSelection(focusID int32, text string, pos int) selection {
	s, ok := r.selection[focusID]
	if !ok {
		return collapsedSelection(pos)
	}
	s.Anchor = clampCursor(text, s.Anchor)
	s.Cursor = clampCursor(text, s.Cursor)
	return s
}

func selectionRange(sel selection) (int, int) {
	result := TextInput_TextSelectionRangeFor(int32(sel.Anchor), int32(sel.Cursor))
	return int(result.Start), int(result.End)
}

func collapsedSelection(pos int) selection {
	result := TextInput_TextSelectionCollapsed(int32(pos))
	return selection{Anchor: int(result.Anchor), Cursor: int(result.Cursor)}
}

func selectAllSelection(length int) selection {
	result := TextInput_TextSelectionAll(int32(length))
	return selection{Anchor: int(result.Anchor), Cursor: int(result.Cursor)}
}

func deleteSelection(text string, sel selection) (string, int, bool) {
	start, end := selectionRange(sel)
	if start == end {
		return text, start, false
	}
	return text[:start] + text[end:], start, true
}

func insertText(text string, pos int, sel selection, value string, limit int) (string, int, bool) {
	if value == "" {
		return text, pos, false
	}
	if sel.Anchor != sel.Cursor {
		var deleted bool
		text, pos, deleted = deleteSelection(text, sel)
		_ = deleted
	}
	available := limit - len([]byte(text))
	if available <= 0 {
		return text, pos, false
	}
	value = trimUTF8Bytes(value, available)
	if value == "" {
		return text, pos, false
	}
	text = text[:pos] + value + text[pos:]
	return text, pos + len(value), true
}

func textLimit(buf []byte, maxCodepoints int32) int {
	return int(TextInput_TextInputBufferLimit(int32(len(buf)), maxCodepoints))
}

func trimUTF8Bytes(text string, limit int) string {
	if len(text) <= limit {
		return text
	}
	if limit <= 0 {
		return ""
	}
	for limit > 0 && !utf8.RuneStart(text[limit]) {
		limit--
	}
	return text[:limit]
}

func zeroIndex(buf []byte) int {
	for i, b := range buf {
		if b == 0 {
			return i
		}
	}
	return len(buf)
}

// CString returns the text before the first NUL byte in a generated fixed
// char buffer.
func CString(buf []byte) string {
	return string(buf[:zeroIndex(buf)])
}

func clampRuneCursor(text string, pos int) int {
	if pos < 0 {
		return 0
	}
	if pos > len(text) {
		return len(text)
	}
	if pos == len(text) {
		return pos
	}
	for pos > 0 && !utf8.RuneStart(text[pos]) {
		pos--
	}
	return pos
}

func prevRune(text string, pos int) int {
	pos = clampRuneCursor(text, pos)
	if pos == 0 {
		return 0
	}
	_, size := utf8.DecodeLastRuneInString(text[:pos])
	return pos - size
}

func textCodepointAt(text string, pos int) rune {
	pos = clampRuneCursor(text, pos)
	if pos >= len(text) {
		return 0
	}
	codepoint, _ := utf8.DecodeRuneInString(text[pos:])
	return codepoint
}

func textIsWordBoundary(text string, pos int) bool {
	if pos <= 0 {
		return false
	}
	previous := textCodepointAt(text, prevRune(text, pos))
	current := textCodepointAt(text, pos)
	return TextInput_TextWordBoundaryFor(int32(previous), int32(current))
}

func textWordLeft(text string, pos int) int {
	pos = previousGrapheme(text, pos)
	for pos > 0 && !textIsWordBoundary(text, pos) {
		pos = previousGrapheme(text, pos)
	}
	return pos
}

func textWordRight(text string, pos int) int {
	pos = nextGrapheme(text, pos)
	for pos < len(text) && !textIsWordBoundary(text, pos) {
		pos = nextGrapheme(text, pos)
	}
	return pos
}

func textDeleteKey(
	text string,
	pos int,
	current selection,
	key int32,
	word bool,
	secure bool,
) (string, int, selection, bool) {
	pos = clampCursor(text, pos)
	current.Anchor = clampCursor(text, current.Anchor)
	current.Cursor = clampCursor(text, current.Cursor)
	start, end := selectionRange(current)
	action := TextInput_TextDeleteNone()
	if key == KeyBackspace {
		action = TextInput_TextDeleteBackspace()
	} else if key == KeyDelete {
		action = TextInput_TextDeleteForward()
	}
	decision := TextInput_TextDeleteDecisionFor(action, word, secure, start != end)
	if !decision.Consumed {
		return text, pos, current, false
	}
	if start == end {
		start, end = pos, pos
		switch {
		case decision.DocumentEdge < 0:
			start = 0
		case decision.DocumentEdge > 0:
			end = len(text)
		case decision.WordDirection < 0:
			start = textWordLeft(text, pos)
		case decision.WordDirection > 0:
			end = textWordRight(text, pos)
		case decision.CharDirection < 0:
			start = previousGrapheme(text, pos)
		case decision.CharDirection > 0:
			end = nextGrapheme(text, pos)
		}
	}
	if end <= start {
		return text, pos, current, false
	}
	text = text[:start] + text[end:]
	current = collapsedSelection(start)
	return text, start, current, true
}

func textLineStart(text string, pos int) int {
	pos = clampCursor(text, pos)
	if start := strings.LastIndexByte(text[:pos], '\n'); start >= 0 {
		return start + 1
	}
	return 0
}

func textLineEnd(text string, pos int) int {
	pos = clampCursor(text, pos)
	if end := strings.IndexByte(text[pos:], '\n'); end >= 0 {
		return clampCursor(text, pos+end)
	}
	return len(text)
}

func textMoveVertical(text string, pos, direction, rows int) int {
	start := textLineStart(text, pos)
	column := graphemeCount(text[start:clampCursor(text, pos)])
	for step := 0; step < max(1, rows); step++ {
		if direction < 0 {
			if start == 0 {
				break
			}
			pos = start - 1
			start = textLineStart(text, pos)
		} else {
			end := textLineEnd(text, start)
			if end == len(text) {
				break
			}
			start = nextGrapheme(text, end)
		}
	}
	pos = start
	end := textLineEnd(text, start)
	for column > 0 && pos < end {
		pos = nextGrapheme(text, pos)
		column--
	}
	return pos
}

func textMoveSelection(current selection, cursor, target int, extend bool) (int, selection) {
	next := TextInput_TextSelectionAfterMove(
		int32(current.Anchor), int32(cursor), int32(target), extend,
	)
	return int(next.Cursor), selection{Anchor: int(next.Anchor), Cursor: int(next.Cursor)}
}
