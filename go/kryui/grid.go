package kryui

// GridCellEditor is the reusable in-place editor for table and spreadsheet
// cells. It delegates text interaction to TextField, so caret movement,
// selection, clipboard shortcuts, and context menus have the same behavior as
// every other Kryon text input. The grid remains responsible for choosing the
// active cell and deciding where Enter or Tab moves afterward.
type GridCellEditor struct {
	field  *TextFieldState
	active bool
}

func NewGridCellEditor(focusID, capacity int) *GridCellEditor {
	return &GridCellEditor{field: NewTextField(focusID, capacity)}
}

func (e *GridCellEditor) Begin(text string) {
	if e == nil {
		return
	}
	e.field.SetText(text)
	e.field.SetFocused(true)
	e.active = true
}

func (e *GridCellEditor) Active() bool { return e != nil && e.active }

func (e *GridCellEditor) Text() string {
	if e == nil {
		return ""
	}
	return e.field.Text()
}

func (e *GridCellEditor) SetText(text string) {
	if e != nil {
		e.field.SetText(text)
	}
}

func (e *GridCellEditor) Cancel() {
	if e == nil {
		return
	}
	e.active = false
	e.field.SetFocused(false)
}

// Draw renders the active cell editor. committed reports Enter; canceled
// reports Escape. Arrow keys remain owned by the text caret and never imply a
// grid-cell transition.
func (e *GridCellEditor) Draw(bounds Rectangle, font int32, style UITextInputStyle) (changed, committed, canceled bool) {
	if e == nil || !e.active {
		return false, false, false
	}
	changed, committed = e.field.Draw(bounds, font, style)
	if committed {
		e.Cancel()
		return changed, true, false
	}
	if IsKeyPressed(KeyEscape) {
		e.Cancel()
		return changed, false, true
	}
	return changed, false, false
}
