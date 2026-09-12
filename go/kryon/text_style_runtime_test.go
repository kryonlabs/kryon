package kryon

import "testing"

func TestTextUsesActiveKSS(t *testing.T) {
	ClearStylePacks()
	defer ClearStylePacks()
	if !RegisterStylePackSource(`
@pack app.text;
Text {
  foreground: #123456;
  font-size: 21;
}
`, "Text", "") {
		t.Fatal("style pack did not register")
	}

	r := New(AppConfig{Width: 320, Height: 200}).(*runtime)
	r.BeginFrame()
	r.Text(TextProps{Text: "styled", Wrap: TextWrapNone})
	r.Text(TextProps{Text: "explicit", Color: Color{0xaa, 0xbb, 0xcc, 0xff}, Wrap: TextWrapNone})
	r.EndFrame()

	var styled, explicit *FrameOp
	for i := range r.ops {
		if r.ops[i].Kind != FrameOpText {
			continue
		}
		switch r.ops[i].Text {
		case "styled":
			styled = &r.ops[i]
		case "explicit":
			explicit = &r.ops[i]
		}
	}
	if styled == nil || styled.Color != (Color{0x12, 0x34, 0x56, 0xff}) ||
		styled.FontSize != 21 {
		t.Fatalf("text did not use KSS: %#v", styled)
	}
	if explicit == nil || explicit.Color != (Color{0xaa, 0xbb, 0xcc, 0xff}) {
		t.Fatalf("explicit text color did not win during migration: %#v", explicit)
	}
}
