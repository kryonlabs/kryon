package kryon

import (
	"fmt"
	"math"
	"strconv"
	"time"
)

type dragFloatProps struct {
	Bounds     Rectangle
	ID         int32
	ClassName  int32
	Label      string
	Values     []float32
	ValueCount int32
	Speed      float32
	Min        float32
	Max        float32
	Format     string
	Disabled   bool
}

type dragIntProps struct {
	Bounds     Rectangle
	ID         int32
	ClassName  int32
	Label      string
	Values     []int32
	ValueCount int32
	Speed      float32
	Min        int32
	Max        int32
	Format     string
	Disabled   bool
}

type dragFloatRangeProps struct {
	Bounds     Rectangle
	ID         int32
	ClassName  int32
	Label      string
	CurrentMin *float32
	CurrentMax *float32
	Speed      float32
	Min        float32
	Max        float32
	Format     string
	FormatMax  string
	Disabled   bool
}

type dragIntRangeProps struct {
	Bounds     Rectangle
	ID         int32
	ClassName  int32
	Label      string
	CurrentMin *int32
	CurrentMax *int32
	Speed      float32
	Min        int32
	Max        int32
	Format     string
	FormatMax  string
	Disabled   bool
}

type sliderFloatProps struct {
	Bounds     Rectangle
	ID         int32
	Label      string
	Values     []float32
	ValueCount int32
	Min        float32
	Max        float32
	Format     string
	Disabled   bool
	ClassName  int32
}

type sliderIntProps struct {
	Bounds     Rectangle
	ID         int32
	Label      string
	Values     []int32
	ValueCount int32
	Min        int32
	Max        int32
	Format     string
	Disabled   bool
	ClassName  int32
}

type sliderAngleProps struct {
	Bounds     Rectangle
	ID         int32
	Label      string
	Value      *float32
	MinDegrees float32
	MaxDegrees float32
	Format     string
	Disabled   bool
	ClassName  int32
}

type inputFloatProps struct {
	Bounds     Rectangle
	ID         int32
	Label      string
	Values     []float32
	ValueCount int32
	Step       float32
	StepFast   float32
	Format     string
	Disabled   bool
}

type inputIntProps struct {
	Bounds     Rectangle
	ID         int32
	Label      string
	Values     []int32
	ValueCount int32
	Step       int32
	StepFast   int32
	Format     string
	Disabled   bool
}

type inputDoubleProps struct {
	Bounds     Rectangle
	ID         int32
	Label      string
	Values     []float64
	ValueCount int32
	Step       float64
	StepFast   float64
	Format     string
	Disabled   bool
}

func (r *runtime) dragDelta(token, focusID int32, bounds Rectangle, disabled bool) (float32, bool) {
	disabled = disabled || r.contentDisabled()
	if r.drag.active && r.popupInputOwnerCaptures(r.drag.owner) {
		r.drag = scalarDrag{}
	}
	if disabled && r.drag.active && r.drag.token == token {
		r.drag = scalarDrag{}
	}
	if !disabled && r.mousePressed[MouseButtonLeft] && r.consumeTap(bounds) {
		r.drag = scalarDrag{active: true, token: token, lastX: r.mousePos.X, owner: r.currentPopupInputOwner()}
		if focusID > 0 {
			r.setFocus(focusID)
		}
	}
	if r.drag.active && r.drag.token == token && r.mouseDown[MouseButtonLeft] {
		delta := r.mousePos.X - r.drag.lastX
		r.drag.lastX = r.mousePos.X
		return delta, delta != 0
	}
	if r.drag.active && r.drag.token == token && r.mouseReleased[MouseButtonLeft] {
		r.drag = scalarDrag{}
	}
	return 0, false
}

func (r *runtime) dragFloatKeyboard(focusID int32, speed, minimum, maximum, value float32) (float32, bool) {
	if focusID <= 0 || r.focusID != focusID || r.popupFocusCaptures(focusID) {
		return value, false
	}
	next := value
	remaining := r.inputEvents[:0]
	for _, event := range r.inputEvents {
		handled := false
		if !event.shortcut {
			direction := r.sliderKeyboardDirection(false, event.key)
			input := Drag_DragKeyboardInputFor(direction,
				event.key == KeyHome, event.key == KeyEnd,
				r.keyDown[KeyLeftAlt] || r.keyDown[KeyRightAlt],
				event.shift || r.keyDown[KeyLeftShift] || r.keyDown[KeyRightShift])
			step := Drag_DragKeyboardValue(next, speed, minimum, maximum, input)
			if step.Changed {
				next, handled = step.Value, true
			}
		}
		if !handled {
			remaining = append(remaining, event)
		}
	}
	r.inputEvents = remaining
	return next, next != value
}

func (r *runtime) dragIntKeyboard(focusID int32, speed float32, minimum, maximum, value int32) (int32, bool) {
	if focusID <= 0 || r.focusID != focusID || r.popupFocusCaptures(focusID) {
		return value, false
	}
	next := value
	remaining := r.inputEvents[:0]
	for _, event := range r.inputEvents {
		handled := false
		if !event.shortcut {
			direction := r.sliderKeyboardDirection(false, event.key)
			input := Drag_DragKeyboardInputFor(direction,
				event.key == KeyHome, event.key == KeyEnd,
				r.keyDown[KeyLeftAlt] || r.keyDown[KeyRightAlt],
				event.shift || r.keyDown[KeyLeftShift] || r.keyDown[KeyRightShift])
			step := Drag_DragDiscreteKeyboardValue(next, speed, minimum, maximum, input)
			if step.Changed {
				next, handled = step.Value, true
			}
		}
		if !handled {
			remaining = append(remaining, event)
		}
	}
	r.inputEvents = remaining
	return next, next != value
}

