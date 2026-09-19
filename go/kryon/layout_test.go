package kryon

import (
	"math"
	"testing"
)

func TestFlexAlignment(t *testing.T) {
	starts := []float32{14, 69, 124, 14, 41.5, 50.666667}
	seconds := []float32{44, 99, 154, 154, 126.5, 117.333333}
	check := func(got, want Rectangle) {
		t.Helper()
		if math.Abs(float64(got.X-want.X)) > 0.001 ||
			math.Abs(float64(got.Y-want.Y)) > 0.001 ||
			math.Abs(float64(got.Width-want.Width)) > 0.001 ||
			math.Abs(float64(got.Height-want.Height)) > 0.001 {
			t.Fatalf("bounds = %+v, want %+v", got, want)
		}
	}
	for direction := FlexRow; direction <= FlexColumn; direction++ {
		for justify := JustifyStart; justify <= JustifySpaceEvenly; justify++ {
			for align := AlignStart; align <= AlignStretch; align++ {
				props := FlexProps{
					Bounds: NewRectangle(10, 20, 188, 88), Gap: 10, Padding: 4,
					Direction: FlexDirection(direction), JustifyContent: JustifyContent(justify),
					AlignItems: AlignItems(align),
				}
				if direction == FlexColumn {
					props.Bounds = NewRectangle(20, 10, 88, 188)
				}
				cross := float32(24)
				if align == AlignCenter {
					cross = 49
				} else if align == AlignEnd {
					cross = 74
				}
				cursor := Layout_BeginFlexCursor(props, 2, 60)
				if direction == FlexRow {
					cursor = Layout_FlexStep(cursor, 20, 30)
					check(cursor.Item, NewRectangle(starts[justify], cross, 20, 30))
					cursor = Layout_FlexStep(cursor, 40, 30)
					check(cursor.Item, NewRectangle(seconds[justify], cross, 40, 30))
				} else {
					cursor = Layout_FlexStep(cursor, 30, 20)
					check(cursor.Item, NewRectangle(cross, starts[justify], 30, 20))
					cursor = Layout_FlexStep(cursor, 30, 40)
					check(cursor.Item, NewRectangle(cross, seconds[justify], 30, 40))
				}
				check(Layout_FlexStep(cursor, 10, 10).Item, Rectangle{})
			}
		}
	}
	props := FlexProps{Bounds: NewRectangle(10, 20, 180, 80), AlignItems: AlignItems(AlignStretch)}
	check(Layout_FlexStep(Layout_BeginFlexCursor(props, 1, 20), 20, 0).Item,
		NewRectangle(10, 20, 20, 80))
	props.Direction = FlexDirection(FlexColumn)
	check(Layout_FlexStep(Layout_BeginFlexCursor(props, 1, 20), 0, 20).Item,
		NewRectangle(10, 20, 180, 20))
	for justify := JustifyStart; justify <= JustifySpaceEvenly; justify++ {
		props.Direction = FlexDirection(FlexRow)
		props.JustifyContent = JustifyContent(justify)
		props.AlignItems = AlignItems(AlignStart)
		x := float32(10)
		if justify == JustifyEnd {
			x = 170
		} else if justify == JustifyCenter || justify == JustifySpaceAround || justify == JustifySpaceEvenly {
			x = 90
		}
		check(Layout_FlexStep(Layout_BeginFlexCursor(props, 1, 20), 20, 30).Item,
			NewRectangle(x, 20, 20, 30))
		check(Layout_FlexStep(Layout_BeginFlexCursor(props, 0, 0), 20, 30).Item, Rectangle{})
		check(Layout_FlexStep(Layout_BeginFlexCursor(props, -1, 0), 20, 30).Item, Rectangle{})
		props.AlignItems = AlignItems(AlignCenter)
		check(Layout_FlexStep(Layout_BeginFlexCursor(props, 1, 200), 200, 100).Item,
			NewRectangle(10, 20, 200, 100))
	}
}
