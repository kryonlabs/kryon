package kryui

/*
#cgo CFLAGS: -I${SRCDIR}/../../include
#include <stdlib.h>
#include <kryon.h>

static int kry_event_value(UIEvent event) { return event.data.value; }
static int kry_event_selection_start(UIEvent event) { return event.data.selection.start; }
static int kry_event_selection_end(UIEvent event) { return event.data.selection.end; }
static int kry_event_text_bytes(UIEvent event) { return event.data.text.bytes; }
*/
import "C"

import "unsafe"

type UIKey uint64

type UIEventKind int32

const (
	EventNone UIEventKind = iota
	EventClick
	EventValueChanged
	EventTextChanged
	EventTextCommit
	EventSelectionChanged
	EventFocus
	EventBlur
)

type UIEvent struct {
	Key            UIKey
	Kind           UIEventKind
	Timestamp      float64
	Value          int32
	SelectionStart int32
	SelectionEnd   int32
	TextBytes      int32
}

type ColumnProps struct {
	Bounds  Rectangle
	Gap     int32
	Padding int32
	Key     UIKey
}

type RowProps = ColumnProps

type FieldProps struct {
	Bounds Rectangle
	Font   int32
	Style  UITextInputStyle
}

func Key(text string) UIKey {
	value := C.CString(text)
	defer C.free(unsafe.Pointer(value))
	return UIKey(C.Key(value))
}

func BeginUI(key UIKey) {
	w, h := GetScreenWidth(), GetScreenHeight()
	dpi := GetWindowScaleDPI().X
	if dpi <= 0 {
		dpi = 1
	}
	C.BeginUIFrame(C.int(w), C.int(h), C.float(dpi))
	C.BeginUI(C.UIKey(key))
}

func EndUI() {
	C.EndUI()
	C.EndUIFrame()
}

func End() { C.End() }

func Column(props ColumnProps) {
	C.Column(C.ColumnProps{bounds: props.Bounds.toC(), gap: C.int(props.Gap),
		padding: C.int(props.Padding), key: C.UIKey(props.Key)})
}

func Row(props RowProps) {
	C.Row(C.RowProps{bounds: props.Bounds.toC(), gap: C.int(props.Gap),
		padding: C.int(props.Padding), key: C.UIKey(props.Key)})
}

func Stack(props ColumnProps) {
	C.Stack(C.ColumnProps{bounds: props.Bounds.toC(), gap: C.int(props.Gap),
		padding: C.int(props.Padding), key: C.UIKey(props.Key)})
}

func TextField(state *TextFieldState, props FieldProps) {
	if state == nil || state.buffer == nil {
		return
	}
	secure := C.int(0)
	if state.secure {
		secure = 1
	}
	C.TextField(C.TextFieldProps{
		bounds: props.Bounds.toC(), text: state.buffer,
		text_size: C.size_t(state.capacity), cursor_position: state.cursor,
		focused: state.focused, max_codepoints: C.int(state.maxCodepoints),
		font: C.int(props.Font), focus_id: C.int(state.focusID),
		style: props.Style.toC(), commit_pressed: state.commitPressed,
		secure: secure,
	})
}

func declareTextField(props TextFieldProps) {
	if len(props.Text) == 0 || props.CursorPosition == nil {
		return
	}
	var focused C.int
	if props.Focused != nil && *props.Focused {
		focused = 1
	}
	secure := C.int(0)
	if props.Secure {
		secure = 1
	}
	C.TextField(C.TextFieldProps{
		bounds: props.Bounds.toC(), text: (*C.char)(unsafe.Pointer(&props.Text[0])),
		text_size: C.size_t(len(props.Text)), cursor_position: (*C.int)(unsafe.Pointer(props.CursorPosition)),
		focused: &focused, max_codepoints: C.int(props.MaxCodepoints), font: C.int(props.Font),
		focus_id: C.int(props.FocusID), style: props.Style.toC(), secure: secure,
	})
	if props.Focused != nil {
		*props.Focused = focused != 0
	}
}

func SetSelection(key UIKey, anchor, cursor int32) bool {
	return C.SetSelection(C.UIKey(key), C.int(anchor), C.int(cursor)) != 0
}

func NextUIEvent() (UIEvent, bool) {
	var event C.UIEvent
	if C.NextUIEvent(&event) == 0 {
		return UIEvent{}, false
	}
	return UIEvent{
		Key: UIKey(event.key), Kind: UIEventKind(event.kind),
		Timestamp:      float64(event.timestamp),
		Value:          int32(C.kry_event_value(event)),
		SelectionStart: int32(C.kry_event_selection_start(event)),
		SelectionEnd:   int32(C.kry_event_selection_end(event)),
		TextBytes:      int32(C.kry_event_text_bytes(event)),
	}, true
}
