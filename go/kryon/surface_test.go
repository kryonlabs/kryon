package kryon

import (
	"image"
	"image/color"
	"math"
	"testing"
)

func TestBroadSaturatedContactLight(t *testing.T) {
	for _, width := range []float32{72, 160, 400, 720} {
		for _, chroma := range []uint32{0, 128, 160, 192, 255} {
			for _, press := range []float32{0, 0.5, 1} {
				for _, ambient := range []uint32{0x00172dff, 0xffffffff} {
					light := chroma<<8 | 255
					layer := Surface_LightfieldLayer(0, width, 40, 8, 1,
						0x006cffff, 0x006cffff, light, light,
						0, press, 0, false, 0.5, ambient)
					broad := Surface_Unit((width/40 - 4) / 12)
					strength := (0.025 + 0.025*broad) * (1 - press*0.8)
					if ambient == 0x00172dff {
						strength += 0.25 * broad * Surface_Unit((float32(chroma)-128)/64) * (1 - press)
					} else {
						strength *= 0.6
					}
					want := Surface_Opacity(light, strength*0.5)
					if layer.Color != want {
						t.Fatalf("width=%g chroma=%d press=%g ambient=%08x: got=%08x want=%08x",
							width, chroma, press, ambient, layer.Color, want)
					}
				}
			}
		}
	}
}

func TestDarkHoverBevelKeepsLowerEdgeAndClipsItsCrown(t *testing.T) {
	for _, hover := range []float32{0, 0.25, 0.5, 0.75, 1} {
		for _, press := range []float32{0, 0.25, 0.5, 0.75, 1} {
			for _, alpha := range []uint32{0, 128, 255} {
				layer := Surface_LightfieldLayer(5, 72, 38, 8, 1,
					0x006cff00|alpha, 0x006cffff, 0x006cffff, 0x409cffff,
					hover, press, 0, false, 1, 0x00172dff)
				motion := hover * (1 - press)
				faceY := Surface_FaceOffset(hover, press, false)
				if layer.Y != faceY+1-motion || layer.Height != 38-(2-motion) || layer.Y+layer.Height != faceY+37 {
					t.Fatalf("hover=%g press=%g: bevel moved its lower edge: %+v", hover, press, layer)
				}
				light := Surface_LiftColor(0x006cffff, motion)
				strength := (float32(0.20) + 0.28*0.375 + 0.50*hover + 0.25*hover) * (1 - press)
				want := Surface_Opacity(Surface_GradientColor(light, 0xffffffff, 0.72), strength)
				want = Surface_Opacity(want, float32(alpha)/255)
				if layer.EndColor != want {
					t.Fatalf("hover=%g press=%g alpha=%d: reflection=%08x want=%08x", hover, press, alpha, layer.EndColor, want)
				}
				if Surface_SampleCoverage(layer, -1, 10, 1) != 0 || Surface_SampleCoverage(layer, 30, -1, 1) != 0 {
					t.Fatal("hover bevel escaped its rounded bounds")
				}
			}
		}
	}
}

func TestTallBevelReflectionRespectsLightBrightness(t *testing.T) {
	for _, test := range []struct {
		light uint32
		gain  float32
	}{{0xff0000ff, 0.33}, {0x0000ffff, 0.33}, {0x00ff00ff, 0}, {0x808080ff, 0}} {
		for _, height := range []float32{32, 40, 44, 48, 64} {
			for _, press := range []float32{0, 0.25, 0.5, 0.75, 1} {
				for _, alpha := range []uint32{0, 128, 255} {
					layer := Surface_LightfieldLayer(5, 72, height, 8, 1,
						0x006cff00|alpha, 0x006cffff, test.light, test.light,
						0, press, 0, false, 1, 0x00172dff)
					raised := Surface_Unit((height - 40) / 8)
					raised *= raised
					strength := (0.20 + 0.28*Surface_Unit((height-32)/16) + (0.17+test.gain)*raised) * (1 - press)
					want := Surface_Opacity(Surface_GradientColor(test.light, 0xffffffff, 0.72), strength)
					want = Surface_Opacity(want, float32(alpha)/255)
					if layer.EndColor != want {
						t.Fatalf("light=%08x height=%g press=%g alpha=%d: got=%08x want=%08x", test.light, height, press, alpha, layer.EndColor, want)
					}
				}
			}
		}
	}
}

func TestBroadInnerReflectionKeepsItsClipAndOpacity(t *testing.T) {
	for _, alpha := range []uint32{0, 128, 255} {
		for _, press := range []float32{0, 0.5, 1} {
			layer := Surface_LightfieldLayer(8, 720, 40, 8, 1,
				0x006cff00|alpha, 0x006cffff, 0x006cffff, 0x409cffff,
				0, press, 0, false, 1, 0x00172dff)
			lift := 1 - press
			light := Surface_LiftColor(Surface_LiftColor(0x006cffff, lift), lift)
			whitening := float32(0.22) * (1 - 0.5*lift)
			strength := float32(0.38) * (1 - press*0.85) * (float32(alpha) / 255)
			want := Surface_Opacity(Surface_GradientColor(light, 0xffffffff, whitening), strength)
			if layer.EndColor != want || layer.Color&255 != 0 || layer.Blur != 0 || layer.InnerBlur <= 0 {
				t.Fatalf("alpha=%d press=%g: unexpected inward reflection %+v, want end=%08x", alpha, press, layer, want)
			}
			for _, x := range []float32{-1, layer.Width + 1} {
				if Surface_SampleCoverage(layer, x, 10, 1) != 0 {
					t.Fatal("inner reflection escaped the face")
				}
			}
		}
	}
}

func TestLiftColorPreservesPeakNeutralAndOpacity(t *testing.T) {
	for _, alpha := range []uint32{0, 128, 255} {
		for _, test := range []struct {
			amount float32
			green  uint32
		}{{-1, 108}, {0, 108}, {0.5, 139}, {1, 170}, {2, 170}} {
			got := Surface_LiftColor(0x006cff00|alpha, test.amount)
			want := test.green<<16 | 0x0000ff00 | alpha
			if got != want {
				t.Fatalf("alpha=%d amount=%g: got=%08x want=%08x", alpha, test.amount, got, want)
			}
			for _, neutral := range []uint32{alpha, 0x80808000 | alpha, 0xffffff00 | alpha} {
				if Surface_LiftColor(neutral, test.amount) != neutral {
					t.Fatal("color lift changed a neutral color or its opacity")
				}
			}
		}
	}
	previous := uint32(255)
	for _, press := range []float32{0, 0.25, 0.5, 0.75, 1} {
		layer := Surface_LightfieldLayer(5, 720, 34, 8, 1,
			0x006cffff, 0x006cffff, 0x006cffff, 0x409cffff,
			0, press, 0, false, 1, 0x00172dff)
		if layer.EndColor&255 > previous {
			t.Fatal("wide rim must fade as pressure increases")
		}
		previous = layer.EndColor & 255
	}
	if previous != 0 {
		t.Fatal("pressed face retained its raised rim")
	}
}

func TestPaleFocusFaceBalancesWithoutChangingAlpha(t *testing.T) {
	for _, alpha := range []uint32{0, 128, 255} {
		for _, hover := range []float32{0, 0.5, 1} {
			for _, press := range []float32{0, 0.5, 1} {
				for _, focus := range []float32{0, 0.5, 1} {
					face := func(amount float32) SurfaceLayer {
						return Surface_LightfieldLayer(3, 72, 40, 8, 1,
							0xeef5ff00|alpha, 0x064cffff, 0x064cffff, 0x064cff80,
							hover, press, amount, false, 1, 0xffffffff)
					}
					base := face(0)
					actual := face(focus)
					middle := Surface_GradientColor(base.Color, base.EndColor, 0.5)
					balance := float32(0.75) * focus * (1 - hover) * (1 - press)
					top := Surface_GradientColor(base.Color, middle, balance)
					bottom := Surface_GradientColor(base.EndColor, middle, balance)
					if actual.Color != top || actual.EndColor != bottom || !actual.Gradient {
						t.Fatalf("alpha=%d hover=%g press=%g focus=%g: face=%+v want=%08x -> %08x",
							alpha, hover, press, focus, actual, top, bottom)
					}
					if actual.Color&255 != alpha || actual.EndColor&255 != alpha {
						t.Fatal("focus changed face opacity")
					}
				}
			}
		}
	}
}

func TestPaleHoverBorderKeepsAlphaAndPressEndpoint(t *testing.T) {
	for _, alpha := range []uint32{0, 128, 255} {
		for _, hover := range []float32{0, 0.5, 1} {
			for _, press := range []float32{0, 0.5, 1} {
				for _, focus := range []float32{0, 0.5, 1} {
					border := uint32(0x064cff00) | alpha
					layer := Surface_LightfieldLayer(4, 72, 40, 8, 1,
						0xeef5ffff, border, border, 0x064cff80,
						hover, press, focus, false, 0.5, 0xffffffff)
					amount := hover * (1 - press)
					focusOpacity := 1 - 0.5*focus*128/255
					top := Surface_GradientColor(border, 0xeef5ff00|alpha, 0.65*amount)
					bottom := Surface_GradientColor(border, 0xffffff00|alpha, 0.30*amount)
					top = Surface_Opacity(Surface_Opacity(top, focusOpacity), 0.5)
					bottom = Surface_Opacity(Surface_Opacity(bottom, focusOpacity), 0.5)
					if layer.Color != top || layer.EndColor != bottom || !layer.Gradient {
						t.Fatalf("alpha=%d hover=%g press=%g focus=%g: border=%+v want=%08x -> %08x",
							alpha, hover, press, focus, layer, top, bottom)
					}
					if layer.Color&255 != layer.EndColor&255 {
						t.Fatal("hover changed border endpoint opacity")
					}
				}
			}
		}
	}
}

func TestPaleBorderlessContactShadow(t *testing.T) {
	for _, alpha := range []uint32{0, 128, 255} {
		for _, hover := range []float32{0, 0.5, 1} {
			for _, press := range []float32{0, 0.5, 1} {
				for _, border := range []uint32{0, 0x064cff80, 0x064cffff} {
					for _, disabled := range []bool{false, true} {
						layer := Surface_LightfieldLayer(1, 72, 40, 8, 1,
							0xf8fbff00|alpha, border, 0x064cffff, 0x064cffff,
							hover, press, 0, disabled, 0.5, 0xffffffff)
						strength := 24 + 10*hover - 20*hover*(1-press) - 18*press
						if border&255 == 0 {
							strength *= 0.5 + 0.5*press
						}
						if disabled {
							strength = 8
						}
						want := Surface_Opacity(Surface_Opacity(uint32(strength), float32(alpha)/255), 0.5)
						if layer.Color != want {
							t.Fatalf("alpha=%d hover=%g press=%g border=%08x disabled=%t: shadow=%08x want=%08x",
								alpha, hover, press, border, disabled, layer.Color, want)
						}
					}
				}
			}
		}
	}
}

func TestHoverShadowSofteningRequiresPaleNeutralMaterial(t *testing.T) {
	for _, test := range []struct {
		background uint32
		pale       float32
	}{
		{0xffffffff, 1}, {0xe0e0e0ff, 1}, {0xc0c0c0ff, 0}, {0xd0d0d0ff, 0.5},
		{0xffe7ffff, 0.5}, {0xffdfffff, 0}, {0x006cffff, 0}, {0x101828ff, 0},
	} {
		for _, press := range []float32{0, 0.5, 1} {
			for _, focus := range []float32{0, 0.5, 1} {
				layer := Surface_LightfieldLayer(1, 72, 40, 8, 1,
					test.background, 0x064cffff, 0x064cffff, 0x064cffff,
					1, press, focus, false, 1, 0xffffffff)
				want := uint32((34 - 20*(1-press)*test.pale - 18*press) * (1 - 0.5*focus))
				if layer.Color != want {
					t.Fatalf("background=%08x press=%g focus=%g: shadow=%08x want=%08x", test.background, press, focus, layer.Color, want)
				}
			}
		}
	}
}

func TestFocusSoftensPaleContactShadow(t *testing.T) {
	for _, ambient := range []uint32{0xffffffff, 0x00172dff} {
		for _, disabled := range []bool{false, true} {
			for step := 0; step <= 4; step++ {
				focus := float32(step) / 4
				layer := Surface_LightfieldLayer(1, 72, 40, 8, 1,
					0xeef5ffff, 0x064cffff, 0x064cffff, 0x064cffff,
					0, 0, focus, disabled, 1, ambient)
				strength := float32(32)
				if ambient == 0xffffffff {
					strength = 24 * (1 - 0.5*focus)
				}
				if disabled {
					strength = 8
				}
				if layer.Color != uint32(strength) {
					t.Fatalf("ambient=%08x disabled=%t focus=%g: shadow=%08x want=%d",
						ambient, disabled, focus, layer.Color, uint32(strength))
				}
			}
		}
	}
}

func TestNeutralBorderlessHoverLightStaysInside(t *testing.T) {
	for _, alpha := range []uint32{0, 128, 255} {
		for _, hover := range []float32{0, 0.5, 1} {
			for _, press := range []float32{0, 0.5, 1} {
				background := uint32(0x0a223b00) | alpha
				light := uint32(0x409cff00) | alpha
				layer := Surface_LightfieldLayer(8, 72, 40, 8, 1,
					background, 0, light, light, hover, press, 0, false, 1, 0x092039ff)
				boost := hover * (1 - press) * (15.0 / 16.0)
				strength := (float32(0.38) + 0.32*hover) * (1 - press*0.85)
				strength *= 0.15 + 0.45*boost
				strength *= float32(alpha) / 255
				reflected := Surface_GradientColor(light, Surface_ChromaColor(light, 255), boost)
				want := Surface_Opacity(Surface_GradientColor(reflected, 0xffffff00|alpha, 0.22*(1-boost)), strength)
				if layer.EndColor != want || layer.Blur != 0 || layer.InnerBlur <= 0 || !layer.Gradient {
					t.Fatalf("alpha %d hover %g press %g: got %+v, want endpoint %#x", alpha, hover, press, layer, want)
				}
				if Surface_InnerBlurCoverage(-1, 10, layer.Width, layer.Height, layer.Radius, layer.InnerBlur) != 0 {
					t.Fatal("borderless hover reflection escaped the face")
				}
			}
		}
	}
}