func (r *runtime) dragFloat(props dragFloatProps) bool {
	props.Bounds = r.layoutRect(props.Bounds)
	count := int(props.ValueCount)
	if count <= 0 || count > len(props.Values) {
		count = len(props.Values)
	}
	if count == 0 {
		return false
	}
	speed := Drag_DragEffectiveSpeed(props.Speed)
	changed := false
	for i := 0; i < count; i++ {
		focusID := sliderFocusID(props.ID, int32(i), false)
		cell := Rectangle{X: props.Bounds.X + float32(i)*props.Bounds.Width/float32(count), Y: props.Bounds.Y, Width: props.Bounds.Width / float32(count), Height: props.Bounds.Height}
		enabled := !props.Disabled && !r.contentDisabled()
		if enabled {
			r.registerField(focusID)
		}
		edited, editing := r.numericTempFloat(cell,
			numericInputKey{kind: numericEditDragContinuous, widgetID: props.ID, component: int32(i)},
			focusID, props.Values, i, props.Format, props.Disabled)
		changed = changed || edited
		if editing {
			continue
		}
		if enabled {
			if next, keyboardChanged := r.dragFloatKeyboard(focusID, speed, props.Min, props.Max, props.Values[i]); keyboardChanged {
				props.Values[i] = next
				changed = true
			}
		}
		if delta, dragged := r.dragDelta(Drag_DragComponentTokenFor(props.ID, int32(i)), focusID, cell, props.Disabled); dragged {
			step := Drag_DragDeltaValue(props.Values[i], delta, speed, props.Min, props.Max)
			changed = changed || step.Changed
			props.Values[i] = step.Value
		}
		format := props.Format
		if format == "" {
			format = "%.3f"
		}
		focused := enabled && focusID > 0 && r.focusID == focusID && !r.popupFocusCaptures(focusID)
		r.drawDragCell(cell, fmt.Sprintf(format, props.Values[i]), !enabled, focused, props.ClassName, props.ID, int32(i))
	}
	r.drawDragLabel(props.Bounds, props.Label, props.ClassName)
	return changed
}

func (r *runtime) dragInt(props dragIntProps) bool {
	props.Bounds = r.layoutRect(props.Bounds)
	count := int(props.ValueCount)
	if count <= 0 || count > len(props.Values) {
		count = len(props.Values)
	}
	if count == 0 {
		return false
	}
	speed := Drag_DragEffectiveSpeed(props.Speed)
	changed := false
	for i := 0; i < count; i++ {
		focusID := sliderFocusID(props.ID, int32(i), true)
		cell := Rectangle{X: props.Bounds.X + float32(i)*props.Bounds.Width/float32(count), Y: props.Bounds.Y, Width: props.Bounds.Width / float32(count), Height: props.Bounds.Height}
		enabled := !props.Disabled && !r.contentDisabled()
		if enabled {
			r.registerField(focusID)
		}
		edited, editing := r.numericTempInt(cell,
			numericInputKey{kind: numericEditDragDiscrete, widgetID: props.ID, component: int32(i)},
			focusID, props.Values, i, props.Format, props.Disabled)
		changed = changed || edited
		if editing {
			continue
		}
		if enabled {
			if next, keyboardChanged := r.dragIntKeyboard(focusID, speed, props.Min, props.Max, props.Values[i]); keyboardChanged {
				props.Values[i] = next
				changed = true
			}
		}
		if delta, dragged := r.dragDelta(Drag_DragComponentTokenFor(props.ID, int32(i)), focusID, cell, props.Disabled); dragged {
			step := Drag_DragDiscreteDeltaValue(props.Values[i], delta, speed, props.Min, props.Max)
			changed = changed || step.Changed
			props.Values[i] = step.Value
		}
		format := props.Format
		if format == "" {
			format = "%d"
		}
		focused := enabled && focusID > 0 && r.focusID == focusID && !r.popupFocusCaptures(focusID)
		r.drawDragCell(cell, fmt.Sprintf(format, props.Values[i]), !enabled, focused, props.ClassName, props.ID, int32(i))
	}
	r.drawDragLabel(props.Bounds, props.Label, props.ClassName)
	return changed
}

func (r *runtime) dragFloatRange(props dragFloatRangeProps) bool {
	if props.CurrentMin == nil || props.CurrentMax == nil {
		return false
	}
	props.Bounds = r.layoutRect(props.Bounds)
	speed := Drag_DragEffectiveSpeed(props.Speed)
	changed := false
	values := [2]*float32{props.CurrentMin, props.CurrentMax}
	formats := [2]string{props.Format, props.FormatMax}
	for i := 0; i < 2; i++ {
		focusID := sliderFocusID(props.ID, int32(i), false)
		cell := Rectangle{X: props.Bounds.X + float32(i)*props.Bounds.Width/2, Y: props.Bounds.Y, Width: props.Bounds.Width / 2, Height: props.Bounds.Height}
		low, high := props.Min, props.Max
		if i == 0 && *props.CurrentMax < high {
			high = *props.CurrentMax
		}
		if i == 1 && *props.CurrentMin > low {
			low = *props.CurrentMin
		}
		enabled := !props.Disabled && !r.contentDisabled()
		if enabled {
			r.registerField(focusID)
			if next, keyboardChanged := r.dragFloatKeyboard(focusID, speed, low, high, *values[i]); keyboardChanged {
				*values[i] = next
				changed = true
			}
		}
		if delta, dragged := r.dragDelta(Drag_DragComponentTokenFor(props.ID, int32(i)), focusID, cell, props.Disabled); dragged {
			step := Drag_DragDeltaValue(*values[i], delta, speed, low, high)
			changed = changed || step.Changed
			*values[i] = step.Value
		}
		format := formats[i]
		if format == "" {
			format = props.Format
		}
		if format == "" {
			format = "%.3f"
		}
		focused := enabled && focusID > 0 && r.focusID == focusID && !r.popupFocusCaptures(focusID)
		r.drawDragCell(cell, fmt.Sprintf(format, *values[i]), !enabled, focused, props.ClassName, props.ID, int32(i))
	}
	if *props.CurrentMin > *props.CurrentMax {
		*props.CurrentMin = *props.CurrentMax
	}
	r.drawDragLabel(props.Bounds, props.Label, props.ClassName)
	return changed
}

