package kryui

// This file is the Go-native stateful control layer. The lower-level Props
// functions remain available for generated code and unusual integrations,
// while applications normally use these owned-state types.

// TextField owns the fixed storage, cursor, focus and selection identity
// needed by Kryon's native single-line editor.
type TextField struct {
	buffer        []byte
	cursor        int32
	focused       bool
	commitPressed bool
	focusID       int32
	maxCodepoints int32
	secure        bool
}

func NewTextField(focusID, capacity int) *TextField {
	if capacity < 2 {
		capacity = 2
	}
	return &TextField{
		buffer:        make([]byte, capacity),
		focusID:       int32(focusID),
		maxCodepoints: int32(capacity - 1),
	}
}

// NewPasswordField creates a text field that masks its contents and prevents
// clipboard copy and cut operations. Pasting remains available.
func NewPasswordField(focusID, capacity int) *TextField {
	f := NewTextField(focusID, capacity)
	f.secure = true
	return f
}

func (f *TextField) Text() string {
	if f == nil {
		return ""
	}
	n := 0
	for n < len(f.buffer) && f.buffer[n] != 0 {
		n++
	}
	return string(f.buffer[:n])
}

func (f *TextField) SetText(text string) {
	if f == nil || len(f.buffer) == 0 {
		return
	}
	clear(f.buffer)
	n := copy(f.buffer[:len(f.buffer)-1], text)
	f.cursor = int32(n)
}

func (f *TextField) Clear() { f.SetText("") }

func (f *TextField) Focused() bool { return f != nil && f.focused }

func (f *TextField) SetFocused(focused bool) {
	if f != nil {
		f.focused = focused
	}
}

// SetSecure controls whether the field masks its value and blocks copy/cut.
// It can be toggled temporarily to implement a reveal-password affordance.
func (f *TextField) SetSecure(secure bool) {
	if f != nil {
		f.secure = secure
	}
}

func (f *TextField) Secure() bool { return f != nil && f.secure }

// Draw renders and edits the field. committed is true for the frame in which
// Enter was pressed. Mouse selection, Ctrl+A/C/X/V and keyboard navigation
// are handled by Kryon.
func (f *TextField) Draw(bounds Rectangle, font int32, style UITextInputStyle) (changed, committed bool) {
	if f == nil {
		return false, false
	}
	f.commitPressed = false
	changed = DrawUITextField(TextFieldProps{
		Bounds:         bounds,
		Text:           f.buffer,
		CursorPosition: &f.cursor,
		Focused:        &f.focused,
		MaxCodepoints:  f.maxCodepoints,
		Font:           font,
		FocusID:        f.focusID,
		Style:          style,
		CommitPressed:  &f.commitPressed,
		Secure:         f.secure,
	})
	return changed, f.commitPressed
}

// TextArea owns a multiline editor and its scroll state.
type TextArea struct {
	buffer        []byte
	cursor        int32
	focused       bool
	scrollY       int32
	focusID       int32
	maxCodepoints int32
}

func NewTextArea(focusID, capacity int) *TextArea {
	if capacity < 2 {
		capacity = 2
	}
	return &TextArea{
		buffer:        make([]byte, capacity),
		focusID:       int32(focusID),
		maxCodepoints: int32(capacity - 1),
	}
}

func (a *TextArea) Text() string {
	if a == nil {
		return ""
	}
	n := 0
	for n < len(a.buffer) && a.buffer[n] != 0 {
		n++
	}
	return string(a.buffer[:n])
}

func (a *TextArea) SetText(text string) {
	if a == nil || len(a.buffer) == 0 {
		return
	}
	clear(a.buffer)
	n := copy(a.buffer[:len(a.buffer)-1], text)
	a.cursor = int32(n)
}

func (a *TextArea) Clear() { a.SetText("") }

func (a *TextArea) Focused() bool { return a != nil && a.focused }

func (a *TextArea) SetFocused(focused bool) {
	if a != nil {
		a.focused = focused
	}
}

func (a *TextArea) Draw(bounds Rectangle, font, lineGap int32, placeholder string,
	syntax UISyntaxMode, style UITextInputStyle) bool {
	if a == nil {
		return false
	}
	return DrawUITextArea(TextAreaProps{
		Bounds: bounds, Text: a.buffer, CursorPosition: &a.cursor,
		Focused: &a.focused, ScrollY: &a.scrollY,
		MaxCodepoints: a.maxCodepoints, Font: font, LineGap: lineGap,
		FocusID: a.focusID, Placeholder: placeholder, Syntax: syntax, Style: style,
	})
}

// ScrollState owns a draggable/wheel-scrollable container offset.
type ScrollState struct{ Offset int }

func (s *ScrollState) Begin(bounds Rectangle, contentHeight int, options ScrollOptions) ScrollView {
	return BeginScrollContainer(bounds, contentHeight, &s.Offset, options)
}

func (s *ScrollState) End(bounds Rectangle, view ScrollView) {
	EndScrollContainer(bounds, view)
}

// SelectableText renders text with Kryon's I-beam, character selection and
// Ctrl+C behavior.
func SelectableText(value string, x, y, fontSize int32, color Color) {
	DrawUIText(value, x, y, fontSize, color)
}