func TestDisabledBlueMaterialRetainsDepthWithoutInteraction(t *testing.T) {
	for _, alpha := range []uint32{0, 128, 255} {
		background := uint32(0x072f6100) | alpha
		layer := func(hover, press, focus float32) SurfaceLayer {
			return Surface_LightfieldLayer(3, 72, 38, 8, 1,
				background, 0x064292ff, 0x064292ff, 0x409cffff,
				hover, press, focus, true, 1, 0x092039ff)
		}
		rest := layer(0, 0, 0)
		wantTop := Surface_GradientColor(background, alpha, 0.15)
		wantBottom := Surface_GradientColor(background, 0x064292ff, 0.30)&0xffffff00 | alpha
		if rest.Color != wantTop || rest.EndColor != wantBottom || !rest.Gradient {
			t.Fatalf("disabled alpha=%d: face=%+v want=%08x -> %08x", alpha, rest, wantTop, wantBottom)
		}
		if rest != layer(1, 1, 1) || rest != layer(0.5, 0.5, 0.5) {
			t.Fatal("disabled material reacted to hover, press, or focus")
		}
		bloom := Surface_LightfieldLayer(0, 72, 38, 8, 1,
			background, 0x064292ff, 0x064292ff, 0x409cffff,
			1, 1, 1, true, 1, 0x092039ff)
		if bloom.Color&255 != 0 {
			t.Fatal("disabled depth restored an active outer bloom")
		}
	}
}

func TestBorderlessFocusKeepsItsMaterialBody(t *testing.T) {
	for _, alpha := range []uint32{0, 128, 255} {
		background := uint32(0x09203900) | alpha
		face := func(hover, press, focus float32) SurfaceLayer {
			return Surface_LightfieldLayer(3, 72, 40, 8, 1,
				background, 0, 0, 0x409cffff, hover, press, focus, false, 1, 0x092039ff)
		}
		normal, focused := face(0, 0, 0), face(0, 0, 1)
		if focused.EndColor != normal.EndColor || focused.EndColor != Surface_DepthColor(background, 0.48) ||
			focused.Color&255 != alpha || focused.EndColor&255 != alpha {
			t.Fatal("borderless focus must not add body absorption or change its alpha")
		}
		for _, interaction := range [][2]float32{{1, 0}, {0, 1}} {
			plain, focused := face(interaction[0], interaction[1], 0), face(interaction[0], interaction[1], 1)
			if plain.Color != focused.Color || plain.EndColor != focused.EndColor {
				t.Fatal("hover and press must retain their own face response under focus")
			}
		}
	}
}

func TestDarkFocusFaceRetainsItsColorAndAlpha(t *testing.T) {
	for _, alpha := range []uint32{0, 128, 255} {
		background := uint32(0x0055d400) | alpha
		light := uint32(0x006cffff)
		faceLight := Surface_GradientColor(light, 0xffffffff, 0.06)&0xffffff00 | alpha
		for _, focus := range []float32{0, 0.25, 0.5, 0.75, 1} {
			layer := Surface_LightfieldLayer(3, 72, 38, 8, 1, background,
				light, light, 0x409cffff, 0, 0, focus, false, 1, 0x001a30ff)
			absorbed := Surface_DepthColor(background, 0.30*focus)
			wantTop := Surface_GradientColor(absorbed, faceLight, 0.38*(1-0.70*focus))
			wantBottom := Surface_DepthColor(absorbed, 0.48)
			if layer.Color != wantTop || layer.EndColor != wantBottom ||
				layer.Color&255 != alpha || layer.EndColor&255 != alpha {
				t.Fatalf("focus %v alpha %d: face %+v, want %#x -> %#x", focus, alpha, layer, wantTop, wantBottom)
			}
		}
	}
}

func TestMotionExpiresInItsOwnersFrames(t *testing.T) {
	for _, age := range []int64{-1, 0, 1, 12, 13, 1000} {
		if Instance_InstanceExpired(age) != (age < 0 || age > 12) {
			t.Fatalf("wrong motion lifetime at age %d", age)
		}
	}
}

func TestLightFocusUsesOneEdgeAndPreservesCustomAlpha(t *testing.T) {
	for _, alpha := range []uint32{0, 128, 255} {
		focus := uint32(0x8844cc00) | alpha
		for _, amount := range []float32{0, 0.25, 0.5, 1} {
			layer := func(index int32, border uint32) SurfaceLayer {
				return Surface_LightfieldLayer(index, 72, 40, 8, 1, 0xeef5ffff,
					border, border, focus, 0, 0, amount, false, 1, 0xffffffff)
			}
			if layer(6, 0x12882280).Color&255 != 0 {
				t.Fatal("light focus must not stack a second outline")
			}
			if layer(7, 0x12882280).Color != Surface_Opacity(focus, amount) {
				t.Fatal("explicit focus color must retain its alpha")
			}
			for _, border := range []uint32{0x12882200, 0x12882280, 0x128822ff} {
				want := Surface_Opacity(border, 1-0.5*amount*float32(alpha)/255)
				if layer(4, border).Color != want {
					t.Fatal("resting border must retain hue and explicit transparency")
				}
			}
		}
	}
}

func TestBroadNeutralFaceCollectsLightWithoutChangingAlpha(t *testing.T) {
	for _, alpha := range []uint32{0, 128, 255} {
		for _, background := range []uint32{0x18385800 | alpha, 0x006cff00 | alpha} {
			for _, ambient := range []uint32{0x092039ff, 0xffffffff} {
				for state := 0; state < 4; state++ {
					press, focus := float32(0), float32(0)
					if state == 1 {
						press = 1
					}
					if state == 2 {
						focus = 1
					}
					for _, light := range []uint32{0x006cff00, 0x006cffff} {
						narrow := Surface_LightfieldLayer(3, 72, 40, 8, 1, background, 0x183858ff,
							0x183858ff, light, 0, press, focus, state == 3, 1, ambient)
						wide := Surface_LightfieldLayer(3, 720, 40, 8, 1, background, 0x183858ff,
							0x183858ff, light, 0, press, focus, state == 3, 1, ambient)
						if wide.Color&255 != alpha || wide.EndColor&255 != alpha {
							t.Fatal("broad reflection changed material opacity")
						}
						if background>>8 == 0x183858 && ambient == 0x092039ff && state == 0 && light&255 != 0 {
							if wide.Color == narrow.Color || wide.EndColor == narrow.EndColor {
								t.Fatal("broad neutral material did not collect the light")
							}
						} else if ambient == 0xffffffff && state != 3 {
							// Saturated broad crowns preserve the source hue;
							// neutral faces retain their compact highlight.
							want := narrow.Color
							if background>>8 == 0x006cff {
								want = background
							}
							if wide.Color != want {
								t.Fatal("broad reflection changed the expected light upper face")
							}
						} else if wide.Color != narrow.Color || wide.EndColor != narrow.EndColor {
							t.Fatal("reflection changed a saturated, light, pressed, focused, disabled or unlit face")
						}
					}
				}
			}
		}
	}
}

func TestHoverContactLightStaysNearFaceAndFades(t *testing.T) {
	for _, hover := range []float32{0, 0.25, 0.5, 0.75, 1} {
		layer := Surface_LightfieldLayer(2, 72, 38, 8, 1, 0x006cffff, 0x006cffff,
			0x006cffff, 0x006cffff, hover, 0, 0, false, 1, 0x092039ff)
		face := Surface_FaceOffset(hover, 0, false)
		if layer.Y-face != 2-hover || layer.Blur != 5+2*hover {
			t.Fatalf("contact light detached from face at hover %v: %+v", hover, layer)
		}
		previous := float32(1)
		for distance := float32(0); distance < layer.Blur; distance++ {
			coverage := Surface_SampleCoverage(layer, 30, layer.Height+distance, 1)
			if coverage >= previous || coverage < 0 {
				t.Fatalf("contact light must fade outside its edge: %v >= %v", coverage, previous)
			}
			previous = coverage
		}
		if Surface_SampleCoverage(layer, 30, layer.Height+layer.Blur, 1) != 0 {
			t.Fatal("contact light escaped its blur support")
		}
	}
}

func TestHoverReflectionLeavesBevelExposed(t *testing.T) {
	for _, ambient := range []uint32{0x092039ff, 0xffffffff} {
		for _, hover := range []float32{0, 0.25, 0.5, 0.75, 1} {
			layer := Surface_LightfieldLayer(8, 72, 38, 8, 1, 0x006cffff,
				0x006cffff, 0x006cffff, 0x006cffff, hover, 0, 0, false, 1, ambient)
			inset := float32(1.5)
			if ambient == 0x092039ff {
				inset += 0.5 * hover
			}
			if layer.X != inset || layer.Y != Surface_FaceOffset(hover, 0, false)+inset ||
				layer.Width != 72-2*inset || layer.Height != 38-2*inset || layer.Radius != 8-inset {
				t.Fatalf("reflection must inset continuously with the face: %+v", layer)
			}
			if Surface_SampleCoverage(layer, 30, layer.Height, 1) != 0 {
				t.Fatal("reflection escaped its inset boundary")
			}
		}
	}
}

func TestLightHoverFaceSpreadsLightWithoutChangingAlpha(t *testing.T) {
	for _, alpha := range []uint32{0, 128, 255} {
		background := uint32(0x90c6fe00) | alpha
		white := uint32(0xffffff00) | alpha
		for _, hover := range []float32{0, 0.25, 0.5, 0.75, 1} {
			base := Surface_LightfieldLayer(3, 72, 36, 8, 1, background,
				0x006cffff, 0x006cffff, 0x006cffff, 0, 0, 0, false, 1, 0xffffffff)
			layer := Surface_LightfieldLayer(3, 72, 36, 8, 1, background,
				0x006cffff, 0x006cffff, 0x006cffff, hover, 0, 0, false, 1, 0xffffffff)
			want := Surface_GradientColor(base.EndColor, Surface_GradientColor(background, white, 0.12), hover)
			if layer.EndColor != want || layer.Color&255 != alpha || layer.EndColor&255 != alpha {
				t.Fatalf("light hover must smoothly illuminate the lower face, preserving alpha: %+v", layer)
			}
			pressed := Surface_LightfieldLayer(3, 72, 36, 8, 1, background,
				0x006cffff, 0x006cffff, 0x006cffff, hover, 1, 0, false, 1, 0xffffffff)
			if pressed.Color != background || pressed.EndColor == layer.EndColor {
				t.Fatal("press must retain recessed shading instead of hover light")
			}
		}
	}
}

func TestInteractionMotionAdvancesAllChannelsTogether(t *testing.T) {
	seed := InteractionMotion{
		Hover: MotionTrack{Value: 0.25, Target: 1},
		Press: MotionTrack{Value: 0.5, Target: 1},
		Focus: MotionTrack{Value: 0.75, Target: 1},
	}
	for flags := 0; flags < 128; flags++ {
		hovered, pressed, focused := flags&1 != 0, flags&2 != 0, flags&4 != 0
		enabled, explicit, disabled, loading := flags&8 != 0, flags&16 != 0, flags&32 != 0, flags&64 != 0
		for _, delta := range []float32{-1, 0, 40, 140} {
			actual := Surface_AdvanceInteractionMotion(seed, hovered, pressed, focused,
				enabled, explicit, disabled, loading, delta, 140, 80)
			expected := InteractionMotion{
				Hover: Surface_AdvanceInteraction(seed.Hover, 0, hovered, pressed, focused, enabled, explicit, disabled, loading, delta, 140, 80),
				Press: Surface_AdvanceInteraction(seed.Press, 1, hovered, pressed, focused, enabled, explicit, disabled, loading, delta, 140, 80),
				Focus: Surface_AdvanceInteraction(seed.Focus, 2, hovered, pressed, focused, enabled, explicit, disabled, loading, delta, 140, 80),
			}
			expected.Active = Surface_MotionActive(expected.Hover) || Surface_MotionActive(expected.Press) || Surface_MotionActive(expected.Focus)
			if actual != expected {
				t.Fatalf("flags=%d delta=%g: got %+v, want %+v", flags, delta, actual, expected)
			}
		}
	}
}

func TestBorderlessDarkReflectionConcentratesAtHoveredLowerEdge(t *testing.T) {
	for state, expected := range [][2]uint32{{7, 21}, {15, 160}, {7, 47}, {5, 5}} {
		hover, focus := float32(0), float32(0)
		if state == 1 {
			hover = 1
		}
		if state == 2 {
			focus = 1
		}
		layer := Surface_LightfieldLayer(5, 72, 40, 8, 1,
			0x092039ff, 0, 0x006cffff, 0x409cffff,
			hover, 0, focus, state == 3, 1, 0x092039ff)
		if layer.Color&255 != expected[0] || layer.EndColor&255 != expected[1] {
			t.Fatalf("state %d: borderless reflection alpha=%d/%d, want %v", state, layer.Color&255, layer.EndColor&255, expected)
		}
	}
}

func TestSurfaceSampleCoverage(t *testing.T) {
	for mode := 0; mode < 5; mode++ {
		for _, scale := range []float32{0.75, 1, 2} {
			layer := SurfaceLayer{Width: 12, Height: 8, Radius: 3}
			if mode == 1 || mode == 3 {
				layer.Stroke = 1
			}
			if mode == 2 || mode == 3 {
				layer.Blur = 2
			}
			if mode >= 3 {
				layer.InnerBlur = 2
			}
			for y := float32(-5); y <= 20; y++ {
				for x := float32(-5); x <= 28; x++ {
					width, height, radius := layer.Width*scale, layer.Height*scale, layer.Radius*scale
					var expected float32
					switch mode {
					case 3:
						expected = Surface_BlurStrokeCoverage(x, y, width, height, radius, layer.Stroke*scale, layer.Blur*scale)
					case 2:
						expected = Surface_BlurCoverage(x, y, width, height, radius, layer.Blur*scale)
					case 4:
						expected = Surface_InnerBlurCoverage(x, y, width, height, radius, layer.InnerBlur*scale)
					default:
						expected = Surface_LayerCoverage(x, y, width, height, radius, layer.Stroke*scale)
					}
					if got := Surface_SampleCoverage(layer, x, y, scale); got != expected {
						t.Fatalf("mode=%d scale=%g sample=(%g,%g): got %g want %g", mode, scale, x, y, got, expected)
					}
				}
			}
		}
	}
}

func TestDarkCrownReflectionIsRestrained(t *testing.T) {
	dark := Surface_LightfieldLayer(9, 72, 40, 8, 1,
		0x006cffff, 0x006cffff, 0x006cffff, 0x409cffff,
		0, 0, 0, false, 1, 0x092039ff)
	light := Surface_LightfieldLayer(9, 72, 40, 8, 1,
		0x006cffff, 0x006cffff, 0x006cffff, 0x409cffff,
		0, 0, 0, false, 1, 0xffffffff)
	if dark.Color&255 != 10 || light.Color&255 != 23 {
		t.Fatalf("crown alpha: dark=%d light=%d", dark.Color&255, light.Color&255)
	}
}

func TestDarkFocusAttenuatesOnlyTheRestingReflection(t *testing.T) {
	for state, alpha := range []uint32{96, 49, 178, 191} {
		reflection := Surface_LightfieldLayer(8, 72, 40, 8, 1,
			0x006cffff, 0x006cffff, 0x006cffff, 0x409cffff,
			float32(state/2), 0, float32(state%2), false, 1, 0x092039ff)
		if reflection.EndColor&255 != alpha {
			t.Fatalf("state %d: reflection alpha=%d, want %d", state, reflection.EndColor&255, alpha)
		}
	}
}

