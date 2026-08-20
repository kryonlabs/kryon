package kryui

/*
#include <stdlib.h>
#include <string.h>
*/
import "C"

import (
	gort "runtime"
	"unsafe"
)

// This file is the Go-native stateful control layer. The lower-level Props
// functions remain available for generated code and unusual integrations,
// while applications normally use these owned-state types.

// TextField owns the fixed storage, cursor, focus and selection identity
// needed by Kryon's native single-line editor.
type TextFieldState struct {
	buffer        *C.char
	capacity      int
	cursor        *C.int
	focused       *C.int
	commitPressed *C.int
	focusID       int32
	maxCodepoints int32
	secure        bool
}

func NewTextField(focusID, capacity int) *TextFieldState {
	if capacity < 2 {
		capacity = 2
	}
	f := &TextFieldState{
		buffer:        (*C.char)(C.calloc(C.size_t(capacity), 1)),
		capacity:      capacity,
		cursor:        (*C.int)(C.calloc(1, C.size_t(unsafe.Sizeof(C.int(0))))),
		focused:       (*C.int)(C.calloc(1, C.size_t(unsafe.Sizeof(C.int(0))))),
		commitPressed: (*C.int)(C.calloc(1, C.size_t(unsafe.Sizeof(C.int(0))))),
		focusID:       int32(focusID),
		maxCodepoints: int32(capacity - 1),
	}
	gort.SetFinalizer(f, (*TextFieldState).close)
	return f
}

// NewPasswordField creates a text field that masks its contents and prevents
// clipboard copy and cut operations. Pasting remains available.
func NewPasswordField(focusID, capacity int) *TextFieldState {
	f := NewTextField(focusID, capacity)
	f.secure = true
	return f
}

func (f *TextFieldState) close() {
	if f == nil || f.buffer == nil {
		return
	}
	C.free(unsafe.Pointer(f.buffer))
	C.free(unsafe.Pointer(f.cursor))
	C.free(unsafe.Pointer(f.focused))
	C.free(unsafe.Pointer(f.commitPressed))
	f.buffer, f.cursor, f.focused, f.commitPressed = nil, nil, nil, nil
}

func (f *TextFieldState) Text() string {
	if f == nil || f.buffer == nil {
		return ""
	}
	return C.GoString(f.buffer)
}

func (f *TextFieldState) SetText(text string) {
	if f == nil || f.buffer == nil || f.capacity < 2 {
		return
	}
	bytes := []byte(text)
	if len(bytes) >= f.capacity {
		bytes = bytes[:f.capacity-1]
	}
	C.memset(unsafe.Pointer(f.buffer), 0, C.size_t(f.capacity))
	if len(bytes) > 0 {
		C.memcpy(unsafe.Pointer(f.buffer), unsafe.Pointer(&bytes[0]), C.size_t(len(bytes)))
	}
	*f.cursor = C.int(len(bytes))
}

func (f *TextFieldState) Clear() { f.SetText("") }

func (f *TextFieldState) Focused() bool {
	return f != nil && f.focused != nil && *f.focused != 0
}

func (f *TextFieldState) SetFocused(focused bool) {
	if f != nil && f.focused != nil {
		*f.focused = C.int(0)
		if focused {
			*f.focused = 1
		}
	}
}

// SetSecure controls whether the field masks its value and blocks copy/cut.
// It can be toggled temporarily to implement a reveal-password affordance.
func (f *TextFieldState) SetSecure(secure bool) {
	if f != nil {
		f.secure = secure
	}
}

func (f *TextFieldState) Secure() bool { return f != nil && f.secure }

// Draw renders and edits the field. committed is true for the frame in which
// Enter was pressed. Mouse selection, Ctrl+A/C/X/V and keyboard navigation
// are handled by Kryon.
func (f *TextFieldState) Draw(bounds Rectangle, font int32, style UITextInputStyle) (changed, committed bool) {
	if f == nil {
		return false, false
	}
	*f.commitPressed = 0
	changed = DrawUITextField(TextFieldProps{
		Bounds:         bounds,
		Text:           unsafe.Slice((*byte)(unsafe.Pointer(f.buffer)), f.capacity),
		CursorPosition: (*int32)(unsafe.Pointer(f.cursor)),
		Focused:        nil,
		MaxCodepoints:  f.maxCodepoints,
		Font:           font,
		FocusID:        f.focusID,
		Style:          style,
		CommitPressed:  nil,
		Secure:         f.secure,
	})
	return changed, *f.commitPressed != 0
}

// TextArea owns a multiline editor and its scroll state.
type TextArea struct {
	buffer         []byte
	cursor         int32
	focused        bool
	scrollY        int32
	focusID        int32
	maxCodepoints  int32
	contentVersion int32
}

func NewTextArea(focusID, capacity int) *TextArea {
	if capacity < 2 {
		capacity = 2
	}
	return &TextArea{
		buffer:         make([]byte, capacity),
		focusID:        int32(focusID),
		maxCodepoints:  int32(capacity - 1),
		contentVersion: 1,
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
	old := a.Text()
	clear(a.buffer)
	n := copy(a.buffer[:len(a.buffer)-1], text)
	a.cursor = int32(n)
	if old != a.Text() {
		a.bumpContentVersion()
	}
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
	changed := DrawUITextArea(TextAreaProps{
		Bounds: bounds, Text: a.buffer, CursorPosition: &a.cursor,
		Focused: &a.focused, ScrollY: &a.scrollY,
		MaxCodepoints: a.maxCodepoints, Font: font, LineGap: lineGap,
		FocusID: a.focusID, Placeholder: placeholder, Syntax: syntax, Style: style,
		ContentVersion: a.contentVersion,
	})
	if changed {
		a.bumpContentVersion()
	}
	return changed
}

func (a *TextArea) bumpContentVersion() {
	a.contentVersion++
	if a.contentVersion == 0 {
		a.contentVersion = 1
	}
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
