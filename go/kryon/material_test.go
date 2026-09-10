package kryon

import (
	"bytes"
	"testing"
)

func TestSurfaceAndButtonShareMaterialRendering(t *testing.T) {
	for _, material := range []MaterialKind{MaterialFlat, MaterialLightfield} {
		for _, gradient := range []bool{false, true} {
			r := New(AppConfig{Width: 120, Height: 80}).(*runtime)
			r.BeginFrame()
			style := Style{Fields: StyleMaterial | StyleBackground | StyleBorder | StyleRadius | StyleBorderWidth | StyleOpacity,
				Material: material, Background: Color{18, 52, 86, 255}, Border: Color{80, 120, 200, 255},
				Radius: 8, BorderWidth: 1, Opacity: 0.75}
			if gradient {
				style.Fields |= StyleBackgroundEnd
				style.BackgroundEnd = Color{80, 20, 40, 128}
			}
			bounds := Rectangle{X: 20, Y: 20, Width: 80, Height: 40}
			r.Surface(bounds, style)
			surface := r.FrameOps()[0]
			button, _ := r.surfaceButtonFrame(ButtonProps{Bounds: bounds, ID: 78, State: ButtonStateNormal,
				Style: ControlStyle{Normal: style}}, Rectangle{}, false)
			if surface.Material != material {
				t.Fatal("surface discarded explicit material selection")
			}
			actual := RenderFrame(120, 80, []FrameOp{surface})
			want := RenderFrame(120, 80, []FrameOp{button})
			if !bytes.Equal(actual.Pix, want.Pix) {
				t.Fatalf("surface and button material differ: material=%d gradient=%v", material, gradient)
			}
			r.EndFrame()
		}
	}
	r := New(AppConfig{}).(*runtime)
	r.Surface(Rectangle{Width: 80, Height: 40}, Style{})
	if r.FrameOps()[0].Material != MaterialFlat {
		t.Fatal("plain surface stopped defaulting to flat material")
	}
}

func TestMaterialSelectionAndExplicitZero(t *testing.T) {
	r := New(AppConfig{Width: 100, Height: 60}).(*runtime)
	props := ButtonProps{Bounds: Rectangle{X: 10, Y: 10, Width: 80, Height: 40}, ID: 77,
		Style: ControlStyle{Normal: Style{
			Fields:   StyleMaterial | StyleBackground | StyleBorder | StyleFocus | StyleRadius,
			Material: MaterialFlat, Background: Color{18, 52, 86, 255},
		}}}
	for _, state := range []ButtonState{ButtonStateNormal, ButtonStateHover, ButtonStatePressed, ButtonStateFocus} {
		props.State = state
		frame, _ := r.surfaceButtonFrame(props, Rectangle{}, false)
		if MaterialKind(frame.Button.Material.Value.Material) != MaterialFlat {
			t.Fatal("resolved material did not reach the renderer")
		}
		img := RenderFrame(100, 60, []FrameOp{frame})
		for _, point := range [][2]int{{20, 15}, {50, 30}, {80, 45}} {
			pixel := img.RGBAAt(point[0], point[1])
			if pixel.R != 18 || pixel.G != 52 || pixel.B != 86 || pixel.A != 255 {
				t.Fatalf("state=%d: flat fill was shaded at %v: %v", state, point, pixel)
			}
		}
		blank := RenderFrame(100, 60, nil)
		if img.RGBAAt(50, 51) != blank.RGBAAt(50, 51) || img.RGBAAt(9, 30) != blank.RGBAAt(9, 30) {
			t.Fatal("flat material drew outside its bounds")
		}
	}
	props.Style.Hover = Style{Fields: StyleMaterial, Material: MaterialLightfield}
	props.State = ButtonStateHover
	frame, _ := r.surfaceButtonFrame(props, Rectangle{}, false)
	if MaterialKind(frame.Button.Material.Value.Material) != MaterialLightfield {
		t.Fatal("explicit zero-valued Lightfield did not replace inherited flat material")
	}
}

func TestFlatMaterialLayersPreserveAlphaAndFocus(t *testing.T) {
	if Surface_MaterialLayerCount(int32(MaterialFlat)) != 3 || Surface_MaterialLayerCount(int32(MaterialLightfield)) != 12 {
		t.Fatal("material layer counts do not match their definitions")
	}
	for _, alpha := range []uint32{0, 128, 255} {
		for index := int32(0); index < Surface_MaterialLayerCount(int32(MaterialLightfield)); index++ {
			layer := Surface_MaterialLayer(1, index, 80, 40, 8, 2,
				0x12345600|alpha, 0x789abc00|alpha, 0xffffffff, 0xff000000|alpha,
				1, 1, 1, false, 1, 0x092039ff)
			if layer.Blur != 0 || layer.InnerBlur != 0 || layer.X != 0 || layer.Y != 0 {
				t.Fatal("flat material retained depth effects")
			}
			if index < 3 && layer.Color&255 != alpha || index >= 3 && layer.Color != 0 {
				t.Fatalf("flat layer %d changed explicit alpha: %+v", index, layer)
			}
		}
	}
	if Surface_MaterialOffset(1, 1, 1, false) != 0 {
		t.Fatal("flat content acquired an elevation offset")
	}
}

func TestMaterialDrawingCoordinatesAndFill(t *testing.T) {
	paint := Material_PrepareMaterial(MaterialPaint{
		Bounds:  Rectangle{X: 50, Y: 20, Width: 80, Height: 40},
		Surface: Rectangle{X: 10, Y: 20, Width: 160, Height: 40}, Scale: 2,
		Value: StyleData{Material: int32(MaterialFlat), Radius: 4, Opacity: 1,
			Fields: uint32(StyleBackgroundEnd), Background: 0x12345600, BackgroundEnd: 0xabcdef80},
	})
	command := Material_PaintMaterialLayer(paint, 0)
	if !command.Visible || command.Bounds != paint.Surface || command.Area != paint.Surface ||
		command.Segment != paint.Bounds || command.Scale != 2 ||
		command.Layer.Width != 80 || command.Layer.Height != 20 || command.Layer.Radius != 4 {
		t.Fatalf("scaled joined surface: %+v", command)
	}
	if !command.Layer.Gradient || command.Layer.Color != 0x12345600 || command.Layer.EndColor != 0xabcdef80 {
		t.Fatalf("transparent gradient start was lost: %+v", command.Layer)
	}
	paint.FillValid = true
	if command = Material_PaintMaterialLayer(paint, 0); command.Visible || command.Layer.Gradient {
		t.Fatal("absent retained endpoints must preserve the material, not use the fallback gradient")
	}
	paint.Fill = FillStates{Normal: true, NormalStart: 0xff000080, NormalEnd: 0x00ff0040}
	if command = Material_PaintMaterialLayer(paint, 0); !command.Visible || !command.Layer.Gradient ||
		command.Layer.Color != 0xff000080 || command.Layer.EndColor != 0x00ff0040 {
		t.Fatalf("retained fill override: %+v", command.Layer)
	}
	paint.Surface, paint.Scale = Rectangle{Width: 30}, 0
	paint = Material_PrepareMaterial(paint)
	if paint.Surface != paint.Bounds || paint.Scale != 1 || Material_MaterialContentBounds(paint) != paint.Bounds {
		t.Fatalf("invalid surface and scale fallback: %+v", paint)
	}
}