func (r *runtime) dragIntRange(props dragIntRangeProps) bool {
	if props.CurrentMin == nil || props.CurrentMax == nil {
		return false
	}
	props.Bounds = r.layoutRect(props.Bounds)
	speed := Drag_DragEffectiveSpeed(props.Speed)
	changed := false
	values := [2]*int32{props.CurrentMin, props.CurrentMax}
	formats := [2]string{props.Format, props.FormatMax}
	for i := 0; i < 2; i++ {
		focusID := sliderFocusID(props.ID, int32(i), true)
		cell := Rectangle{X: props.Bounds.X + float32(i)*props.Bounds.Width/2, Y: props.Bounds.Y, Width: props.Bounds.Width / 2, Height: props.Bounds.Height}
		low, high := props.Min, props.Max
		if i == 0 && *props.CurrentMax < high {
			high = *props.CurrentMax
		}
		if i == 1 && *props.CurrentMin > low {
			low = *props.CurrentMin
		}
		enabled := !props.Disabled && !r.contentDisabled()
		if enabled {
			r.registerField(focusID)
			if next, keyboardChanged := r.dragIntKeyboard(focusID, speed, low, high, *values[i]); keyboardChanged {
				*values[i] = next
				changed = true
			}
		}
		if delta, dragged := r.dragDelta(Drag_DragComponentTokenFor(props.ID, int32(i)), focusID, cell, props.Disabled); dragged {
			step := Drag_DragDiscreteDeltaValue(*values[i], delta, speed, low, high)
			changed = changed || step.Changed
			*values[i] = step.Value
		}
		format := formats[i]
		if format == "" {
			format = props.Format
		}
		if format == "" {
			format = "%d"
		}
		focused := enabled && focusID > 0 && r.focusID == focusID && !r.popupFocusCaptures(focusID)
		r.drawDragCell(cell, fmt.Sprintf(format, *values[i]), !enabled, focused, props.ClassName, props.ID, int32(i))
	}
	if *props.CurrentMin > *props.CurrentMax {
		*props.CurrentMin = *props.CurrentMax
	}
	r.drawDragLabel(props.Bounds, props.Label, props.ClassName)
	return changed
}

func (r *runtime) Drag(props DragProps) bool {
	count := props.ValueCount
	if count <= 0 {
		if props.Kind == NumericInt {
			count = int32(len(props.IntValues))
		} else {
			count = int32(len(props.FloatValues))
		}
	}
	if props.Mode == DragRange {
		if props.Kind == NumericInt {
			return r.dragIntRange(dragIntRangeProps{
				Bounds:     props.Bounds,
				ID:         props.ID,
				ClassName:  props.ClassName,
				Label:      props.Label,
				CurrentMin: props.IntMin,
				CurrentMax: props.IntMax,
				Speed:      props.Speed,
				Min:        int32(props.Min),
				Max:        int32(props.Max),
				Format:     props.Format,
				FormatMax:  props.FormatMax,
				Disabled:   props.Disabled,
			})
		}
		return r.dragFloatRange(dragFloatRangeProps{
			Bounds:     props.Bounds,
			ID:         props.ID,
			ClassName:  props.ClassName,
			Label:      props.Label,
			CurrentMin: props.FloatMin,
			CurrentMax: props.FloatMax,
			Speed:      props.Speed,
			Min:        float32(props.Min),
			Max:        float32(props.Max),
			Format:     props.Format,
			FormatMax:  props.FormatMax,
			Disabled:   props.Disabled,
		})
	}
	if props.Kind == NumericInt {
		return r.dragInt(dragIntProps{
			Bounds:     props.Bounds,
			ID:         props.ID,
			ClassName:  props.ClassName,
			Label:      props.Label,
			Values:     props.IntValues,
			ValueCount: count,
			Speed:      props.Speed,
			Min:        int32(props.Min),
			Max:        int32(props.Max),
			Format:     props.Format,
			Disabled:   props.Disabled,
		})
	}
	return r.dragFloat(dragFloatProps{
		Bounds:     props.Bounds,
		ID:         props.ID,
		ClassName:  props.ClassName,
		Label:      props.Label,
		Values:     props.FloatValues,
		ValueCount: count,
		Speed:      props.Speed,
		Min:        float32(props.Min),
		Max:        float32(props.Max),
		Format:     props.Format,
		Disabled:   props.Disabled,
	})
}