func TestSharedArcCoverage(t *testing.T) {
	for angle := -720; angle <= 720; angle++ {
		want := math.Sin(float64(angle) * math.Pi / 180)
		if math.Abs(float64(Surface_SinDegrees(float32(angle)))-want) > 0.00001 {
			t.Fatalf("sine endpoint at %d degrees", angle)
		}
	}
	if Surface_ArcCoverage(0, 0, 7, 9, 0, 360) != 0 {
		t.Fatal("ring center must remain empty")
	}
	if Surface_ArcCoverage(8, 0, 7, 9, 0, 90) != 1 ||
		Surface_ArcCoverage(-9, 0, 7, 9, 0, 90) != 0 {
		t.Fatal("quarter arc points in the wrong direction")
	}
	partial := false
	for y := -10; y < 10; y++ {
		for x := -10; x < 10; x++ {
			a := Surface_ArcCoverage(float32(x), float32(y), 7, 9, 315, 585)
			b := Surface_ArcCoverage(float32(x), float32(y), 7, 9, -45, 225)
			if a != b {
				t.Fatal("arc must wrap across zero degrees")
			}
			if a > 0 && a < 1 {
				partial = true
			}
		}
	}
	if !partial {
		t.Fatal("arc must have antialiased edge coverage")
	}
}

func TestSurfaceGradientTransparencyAndRoundedClip(t *testing.T) {
	r := New(AppConfig{Width: 32, Height: 32}).(*runtime)
	r.Surface(Rectangle{X: 4, Y: 4, Width: 24, Height: 24}, Style{
		Fields:     StyleBackground | StyleBackgroundEnd | StyleRadius | StyleOpacity,
		Background: Color{255, 0, 0, 0}, BackgroundEnd: Color{0, 0, 255, 255},
		Radius: 8, Opacity: 0.5,
	})
	ops := r.FrameOps()
	op := ops[len(ops)-1]
	if !op.HasBackgroundEnd || op.BackgroundEnd != (Color{0, 0, 255, 255}) {
		t.Fatalf("gradient lost by Surface adapter: %+v", op)
	}
	img := image.NewRGBA(image.Rect(0, 0, 32, 32))
	renderMaterial(img, op)
	if img.RGBAAt(4, 4).A != 0 || img.RGBAAt(16, 26).A < 110 {
		t.Fatal("rounded gradient lost its clip or transparent-to-opaque fill")
	}
	if img.RGBAAt(16, 5).A >= img.RGBAAt(16, 26).A {
		t.Fatal("gradient opacity should increase toward the bottom")
	}
}

func TestButtonUsesSharedStyleGradient(t *testing.T) {
	r := New(AppConfig{Width: 80, Height: 48}).(*runtime)
	r.SetThemeMode(ThemeModeDark)
	r.Button(ButtonProps{Bounds: Rectangle{X: 4, Y: 4, Width: 72, Height: 40},
		ID: 1, State: ButtonStateNormal,
		Style: ControlStyle{Normal: Style{
			Fields:     StyleBackground | StyleBackgroundEnd,
			Background: Color{200, 0, 0, 255}, BackgroundEnd: Color{0, 0, 200, 255},
		}},
	})
	ops := r.FrameOps()
	op := ops[len(ops)-1]
	if op.Kind != FrameOpButton || !(op.Button.Appearance.Value.Fields&uint32(StyleBackgroundEnd) != 0) || unpackRGBA(op.Button.Appearance.Value.BackgroundEnd).B != 200 {
		t.Fatalf("button lost the shared gradient style: %+v", op)
	}
	img := image.NewRGBA(image.Rect(0, 0, 80, 48))
	renderButton(img, op)
	top, bottom := img.RGBAAt(40, 10), img.RGBAAt(40, 37)
	if top.R <= top.B || bottom.B <= bottom.R {
		t.Fatalf("button did not paint the custom red-to-blue face: %v -> %v", top, bottom)
	}
}

func TestCustomGradientPresenceFadesWithInteraction(t *testing.T) {
	r := New(AppConfig{Width: 100, Height: 50}).(*runtime)
	r.BeginFrame()
	defer r.EndFrame()
	r.frameDeltaMS = 70
	props := ButtonProps{Bounds: Rectangle{X: 10, Y: 10, Width: 80, Height: 30}, ID: 91,
		Style: ControlStyle{Hover: Style{Fields: StyleBackgroundEnd,
			BackgroundEnd: Color{B: 255}}}}
	material := Surface_FillGradient(Surface_FlatLayer(0, 80, 30, 8, 0, 0, 0, 1),
		true, 0x102030ff, 0x405060ff, 1)
	for _, inside := range []bool{true, false} {
		r.mousePos = Vector2{}
		if inside {
			r.mousePos = Vector2{X: 20, Y: 20}
		}
		r.buttonAt(props)
		ops := r.FrameOps()
		op := ops[len(ops)-1]
		if !op.Button.Material.FillValid || op.Button.Appearance.Fill.Normal || !op.Button.Appearance.Fill.Hover {
			t.Fatalf("lost state-specific gradient presence: %+v", op.Button.Appearance.Fill)
		}
		amount := op.Button.Appearance.Fill.HoverAmount
		if amount <= 0 || amount >= 1 {
			t.Fatalf("expected an in-flight fade, got %v", amount)
		}
		got := Surface_ApplyFillStates(material, op.Button.Appearance.Fill, 1)
		want := Surface_GradientColor(material.EndColor, 0x0000ff00, amount)
		if got.EndColor != want {
			t.Fatalf("gradient snapped on entering/leaving hover: %08x != %08x", got.EndColor, want)
		}
	}
}

func TestFillStatesFallbackAndPrecedence(t *testing.T) {
	material := Surface_FillGradient(Surface_FlatLayer(0, 72, 40, 8, 0, 0, 0, 1),
		true, 0x102030ff, 0x405060ff, 1)
	states := FillStates{Hover: true, HoverStart: 0xff0000ff, HoverEnd: 0x0000ff00}
	for _, amount := range []float32{0, 0.25, 0.5, 0.75, 1} {
		states.HoverAmount = amount
		got := Surface_ApplyFillStates(material, states, 1)
		if got.Color != Surface_GradientColor(material.Color, states.HoverStart, amount) ||
			got.EndColor != Surface_GradientColor(material.EndColor, states.HoverEnd, amount) {
			t.Fatalf("fill interpolation differs from C policy at %v", amount)
		}
	}
	states.PressAmount = 1
	got := Surface_ApplyFillStates(material, states, 1)
	if got.Color != material.Color || got.EndColor != material.EndColor {
		t.Fatal("a pressed state without a custom gradient must reveal its material")
	}
	states.Press = true
	states.PressStart, states.PressEnd = 0x00ff0080, 0xff000080
	got = Surface_ApplyFillStates(material, states, 0.5)
	if got.Color != 0x00ff0040 || got.EndColor != 0xff000040 {
		t.Fatal("custom endpoint alpha and surface opacity must each apply once")
	}
	states = FillStates{Focus: true, FocusStart: 0xff0000ff, FocusEnd: 0x00ff00ff,
		FocusAmount: 0.5}
	got = Surface_ApplyFillStates(material, states, 1)
	if got.Color != Surface_GradientColor(material.Color, states.FocusStart, 0.5) ||
		got.EndColor != Surface_GradientColor(material.EndColor, states.FocusEnd, 0.5) {
		t.Fatal("focus-only gradients must fade from the live material")
	}
	states.HoverAmount = 1
	got = Surface_ApplyFillStates(material, states, 1)
	if got.Color != material.Color || got.EndColor != material.EndColor {
		t.Fatal("hover must take precedence over the focused custom gradient")
	}
}

func TestSharedControlIconCoverage(t *testing.T) {
	for shape := int32(1); shape <= 4; shape++ {
		for _, size := range []float32{14, 18, 24, 36} {
			partial, covered := false, false
			for y := 0; y < int(size); y++ {
				for x := 0; x < int(size); x++ {
					v := Surface_IconCoverage(shape, float32(x), float32(y), size, size)
					covered = covered || v > 0
					partial = partial || (v > 0 && v < 1)
				}
			}
			if !covered || !partial {
				t.Fatalf("shape %d at %v lacks antialiased coverage", shape, size)
			}
		}
	}
	if Surface_IconCoverage(1, 11, 11, 24, 24) != 1 ||
		Surface_IconCoverage(3, 11, 11, 24, 24) != 0 {
		t.Fatal("plus center must be solid and trash body must be open")
	}
	if Surface_IconCoverage(99, 0, 0, 24, 24) != 0 ||
		Surface_IconCoverage(1, 0, 0, 0, 24) != 0 {
		t.Fatal("invalid icon must be empty")
	}
}

func TestStyledSurfaceUsesSharedLayersAndExplicitZeros(t *testing.T) {
	r := New(AppConfig{}).(*runtime)
	r.BeginFrame()
	r.Surface(Rectangle{X: 4, Y: 4, Width: 24, Height: 24}, Style{
		Fields:     StyleBackground | StyleBorder | StyleRadius | StyleBorderWidth | StyleOpacity,
		Background: Color{R: 255, A: 255}, Border: Color{B: 255, A: 255},
		Radius: 8, BorderWidth: 1, Opacity: 1,
	})
	op := r.ops[len(r.ops)-1]
	img := image.NewRGBA(image.Rect(0, 0, 32, 32))
	renderMaterial(img, op)
	if img.RGBAAt(4, 4).A != 0 || img.RGBAAt(16, 16).R != 255 || img.RGBAAt(16, 4).B != 255 {
		t.Fatal("surface must have transparent rounded corners, a filled center, and an inset border")
	}
	r.Surface(op.Bounds, Style{Fields: StyleBackground | StyleRadius | StyleOpacity,
		Background: Color{}, Radius: 0, Opacity: 0})
	op = r.ops[len(r.ops)-1]
	if op.Color.A != 0 || op.Radius != 0 || op.Opacity != 0 {
		t.Fatal("surface lost explicit zeros")
	}
	flat := Surface_FlatLayer(1, 80, 40, 12, 2, 0x102030ff, 0x50607080, 0.5)
	if flat.Radius != 12 || flat.Stroke != 2 || flat.Color != 0x50607040 {
		t.Fatal("flat layer differs from C fixture")
	}
}

func TestDefaultButtonMotionAndZeroDurationOptOut(t *testing.T) {
	r := New(AppConfig{Width: 100, Height: 50}).(*runtime)
	r.BeginFrame()
	r.frameDeltaMS = 70
	r.mousePos = Vector2{X: 20, Y: 20}
	props := ButtonProps{Bounds: Rectangle{X: 10, Y: 10, Width: 80, Height: 30}, ID: 90}
	r.buttonAt(props)
	if got := instanceState[ButtonInstance](r, uint64(uint32(90))).Motion.Hover.Value; got != 0.875 {
		t.Fatalf("default hover should animate halfway through 140ms: %v", got)
	}
	r.mousePos = Vector2{X: 0, Y: 0}
	r.buttonAt(props)
	if got := instanceState[ButtonInstance](r, uint64(uint32(90))).Motion.Hover.Value; got != 0.109375 {
		t.Fatalf("hover exit should reverse smoothly: %v", got)
	}
	theme := ThemeDefaultLight()
	theme.Metrics.TransitionNormalMS = 0
	theme.Metrics.TransitionFastMS = 0
	r.SetTheme(theme)
	r.mousePos = Vector2{X: 20, Y: 20}
	r.buttonAt(props)
	if got := instanceState[ButtonInstance](r, uint64(uint32(90))).Motion.Hover.Value; got != 1 {
		t.Fatalf("zero-duration motion opt-out did not snap: %v", got)
	}
	r.EndFrame()
}

func TestCustomPaintMetricsFadeAndReverseWithoutMovingHitBounds(t *testing.T) {
	r := New(AppConfig{Width: 120, Height: 60}).(*runtime)
	r.BeginFrame()
	defer r.EndFrame()
	fields := StyleRadius | StyleBorderWidth | StyleOpacity | StyleContentOffset
	props := ButtonProps{Bounds: Rectangle{X: 10, Y: 10, Width: 100, Height: 40}, ID: 92,
		Style: ControlStyle{
			Normal: Style{Fields: fields, Radius: 4, BorderWidth: 1, Opacity: 1},
			Hover: Style{Fields: fields, Radius: 12, BorderWidth: 3, Opacity: 0.25,
				ContentOffset: Vector2{X: 4, Y: -2}},
		}}
	paint := func() FrameOp {
		r.buttonAt(props)
		ops := r.FrameOps()
		return ops[len(ops)-1]
	}
	r.frameDeltaMS = 70
	r.mousePos = Vector2{X: 30, Y: 30}
	hover := paint()
	if hover.Button.Appearance.Value.Radius != 11 || hover.Button.Appearance.Value.BorderWidth != 2.75 || hover.Button.Appearance.Value.Opacity != 0.34375 ||
		(Vector2{X: hover.Button.Appearance.Value.OffsetX, Y: hover.Button.Appearance.Value.OffsetY}) != (Vector2{X: 3.5, Y: -1.75}) || hover.Bounds != props.Bounds {
		t.Fatalf("paint metrics must fade without changing hit geometry: %+v", hover)
	}
	r.frameDeltaMS = 0
	r.mousePos = Vector2{}
	reversed := paint()
	if reversed.Button.Appearance.Value.Radius != hover.Button.Appearance.Value.Radius || reversed.Button.Appearance.Value.BorderWidth != hover.Button.Appearance.Value.BorderWidth ||
		reversed.Button.Appearance.Value.Opacity != hover.Button.Appearance.Value.Opacity || (Vector2{X: reversed.Button.Appearance.Value.OffsetX, Y: reversed.Button.Appearance.Value.OffsetY}) != (Vector2{X: hover.Button.Appearance.Value.OffsetX, Y: hover.Button.Appearance.Value.OffsetY}) {
		t.Fatal("reversing an interaction must begin at the current displayed metrics")
	}
	r.frameDeltaMS = 140
	settled := paint()
	if settled.Button.Appearance.Value.Radius != 4 || settled.Button.Appearance.Value.BorderWidth != 1 || settled.Button.Appearance.Value.Opacity != 1 || (Vector2{X: settled.Button.Appearance.Value.OffsetX, Y: settled.Button.Appearance.Value.OffsetY}) != (Vector2{}) {
		t.Fatal("settled paint metrics must return to their exact normal values")
	}
	props.State = ButtonStateHover
	r.frameDeltaMS = 0
	explicit := paint()
	if explicit.Button.Appearance.Value.Radius != 12 || explicit.Button.Appearance.Value.BorderWidth != 3 || explicit.Button.Appearance.Value.Opacity != 0.25 ||
		(Vector2{X: explicit.Button.Appearance.Value.OffsetX, Y: explicit.Button.Appearance.Value.OffsetY}) != (Vector2{X: 4, Y: -2}) {
		t.Fatal("explicit preview states must use their exact target metrics immediately")
	}
	props.Style.Pressed = Style{Fields: fields}
	props.State = ButtonStatePressed
	zero := paint()
	if zero.Radius != 0 || zero.BorderWidth != 0 || zero.Opacity != 0 || zero.ContentOffset != (Vector2{}) {
		t.Fatal("explicitly flagged zero values must remain valid transition endpoints")
	}
}

