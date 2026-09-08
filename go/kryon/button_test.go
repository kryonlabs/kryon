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