func (r *runtime) drawDragCell(bounds Rectangle, text string, disabled, focused bool, className, id, component int32) {
	pressed := r.drag.active && r.drag.token == Drag_DragComponentTokenFor(id, component)
	state := ButtonStateNormal
	if disabled {
		state = ButtonStateDisabled
	} else if pressed {
		state = ButtonStatePressed
	} else if focused {
		state = ButtonStateFocus
	}
	props := ButtonProps{
		Bounds:    bounds,
		Label:     text,
		ID:        id,
		ClassName: className,
		Tone:      ButtonToneNeutral,
		Emphasis:  ButtonEmphasisSoft,
		Size:      ControlSizeMedium,
		Disabled:  disabled,
	}
	frame := simpleStyleFrameWithClassRole(ButtonToneNeutral, state, disabled, false,
		className, StyleSheet_StyleKindDragValue(), StyleSheet_StyleAny())
	button := Button_BuildFrame(props, ButtonInput{}, frame, InteractionMotion{},
		Rectangle{}, packRGBA(r.appAmbientColor()), 1, Text14, Text14)
	style := unpackStyle(button.Appearance.Value)
	font, fontID := styleTextFace(style, Text14)
	r.recordButton(FrameOp{Kind: FrameOpButton, Button: button,
		Opacity: style.Opacity, BorderWidth: style.BorderWidth, Radius: style.Radius,
		Material: MaterialKind(style.Material), FillStates: styleFill(style),
		FillStatesValid: true, AmbientColor: r.appAmbientColor(), FocusColor: style.Focus,
		Bounds: bounds, Text: text, Color: style.Background, BorderColor: style.Border,
		TextColor: style.Foreground, FontSize: font, FontID: fontID, ID: id, Row: component,
		Disabled: disabled, Pressed: pressed, Focused: focused})
}

func (r *runtime) drawDragLabel(bounds Rectangle, label string, className int32) {
	if label == "" {
		return
	}
	style := unpackStyle(simpleStyleFrameWithClassRole(ButtonToneNeutral, ButtonStateNormal, false,
		false, className, StyleSheet_StyleKindDrag(), StyleSheet_StyleAny()).Value)
	font, fontID := styleTextFace(style, Text14)
	r.record(FrameOp{Kind: FrameOpText, Bounds: Rectangle{X: bounds.X + 6, Y: bounds.Y - float32(font) - 4, Width: bounds.Width - 12, Height: float32(font)}, Text: label, Color: style.Foreground, Opacity: style.Opacity, FontSize: font, FontID: fontID})
}

func (r *runtime) numericTempEdit(bounds Rectangle, key numericInputKey, focusID int32, formatted string, disabled bool) (*numericInputState, bool, bool) {
	enabled := !disabled && !r.contentDisabled()
	control := r.keyDown[KeyLeftControl] || r.keyDown[KeyRightControl]
	pressed := enabled && r.mousePressed[MouseButtonLeft] && r.hasTap(bounds)
	now := time.Now()
	dx := r.mousePos.X - r.lastNumericClick.x
	dy := r.mousePos.Y - r.lastNumericClick.y
	doubleClick := pressed && r.lastNumericClick.key == key &&
		now.Sub(r.lastNumericClick.when) <= 300*time.Millisecond &&
		dx >= -6 && dx <= 6 && dy >= -6 && dy <= 6
	activate := pressed && (control || doubleClick) && r.consumeTap(bounds)
	if pressed {
		r.lastNumericClick = numericClick{key: key, x: r.mousePos.X, y: r.mousePos.Y, when: now}
		if activate {
			r.lastNumericClick = numericClick{}
		}
	}
	state := r.numericInputs[key]
	if state == nil && !activate {
		return nil, false, false
	}
	if state == nil {
		state = r.numericInputState(key, formatted)
	}
	if !enabled && state.focused {
		state.focused = false
		if r.focusID == focusID {
			r.setFocus(0)
		}
	}
	if activate {
		r.setNumericInputText(key, formatted)
		state.focused = true
		r.setFocus(focusID)
		r.drag = scalarDrag{}
		r.slider = scalarDrag{}
	}
	if !state.focused {
		return state, false, false
	}
	commit := false
	textChanged := r.editText(bounds, state.text, &state.cursor, &state.focused, &commit, focusID, textEditOptions{maxCodepoints: 63})
	r.recordTextInput(FrameOpTextField, bounds, state.text, &state.cursor, &state.focused, focusID, Text14, false, false)
	if commit {
		state.focused = false
		r.setFocus(focusID)
	}
	return state, textChanged, state.focused
}

func (r *runtime) numericTempFloat(bounds Rectangle, key numericInputKey, focusID int32, values []float32, index int, format string, disabled bool) (bool, bool) {
	if format == "" {
		format = "%.3f"
	}
	state, textChanged, editing := r.numericTempEdit(bounds, key, focusID, fmt.Sprintf(format, values[index]), disabled)
	if textChanged {
		text := string(state.text[:zeroIndex(state.text)])
		if parsed, err := strconv.ParseFloat(text, 32); err == nil && !math.IsInf(parsed, 0) && !math.IsNaN(parsed) && values[index] != float32(parsed) {
			values[index] = float32(parsed)
			return true, editing
		}
	}
	return false, editing
}

func (r *runtime) numericTempInt(bounds Rectangle, key numericInputKey, focusID int32, values []int32, index int, format string, disabled bool) (bool, bool) {
	if format == "" {
		format = "%d"
	}
	state, textChanged, editing := r.numericTempEdit(bounds, key, focusID, fmt.Sprintf(format, values[index]), disabled)
	if textChanged {
		text := string(state.text[:zeroIndex(state.text)])
		if parsed, err := strconv.ParseInt(text, 0, 32); err == nil && values[index] != int32(parsed) {
			values[index] = int32(parsed)
			return true, editing
		}
	}
	return false, editing
}

func (r *runtime) sliderRatio(token, focusID int32, bounds Rectangle, disabled, vertical bool) (float32, bool) {
	disabled = disabled || r.contentDisabled()
	if r.slider.active && r.popupInputOwnerCaptures(r.slider.owner) {
		r.slider = scalarDrag{}
	}
	if disabled && r.slider.active && r.slider.token == token {
		r.slider = scalarDrag{}
	}
	pressed := !disabled && r.mousePressed[MouseButtonLeft] && r.consumeTap(bounds)
	if pressed {
		r.slider = scalarDrag{active: true, token: token, owner: r.currentPopupInputOwner()}
		if focusID > 0 {
			r.setFocus(focusID)
		}
	}
	if r.slider.active && r.slider.token == token && (pressed || r.mouseDown[MouseButtonLeft]) {
		var ratio float32
		if vertical {
			if bounds.Height > 0 {
				ratio = (bounds.Y + bounds.Height - r.mousePos.Y) / bounds.Height
			}
		} else if bounds.Width > 0 {
			ratio = (r.mousePos.X - bounds.X) / bounds.Width
		}
		if ratio < 0 {
			ratio = 0
		} else if ratio > 1 {
			ratio = 1
		}
		return ratio, true
	}
	if r.slider.active && r.slider.token == token && r.mouseReleased[MouseButtonLeft] {
		r.slider = scalarDrag{}
	}
	return 0, false
}