func TestInteractionValueHasExactEndpointsAndPrecedence(t *testing.T) {
	for _, test := range []struct{ h, p, f, want float32 }{
		{0, 0, 0, 4}, {0.875, 0, 0, 11}, {0, 0, 0.5, 7},
		{1, 0, 1, 12}, {1, 1, 1, 2}, {-1, -1, -1, 4}, {2, 2, 2, 2},
	} {
		if got := Surface_InteractionValue(4, 12, 2, 10, test.h, test.p, test.f); got != test.want {
			t.Fatalf("scalar transition differs from C policy: got %v want %v", got, test.want)
		}
	}
}

func TestCustomFocusColorFadesAndReverses(t *testing.T) {
	r := New(AppConfig{Width: 120, Height: 60}).(*runtime)
	r.SetThemeMode(ThemeModeDark)
	r.BeginFrame()
	defer r.EndFrame()
	props := ButtonProps{Bounds: Rectangle{X: 10, Y: 10, Width: 100, Height: 40}, ID: 93,
		Style: ControlStyle{
			Normal:  Style{Fields: StyleFocus, Focus: Color{R: 255, A: 128}},
			Focused: Style{Fields: StyleFocus, Focus: Color{B: 255, A: 64}},
			Hover:   Style{Fields: StyleFocus, Focus: Color{G: 255, A: 192}},
			Pressed: Style{Fields: StyleFocus},
		}}
	paint := func() Color {
		r.buttonAt(props)
		ops := r.FrameOps()
		return unpackRGBA(ops[len(ops)-1].Button.Appearance.Value.Focus)
	}
	r.mousePos = Vector2{}
	r.focusID = 93
	r.frameDeltaMS = 70
	enter := paint()
	if enter != (Color{R: 31, B: 223, A: 72}) {
		t.Fatalf("custom focus color snapped instead of fading: %+v", enter)
	}
	r.focusID = 0
	r.frameDeltaMS = 0
	if paint() != enter {
		t.Fatal("focus reversal jumped at zero elapsed time")
	}
	r.frameDeltaMS = 140
	if paint() != props.Style.Normal.Focus {
		t.Fatal("focus exit did not settle to its exact normal color")
	}
	r.focusID = 93
	if paint() != props.Style.Focused.Focus {
		t.Fatal("focus did not settle to its exact custom color")
	}
	r.mousePos = Vector2{X: 30, Y: 30}
	r.frameDeltaMS = 70
	hover := paint()
	if hover != (Color{G: 223, B: 31, A: 176}) {
		t.Fatalf("hover must fade over the focused resting color: %+v", hover)
	}
	r.mousePos = Vector2{}
	r.frameDeltaMS = 0
	if paint() != hover {
		t.Fatal("hover reversal jumped while focused")
	}
	r.frameDeltaMS = 140
	if paint() != props.Style.Focused.Focus {
		t.Fatal("hover exit must reveal the focused color")
	}
	r.mousePos = Vector2{X: 30, Y: 30}
	if paint() != props.Style.Hover.Focus {
		t.Fatal("hover must reach its exact color before pressing")
	}
	r.mouseDown[MouseButtonLeft] = true
	r.frameDeltaMS = 40
	press := paint()
	if press != (Color{G: 31, A: 24}) {
		t.Fatalf("press must fade toward the transparent endpoint: %+v", press)
	}
	r.mouseDown[MouseButtonLeft] = false
	r.frameDeltaMS = 0
	if paint() != press {
		t.Fatal("release jumped at zero elapsed time")
	}
	r.frameDeltaMS = 80
	if paint() != props.Style.Hover.Focus {
		t.Fatal("release must reveal the hover color while the pointer remains inside")
	}
	props.State = ButtonStatePressed
	if paint() != (Color{}) {
		t.Fatal("explicit transparent focus must remain an exact preview endpoint")
	}
}

func TestAutomaticFocusFadesMaterialColors(t *testing.T) {
	r := New(AppConfig{Width: 100, Height: 50}).(*runtime)
	r.SetThemeMode(ThemeModeDark)
	r.BeginFrame()
	r.mousePos = Vector2{X: -100, Y: -100}
	r.focusID = 90
	r.frameDeltaMS = 70
	props := ButtonProps{Bounds: Rectangle{X: 10, Y: 10, Width: 80, Height: 30}, ID: 90, Tone: ButtonToneAccent}
	normal := resolveButtonStyle(r.theme(), true, r.activeTheme, props, ButtonStateNormal)
	focused := resolveButtonStyle(r.theme(), true, r.activeTheme, props, ButtonStateFocus)
	r.buttonAt(props)
	ops := r.FrameOps()
	got := unpackRGBA(ops[len(ops)-1].Button.Appearance.Value.Background)
	want := unpackRGBA(Surface_GradientColor(packRGBA(normal.Background), packRGBA(focused.Background), 0.875))
	if got != want || got == normal.Background || got == focused.Background {
		t.Fatalf("focus material did not fade: got=%v want=%v", got, want)
	}
	r.buttonAt(props)
	ops = r.FrameOps()
	if unpackRGBA(ops[len(ops)-1].Button.Appearance.Value.Background) != focused.Background {
		t.Fatal("automatic focus did not reach the explicitly focused material")
	}
	r.focusID = 0
	r.buttonAt(props)
	ops = r.FrameOps()
	if unpackRGBA(ops[len(ops)-1].Button.Appearance.Value.Background) == normal.Background || unpackRGBA(ops[len(ops)-1].Button.Appearance.Value.Background) == focused.Background {
		t.Fatal("focus exit snapped instead of fading")
	}
	r.EndFrame()
}

func TestTransparentSurfaceHasNoFallbackPaint(t *testing.T) {
	for _, state := range []ButtonState{ButtonStateNormal, ButtonStateHover, ButtonStatePressed, ButtonStateDisabled} {
		r := New(AppConfig{Width: 100, Height: 50}).(*runtime)
		r.Button(ButtonProps{Bounds: Rectangle{X: 10, Y: 10, Width: 80, Height: 30},
			Label: "Hidden", State: state, Style: ControlStyle{Normal: Style{
				Fields: StyleBackground | StyleForeground | StyleBorder | StyleFocus,
			}}})
		img := image.NewRGBA(image.Rect(0, 0, 100, 50))
		for _, op := range r.FrameOps() {
			if op.Kind == FrameOpButton {
				renderButton(img, op)
			}
		}
		for y := 0; y < 50; y++ {
			for x := 0; x < 100; x++ {
				if pixel := img.RGBAAt(x, y); pixel.A != 0 {
					t.Fatalf("state %d resurrected transparent paint at %d,%d: %+v", state, x, y, pixel)
				}
			}
		}
	}
}

func TestFallbackIconBlendsPartialOpacity(t *testing.T) {
	img := image.NewRGBA(image.Rect(0, 0, 16, 16))
	fillRectPixels(img, 0, 0, 16, 16, Color{20, 40, 60, 255})
	renderIcon(img, FrameOp{Bounds: Rectangle{Width: 16, Height: 16},
		IconType: UIIconTypeX, Color: Color{200, 100, 50, 128}})
	if got := img.RGBAAt(2, 1); got != (color.RGBA{110, 70, 55, 255}) {
		t.Fatalf("half-opacity icon did not composite over its surface: %+v", got)
	}
	if got := img.RGBAAt(0, 0); got != (color.RGBA{20, 40, 60, 255}) {
		t.Fatalf("icon changed an uncovered pixel: %+v", got)
	}
}

func TestTransparentButtonForegroundDoesNotResurrectIcons(t *testing.T) {
	for _, state := range []ButtonState{ButtonStateNormal, ButtonStateHover, ButtonStatePressed,
		ButtonStateFocus, ButtonStateDisabled, ButtonStateLoading, ButtonStateSelected} {
		for _, icon := range []int32{UIIconTypePlay, UIIconTypeX, UIIconTypeWorkbookFillColor} {
			r := New(AppConfig{Width: 100, Height: 60}).(*runtime)
			r.Button(ButtonProps{Bounds: Rectangle{X: 10, Y: 10, Width: 80, Height: 40},
				State: state, IconType: icon, Style: ControlStyle{Normal: Style{
					Fields: StyleForeground, Foreground: Color{R: 123, G: 45, B: 67, A: 0},
				}}})
			for _, op := range r.FrameOps() {
				if op.Kind != FrameOpButton {
					continue
				}
				actual := image.NewRGBA(image.Rect(0, 0, 100, 60))
				expected := image.NewRGBA(actual.Bounds())
				renderButton(actual, op)
				op.IconType = UIIconTypeNone
				renderButton(expected, op)
				for i, pixel := range actual.Pix {
					if pixel != expected.Pix[i] {
						t.Fatalf("transparent foreground painted icon %d in state %d at byte %d", icon, state, i)
					}
				}
			}
		}
	}
}

func TestColorButtonSuppliesVisibleSurfaceDefaults(t *testing.T) {
	r := New(AppConfig{Width: 100, Height: 50}).(*runtime)
	r.ColorButton(ColorButtonProps{Bounds: Rectangle{X: 10, Y: 10, Width: 80, Height: 30},
		Color: Color{R: 200, G: 30, B: 20, A: 255}})
	for _, op := range r.FrameOps() {
		if op.Kind != FrameOpButton {
			continue
		}
		if op.Opacity != 1 || op.BorderWidth != r.themeMetrics().BorderWidth {
			t.Fatalf("control omitted resolved surface defaults: %+v", op)
		}
		img := image.NewRGBA(image.Rect(0, 0, 100, 50))
		renderButton(img, op)
		if img.RGBAAt(30, 25).A != 255 {
			t.Fatal("color control surface is invisible")
		}
		return
	}
	t.Fatal("missing color button surface")
}

func TestSurfaceDepthRetainsColorAndAlpha(t *testing.T) {
	if Surface_DepthColor(0x006cff80, 0) != 0x006cff80 ||
		Surface_DepthColor(0x006cff80, 0.3)&0xffff != 0xff80 ||
		Surface_DepthColor(0x000000ff, 1) != 0x000000ff {
		t.Fatal("depth shading must preserve the strongest channel and alpha")
	}
}

func TestSurfaceChevron(t *testing.T) {
	if Surface_ChevronCoverage(8, 10, 18) <= 0 || Surface_ChevronCoverage(8, 2, 18) != 0 {
		t.Fatal("disclosure must point down, with an open upper center")
	}
	for y := float32(0); y < 18; y++ {
		for x := float32(0); x < 18; x++ {
			if Surface_ChevronCoverage(x, y, 18) != Surface_ChevronCoverage(17-x, y, 18) {
				t.Fatalf("asymmetric chevron at %v,%v", x, y)
			}
		}
	}
}

func TestSurfaceExternalBlur(t *testing.T) {
	if Surface_BlurFalloff(0, 10) != 1 || Surface_BlurFalloff(5, 10) != 0.1875 || Surface_BlurFalloff(10, 10) != 0 {
		t.Fatal("external blur must fade smoothly to zero at its support boundary")
	}
	if Surface_BlurCoverage(-5.5, 5, 20, 12, 0, 10) != 0.1875 ||
		Surface_BlurCoverage(-10.5, 5, 20, 12, 0, 10) != 0 {
		t.Fatal("blur must extend outside the source bounds")
	}
	previous := float32(1)
	for distance := float32(0); distance <= 10; distance += 0.125 {
		coverage := Surface_BlurFalloff(distance, 10)
		if coverage > previous || coverage < 0 {
			t.Fatalf("blur brightens away from the source at %v", distance)
		}
		previous = coverage
	}
}

func TestSurfaceRoundedCoverage(t *testing.T) {
	if got := Surface_LayerCoverage(-0.5, 4, 20, 12, 0, 0); got != 0.5 {
		t.Fatalf("fractional edge coverage = %v, want 0.5", got)
	}
	if Surface_LayerCoverage(5, 5, 20, 12, 4, 1) != 0 ||
		Surface_LayerCoverage(0, 5, 20, 12, 4, 1) != 1 {
		t.Fatal("stroke must cover the inset edge, not the interior")
	}
	corner := Surface_LayerCoverage(1, 1, 20, 12, 4, 0)
	if corner <= 0 || corner >= 1 || corner != Surface_LayerCoverage(18, 1, 20, 12, 4, 0) {
		t.Fatalf("corner coverage must be fractional and symmetric: %v", corner)
	}
	if Surface_LayerCoverage(1, 1, -2, 12, 4, 0) != 0 {
		t.Fatal("negative bounds must not paint")
	}
}

func TestSurfaceAlphaCompositing(t *testing.T) {
	img := image.NewRGBA(image.Rect(0, 0, 1, 1))
	img.SetRGBA(0, 0, color.RGBA{B: 255, A: 255})
	blendPixel(img, 0, 0, Color{R: 255, A: 128})
	if got := img.RGBAAt(0, 0); got != (color.RGBA{R: 128, B: 127, A: 255}) {
		t.Fatalf("surface layer did not blend over background: %+v", got)
	}
	img.SetRGBA(0, 0, color.RGBA{})
	blendPixel(img, 0, 0, Color{R: 255, A: 128})
	if got := img.RGBAAt(0, 0); got != (color.RGBA{R: 128, A: 128}) {
		t.Fatalf("surface layer is not premultiplied: %+v", got)
	}
}

func TestSurfaceTransitionEndpoints(t *testing.T) {
	for _, elapsed := range []float32{100, 200} {
		if got := Surface_Transition(3, 7, elapsed, 100); got != 7 {
			t.Fatalf("transition endpoint: got %v", got)
		}
	}
	if got := Surface_Transition(3, 7, -10, 100); got != 3 {
		t.Fatalf("negative elapsed: got %v", got)
	}
	if got := Surface_Transition(3, 7, 0, 0); got != 7 {
		t.Fatalf("instant transition: got %v", got)
	}
}

