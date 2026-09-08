package kryon

import (
	"reflect"
	"testing"
)

func TestButtonLabelAndTextChildUseSameTextPath(t *testing.T) {
	rt := New(AppConfig{Width: 320, Height: 160}).(*runtime)
	props := ButtonProps{
		Bounds: Rectangle{X: 20, Y: 20, Width: 120, Height: 40},
		Label:  "Save", ID: 991,
	}

	rt.BeginButton(props)
	rt.End()
	shorthand := append([]FrameOp(nil), rt.ops...)
	rt.ops = nil
	props.Label = ""
	rt.BeginButton(props)
	rt.Text(TextProps{Text: "Save", Wrap: TextWrapNone})
	rt.End()
	composed := append([]FrameOp(nil), rt.ops...)

	if !reflect.DeepEqual(shorthand, composed) {
		t.Fatalf("label shorthand and Text child differ:\nshort: %#v\nchild: %#v",
			shorthand, composed)
	}
}

func TestGeneratedButtonGeometryAndStatePolicy(t *testing.T) {
	if got := Button_ShapeWidth(120, 48, 96, 300, false, false, true); got != 48 {
		t.Fatalf("circle width = %v, want 48", got)
	}
	if got := Button_ShapeRadius(0.16, 0.5, false, true); got != 0.5 {
		t.Fatalf("circle radius = %v, want 0.5", got)
	}
	if got := Button_ResolveState(int32(ButtonStateAuto), false, true,
		true, true, true, true); got != int32(ButtonStateLoading) {
		t.Fatalf("loading precedence = %v, want %v", got, ButtonStateLoading)
	}
	if got := Button_ResolveState(int32(ButtonStateAuto), true, false,
		true, true, true, true); got != int32(ButtonStateDisabled) {
		t.Fatalf("disabled precedence = %v, want %v", got, ButtonStateDisabled)
	}
	if got := Button_TransitionStep(40, 80); got != 0.5 {
		t.Fatalf("transition step = %v, want 0.5", got)
	}
}

func TestButtonControlStyleLayersTransparentAndStateValues(t *testing.T) {
	r := New(AppConfig{Width: 240, Height: 120}).(*runtime)
	transparent := Color{}
	hoverText := Color{R: 103, G: 232, B: 249, A: 255}
	r.Button(ButtonProps{
		Bounds: Rectangle{X: 10, Y: 10, Width: 140, Height: 40},
		Label:  "Custom", State: ButtonStateHover,
		Style: ControlStyle{
			Normal: Style{Fields: StyleBackground | StyleRadius,
				Background: transparent, Radius: 0},
			Hover: Style{Fields: StyleForeground | StyleContentOffset,
				Foreground: hoverText, ContentOffset: Vector2{X: 2, Y: 1}},
		},
	})
	op := r.ops[len(r.ops)-1]
	if op.Color != transparent || op.TextColor != hoverText {
		t.Fatalf("custom colors = %#v/%#v", op.Color, op.TextColor)
	}
	if op.Radius != 0 || op.ContentOffset != (Vector2{X: 2, Y: 1}) {
		t.Fatalf("custom geometry = radius %v offset %#v", op.Radius, op.ContentOffset)
	}
}

func TestGeneratedButtonThemePolicy(t *testing.T) {
	if ButtonStateHover != 2 || StatePolicyStateHover != 2 ||
		ButtonToneAccent != 1 || TonePolicyToneAccent != 1 ||
		ButtonEmphasisFilled != 0 || EmphasisPolicyEmphasisFilled != 0 {
		t.Fatalf("public/generated enum values diverged: state=%d/%d tone=%d/%d emphasis=%d/%d",
			ButtonStateHover, StatePolicyStateHover, ButtonToneAccent, TonePolicyToneAccent,
			ButtonEmphasisFilled, EmphasisPolicyEmphasisFilled)
	}
	surface := Color{16, 24, 40, 255}
	accent := Color{37, 99, 235, 255}
	hover := Color{59, 130, 246, 255}
	pressed := Color{29, 78, 216, 255}
	neutral := Color{51, 65, 85, 255}
	danger := Color{220, 38, 38, 255}
	success := Color{5, 150, 105, 255}
	warning := Color{217, 119, 6, 255}

	got := unpackRGBA(Button_ButtonBackground(
		int32(ButtonToneAccent), int32(ButtonEmphasisFilled), int32(ButtonStateHover),
		packRGBA(surface), packRGBA(accent), packRGBA(hover), packRGBA(pressed),
		packRGBA(neutral), packRGBA(danger), packRGBA(success), packRGBA(warning)))
	if got != hover {
		t.Fatalf("accent hover = %#v, want %#v", got, hover)
	}

	got = unpackRGBA(Button_ButtonBackground(
		int32(ButtonToneDanger), int32(ButtonEmphasisOutline), int32(ButtonStateNormal),
		packRGBA(surface), packRGBA(accent), packRGBA(hover), packRGBA(pressed),
		packRGBA(neutral), packRGBA(danger), packRGBA(success), packRGBA(warning)))
	if got != surface {
		t.Fatalf("outlined danger background = %#v, want surface %#v", got, surface)
	}

	border := unpackRGBA(Button_ButtonBorder(
		int32(ButtonToneNeutral), int32(ButtonEmphasisGhost), int32(ButtonStateNormal),
		packRGBA(surface), packRGBA(accent), packRGBA(neutral), packRGBA(danger),
		packRGBA(success), packRGBA(warning)))
	if border != (Color{}) {
		t.Fatalf("ghost border = %#v, want transparent", border)
	}
}