func sliderFocusID(id, component int32, integer bool) int32 {
	return Slider_SliderFocusIdFor(id, component, integer)
}

func (r *runtime) sliderKeyboardDirection(vertical bool, key int32) int32 {
	if vertical {
		if key == KeyUp {
			return 1
		}
		if key == KeyDown {
			return -1
		}
	} else {
		if key == KeyRight {
			return 1
		}
		if key == KeyLeft {
			return -1
		}
	}
	return 0
}

func (r *runtime) sliderFloatKeyboard(focusID int32, vertical bool, minimum, maximum, value float32) (float32, bool) {
	if focusID <= 0 || r.focusID != focusID || r.popupFocusCaptures(focusID) {
		return value, false
	}
	next := value
	remaining := r.inputEvents[:0]
	for _, event := range r.inputEvents {
		handled := false
		if !event.shortcut {
			direction := r.sliderKeyboardDirection(vertical, event.key)
			step := Slider_SliderKeyboardValue(next, minimum, maximum,
				direction, event.key == KeyHome, event.key == KeyEnd,
				r.keyDown[KeyLeftAlt] || r.keyDown[KeyRightAlt],
				event.shift || r.keyDown[KeyLeftShift] || r.keyDown[KeyRightShift])
			if step.Changed {
				next, handled = step.Value, true
			}
		}
		if !handled {
			remaining = append(remaining, event)
		}
	}
	r.inputEvents = remaining
	return next, next != value
}

func (r *runtime) sliderIntKeyboard(focusID int32, vertical bool, minimum, maximum, value int32) (int32, bool) {
	if focusID <= 0 || r.focusID != focusID || r.popupFocusCaptures(focusID) {
		return value, false
	}
	next := value
	remaining := r.inputEvents[:0]
	for _, event := range r.inputEvents {
		handled := false
		if !event.shortcut {
			direction := r.sliderKeyboardDirection(vertical, event.key)
			step := Slider_SliderDiscreteKeyboardValue(next, minimum, maximum,
				direction, event.key == KeyHome, event.key == KeyEnd,
				r.keyDown[KeyLeftAlt] || r.keyDown[KeyRightAlt],
				event.shift || r.keyDown[KeyLeftShift] || r.keyDown[KeyRightShift])
			if step.Changed {
				next, handled = step.Value, true
			}
		}
		if !handled {
			remaining = append(remaining, event)
		}
	}
	r.inputEvents = remaining
	return next, next != value
}

func (r *runtime) drawSliderCell(bounds Rectangle, ratio float32, text string, disabled, vertical, focused bool, className, id, component int32) {
	hovered := !disabled && pointInRect(r.mousePos.X, r.mousePos.Y, bounds)
	state := ButtonStateNormal
	if disabled {
		state = ButtonStateDisabled
	} else if focused {
		state = ButtonStateFocus
	} else if hovered {
		state = ButtonStateHover
	}
	trackFrame := simpleStyleFrameWithClassRole(ButtonToneNeutral, state, disabled, false, className, StyleSheet_StyleKindSlider(), Slider_SliderTrackRole())
	activeFrame := simpleStyleFrameWithClassRole(ButtonToneAccent, state, disabled, true, className, StyleSheet_StyleKindSlider(), Slider_SliderFillRole())
	labelFrame := simpleStyleFrameWithClassRole(ButtonToneNeutral, state, disabled, false, className, StyleSheet_StyleKindSlider(), Slider_SliderLabelRole())
	trackStyle := unpackStyle(trackFrame.Value)
	activeStyle := unpackStyle(activeFrame.Value)
	labelStyle := unpackStyle(labelFrame.Value)
	trackOp := styleFrameRectOp(bounds, Rectangle{}, trackFrame)
	trackOp.ID = id
	trackOp.Row = component
	trackOp.Disabled = disabled
	trackOp.Focused = focused
	trackOp.Hovered = hovered
	if focused {
		trackOp.BorderColor = trackOp.FocusColor
	}
	r.record(trackOp)
	if vertical {
		fill := Rectangle{X: bounds.X, Y: bounds.Y + bounds.Height*(1-ratio), Width: bounds.Width, Height: bounds.Height * ratio}
		r.record(FrameOp{Kind: FrameOpRect, Bounds: fill, Color: activeStyle.Background, BorderColor: activeStyle.Border, ID: id, Row: component, Selected: true, Disabled: disabled})
		y := bounds.Y + bounds.Height*(1-ratio)
		r.record(FrameOp{Kind: FrameOpLine, Bounds: Rectangle{X: bounds.X, Y: y, Width: bounds.Width}, Color: trackStyle.Foreground, ID: id, Row: component})
	} else {
		fill := Rectangle{X: bounds.X, Y: bounds.Y, Width: bounds.Width * ratio, Height: bounds.Height}
		r.record(FrameOp{Kind: FrameOpRect, Bounds: fill, Color: activeStyle.Background, BorderColor: activeStyle.Border, ID: id, Row: component, Selected: true, Disabled: disabled})
		x := bounds.X + bounds.Width*ratio
		r.record(FrameOp{Kind: FrameOpLine, Bounds: Rectangle{X: x, Y: bounds.Y, Height: bounds.Height}, Color: trackStyle.Foreground, ID: id, Row: component})
	}
	labelFont, labelFontID := styleTextFace(labelStyle, Text14)
	r.record(FrameOp{Kind: FrameOpText, Bounds: Rectangle{X: bounds.X + 6, Y: bounds.Y + (bounds.Height-float32(labelFont))/2, Width: bounds.Width - 12, Height: float32(labelFont)}, Text: text, Color: labelStyle.Foreground, Opacity: labelStyle.Opacity, FontSize: labelFont, FontID: labelFontID, ID: id, Row: component})
}