func TestSurfaceMotionRetargeting(t *testing.T) {
	track := Surface_AdvanceMotion(MotionTrack{}, 1, 50, 100, false)
	if track.Value != 0.875 {
		t.Fatalf("half-time cubic ease-out: %+v", track)
	}
	reversed := Surface_AdvanceMotion(track, 0, 0, 100, false)
	if reversed.Value != track.Value || reversed.Origin != track.Value {
		t.Fatalf("retargeting jumped: before %+v, after %+v", track, reversed)
	}
	reversed = Surface_AdvanceMotion(reversed, 0, 50, 100, false)
	if reversed.Value != 0.109375 {
		t.Fatalf("reversal curve: %+v", reversed)
	}
	reversed = Surface_AdvanceMotion(reversed, 0, 50, 100, false)
	if reversed.Value != 0 {
		t.Fatalf("reversal never reached zero: %+v", reversed)
	}
	oneFrame := Surface_AdvanceMotion(MotionTrack{}, 1, 100, 100, false)
	var tenFrames MotionTrack
	for i := 0; i < 10; i++ {
		tenFrames = Surface_AdvanceMotion(tenFrames, 1, 10, 100, false)
	}
	if oneFrame.Value != tenFrames.Value || oneFrame.Value != 1 {
		t.Fatalf("frame-rate dependent endpoint: %+v versus %+v", oneFrame, tenFrames)
	}
	if got := Surface_AdvanceMotion(track, 0, 0, 100, true); got.Value != 0 {
		t.Fatalf("immediate state did not snap: %+v", got)
	}
	if got := Surface_AdvanceMotion(track, 0, 0, 0, false); got.Value != 0 {
		t.Fatalf("zero-duration transition did not snap: %+v", got)
	}
}

func TestSurfaceOpacity(t *testing.T) {
	for _, test := range []struct {
		opacity float32
		want    uint32
	}{{-1, 0x12345600}, {0, 0x12345600}, {0.5, 0x12345640}, {1, 0x12345680}, {2, 0x12345680}} {
		if got := Surface_Opacity(0x12345680, test.opacity); got != test.want {
			t.Fatalf("opacity %v: got %#x, want %#x", test.opacity, got, test.want)
		}
	}
}

func TestSurfaceLightingRespondsToGeometryAndSurroundings(t *testing.T) {
	rim := func(width, height float32, ambient uint32) SurfaceLayer {
		return Surface_LightfieldLayer(5, width, height, 8, 1,
			0x006cffFF, 0x006cffFF, 0x006cffFF, 0x409cffFF,
			0, 0, 0, false, 1, ambient)
	}
	normal := rim(72, 34, 0x00172dff)
	large := rim(72, 48, 0x00172dff)
	wide := rim(720, 34, 0x00172dff)
	light := rim(720, 34, 0xffffffff)
	if large.EndColor&255 <= normal.EndColor&255 || wide.EndColor&255 <= normal.EndColor&255 {
		t.Fatal("larger surfaces should carry a stronger lower-edge reflection")
	}
	if light.EndColor&255 >= wide.EndColor&255 {
		t.Fatal("light surroundings should soften the luminous lower edge")
	}
	if normal.Radius != 7 {
		t.Fatal("inset rim must share the face's corner center")
	}
}

func TestLightLowerRimRespondsToHeightWithoutEscapingFace(t *testing.T) {
	for _, tint := range []uint32{0x006cffff, 0xff2848ff, 0x00bb88ff, 0xffbf24ff} {
		previous := uint32(0)
		for _, height := range []float32{32, 36, 40, 44, 48} {
			rim := func(press float32, disabled bool) SurfaceLayer {
				return Surface_LightfieldLayer(5, 72, height, 8, 1,
					tint, tint, tint, 0x409cffff, 0, press, 0, disabled, 1, 0xffffffff)
			}
			resting := rim(0, false)
			if resting.EndColor&255 <= previous {
				t.Fatal("taller light surfaces must gather more lower-rim reflection")
			}
			previous = resting.EndColor & 255
			if resting.X != 1 || resting.Y != 1 || resting.Width != 70 ||
				resting.Height != height-2 || resting.Radius != 7 || resting.Stroke != 1 {
				t.Fatal("height-dependent reflection must remain a one-pixel inset rim")
			}
			if rim(1, false).EndColor&255 != 0 || rim(0, true).EndColor&255 > 5 {
				t.Fatal("pressed and disabled controls must suppress the raised reflection")
			}
		}
	}
}

func TestTallDarkRimBuildsAboveStandardHeight(t *testing.T) {
	rim := func(height, press float32, disabled bool, background uint32) SurfaceLayer {
		return Surface_LightfieldLayer(5, 72, height, 8, 1,
			background, 0x006cffff, 0x006cffff, 0x409cffff,
			0, press, 0, disabled, 1, 0x092039ff)
	}
	standard := rim(40, 0, false, 0x006cffff).EndColor & 255
	middle := rim(44, 0, false, 0x006cffff).EndColor & 255
	tall := rim(48, 0, false, 0x006cffff).EndColor & 255
	if standard != 86 || middle <= standard || tall-middle <= middle-standard {
		t.Fatal("extra dark reflection must build smoothly above the unchanged standard-height rim")
	}
	for _, height := range []float32{40, 44, 48, 64} {
		layer := rim(height, 0, false, 0x006cffff)
		if layer.Stroke != 1 || layer.X != 1 || layer.Width != 70 || layer.Height != height-2 {
			t.Fatal("tall reflection must not expand the inner rim")
		}
		if rim(height, 1, false, 0x006cffff).EndColor&255 != 0 ||
			rim(height, 0, true, 0x006cffff).EndColor&255 > 5 ||
			rim(height, 0, false, 0x006cff00).EndColor&255 != 0 {
			t.Fatal("pressed, disabled, and transparent surfaces must suppress raised reflection")
		}
	}
}

func TestSharedInteractionTimingAndSuppression(t *testing.T) {
	for channel := int32(0); channel < 3; channel++ {
		delta := float32(70)
		if channel == 1 {
			delta = 40
		}
		track := Surface_AdvanceInteraction(MotionTrack{}, channel, true, true, true,
			true, false, false, false, delta, 140, 80)
		if track.Value != 0.875 {
			t.Fatalf("channel %d used the wrong transition duration: %+v", channel, track)
		}
		for _, loading := range []bool{false, true} {
			stopped := Surface_AdvanceInteraction(track, channel, true, true, true,
				true, false, !loading, loading, 0, 140, 80)
			if stopped.Value != 0 || Surface_MotionActive(stopped) {
				t.Fatal("disabled/loading interaction must extinguish immediately")
			}
		}
		for _, explicit := range []bool{false, true} {
			snapped := Surface_AdvanceInteraction(MotionTrack{}, channel, true, true, true,
				explicit, explicit, false, false, 0, 140, 80)
			if snapped.Value != 1 || Surface_MotionActive(snapped) {
				t.Fatal("explicit states and disabled animation must snap to their endpoint")
			}
		}
	}
}

func TestSharedFillAssemblyPreservesPresenceAndTransparentEndpoints(t *testing.T) {
	normal := Surface_FillState(0, 0x112233ff, 0x445566ff)
	hover := Surface_FillState(StyleBackgroundEnd, 0x77889900, 0)
	press := Surface_FillState(0, 0, 0)
	focus := Surface_FillState(StyleBackgroundEnd, 0xabcdef80, 0x12345600)
	got := Surface_FillTransition(normal, hover, press, focus, 0.25, 0.5, 0.75)
	if got.Normal || !got.Hover || got.Press || !got.Focus || got.NormalStart != normal.NormalStart ||
		got.HoverStart != 0x77889900 || got.HoverEnd != 0 || got.FocusEnd != 0x12345600 ||
		got.HoverAmount != 0.25 || got.PressAmount != 0.5 || got.FocusAmount != 0.75 {
		t.Fatalf("shared fill assembly lost presence or transition values: %+v", got)
	}
}

func TestSurfaceLayers(t *testing.T) {
	layer := func(index int32, disabled bool, opacity float32) SurfaceLayer {
		return Surface_LightfieldLayer(index, 100, 40, 8, 1,
			0x102030ff, 0x405060ff, 0x708090ff, 0xa0b0c0ff,
			1, 0, 1, disabled, opacity, 0x00172dff)
	}
	face := layer(3, false, 1)
	if face.Width != 100 || face.Height != 40 || face.Y != -1 || !face.Gradient {
		t.Fatalf("hover face: %+v", face)
	}
	for i := int32(0); i < Surface_MaterialLayerCount(int32(MaterialLightfield)); i++ {
		if got := layer(i, false, 0); got.Color&255 != 0 {
			t.Fatalf("layer %d ignores explicit zero opacity: %+v", i, got)
		}
	}
	for _, i := range []int32{6, 7} {
		if got := layer(i, true, 1); got.Color&255 != 0 {
			t.Fatalf("disabled focus layer %d is visible: %+v", i, got)
		}
	}
	if got := Surface_FaceOffset(1, 1, true); got != 0 {
		t.Fatalf("disabled surface moves: %v", got)
	}
}

func TestSurfaceIdentifiesItsFillLayer(t *testing.T) {
	for _, ambient := range []uint32{0x092039ff, 0xffffffff} {
		for _, disabled := range []bool{false, true} {
			for _, opacity := range []float32{0, 0.5, 1} {
				faces := 0
				for index := int32(0); index < Surface_MaterialLayerCount(int32(MaterialLightfield)); index++ {
					layer := Surface_LightfieldLayer(index, 72, 40, 8, 1,
						0x006cffff, 0x006cffff, 0x006cffff, 0x409cff80,
						0.5, 0.25, 0.75, disabled, opacity, ambient)
					if !layer.IsFace {
						continue
					}
					faces++
					if !layer.Gradient || layer.Stroke != 0 || layer.Blur != 0 ||
						layer.InnerBlur != 0 || layer.OutsideOnly || layer.Width != 72 || layer.Height != 40 {
						t.Fatal("only the material face may receive the application fill")
					}
					styled := Surface_FillGradient(layer, true, 0xff000080, 0x0000ff00, opacity)
					if !styled.IsFace || styled.X != layer.X || styled.Y != layer.Y {
						t.Fatal("a custom gradient must preserve the face role and geometry")
					}
				}
				if faces != 1 {
					t.Fatalf("surface declares %d fill layers, want exactly one", faces)
				}
			}
		}
	}
}

func TestRaisedSurfaceHasShadedFaceAndSoftContactShadow(t *testing.T) {
	for _, ambient := range []uint32{0x00172dff, 0xffffffff} {
		layer := func(index int32, press float32) SurfaceLayer {
			return Surface_LightfieldLayer(index, 72, 40, 8, 1,
				0x006cffff, 0x006cffff, 0x006cffff, 0x409cffff,
				0, press, 0, false, 1, ambient)
		}
		face := layer(3, 0)
		if !face.Gradient || face.Color == face.EndColor {
			t.Fatal("raised face needs a visible lighting gradient")
		}
		if face.Color&255 != 255 || face.EndColor&255 != 255 {
			t.Fatal("lighting must preserve opaque face coverage")
		}
		if (face.EndColor>>8)&255 != 255 {
			t.Fatal("resting luminous blue must retain its strongest color channel")
		}
		shadow := layer(1, 0)
		if shadow.Blur <= 0 || layer(2, 0).Blur <= 0 {
			t.Fatal("contact shadow and lower light must have soft edges")
		}
		if layer(1, 1).Color&255 >= shadow.Color&255 {
			t.Fatal("pressed surface must reduce its contact shadow")
		}
	}
}

func TestMaterialLightStaysInsideFace(t *testing.T) {
	for _, disabled := range []bool{false, true} {
		inner := Surface_LightfieldLayer(8, 72, 40, 8, 1,
			0x006cffff, 0x006cffff, 0x006cffff, 0x409cffff,
			1, 0, 0, disabled, 1, 0x00172dff)
		if inner.Blur != 0 || inner.InnerBlur <= 0 || inner.X <= 0 || inner.Y < 0 ||
			inner.X+inner.Width >= 72 || inner.Y+inner.Height >= 40 {
			t.Fatalf("material lighting escaped the face: %+v", inner)
		}
		if !inner.Gradient || inner.Color&255 != 0 {
			t.Fatal("interior light should fade to transparent above the lower edge")
		}
		if disabled != (inner.EndColor&255 == 0) {
			t.Fatal("disabled material must not emit interior light")
		}
	}
}

func TestCrownReflectionStaysInsideAndDimsOnPress(t *testing.T) {
	for _, ambient := range []uint32{0x00172dff, 0xffffffff} {
		for _, disabled := range []bool{false, true} {
			for _, press := range []float32{0, 0.5, 1} {
				crown := Surface_LightfieldLayer(9, 72, 40, 8, 1,
					0x006cffff, 0x006cffff, 0x006cffff, 0x409cffff,
					1, press, 0, disabled, 1, ambient)
				if crown.Blur != 0 || crown.InnerBlur <= 0 || crown.X <= 0 || crown.X+crown.Width >= 72 {
					t.Fatalf("crown escaped the face: %+v", crown)
				}
				if !crown.Gradient || crown.EndColor&255 != 0 {
					t.Fatal("crown must fade to transparent")
				}
				if (disabled || press == 1) != (crown.Color&255 == 0) {
					t.Fatal("pressed and disabled faces must extinguish the reflection")
				}
				if Surface_InnerBlurCoverage(-2, 20, crown.Width, crown.Height, crown.Radius, crown.InnerBlur) != 0 {
					t.Fatal("reflection leaked outside its clip")
				}
			}
		}
	}
}

func TestLightContactShadowFallsBelowTheFace(t *testing.T) {
	shadow := func(width, press float32, disabled bool) SurfaceLayer {
		return Surface_LightfieldLayer(2, width, 40, 8, 1,
			0x006cffff, 0x006cffff, 0x006cffff, 0x409cffff,
			0, press, 0, disabled, 1, 0xffffffff)
	}
	resting := shadow(72, 0, false)
	if !resting.Gradient || resting.Color&255 != 0 || resting.EndColor&255 == 0 {
		t.Fatal("light contact shadow must fade from transparent above to tinted below")
	}
	if resting.Blur < 5 || resting.EndColor>>24 != 0 {
		t.Fatal("blue contact shadow must stay soft and chromatic, not acquire a white rim")
	}
	for _, reduced := range []SurfaceLayer{shadow(72, 1, false), shadow(72, 0, true), shadow(720, 0, false)} {
		if reduced.EndColor&255 >= resting.EndColor&255 {
			t.Fatal("pressed, disabled, and broad surfaces must reduce the contact shadow")
		}
	}
}

func TestLightHoverSoftensContactShadowContinuously(t *testing.T) {
	for _, tint := range []uint32{0x006cffff, 0xff2848ff, 0x00bb88ff, 0xffbf24ff} {
		previous := uint32(256)
		for step := 0; step <= 4; step++ {
			hover := float32(step) / 4
			layer := Surface_LightfieldLayer(2, 72, 40, 8, 1,
				tint, tint, tint, 0x409cffff, hover, 0, 0, false, 1, 0xffffffff)
			alpha := layer.EndColor & 255
			if alpha >= previous || alpha == 0 {
				t.Fatal("light hover must progressively soften, not extinguish, the contact shadow")
			}
			if layer.EndColor>>8 != Surface_DepthColor(tint, 0.65-0.30*hover)>>8 {
				t.Fatal("hover must brighten the contact reflection continuously")
			}
			previous = alpha
		}
	}
}

