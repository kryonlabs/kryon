package kryon

import "testing"

func TestCanvasNestedTransformAndClipRestoration(t *testing.T) {
	r := New(AppConfig{Width: 240, Height: 180}).(*runtime)
	r.BeginFrame()
	zoom, offset := float32(2), int32(10)
	outer := Canvas{Bounds: NewRectangle(20, 20, 120, 100), ScrollX: &offset, Zoom: &zoom}
	child := Canvas{Bounds: NewRectangle(30, 30, 60, 60)}
	r.CanvasScope(outer)
	r.CanvasScope(child)
	r.CanvasEndScope(child)
	red := Color{255, 0, 0, 255}
	r.record(FrameOp{Kind: FrameOpRect, Bounds: NewRectangle(35, 35, 10, 10), Color: red})
	op := r.ops[len(r.ops)-1]
	if op.Bounds != NewRectangle(30, 50, 20, 20) || !op.HasClip || op.Clip != outer.Bounds {
		t.Fatalf("parent transform or clip lost after plain child: %+v", op)
	}
	childZoom := float32(3)
	child.Zoom = &childZoom
	r.CanvasScope(child)
	r.record(FrameOp{Kind: FrameOpRect, Bounds: NewRectangle(35, 35, 10, 10), Color: red})
	if r.ops[len(r.ops)-1].Bounds != NewRectangle(45, 45, 30, 30) {
		t.Fatal("explicit child camera did not replace the parent")
	}
	r.CanvasEndScope(child)
	r.record(FrameOp{Kind: FrameOpRect, Bounds: NewRectangle(35, 35, 10, 10), Color: red})
	if r.ops[len(r.ops)-1].Bounds != op.Bounds {
		t.Fatal("parent transform lost after transformed child")
	}
	r.CanvasEndScope(outer)
	r.record(FrameOp{Kind: FrameOpRect, Bounds: NewRectangle(170, 140, 10, 10), Color: red})
	last := r.ops[len(r.ops)-1]
	if last.HasClip || last.Bounds != NewRectangle(170, 140, 10, 10) {
		t.Fatal("Canvas scope leaked into following content")
	}
	img := RenderFrame(240, 180, r.ops)
	if got := img.RGBAAt(175, 145); got.R != 255 || got.G != 0 {
		t.Fatal("following content was clipped")
	}
	r.EndFrame()
}

func TestCanvasClipsTextAndImageOperations(t *testing.T) {
	r := New(AppConfig{Width: 200, Height: 160}).(*runtime)
	r.BeginFrame()
	canvas := Canvas{Bounds: NewRectangle(20, 20, 40, 40)}
	r.CanvasScope(canvas)
	for _, kind := range []FrameOpKind{FrameOpText, FrameOpImage} {
		r.record(FrameOp{Kind: kind, Bounds: NewRectangle(0, 0, 100, 100), Text: "clipped", FontSize: 18, Color: RED})
		op := r.ops[len(r.ops)-1]
		if !op.HasClip || op.Clip != canvas.Bounds {
			t.Fatalf("%s escaped its canvas clip", kind)
		}
	}
	r.CanvasEndScope(canvas)
	r.EndFrame()
}