func (r *runtime) drawSliderLabel(bounds Rectangle, label string, className, id int32) {
	if label != "" {
		style := unpackStyle(simpleStyleFrameWithClassRole(ButtonToneNeutral, ButtonStateNormal, false, false, className, StyleSheet_StyleKindSlider(), Slider_SliderLabelRole()).Value)
		font, fontID := styleTextFace(style, Text14)
		r.record(FrameOp{Kind: FrameOpText, Bounds: Rectangle{X: bounds.X + 6, Y: bounds.Y - float32(font) - 4, Width: bounds.Width - 12, Height: float32(font)}, Text: label, Color: style.Foreground, Opacity: style.Opacity, FontSize: font, FontID: fontID, ID: id})
	}
}

func (r *runtime) sliderFloat(props sliderFloatProps, vertical bool) bool {
	props.Bounds = r.layoutRect(props.Bounds)
	count := int(props.ValueCount)
	if count <= 0 || count > len(props.Values) {
		count = len(props.Values)
	}
	if count == 0 {
		return false
	}
	changed := false
	for i := 0; i < count; i++ {
		focusID := sliderFocusID(props.ID, int32(i), false)
		cell := Rectangle{X: props.Bounds.X + float32(i)*props.Bounds.Width/float32(count), Y: props.Bounds.Y, Width: props.Bounds.Width / float32(count), Height: props.Bounds.Height}
		enabled := !props.Disabled && !r.contentDisabled()
		if enabled {
			r.registerField(focusID)
		}
		edited, editing := r.numericTempFloat(cell,
			numericInputKey{kind: numericEditSliderContinuous, widgetID: props.ID, component: int32(i)},
			focusID, props.Values, i, props.Format, props.Disabled)
		changed = changed || edited
		if editing {
			continue
		}
		ratio := Slider_SliderRatio(props.Values[i], props.Min, props.Max)
		if enabled {
			if next, keyboardChanged := r.sliderFloatKeyboard(focusID, vertical, props.Min, props.Max, props.Values[i]); keyboardChanged {
				props.Values[i] = next
				ratio = Slider_SliderRatio(next, props.Min, props.Max)
				changed = true
			}
		}
		if next, active := r.sliderRatio(Slider_SliderFocusIdFor(props.ID, int32(i), false), focusID, cell, props.Disabled, vertical); active && props.Max > props.Min {
			ratio = next
			value := Slider_SliderValue(props.Min, props.Max, ratio)
			changed = changed || value != props.Values[i]
			props.Values[i] = value
		}
		format := props.Format
		if format == "" {
			format = "%.3f"
		}
		focused := enabled && focusID > 0 && r.focusID == focusID && !r.popupFocusCaptures(focusID)
		r.drawSliderCell(cell, ratio, fmt.Sprintf(format, props.Values[i]), props.Disabled, vertical, focused, props.ClassName, props.ID, int32(i))
	}
	r.drawSliderLabel(props.Bounds, props.Label, props.ClassName, props.ID)
	return changed
}

func (r *runtime) sliderInt(props sliderIntProps, vertical bool) bool {
	props.Bounds = r.layoutRect(props.Bounds)
	count := int(props.ValueCount)
	if count <= 0 || count > len(props.Values) {
		count = len(props.Values)
	}
	if count == 0 {
		return false
	}
	changed := false
	for i := 0; i < count; i++ {
		focusID := sliderFocusID(props.ID, int32(i), true)
		cell := Rectangle{X: props.Bounds.X + float32(i)*props.Bounds.Width/float32(count), Y: props.Bounds.Y, Width: props.Bounds.Width / float32(count), Height: props.Bounds.Height}
		enabled := !props.Disabled && !r.contentDisabled()
		if enabled {
			r.registerField(focusID)
		}
		edited, editing := r.numericTempInt(cell,
			numericInputKey{kind: numericEditSliderDiscrete, widgetID: props.ID, component: int32(i)},
			focusID, props.Values, i, props.Format, props.Disabled)
		changed = changed || edited
		if editing {
			continue
		}
		ratio := Slider_SliderDiscreteRatio(props.Values[i], props.Min, props.Max)
		if enabled {
			if next, keyboardChanged := r.sliderIntKeyboard(focusID, vertical, props.Min, props.Max, props.Values[i]); keyboardChanged {
				props.Values[i] = next
				ratio = Slider_SliderDiscreteRatio(next, props.Min, props.Max)
				changed = true
			}
		}
		if next, active := r.sliderRatio(Slider_SliderFocusIdFor(props.ID, int32(i), true), focusID, cell, props.Disabled, vertical); active && props.Max > props.Min {
			ratio = next
			value := Slider_SliderDiscreteValue(props.Min, props.Max, ratio)
			changed = changed || value != props.Values[i]
			props.Values[i] = value
		}
		format := props.Format
		if format == "" {
			format = "%d"
		}
		focused := enabled && focusID > 0 && r.focusID == focusID && !r.popupFocusCaptures(focusID)
		r.drawSliderCell(cell, ratio, fmt.Sprintf(format, props.Values[i]), props.Disabled, vertical, focused, props.ClassName, props.ID, int32(i))
	}
	r.drawSliderLabel(props.Bounds, props.Label, props.ClassName, props.ID)
	return changed
}