func TestLightContactReflectionCombinesActiveTracks(t *testing.T) {
	for _, hover := range []float32{0, 0.25, 0.5, 0.75, 1} {
		for _, focus := range []float32{0, 0.25, 0.5, 0.75, 1} {
			active := hover + focus
			if active > 1 {
				active = 1
			}
			layer := Surface_LightfieldLayer(2, 72, 40, 8, 1,
				0x006cffff, 0x006cffff, 0x006cffff, 0x409cffff,
				hover, 0, focus, false, 1, 0xffffffff)
			want := Surface_DepthColor(0x006cffff, 0.65-0.30*active)
			if layer.EndColor>>8 != want>>8 || layer.Color&255 != 0 || !layer.Gradient || layer.Blur != 8+4*active {
				t.Fatalf("hover %g focus %g: unexpected contact reflection %+v", hover, focus, layer)
			}
		}
	}
}

func TestPaleBorderlessPressSoftensLowerTint(t *testing.T) {
	for _, alpha := range []uint32{0, 128, 255} {
		for _, press := range []float32{0, 0.5, 1} {
			face := func(border uint32, ambient uint32) SurfaceLayer {
				return Surface_LightfieldLayer(3, 72, 40, 8, 1,
					0xf8fbff00|alpha, border, 0x064cffff, 0x064cffff,
					0, press, 0, false, 1, ambient)
			}
			borderless := face(0, 0xffffffff)
			bordered := face(0x064cffff, 0xffffffff)
			if borderless.EndColor&255 != alpha || borderless.Color&255 != alpha {
				t.Fatal("press reflection must preserve face alpha")
			}
			if press == 0 {
				if borderless.EndColor != bordered.EndColor {
					t.Fatal("resting lower tint must remain unchanged")
				}
			} else if borderless.EndColor>>24 <= bordered.EndColor>>24 {
				t.Fatal("borderless press must retain less saturated lower shading")
			}
			if face(0, 0x00172dff).EndColor != face(0x064cffff, 0x00172dff).EndColor {
				t.Fatal("pale press correction must not change dark surroundings")
			}
		}
	}
}

func TestTallLightSurfaceContactDepth(t *testing.T) {
	for _, test := range []struct {
		height float32
		alpha  uint32
	}{{32, 110}, {40, 110}, {44, 130}, {48, 150}, {64, 150}} {
		layer := Surface_LightfieldLayer(2, 72, test.height, 8, 1,
			0x006cffff, 0x006cffff, 0x006cffff, 0x409cffff,
			0, 0, 0, false, 1, 0xffffffff)
		if layer.EndColor&255 != test.alpha || layer.Color&255 != 0 || layer.Blur != 8 {
			t.Fatalf("height %v: contact must deepen without widening or losing its fade: %+v", test.height, layer)
		}
	}
}

func TestLightSaturatedFaceDepthFollowsAspectRatio(t *testing.T) {
	for _, alpha := range []uint32{0, 128, 255} {
		for _, test := range []struct {
			width float32
			depth float32
		}{{72, 0.35}, {160, 0.35}, {400, 0.415}, {640, 0.48}, {720, 0.48}} {
			background := uint32(0x006cff00) | alpha
			for _, ambient := range []uint32{0xffffffff, 0x00172dff} {
				depth := test.depth
				if ambient == 0x00172dff {
					depth = 0.48
				}
				face := Surface_LightfieldLayer(3, test.width, 40, 8, 1,
					background, background, background, background, 0, 0, 0, false, 1, ambient)
				if face.EndColor != Surface_DepthColor(background, depth) {
					t.Fatalf("width %g ambient %08x: unexpected lower face %08x", test.width, ambient, face.EndColor)
				}
			}
		}
	}
}

func TestDarkBorderlessHoverBevelUsesChromaticLight(t *testing.T) {
	for _, hover := range []float32{0, 0.25, 0.5, 0.75, 1} {
		for _, press := range []float32{0, 0.25, 0.5, 1} {
			layer := Surface_LightfieldLayer(5, 72, 36, 8, 1,
				0x092039ff, 0, 0x409cffff, 0x409cffff, hover, press, 0, false, 1, 0x00172dff)
			amount := hover * (1 - press)
			light := Surface_GradientColor(0x409cffff, Surface_ChromaColor(0x409cffff, 255), amount)
			bottom := (float32(0.20) + 0.28*0.25 + 0.50*hover) * (1 - press)
			bottom *= 0.25 + 0.50*hover
			want := Surface_Opacity(Surface_GradientColor(light, 0xffffffff, 0.72*(1-amount)), bottom)
			if layer.EndColor != want {
				t.Fatalf("hover %g press %g: edge %08x, want %08x", hover, press, layer.EndColor, want)
			}
		}
	}
}

func TestBroadLightSaturatedCrownPreservesHue(t *testing.T) {
	for _, width := range []float32{72, 160, 400, 640, 720} {
		for _, background := range []uint32{0x006cffff, 0xff0000ff, 0xffffffff} {
			for _, hover := range []float32{0, 0.25, 0.5, 1} {
				for _, press := range []float32{0, 0.5, 1} {
					broad := Surface_Unit((width/40 - 4) / 12)
					saturation := Surface_Unit((float32(Surface_ColorChroma(background)) - 128) / 64)
					highlight := float32(0.5) - hover*0.4
					highlight -= 0.5 * broad * saturation * (1 - hover)
					want := Surface_GradientColor(background, Surface_GradientColor(background, 0xffffffff, 0.3), highlight*(1-press))
					face := Surface_LightfieldLayer(3, width, 40, 8, 1,
						background, background, background, 0x409cffff, hover, press, 0, false, 1, 0xffffffff)
					if face.Color != want {
						t.Fatalf("width %g color %08x hover %g press %g: crown %08x, want %08x", width, background, hover, press, face.Color, want)
					}
				}
			}
		}
	}
}

func TestTallLightSaturatedReflectionStaysInside(t *testing.T) {
	for _, height := range []float32{32, 40, 44, 48, 64} {
		for _, background := range []uint32{0x006cffff, 0xffffffff, 0x808080ff} {
			for _, press := range []float32{0, 0.25, 0.5, 1} {
				raised := Surface_Unit((height - 40) / 8)
				raised *= raised * Surface_Unit((float32(Surface_ColorChroma(background))-128)/64)
				layer := Surface_LightfieldLayer(8, 72, height, 8, 1,
					background, background, 0x006cffff, 0x409cffff, 0, press, 0, false, 1, 0xffffffff)
				light := Surface_LiftColor(Surface_LiftColor(0x006cffff, raised), raised)
				strength := float32(0.38) * (1 - press*0.85) * (0.12 + 1.88*raised)
				want := Surface_Opacity(Surface_GradientColor(light, 0xffffffff, 0.22), strength)
				if layer.EndColor != want || layer.Blur != 0 || layer.InnerBlur <= 0 {
					t.Fatalf("height %g background %08x press %g: reflection %+v, want %08x", height, background, press, layer, want)
				}
				if Surface_SampleCoverage(layer, -1, 10, 1) != 0 || Surface_SampleCoverage(layer, 10, layer.Height+1, 1) != 0 {
					t.Fatal("raised reflection escaped the face")
				}
			}
		}
	}
}

func TestDarkInnerVolumeSpreadsWithoutEscapingFace(t *testing.T) {
	for _, height := range []float32{32, 40, 44, 48, 64} {
		for _, press := range []float32{0, 0.25, 0.5, 1} {
			for _, focus := range []float32{0, 0.25, 0.5, 1} {
				for _, ambient := range []uint32{0x00172dff, 0xffffffff} {
					layer := Surface_LightfieldLayer(8, 72, height, 8, 1,
						0x006cffff, 0x006cffff, 0x006cffff, 0x409cffff,
						0, press, focus, false, 1, ambient)
					want := float32(18) + 4*Surface_Unit((height-32)/16)
					if ambient == 0x00172dff {
						want += 12 * Surface_Unit(press+focus+Surface_Unit((height-40)/8))
					}
					if layer.InnerBlur != want || layer.Blur != 0 {
						t.Fatalf("height %g press %g focus %g: unexpected volume %+v", height, press, focus, layer)
					}
					if Surface_SampleCoverage(layer, -1, 10, 1) != 0 ||
						Surface_SampleCoverage(layer, layer.Width+1, 10, 1) != 0 {
						t.Fatal("inner light escaped the face")
					}
				}
			}
		}
	}
}

func TestLightFocusSoftensContactReflectionContinuously(t *testing.T) {
	for _, focused := range []float32{0, 0.25, 0.5, 0.75, 1} {
		layer := Surface_LightfieldLayer(2, 72, 40, 8, 1,
			0x006cffff, 0x006cffff, 0x006cffff, 0x409cffff,
			0, 0, focused, false, 1, 0xffffffff)
		want := uint32(110 - 40*focused)
		if layer.EndColor&255 != want || layer.Color&255 != 0 || layer.Blur != 8+4*focused {
			t.Fatalf("focus %g: expected softened contact alpha %d, got %+v", focused, want, layer)
		}
	}
}

func TestSubduedDarkFaceConcentratesLowerReflection(t *testing.T) {
	for _, test := range []struct {
		background uint32
		border     uint32
		ambient    uint32
		bias       float32
	}{
		{0x00172dff, 0x006cffff, 0x00172dff, 1},
		{0x000050ff, 0x006cffff, 0x00172dff, 0.5},
		{0x006cffff, 0x006cffff, 0x00172dff, 0},
		{0x00172dff, 0x006cff00, 0x00172dff, 0},
		{0x00172dff, 0x006cffff, 0xffffffff, 0},
	} {
		for _, press := range []float32{0, 0.5, 1} {
			for _, alpha := range []uint32{0, 128, 255} {
				layer := Surface_LightfieldLayer(8, 72, 40, 8, 1,
					test.background, test.border, 0x006cff00|alpha, 0x409cff00|alpha,
					0.5, press, 0, false, 1, test.ambient)
				if layer.GradientBias != test.bias*(1-press) {
					t.Fatalf("directional reflection differs: %+v, press %v: %+v", test, press, layer)
				}
				if Surface_SampleColor(layer, 0) != layer.Color || Surface_SampleColor(layer, 1) != layer.EndColor {
					t.Fatal("directional reflection changed its endpoints")
				}
				if layer.Blur != 0 || layer.InnerBlur <= 0 || Surface_SampleCoverage(layer, -1, 10, 1) != 0 {
					t.Fatal("lower reflection must remain inside the face")
				}
				if alpha == 0 && layer.EndColor&255 != 0 {
					t.Fatal("directional reflection restored transparent light")
				}
			}
		}
	}
}

func TestRestingDarkReflectionKeepsItsHue(t *testing.T) {
	for _, alpha := range []uint32{0, 128, 255} {
		for _, hover := range []float32{0, 0.5, 1} {
			for _, press := range []float32{0, 0.5, 1} {
				light := uint32(0x006cff00) | alpha
				layer := Surface_LightfieldLayer(8, 72, 40, 8, 1,
					0x006cffff, 0x006cffff, light, light, hover, press, 0, false, 1, 0x092039ff)
				whitening := float32(0.22) * (1 - 0.64*(1-hover)*(1-press))
				want := Surface_GradientColor(light, 0xffffff00|alpha, whitening)
				if layer.EndColor>>8 != want>>8 {
					t.Fatalf("hover %g press %g: reflection %08x, want hue %08x", hover, press, layer.EndColor, want)
				}
				if layer.EndColor&255 > alpha || Surface_SampleCoverage(layer, -1, 20, 1) != 0 {
					t.Fatal("reflection restored opacity or escaped the face")
				}
			}
		}
	}
}

func TestMaterialFaceHasDepthWithoutAnOuterFog(t *testing.T) {
	layer := func(index int32, hover float32) SurfaceLayer {
		return Surface_LightfieldLayer(index, 72, 40, 8, 1,
			0x006cffff, 0x006cffff, 0x006cffff, 0x409cffff,
			hover, 0, 0, false, 1, 0x00172dff)
	}
	face := layer(3, 0)
	topGreen := (face.Color >> 16) & 255
	bottomGreen := (face.EndColor >> 16) & 255
	if !face.Gradient || topGreen <= bottomGreen+30 {
		t.Fatal("the blue material needs a lit crown and a deeper lower face")
	}
	if (layer(3, 1).EndColor>>16)&255 <= bottomGreen {
		t.Fatal("hover must gather light in the lower face")
	}
	hoverBorder := layer(4, 1)
	if !hoverBorder.Gradient || hoverBorder.EndColor>>24 <= hoverBorder.Color>>24 {
		t.Fatal("hover must brighten the lower material edge")
	}
	for _, hover := range []float32{0, 1} {
		inner := layer(8, hover)
		outer := layer(0, hover)
		if inner.InnerBlur < 10 || inner.Blur != 0 {
			t.Fatal("material light must spread inward, not outside the face")
		}
		if inner.EndColor&255 <= outer.Color&255 {
			t.Fatal("the interior light must dominate the external field")
		}
	}
}

func TestInnerLightFadesIntoRoundedSurface(t *testing.T) {
	if Surface_RoundedDistance(9.5, 9.5, 20, 20, 0) != -10 ||
		Surface_RoundedDistance(21.5, 9.5, 20, 20, 0) != 2 {
		t.Fatal("signed distance differs from C policy")
	}
	for y := float32(-2); y <= 22; y += 0.5 {
		for x := float32(-2); x <= 22; x += 0.5 {
			coverage := Surface_InnerBlurCoverage(x, y, 20, 20, 4, 6)
			clip := Surface_RoundedCoverage(x, y, 20, 20, 4)
			if coverage < 0 || coverage > clip {
				t.Fatalf("inner light escaped rounded coverage at %v,%v", x, y)
			}
		}
	}
	previous := float32(1)
	for x := float32(0); x < 10; x += 0.25 {
		coverage := Surface_InnerBlurCoverage(x, 10, 20, 20, 4, 6)
		if coverage > previous {
			t.Fatal("inner light brightens away from the edge")
		}
		previous = coverage
	}
	if previous != 0 || Surface_InnerBlurCoverage(-1, 10, 20, 20, 4, 6) != 0 {
		t.Fatal("inner light must vanish at the center and outside the face")
	}
}

func TestSurfaceColorSamplingKeepsTransparentEndpoints(t *testing.T) {
	layer := SurfaceLayer{Color: 0x112233ff, EndColor: 0x8899aa00}
	for step := -2; step <= 12; step++ {
		position := float32(step) / 10
		layer.Gradient = false
		if got := Surface_SampleColor(layer, position); got != layer.Color {
			t.Fatalf("solid sampling changed color at %g: %#x", position, got)
		}
		layer.Gradient = true
		want := Surface_GradientColor(layer.Color, layer.EndColor, position)
		if got := Surface_SampleColor(layer, position); got != want {
			t.Fatalf("gradient sampling at %g: %#x, want %#x", position, got, want)
		}
	}
	if Surface_SampleColor(layer, -1) != layer.Color || Surface_SampleColor(layer, 2) != layer.EndColor {
		t.Fatal("samples beyond the face must clamp to exact RGBA endpoints")
	}
}

