package kryon

import "testing"

func TestSliderHeaderLayoutAndFormatting(t *testing.T) {
	useMaterialStyleForTest(t)
	r := New(AppConfig{Width: 480, Height: 640}).(*runtime)
	values := []int32{10}
	height := float32(0)
	props := SliderProps{Bounds: Rectangle{X: 24, Y: 80, Width: 360}, ID: 810,
		Label: "Scale factor", Kind: NumericInt, IntValues: values, ValueCount: 1,
		Min: 5, Max: 25, ValueScale: 0.1, ValueFormat: "%.1f×", StepButtons: true,
		ShowLimits: true, MeasuredHeight: &height}
	r.BeginFrame()
	r.Slider(props)
	r.EndFrame()
	var label, value, track Rectangle
	found := map[string]bool{}
	for _, op := range r.FrameOps() {
		if op.ID != props.ID {
			continue
		}
		if op.Kind == FrameOpText {
			found[op.Text] = true
			if op.Text == props.Label {
				label = op.Bounds
			}
			if op.Text == "1.0×" {
				value = op.Bounds
			}
		}
		if op.Kind == FrameOpRect && !op.Selected {
			track = op.Bounds
		}
	}
	if !found["1.0×"] || !found["0.5×"] || !found["2.5×"] || found["10"] {
		t.Fatalf("formatted slider texts: %v", found)
	}
	if label.X != props.Bounds.X || label.Y != props.Bounds.Y || value.Y != label.Y {
		t.Fatalf("header misaligned: label=%+v value=%+v", label, value)
	}
	if label.Y+label.Height >= track.Y || value.Y+value.Height >= track.Y || height < 90 {
		t.Fatalf("header overlaps track: label=%+v value=%+v track=%+v height=%v", label, value, track, height)
	}
	// A header click must not modify the slider's value.
	r.QueueMouseButtonDown(MouseButtonLeft, 200, 84)
	r.BeginFrame()
	r.Slider(props)
	r.EndFrame()
	if values[0] != 10 {
		t.Fatalf("header click changed value: %d", values[0])
	}
	r.QueueMouseButtonUp(MouseButtonLeft, 200, 84)
	r.BeginFrame()
	r.Slider(props)
	r.EndFrame()
	// Pointer mapping uses the painted track endpoints.
	r.QueueMouseButtonDown(MouseButtonLeft, track.X+track.Width-0.5, track.Y+track.Height/2)
	r.BeginFrame()
	r.Slider(props)
	r.EndFrame()
	if values[0] != 25 {
		t.Fatalf("track maximum = %d", values[0])
	}
	r.QueueMouseButtonUp(MouseButtonLeft, track.X+track.Width-0.5, track.Y)
	r.BeginFrame()
	r.Slider(props)
	r.EndFrame()
	props.Disabled = true
	r.QueueMouseButtonDown(MouseButtonLeft, track.X, track.Y)
	r.BeginFrame()
	r.Slider(props)
	r.EndFrame()
	if values[0] != 25 {
		t.Fatalf("disabled slider changed: %d", values[0])
	}
}

func TestSliderLongLabelMeasuredHeight(t *testing.T) {
	useMaterialStyleForTest(t)
	r := New(AppConfig{Width: 300, Height: 600}).(*runtime)
	values := []int32{10}
	height := float32(0)
	props := SliderProps{Bounds: Rectangle{X: 24, Y: 30, Width: 140}, ID: 811,
		Label: "A long localized label that needs several lines", Kind: NumericInt,
		IntValues: values, Min: 5, Max: 25, StepButtons: true, MeasuredHeight: &height}
	r.BeginFrame()
	r.Slider(props)
	r.EndFrame()
	if height < 140 {
		t.Fatalf("long label did not grow: %v", height)
	}
	previous := height
	props.Bounds.Height = height
	r.BeginFrame()
	r.Slider(props)
	r.EndFrame()
	if height != previous {
		t.Fatalf("re-measure grew again: %v -> %v", previous, height)
	}
}