func (r *runtime) sliderAngle(props sliderAngleProps) bool {
	if props.Value == nil {
		return false
	}
	degrees := *props.Value * 57.29577951308232
	format := props.Format
	if format == "" {
		format = "%.0f deg"
	}
	values := []float32{degrees}
	changed := r.sliderFloat(sliderFloatProps{Bounds: props.Bounds, ID: props.ID, Label: props.Label, Values: values, ValueCount: 1, Min: props.MinDegrees, Max: props.MaxDegrees, Format: format, Disabled: props.Disabled, ClassName: props.ClassName}, false)
	if changed {
		*props.Value = values[0] * 0.017453292519943295
	}
	return changed
}

func (r *runtime) Slider(props SliderProps) bool {
	count := props.ValueCount
	if count <= 0 {
		if props.Kind == NumericInt {
			count = int32(len(props.IntValues))
		} else {
			count = int32(len(props.FloatValues))
		}
	}
	if props.Angle {
		return r.sliderAngle(sliderAngleProps{
			Bounds:     props.Bounds,
			ID:         props.ID,
			Label:      props.Label,
			Value:      props.FloatValue,
			MinDegrees: float32(props.Min),
			MaxDegrees: float32(props.Max),
			Format:     props.Format,
			Disabled:   props.Disabled,
			ClassName:  props.ClassName,
		})
	}
	if props.Kind == NumericInt {
		slider := sliderIntProps{
			Bounds:     props.Bounds,
			ID:         props.ID,
			Label:      props.Label,
			Values:     props.IntValues,
			ValueCount: count,
			Min:        int32(props.Min),
			Max:        int32(props.Max),
			Format:     props.Format,
			Disabled:   props.Disabled,
			ClassName:  props.ClassName,
		}
		if props.Vertical {
			return r.sliderInt(slider, true)
		}
		return r.sliderInt(slider, false)
	}
	slider := sliderFloatProps{
		Bounds:     props.Bounds,
		ID:         props.ID,
		Label:      props.Label,
		Values:     props.FloatValues,
		ValueCount: count,
		Min:        float32(props.Min),
		Max:        float32(props.Max),
		Format:     props.Format,
		Disabled:   props.Disabled,
		ClassName:  props.ClassName,
	}
	if props.Vertical {
		return r.sliderFloat(slider, true)
	}
	return r.sliderFloat(slider, false)
}

func (r *runtime) numericInputState(key numericInputKey, formatted string) *numericInputState {
	if r.numericInputs == nil {
		r.numericInputs = make(map[numericInputKey]*numericInputState)
		r.numericNextToken = 0x60000000
	}
	state := r.numericInputs[key]
	if state == nil {
		if r.numericNextToken > 0x7fffffff-3 {
			panic("numeric input identity space exhausted")
		}
		state = &numericInputState{text: make([]byte, 64), token: r.numericNextToken}
		r.numericNextToken += 3
		r.numericInputs[key] = state
	}
	if !state.focused {
		clear(state.text)
		copy(state.text, formatted)
		state.cursor = int32(len(formatted))
	}
	return state
}

func (r *runtime) setNumericInputText(key numericInputKey, formatted string) {
	state := r.numericInputs[key]
	if state == nil {
		return
	}
	clear(state.text)
	copy(state.text, formatted)
	state.cursor = int32(len(formatted))
}

func (r *runtime) numericInputCell(bounds Rectangle, key numericInputKey, formatted string, disabled bool, stepEnabled bool) (string, int32, bool, bool) {
	state := r.numericInputState(key, formatted)
	token := state.token
	field := bounds
	minus, plus := bounds, bounds
	if stepEnabled {
		buttonWidth := float32(24)
		field.Width -= buttonWidth * 2
		minus = Rectangle{X: field.X + field.Width, Y: bounds.Y, Width: buttonWidth, Height: bounds.Height}
		plus = Rectangle{X: minus.X + buttonWidth, Y: bounds.Y, Width: buttonWidth, Height: bounds.Height}
	}
	textChanged := false
	if disabled {
		state.focused = false
	} else {
		var commit bool
		textChanged = r.editText(field, state.text, &state.cursor, &state.focused, &commit, token, textEditOptions{maxCodepoints: 63})
	}
	r.recordTextInput(FrameOpTextField, field, state.text, &state.cursor, &state.focused, token, Text14, false, false)
	if !stepEnabled {
		return string(state.text[:zeroIndex(state.text)]), 0, false, textChanged
	}
	minusPressed := r.buttonAt(ButtonProps{Bounds: minus, Label: "-", Size: ControlSizeSmall,
		ID: token + 1, Disabled: disabled})
	plusPressed := r.buttonAt(ButtonProps{Bounds: plus, Label: "+", Size: ControlSizeSmall,
		ID: token + 2, Disabled: disabled})
	if !minusPressed && !plusPressed {
		return string(state.text[:zeroIndex(state.text)]), 0, false, textChanged
	}
	direction := int32(1)
	if minusPressed {
		direction = -1
	}
	fast := r.keyDown[KeyLeftShift] || r.keyDown[KeyRightShift]
	return string(state.text[:zeroIndex(state.text)]), direction, fast, textChanged
}

