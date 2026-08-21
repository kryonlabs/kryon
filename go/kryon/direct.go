package kryon

const (
	directButtonWidth  = 96
	directButtonHeight = 32
	directFieldWidth   = 180
	directFieldHeight  = 32
	directFieldBytes   = 256
)

type directTextFieldState struct {
	buf     []byte
	cursor  int32
	focused bool
	focusID int32
	last    string
}

type directTextFieldKey struct {
	label string
	value *string
}

var (
	directTextFields  = map[directTextFieldKey]*directTextFieldState{}
	directNextFocusID = int32(100000)
)

func button(args ...any) bool {
	if len(args) == 1 {
		switch v := args[0].(type) {
		case ButtonProps:
			return active().Button(v)
		case string:
			return active().Button(defaultButtonProps(v))
		}
	}
	return false
}

func textField(args ...any) bool {
	if len(args) == 1 {
		switch v := args[0].(type) {
		case TextFieldProps:
			active().TextField(v)
			return false
		case *string:
			return textFieldString("", v)
		}
	}
	if len(args) == 2 {
		label, ok := args[0].(string)
		if !ok {
			return false
		}
		value, ok := args[1].(*string)
		if !ok {
			return false
		}
		return textFieldString(label, value)
	}
	return false
}

func defaultButtonProps(label string) ButtonProps {
	return ButtonProps{
		Bounds: Rectangle{Width: directButtonWidth, Height: directButtonHeight},
		Label:  label,
		Style:  UIButtonStyleSecondary,
		Font:   Text16,
		ID:     int32(uint64(Key("Button:"+label)) & 0x7fffffff),
	}
}

func textFieldString(label string, value *string) bool {
	if value == nil {
		return false
	}
	key := directTextFieldKey{label: label, value: value}
	state := directTextFields[key]
	if state == nil {
		directNextFocusID++
		state = &directTextFieldState{
			buf:     make([]byte, maxInt(directFieldBytes, len(*value)+1)),
			focusID: directNextFocusID,
		}
		directTextFields[key] = state
	}
	if len(state.buf) <= len(*value) {
		state.buf = make([]byte, len(*value)+64)
	}
	if state.last != *value {
		clear(state.buf)
		copy(state.buf, *value)
		state.cursor = int32(len(*value))
		state.last = *value
	}

	before := string(state.buf[:zeroIndex(state.buf)])
	active().TextField(TextFieldProps{
		Bounds:         Rectangle{Width: directFieldWidth, Height: directFieldHeight},
		Text:           state.buf,
		CursorPosition: &state.cursor,
		Focused:        &state.focused,
		MaxCodepoints:  int32(len(state.buf) - 1),
		Font:           Text16,
		FocusID:        state.focusID,
	})
	after := string(state.buf[:zeroIndex(state.buf)])
	if after == before {
		return false
	}
	*value = after
	state.last = after
	return true
}

func maxInt(a, b int) int {
	if a > b {
		return a
	}
	return b
}