func TestSurfaceGradientBiasAndCustomFill(t *testing.T) {
	layer := SurfaceLayer{Color: 0x000000ff, EndColor: 0xffffff00,
		Gradient: true, GradientBias: 1}
	if got := Surface_SampleColor(layer, 0.5); got != Surface_GradientColor(layer.Color, layer.EndColor, 0.25) {
		t.Fatalf("curved midpoint = %#x", got)
	}
	if Surface_SampleColor(layer, -1) != layer.Color || Surface_SampleColor(layer, 2) != layer.EndColor {
		t.Fatal("curved gradient changed its endpoints")
	}
	previous := uint32(0)
	for step := 0; step <= 100; step++ {
		value := Surface_SampleColor(layer, float32(step)/100) >> 24
		if value < previous {
			t.Fatal("curved gradient reversed direction")
		}
		previous = value
	}
	custom := Surface_FillGradient(layer, true, 0xff0000ff, 0x0000ff00, 1)
	if custom.GradientBias != 0 || Surface_SampleColor(custom, 0.5) != Surface_GradientColor(custom.Color, custom.EndColor, 0.5) {
		t.Fatal("explicit gradient inherited the material curve")
	}
	for _, amount := range []float32{0, 0.25, 0.5, 0.75, 1} {
		states := FillStates{Hover: true, HoverAmount: amount}
		if got := Surface_ApplyFillStates(layer, states, 1).GradientBias; got != 1-amount {
			t.Fatalf("custom hover curve at %g = %g", amount, got)
		}
	}
	if Surface_FillGradient(layer, false, 0, 0, 1) != layer {
		t.Fatal("absent custom gradient changed the material")
	}
}

func TestDarkHoverCurveFadesWithInteraction(t *testing.T) {
	for _, hover := range []float32{0, 0.5, 1} {
		for _, press := range []float32{0, 0.5, 1} {
			for _, ambient := range []uint32{0x092039ff, 0xffffffff} {
				for _, disabled := range []bool{false, true} {
					face := Surface_LightfieldLayer(3, 72, 40, 8, 1,
						0x006cffff, 0x006cffff, 0x006cffff, 0x409cffff,
						hover, press, 0, disabled, 1, ambient)
					want := hover * (1 - press)
					if disabled || ambient == 0xffffffff {
						want = 0
					}
					if face.GradientBias != want {
						t.Fatalf("hover=%g press=%g disabled=%t ambient=%#x: curve=%g, want %g",
							hover, press, disabled, ambient, face.GradientBias, want)
					}
				}
			}
		}
	}
}

func TestDarkFaceRetainsSaturatedHue(t *testing.T) {
	for _, hover := range []float32{0, 1} {
		face := Surface_LightfieldLayer(3, 72, 40, 8, 1,
			0x008063ff, 0x00ffbbff, 0x00ffbbff, 0x409cffff,
			hover, 0, 0, false, 1, 0x092039ff)
		if face.Color>>24 > 10 || face.EndColor>>24 > 10 {
			t.Fatalf("white reflection diluted the green material: %+v", face)
		}
		if (face.Color>>16)&255 < 128 {
			t.Fatal("preserving saturation must not darken every channel")
		}
		if face.Color&255 != 255 || face.EndColor&255 != 255 {
			t.Fatal("reflection must preserve opaque face coverage")
		}
	}
}

func TestDarkLowEmphasisFaceDoesNotBecomeFilled(t *testing.T) {
	face := Surface_LightfieldLayer(3, 72, 40, 8, 1,
		0x092039ff, 0, 0, 0x409cffff, 0, 0, 0, false, 1, 0x092039ff)
	if face.Color>>24 > 16 || (face.Color>>8)&255 > 70 {
		t.Fatalf("focus light washed out low-emphasis material: %#x", face.Color)
	}
}

func TestDarkFaceHighlightVariesContinuously(t *testing.T) {
	var previous uint32
	for blue := uint32(76); blue <= 86; blue++ {
		background := uint32(50)<<16 | blue<<8 | 255
		face := Surface_LightfieldLayer(3, 72, 40, 8, 1,
			background, 0, 0, 0x409cffff, 0, 0, 0, false, 1, 0x092039ff)
		if blue > 76 {
			for shift := uint32(8); shift <= 24; shift += 8 {
				difference := int((face.Color>>shift)&255) - int((previous>>shift)&255)
				if difference < -3 || difference > 3 {
					t.Fatalf("face highlight jumps at blue=%d: %#x -> %#x", blue, previous, face.Color)
				}
			}
		}
		previous = face.Color
	}
}

func TestDarkFocusConcentratesLightAtTheRim(t *testing.T) {
	face := func(hover, focus float32, ambient uint32) SurfaceLayer {
		return Surface_LightfieldLayer(3, 72, 38, 8, 1,
			0x0055d4ff, 0x006cffff, 0x006cffff, 0x409cffff,
			hover, 0, focus, false, 1, ambient)
	}
	resting, focused := face(0, 0, 0x092039ff), face(0, 1, 0x092039ff)
	if focused.Color>>24 >= resting.Color>>24 || (focused.Color>>16)&255 >= (resting.Color>>16)&255 {
		t.Fatal("dark focus must reduce face whitening while retaining the bright rim")
	}
	if focused.Color&255 != 255 || focused.EndColor&255 != 255 {
		t.Fatal("focus depth must not change opaque face coverage")
	}
	if face(1, 1, 0x092039ff) != face(1, 0, 0x092039ff) {
		t.Fatal("hover face lighting must take precedence over focus absorption")
	}
	if face(0, 1, 0xffffffff) != face(0, 0, 0xffffffff) {
		t.Fatal("dark focus absorption must not change the light face")
	}
}

func TestDarkFocusEdgeBalancesHueAndWhite(t *testing.T) {
	for _, material := range []uint32{0x006cffff, 0xff0017ff, 0x00ffc3ff, 0xffb100ff} {
		for _, alpha := range []uint32{0, 128, 255} {
			focus := uint32(0x409cff00) | alpha
			for _, amount := range []float32{0, 0.25, 0.5, 0.75, 1} {
				edge := Surface_LightfieldLayer(7, 72, 40, 8, 1,
					0x808080ff, material, material, focus, 0, 0, amount, false, 1, 0x092039ff)
				want := Surface_Opacity(Surface_GradientColor(focus, 0xffffff00|alpha, 0.5), amount)
				if edge.Color != want || edge.Stroke != 1.5 {
					t.Fatalf("focus %g alpha %d: edge %+v, want %08x", amount, alpha, edge, want)
				}
			}
		}
	}
}

func TestDarkFocusEdgeTracksBodyChroma(t *testing.T) {
	for _, sample := range []struct {
		chroma uint32
		white  float32
	}{{0, 0.5}, {128, 0.5}, {160, 0.61}, {192, 0.72}, {255, 0.72}} {
		for _, alpha := range []uint32{0, 128, 255} {
			focus := uint32(0x409cff00) | alpha
			for _, amount := range []float32{0, 0.25, 0.5, 0.75, 1} {
				edge := Surface_LightfieldLayer(7, 72, 40, 8, 1,
					sample.chroma<<8|255, 0x006cffff, 0x006cffff, focus,
					0, 0, amount, false, 1, 0x092039ff)
				want := Surface_Opacity(Surface_GradientColor(focus, 0xffffff00|alpha, sample.white), amount)
				if edge.Color != want || edge.Stroke != 1.5 {
					t.Fatalf("chroma %d focus %g alpha %d: edge %+v, want %08x", sample.chroma, amount, alpha, edge, want)
				}
			}
		}
	}
}

func TestPressedInsetShadowTracksDepthWithoutLeaking(t *testing.T) {
	for _, ambient := range []uint32{0x092039ff, 0xffffffff} {
		shadow := func(press float32, disabled bool, background uint32, opacity float32) SurfaceLayer {
			return Surface_LightfieldLayer(10, 72, 40, 8, 1,
				background, 0x006cffff, 0x006cffff, 0x409cffff,
				0, press, 0, disabled, opacity, ambient)
		}
		rest := shadow(0, false, 0x006cffff, 1)
		middle := shadow(0.5, false, 0x006cffff, 1)
		pressed := shadow(1, false, 0x006cffff, 1)
		if rest.Color&255 != 0 || middle.Color&255 == 0 || middle.Color&255 >= pressed.Color&255 {
			t.Fatal("inset shadow must deepen continuously with the press track")
		}
		if pressed.Blur != 0 || pressed.InnerBlur <= 0 || !pressed.Gradient || pressed.EndColor != 0 ||
			pressed.X != 1 || pressed.Y != Surface_FaceOffset(0, 1, false)+1 {
			t.Fatalf("pressed shadow must stay inset and fade downward: %+v", pressed)
		}
		if Surface_InnerBlurCoverage(-1, 20, pressed.Width, pressed.Height, pressed.Radius, pressed.InnerBlur) != 0 ||
			Surface_InnerBlurCoverage(35, 19, pressed.Width, pressed.Height, pressed.Radius, pressed.InnerBlur) != 0 {
			t.Fatal("inset shadow must leave both surroundings and the label center clear")
		}
		if shadow(1, true, 0x006cffff, 1).Color&255 != 0 ||
			shadow(1, false, 0x006cff00, 1).Color&255 != 0 ||
			shadow(1, false, 0x006cffff, 0).Color&255 != 0 {
			t.Fatal("disabled, transparent, and zero-opacity faces must not acquire a pressed shadow")
		}
	}
}

func TestDarkFocusHasBrightInnerEdge(t *testing.T) {
	layer := func(index int32, ambient uint32) SurfaceLayer {
		return Surface_LightfieldLayer(index, 72, 40, 8, 1,
			0x006cffff, 0x006cffff, 0x006cffff, 0x409cff80,
			0, 0, 1, false, 1, ambient)
	}
	dark := layer(7, 0x092039ff)
	light := layer(7, 0xffffffff)
	outer := layer(6, 0x092039ff)
	if dark.Color>>24 <= light.Color>>24 || dark.Color&255 != 128 {
		t.Fatal("dark focus highlight must brighten the edge without changing its alpha")
	}
	if light.Color != 0x409cff80 {
		t.Fatal("light-mode focus edge must retain the declared color")
	}
	if dark.Stroke != 1.5 || light.Stroke != 1 || dark.X != light.X || dark.Width != light.Width {
		t.Fatal("dark focus should widen inward without changing the light-mode edge or outer bounds")
	}
	if outer.Color != Surface_DepthColor(Surface_DepthColor(0x409cff80, 1), 0.4) || outer.Blur != 0 || outer.InnerBlur != 12 || outer.Stroke != 0 {
		t.Fatal("dark focus volume must use inward-only chromatic light and preserve alpha")
	}
	if outer.X != 0 || outer.Y != 0 || outer.Width != 72 || outer.Height != 40 ||
		Surface_InnerBlurCoverage(-1, 20, outer.Width, outer.Height, outer.Radius, outer.InnerBlur) != 0 ||
		Surface_InnerBlurCoverage(36, 1, outer.Width, outer.Height, outer.Radius, outer.InnerBlur) <= 0 {
		t.Fatal("focus volume must illuminate the interior without leaking outside the face")
	}
	ghost := Surface_LightfieldLayer(7, 72, 40, 8, 1,
		0x092039ff, 0, 0, 0x409cff80, 0, 0, 1, false, 1, 0x092039ff)
	if ghost.Color != 0x409cff80 || ghost.Stroke != 1 {
		t.Fatal("ghost focus must not acquire the filled material's bright edge")
	}
}

func TestBlurredStrokeHasSoftEdgesAndClearCenter(t *testing.T) {
	coverage := func(y float32) float32 {
		return Surface_BlurStrokeCoverage(36, y, 72, 40, 8, 1, 12)
	}
	if coverage(0) != 1 || coverage(20) != 0 || coverage(-13) != 0 {
		t.Fatal("blurred stroke must retain its edge, clear center, and finite extent")
	}
	previous := float32(1)
	for y := float32(-1); y >= -12; y-- {
		value := coverage(y)
		if value <= 0 || value >= previous {
			t.Fatal("outer stroke falloff must fade smoothly away from the edge")
		}
		previous = value
	}
	if Surface_BlurStrokeCoverage(36, 0, 72, 40, 8, 1, 0) != Surface_LayerCoverage(36, 0, 72, 40, 8, 1) ||
		Surface_BlurStrokeCoverage(36, 0, 72, 40, 8, 0, 12) != 0 ||
		Surface_BlurStrokeCoverage(36, 0, 0, 40, 8, 1, 12) != 0 {
		t.Fatal("zero blur or empty stroke geometry lost its defined behavior")
	}
}

func TestFocusBloomStaysOutsideTheRim(t *testing.T) {
	for _, alpha := range []uint32{0, 128, 255} {
		previous := uint32(0)
		for step := 0; step <= 4; step++ {
			focus := float32(step) / 4
			layer := Surface_LightfieldLayer(11, 72, 40, 8, 1,
				0x006cffff, 0x006cffff, 0x006cffff, 0x409cff00|alpha,
				0, 0, focus, false, 1, 0x092039ff)
			if !layer.OutsideOnly || layer.Blur != 8 || layer.Stroke != 2 {
				t.Fatal("focus bloom must be a bounded outside-only blurred stroke")
			}
			if layer.Color&255 < previous || (alpha == 0 && layer.Color&255 != 0) {
				t.Fatal("focus bloom must fade continuously and preserve transparent focus")
			}
			previous = layer.Color & 255
			for _, scale := range []float32{1, 1.5, 2} {
				if Surface_SampleCoverage(layer, 36*scale, 20*scale, scale) != 0 ||
					Surface_SampleCoverage(layer, -3*scale, 20*scale, scale) <= 0 ||
					Surface_SampleCoverage(layer, -9*scale, 20*scale, scale) != 0 {
					t.Fatal("focus bloom must leave the face clear and have a finite outer extent at every scale")
				}
			}
		}
	}
	for _, ambient := range []uint32{0x092039ff, 0xffffffff} {
		layer := Surface_LightfieldLayer(11, 72, 40, 8, 1,
			0x006cffff, 0x006cffff, 0x006cffff, 0x409cffff,
			0, 0, 1, ambient != 0xffffffff, 1, ambient)
		if layer.Color&255 != 0 {
			t.Fatal("light surfaces and disabled controls must not acquire dark focus bloom")
		}
	}
}