func (r *runtime) inputFloat(props inputFloatProps) bool {
	props.Bounds = r.layoutRect(props.Bounds)
	count := int(props.ValueCount)
	if count <= 0 || count > len(props.Values) {
		count = len(props.Values)
	}
	changed := false
	for i := 0; i < count; i++ {
		format := props.Format
		if format == "" {
			format = "%.3f"
		}
		cell := Rectangle{X: props.Bounds.X + float32(i)*props.Bounds.Width/float32(count), Y: props.Bounds.Y, Width: props.Bounds.Width / float32(count), Height: props.Bounds.Height}
		key := numericInputKey{kind: 0, widgetID: props.ID, component: int32(i)}
		text, direction, fast, edited := r.numericInputCell(cell, key, fmt.Sprintf(format, props.Values[i]), props.Disabled, props.Step != 0)
		value := props.Values[i]
		valid := false
		if edited {
			if parsed, err := strconv.ParseFloat(text, 32); err == nil {
				value, valid = float32(parsed), true
			}
		}
		if direction != 0 {
			step := Input_InputContinuousStepValue(value, props.Step, props.StepFast, direction, fast)
			value, valid = step.Value, true
			r.setNumericInputText(key, fmt.Sprintf(format, value))
		}
		if valid && value != props.Values[i] {
			props.Values[i] = value
			changed = true
		}
	}
	r.drawSliderLabel(props.Bounds, props.Label, 0, props.ID)
	return changed
}

func (r *runtime) inputInt(props inputIntProps) bool {
	props.Bounds = r.layoutRect(props.Bounds)
	count := int(props.ValueCount)
	if count <= 0 || count > len(props.Values) {
		count = len(props.Values)
	}
	changed := false
	for i := 0; i < count; i++ {
		format := props.Format
		if format == "" {
			format = "%d"
		}
		cell := Rectangle{X: props.Bounds.X + float32(i)*props.Bounds.Width/float32(count), Y: props.Bounds.Y, Width: props.Bounds.Width / float32(count), Height: props.Bounds.Height}
		key := numericInputKey{kind: 1, widgetID: props.ID, component: int32(i)}
		text, direction, fast, edited := r.numericInputCell(cell, key, fmt.Sprintf(format, props.Values[i]), props.Disabled, props.Step != 0)
		value := props.Values[i]
		valid := false
		if edited {
			if parsed, err := strconv.ParseInt(text, 0, 32); err == nil {
				value, valid = int32(parsed), true
			}
		}
		if direction != 0 {
			step := Input_InputDiscreteStepValue(value, props.Step, props.StepFast, direction, fast)
			value, valid = step.Value, true
			r.setNumericInputText(key, fmt.Sprintf(format, value))
		}
		if valid && value != props.Values[i] {
			props.Values[i] = value
			changed = true
		}
	}
	r.drawSliderLabel(props.Bounds, props.Label, 0, props.ID)
	return changed
}

func (r *runtime) inputDouble(props inputDoubleProps) bool {
	props.Bounds = r.layoutRect(props.Bounds)
	count := int(props.ValueCount)
	if count <= 0 || count > len(props.Values) {
		count = len(props.Values)
	}
	changed := false
	for i := 0; i < count; i++ {
		format := props.Format
		if format == "" {
			format = "%.6f"
		}
		cell := Rectangle{X: props.Bounds.X + float32(i)*props.Bounds.Width/float32(count), Y: props.Bounds.Y, Width: props.Bounds.Width / float32(count), Height: props.Bounds.Height}
		key := numericInputKey{kind: 2, widgetID: props.ID, component: int32(i)}
		text, direction, fast, edited := r.numericInputCell(cell, key, fmt.Sprintf(format, props.Values[i]), props.Disabled, props.Step != 0)
		value := props.Values[i]
		valid := false
		if edited {
			if parsed, err := strconv.ParseFloat(text, 64); err == nil {
				value, valid = parsed, true
			}
		}
		if direction != 0 {
			step := Input_InputStepValue(value, props.Step, props.StepFast, direction, fast)
			value, valid = step.Value, true
			r.setNumericInputText(key, fmt.Sprintf(format, value))
		}
		if valid && value != props.Values[i] {
			props.Values[i] = value
			changed = true
		}
	}
	r.drawSliderLabel(props.Bounds, props.Label, 0, props.ID)
	return changed
}

func (r *runtime) Input(props InputProps) bool {
	count := props.ValueCount
	if count <= 0 {
		switch props.Kind {
		case NumericInt:
			count = int32(len(props.IntValues))
		case NumericDouble:
			count = int32(len(props.DoubleValues))
		default:
			count = int32(len(props.FloatValues))
		}
	}
	switch props.Kind {
	case NumericInt:
		return r.inputInt(inputIntProps{
			Bounds:     props.Bounds,
			ID:         props.ID,
			Label:      props.Label,
			Values:     props.IntValues,
			ValueCount: count,
			Step:       int32(props.Step),
			StepFast:   int32(props.StepFast),
			Format:     props.Format,
			Disabled:   props.Disabled,
		})
	case NumericDouble:
		return r.inputDouble(inputDoubleProps{
			Bounds:     props.Bounds,
			ID:         props.ID,
			Label:      props.Label,
			Values:     props.DoubleValues,
			ValueCount: count,
			Step:       props.Step,
			StepFast:   props.StepFast,
			Format:     props.Format,
			Disabled:   props.Disabled,
		})
	default:
		return r.inputFloat(inputFloatProps{
			Bounds:     props.Bounds,
			ID:         props.ID,
			Label:      props.Label,
			Values:     props.FloatValues,
			ValueCount: count,
			Step:       float32(props.Step),
			StepFast:   float32(props.StepFast),
			Format:     props.Format,
			Disabled:   props.Disabled,
		})
	}
}

func sliderSuffix(rest ...any) string {
	for _, arg := range rest {
		if s, ok := arg.(string); ok {
			return s
		}
	}
	return ""
}

func number32(v any) float32 {
	switch n := v.(type) {
	case int:
		return float32(n)
	case int32:
		return float32(n)
	case int64:
		return float32(n)
	case float32:
		return n
	case float64:
		return float32(n)
	default:
		return 0
	}
}
