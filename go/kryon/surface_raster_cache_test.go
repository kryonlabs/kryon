package kryon

import (
	"bytes"
	"image"
	"testing"
)

func TestSurfaceRasterMatchesSharedSampler(t *testing.T) {
	for _, material := range []MaterialKind{MaterialFlat, MaterialLightfield, MaterialGlass} {
		for _, opacity := range []float32{1, 0.6} {
			for _, scale := range []float32{1, 1.5} {
				bounds := NewRectangle(4.25, 6.5, 64, 36)
				paint := Material_PrepareMaterial(MaterialPaint{Bounds: bounds, Surface: bounds, Scale: scale,
					Value: StyleData{Fields: StyleBackground | StyleBackgroundEnd, Material: material,
						Background: 0x315578ff, BackgroundEnd: 0x82664390, Border: 0xb46655ff, Focus: 0xaabbccff,
						Radius: 8, BorderWidth: 1, Opacity: opacity}, Hover: 0.4, Focus: 0.7})
				for layer := int32(0); layer < Surface_MaterialLayerCount(material); layer++ {
					command := Material_PaintMaterialLayer(paint, layer)
					for _, clip := range []image.Rectangle{image.Rect(0, 0, 80, 55), image.Rect(13, 11, 54, 31)} {
						for _, background := range []Color{{23, 45, 67, 255}, {12, 34, 56, 90}} {
							want := image.NewRGBA(image.Rect(0, 0, 80, 55))
							fillImage(want, background)
							renderSurfaceDrawingUncached(want.SubImage(clip).(*image.RGBA), command)
							// Exercise cold and warm paths on different destination colors.
							for repeat := 0; repeat < 2; repeat++ {
								got := image.NewRGBA(want.Bounds())
								fillImage(got, background)
								renderSurfaceDrawing(got.SubImage(clip).(*image.RGBA), command)
								if !bytes.Equal(got.Pix, want.Pix) {
									t.Fatalf("raster differs: material=%d opacity=%g scale=%g layer=%d clip=%v", material, opacity, scale, layer, clip)
								}
							}
						}
					}
				}
			}
		}
	}
}

func TestSurfaceRasterCacheBounds(t *testing.T) {
	var cache surfaceRasterCache
	for i := 0; i < surfaceRasterCountLimit+20; i++ {
		key := surfaceRasterKey{pixels: image.Rect(i, 0, i+1, 1)}
		cache.put(key, &surfaceRaster{pixels: make([]byte, 300000)})
		if cache.bytes > surfaceRasterBudget || len(cache.entries) > surfaceRasterCountLimit {
			t.Fatal("surface cache exceeded its retention budget")
		}
	}
	if cache.get(surfaceRasterKey{pixels: image.Rect(0, 0, 1, 1)}) != nil {
		t.Fatal("old unused surface was not evicted")
	}
	large := surfaceRasterKey{pixels: image.Rect(0, 0, 4000, 4000)}
	cache.put(large, &surfaceRaster{pixels: make([]byte, surfaceRasterEntryLimit+1)})
	if cache.get(large) != nil {
		t.Fatal("oversized surface was retained")
	}
}

func TestSurfaceRasterSegmentAndColorChanges(t *testing.T) {
	bounds := NewRectangle(4.25, 6.5, 64, 36)
	paint := Material_PrepareMaterial(MaterialPaint{Bounds: bounds, Surface: bounds, Scale: 1,
		Value: StyleData{Fields: StyleBackground, Material: MaterialFlat,
			Background: 0x315578ff, Radius: 8, Opacity: 1}})
	command := Material_PaintMaterialLayer(paint, 0)
	for _, segment := range []Rectangle{bounds, NewRectangle(14, 6.5, 17, 36), NewRectangle(40, 6.5, 28, 36)} {
		command.Segment = segment
		for _, color := range []uint32{0x315578ff, 0x55332280} {
			command.Layer.Color = color
			want := image.NewRGBA(image.Rect(0, 0, 80, 55))
			got := image.NewRGBA(want.Bounds())
			fillImage(want, Color{23, 45, 67, 255})
			fillImage(got, Color{23, 45, 67, 255})
			renderSurfaceDrawingUncached(want, command)
			renderSurfaceDrawing(got, command)
			if !bytes.Equal(got.Pix, want.Pix) {
				t.Fatalf("stale surface raster for segment=%v color=%x", segment, color)
			}
		}
	}
}

func TestMaterialButtonsUseFlatFills(t *testing.T) {
	useMaterialStyleForTest(t)
	for _, tone := range []ButtonTone{ButtonToneNeutral, ButtonToneAccent, ButtonToneDanger} {
		for _, emphasis := range []ButtonEmphasis{ButtonEmphasisFilled, ButtonEmphasisOutline, ButtonEmphasisGhost} {
			facts := StyleSheet_StyleDefaultFacts(StyleSheet_StyleKindButton())
			facts.Tone, facts.Emphasis = int32(tone), int32(emphasis)
			for _, state := range []ButtonState{ButtonStateNormal, ButtonStateHover, ButtonStatePressed, ButtonStateFocus, ButtonStateDisabled} {
				style := ResolveActiveStyle(StyleData{}, facts, int32(state))
				if style.Background != style.BackgroundEnd || style.Material != MaterialFlat {
					t.Fatalf("non-flat Material button: tone=%d emphasis=%d state=%d", tone, emphasis, state)
				}
			}
		}
	}
}