func TestChromaticFocusBloomUsesTheVisibleEdge(t *testing.T) {
	for _, test := range []struct {
		chroma uint32
		extra  float32
	}{{0, 0}, {128, 0}, {160, 0.075}, {192, 0.15}, {255, 0.15}} {
		for _, borderAlpha := range []uint32{0, 255} {
			for _, focusAlpha := range []uint32{0, 85, 128, 170, 255} {
				for _, focus := range []float32{0, 0.25, 0.5, 0.75, 1} {
					border := test.chroma<<24 | borderAlpha
					focusColor := uint32(0x409cff00) | focusAlpha
					layer := Surface_LightfieldLayer(11, 72, 40, 8, 1,
						0x006cffff, border, border, focusColor, 0, 0, focus, false, 1, 0x092039ff)
					strength := (float32(0.15) + test.extra) * focus
					if borderAlpha == 0 {
						strength = 0.15 * focus * 0.35
					}
					want := Surface_Opacity(Surface_DepthColor(focusColor, 1), strength)
					if layer.Color != want {
						t.Fatalf("border=%08x focus=%g alpha=%d: color=%08x want=%08x", border, focus, focusAlpha, layer.Color, want)
					}
				}
			}
		}
	}
}

func TestLoadingRingUsesSharedTime(t *testing.T) {
	a := Surface_LoadingRing(100, 40, 18, 0, 0x0064ffff, 0x092039ff)
	b := Surface_LoadingRing(100, 40, 18, 750, 0x0064ffff, 0x092039ff)
	c := Surface_LoadingRing(100, 40, 18, 1500, 0x0064ffff, 0x092039ff)
	if a.X != 50 || a.Y != 20 || a.OuterRadius != 10 || a.InnerRadius != 7.5 {
		t.Fatalf("loading geometry: %+v", a)
	}
	if a.StartAngle != 110 || b.StartAngle != 290 || c.StartAngle != 110 || a.EndAngle != 425 {
		t.Fatalf("loading time samples: %+v, %+v, %+v", a, b, c)
	}
	light := Surface_LoadingRing(100, 40, 18, 0, 0x0064ffff, 0xffffffff)
	if light.OuterRadius != 9 || light.InnerRadius != 7 || light.StartAngle != 0 || light.EndAngle != 270 {
		t.Fatalf("light loading geometry: %+v", light)
	}
}

func TestLoadingUsesAccentRatherThanLinkColor(t *testing.T) {
	for _, theme := range []Theme{ThemeDefaultDark(), ThemeDefaultLight()} {
		r := New(AppConfig{Width: 100, Height: 50}).(*runtime)
		r.SetTheme(theme)
		props := ButtonProps{Tone: ButtonToneAccent, Emphasis: ButtonEmphasisFilled}
		style := resolveButtonStyle(r.theme(), r.effectiveDark(), r.activeTheme, props, ButtonStateLoading)
		if style.Foreground != theme.Colors.Accent {
			t.Fatalf("loading indicator must retain the button accent, got %+v", style.Foreground)
		}
	}
}

func TestDarkContactLightSoftensRaisedMaterial(t *testing.T) {
	for _, hover := range []float32{0, 0.5, 1} {
		for _, press := range []float32{0, 0.5, 1} {
			layer := Surface_LightfieldLayer(2, 72, 40, 8, 1,
				0x006cffff, 0x006cffff, 0x006cffff, 0x409cffff,
				hover, press, 0, false, 1, 0x092039ff)
			want := 2 + (3+2*hover)*(1-press)
			if layer.Blur != want {
				t.Fatalf("hover=%g press=%g: contact blur=%g want=%g", hover, press, layer.Blur, want)
			}
			borderless := Surface_LightfieldLayer(2, 72, 40, 8, 1,
				0x006cffff, 0, 0x006cffff, 0x409cffff,
				hover, press, 0, false, 1, 0x092039ff)
			if borderless.Blur != 2+5*hover {
				t.Fatal("raised contact light changed borderless material")
			}
		}
	}
}

func TestTallDarkContactReflectionFollowsHeightAndPressure(t *testing.T) {
	for _, height := range []float32{32, 40, 42, 44, 48, 64} {
		for _, disabled := range []bool{false, true} {
			borderless := Surface_LightfieldLayer(2, 72, height, 8, 1,
				0x006cffff, 0, 0x006cffff, 0x409cffff,
				0, 0, 0, disabled, 1, 0x092039ff)
			wantAlpha := uint32(34)
			if disabled {
				wantAlpha = 8
			}
			if borderless.Blur != 2 || borderless.Color&255 != wantAlpha {
				t.Fatal("tall borderless material must retain its restrained contact")
			}
		}
		for _, press := range []float32{0, 0.25, 0.5, 0.75, 1} {
			for _, hover := range []float32{0, 0.5, 1} {
				raised := Surface_Unit((height - 40) / 8)
				raised *= raised * (1 - press)
				layer := Surface_LightfieldLayer(2, 72, height, 8, 1,
					0x006cffff, 0x006cffff, 0x006cffff, 0x409cffff,
					hover, press, 0, false, 1, 0x092039ff)
				wantBlur := 2 + (3+2*hover)*(1-press) + 3*raised
				wantAlpha := uint32(34 + 90*hover - 26*press + 70*raised)
				if layer.Blur != wantBlur || layer.Color&255 != wantAlpha {
					t.Fatalf("height=%g hover=%g press=%g: blur=%g alpha=%d want=%g,%d",
						height, hover, press, layer.Blur, layer.Color&255, wantBlur, wantAlpha)
				}
			}
		}
		for _, ambient := range []uint32{0x092039ff, 0xffffffff} {
			for _, disabled := range []bool{false, true} {
				layer := Surface_LightfieldLayer(2, 72, height, 8, 1,
					0x006cff00, 0x006cffff, 0x006cffff, 0x409cffff,
					1, 0, 0, disabled, 1, ambient)
				if layer.Color&255 != 0 || layer.EndColor&255 != 0 {
					t.Fatal("height-dependent contact must not resurrect a transparent face")
				}
			}
		}
	}
}

func TestLoadingLightHasLeadingTipAndFadingTrail(t *testing.T) {
	for _, ambient := range []uint32{0x092039ff, 0xffffffff} {
		for _, alpha := range []uint32{0, 128, 255} {
			for _, elapsed := range []float64{0, 375, 997, 1500} {
				ring := Surface_LoadingRing(72, 40, 18, elapsed, 0x006cff00|alpha, ambient)
				radius := (ring.InnerRadius + ring.OuterRadius) * 0.5
				x := radius * Surface_SinDegrees(ring.EndAngle+90)
				y := radius * Surface_SinDegrees(ring.EndAngle)
				want := Surface_GradientColor(ring.Color, ring.TipColor, 0.75)
				if got := Surface_LoadingArcColor(ring, x, y); got != want || got&255 != alpha {
					t.Fatalf("loading lead ambient=%08x alpha=%d time=%g: got=%08x want=%08x", ambient, alpha, elapsed, got, want)
				}
				trail := float32(0.8)
				if ambient == 0xffffffff {
					trail = 0.55
				}
				if ring.TrailOpacity != trail || Surface_LoadingArcColor(ring, -x, -y) != Surface_Opacity(ring.Color, trail) {
					t.Fatal("trailing arc differs from its material opacity")
				}
			}
		}
	}
	ring := Surface_LoadingRing(72, 40, 18, 0, 0x006cff80, 0x092039ff)
	if ring.TipColor != 0xffffff80 {
		t.Fatalf("leading light differs from C policy: %#x", ring.TipColor)
	}
	tipX := 8.75 * Surface_SinDegrees(ring.EndAngle+90)
	tipY := 8.75 * Surface_SinDegrees(ring.EndAngle)
	if Surface_LoadingPaintRadius(ring) != 16 {
		t.Fatal("paint bounds must include the full arc glow")
	}
	if Surface_LoadingTipCoverage(ring, tipX-0.5, tipY-0.5) != 1 ||
		Surface_LoadingTipCoverage(ring, -tipX-0.5, -tipY-0.5) != 0 {
		t.Fatal("leading light is not located at the arc endpoint")
	}
	if Surface_LoadingArcColor(ring, tipX, tipY)&255 <= Surface_LoadingArcColor(ring, -tipX, -tipY)&255 {
		t.Fatal("arc does not fade away from its leading light")
	}
	wrapped := Surface_LoadingRing(72, 40, 18, 1500, 0x006cff80, 0x092039ff)
	for y := float32(-9); y < 9; y++ {
		for x := float32(-9); x < 9; x++ {
			if Surface_LoadingArcColor(ring, x, y) != Surface_LoadingArcColor(wrapped, x, y) {
				t.Fatal("loading light jumps when the animation wraps")
			}
		}
	}
	transparent := Surface_LoadingRing(72, 40, 18, 0, 0x006cff00, 0x092039ff)
	if transparent.TipColor&255 != 0 || Surface_LoadingArcColor(transparent, 0, -8)&255 != 0 {
		t.Fatal("loading highlight resurrected an explicitly transparent color")
	}
}

func TestLoadingTrailChromaKeepsNeutralQuiet(t *testing.T) {
	for _, test := range []struct {
		color uint32
		trail float32
	}{{0x006cff00, 0.8}, {0x00306000, 0.675}, {0xf0f8ff00, 0.55}} {
		for _, alpha := range []uint32{0, 128, 255} {
			for _, ambient := range []uint32{0x092039ff, 0xffffffff} {
				ring := Surface_LoadingRing(72, 40, 18, 375, test.color|alpha, ambient)
				want := test.trail
				if ambient == 0xffffffff {
					want = 0.55
				}
				if ring.TrailOpacity != want || ring.Color&255 != alpha {
					t.Fatalf("trail color=%08x ambient=%08x: %+v", test.color|alpha, ambient, ring)
				}
			}
		}
	}
}

func TestLoadingPhaseDoesNotDriftWithElapsedCycles(t *testing.T) {
	for _, ambient := range []uint32{0x092039ff, 0xffffffff} {
		for _, phase := range []float64{0, 0.25, 1, 375.5, 997, 1499.75} {
			reference := Surface_LoadingRing(72, 40, 18, phase, 0x006cff80, ambient)
			for _, cycles := range []float64{1, 100, 1000, 10000, 1000000, 100000000} {
				got := Surface_LoadingRing(72, 40, 18, phase+cycles*1500, 0x006cff80, ambient)
				if got != reference {
					t.Fatalf("phase %v after %v cycles drifted: %+v, want %+v", phase, cycles, got, reference)
				}
			}
		}
	}
}

func TestSharedLoadingSamplesPreserveBlendInputs(t *testing.T) {
	for _, ambient := range []uint32{0x092039ff, 0xffffffff} {
		for alpha := uint32(0); alpha <= 255; alpha += 85 {
			ring := Surface_LoadingRing(72, 40, 18, 375, 0x006cff00|alpha, ambient)
			for y := float32(-12); y <= 12; y++ {
				for x := float32(-12); x <= 12; x++ {
					got := Surface_LoadingSample(ring, x, y)
					want := RingSample{
						Track: Surface_Opacity(ring.TrackColor,
							Surface_ArcCoverage(x, y, ring.InnerRadius, ring.OuterRadius, 0, 360)),
						Arc: Surface_Opacity(Surface_LoadingArcColor(ring, x+0.5, y+0.5),
							Surface_ArcCoverage(x, y, ring.InnerRadius, ring.OuterRadius, ring.StartAngle, ring.EndAngle)),
						Tip: Surface_Opacity(ring.TipColor, Surface_LoadingTipCoverage(ring, x, y)),
					}
					if ring.GlowBlur > 0 {
						coverage := Surface_BlurStrokeCoverage(x+ring.OuterRadius, y+ring.OuterRadius,
							2*ring.OuterRadius, 2*ring.OuterRadius, ring.OuterRadius,
							ring.OuterRadius-ring.InnerRadius, ring.GlowBlur)
						coverage *= Surface_ArcCoverage(x, y, ring.InnerRadius, ring.OuterRadius+ring.GlowBlur,
							ring.StartAngle, ring.EndAngle)
						want.Glow = Surface_Opacity(Surface_LoadingArcColor(ring, x+0.5, y+0.5), coverage*0.35)
					}
					if got != want {
						t.Fatalf("ambient=%08x alpha=%d pixel=(%g,%g): %+v, want %+v", ambient, alpha, x, y, got, want)
					}
				}
			}
		}
	}
}

func TestLoadingGlowKeepsTheOpeningAndUnlitGapClear(t *testing.T) {
	for _, ambient := range []uint32{0x092039ff, 0xffffffff} {
		for _, color := range []uint32{0x006cffff, 0x006cff80, 0x006cff00, 0x999999ff} {
			ring := Surface_LoadingRing(40, 40, 18, 0, color, ambient)
			ring.StartAngle, ring.EndAngle = 0, 270
			for _, point := range []Vector2{{-0.5, -0.5}, {10, -10}, {16, 0}} {
				if Surface_LoadingSample(ring, point.X, point.Y).Glow&255 != 0 {
					t.Fatalf("glow escaped its lit arc: ambient=%08x color=%08x point=%+v", ambient, color, point)
				}
			}
			sample := Surface_LoadingSample(ring, 11, 0)
			wantGlow := ambient == 0x092039ff && color != 0x999999ff && color&255 != 0
			if (sample.Glow&255 != 0) != wantGlow {
				t.Fatalf("wrong emitted light: ambient=%08x color=%08x sample=%+v", ambient, color, sample)
			}
			img := image.NewRGBA(image.Rect(0, 0, 40, 40))
			renderLoadingRing(img, 20, 20, ring)
			if (img.RGBAAt(31, 20).A != 0) != wantGlow {
				t.Fatal("renderer must blend glow beyond the nominal arc")
			}
		}
	}
}

func TestLoadingHighlightIsNotClippedAtCardinalAngles(t *testing.T) {
	ring := Surface_LoadingRing(40, 40, 18, 0, 0x006cffff, 0x092039ff)
	ring.StartAngle, ring.EndAngle = 45, 360
	img := image.NewRGBA(image.Rect(0, 0, 40, 40))
	renderLoadingRing(img, 20, 20, ring)
	if img.RGBAAt(30, 20).A == 0 {
		t.Fatal("the specular cap was clipped to the nominal ring radius")
	}
}

func TestLoadingButtonIsBusyRatherThanDisabled(t *testing.T) {
	r := New(AppConfig{}).(*runtime)
	r.SetTheme(ThemeDefaultLight())
	r.BeginFrame()
	r.QueueTap(20, 20)
	if r.buttonAt(ButtonProps{Bounds: Rectangle{X: 10, Y: 10, Width: 100, Height: 40},
		ID: 10, Tone: ButtonToneAccent, Loading: true}) {
		t.Fatal("loading button activated")
	}
	ops := r.FrameOps()
	op := ops[len(ops)-1]
	if !op.Button.Props.Loading || op.Disabled || op.Focused {
		t.Fatalf("busy state was conflated with disabled or focused: %+v", op)
	}
	r.EndFrame()
}
