package kryon

import (
	"image"
	"image/color"
	"image/gif"
	"image/jpeg"
	"image/png"
	"os"
	"path/filepath"
	"testing"
	"time"
)

func imageTestPixels() *image.NRGBA {
	pixels := image.NewNRGBA(image.Rect(0, 0, 2, 1))
	pixels.SetNRGBA(0, 0, color.NRGBA{R: 255, A: 255})
	pixels.SetNRGBA(1, 0, color.NRGBA{B: 255, A: 255})
	return pixels
}

func writeTestImage(t *testing.T, path string, pixels image.Image) {
	t.Helper()
	file, err := os.Create(path)
	if err != nil {
		t.Fatal(err)
	}
	if err := png.Encode(file, pixels); err != nil {
		t.Fatal(err)
	}
	if err := file.Close(); err != nil {
		t.Fatal(err)
	}
}

func TestAssetImageFitCropFlipAndClip(t *testing.T) {
	path := filepath.Join(t.TempDir(), "tile.png")
	writeTestImage(t, path, imageTestPixels())
	for _, test := range []struct {
		name   string
		fit    ImageFit
		source Rectangle
		clip   Rectangle
		checks map[image.Point]Color
	}{
		{"stretch", ImageFitStretch, Rectangle{}, Rectangle{}, map[image.Point]Color{{1, 1}: {255, 0, 0, 255}, {4, 4}: {0, 0, 255, 255}}},
		{"contain", ImageFitContain, Rectangle{}, Rectangle{}, map[image.Point]Color{{1, 1}: RAYWHITE, {1, 2}: {255, 0, 0, 255}, {4, 3}: {0, 0, 255, 255}, {4, 4}: RAYWHITE}},
		{"cover", ImageFitCover, Rectangle{}, Rectangle{}, map[image.Point]Color{{1, 1}: {255, 0, 0, 255}, {4, 4}: {0, 0, 255, 255}}},
		{"crop", ImageFitStretch, NewRectangle(1, 0, 1, 1), Rectangle{}, map[image.Point]Color{{1, 1}: {0, 0, 255, 255}, {4, 4}: {0, 0, 255, 255}}},
		{"flip", ImageFitStretch, NewRectangle(0, 0, -2, 1), Rectangle{}, map[image.Point]Color{{1, 1}: {0, 0, 255, 255}, {4, 4}: {255, 0, 0, 255}}},
		{"clip", ImageFitStretch, Rectangle{}, NewRectangle(2, 2, 2, 2), map[image.Point]Color{{1, 1}: RAYWHITE, {2, 2}: {255, 0, 0, 255}, {3, 3}: {0, 0, 255, 255}, {4, 4}: RAYWHITE}},
	} {
		t.Run(test.name, func(t *testing.T) {
			op := imageOperation(ImageProps{AssetPath: path, Bounds: NewRectangle(1, 1, 4, 4), Source: test.source, Fit: test.fit}, WHITE)
			op.Clip, op.HasClip = test.clip, test.clip.Width > 0
			frame := RenderFrame(6, 6, []FrameOp{op})
			for point, want := range test.checks {
				if got := frame.RGBAAt(point.X, point.Y); got != rgba(want) {
					t.Fatalf("pixel %v = %v, want %v", point, got, want)
				}
			}
			if frame.RGBAAt(0, 2) != rgba(RAYWHITE) || frame.RGBAAt(5, 2) != rgba(RAYWHITE) {
				t.Fatal("image escaped widget bounds")
			}
		})
	}
}

func TestAssetImageTintAlphaRotationAndRadius(t *testing.T) {
	pixels := imageTestPixels()
	op := imageOperation(ImageProps{Bounds: NewRectangle(2, 2, 4, 4)}, Color{128, 255, 255, 128})
	frame := image.NewRGBA(image.Rect(0, 0, 8, 8))
	fillImage(frame, WHITE)
	renderImagePixels(frame, pixels, op)
	if got := frame.RGBAAt(2, 2); got != (color.RGBA{191, 127, 127, 255}) {
		t.Fatalf("straight-alpha tint = %v", got)
	}
	op.Color, op.Rotation, op.ImageOrigin = WHITE, 180, Vector2{X: 4, Y: 4}
	fillImage(frame, WHITE)
	renderImagePixels(frame, pixels, op)
	if frame.RGBAAt(2, 2) != (color.RGBA{B: 255, A: 255}) || frame.RGBAAt(5, 5) != (color.RGBA{R: 255, A: 255}) {
		t.Fatal("rotation/origin did not invert the destination")
	}
	op.Rotation, op.ImageOrigin, op.Radius = 0, Vector2{}, 2
	fillImage(frame, WHITE)
	renderImagePixels(frame, pixels, op)
	if frame.RGBAAt(2, 2) != rgba(WHITE) || frame.RGBAAt(3, 3) != (color.RGBA{R: 255, A: 255}) {
		t.Fatal("rounded clipping lost corner or center coverage")
	}
}

func TestImageOperationPreservesCanonicalProps(t *testing.T) {
	r := New(AppConfig{}).(*runtime)
	props := ImageProps{AssetPath: "image.png", Bounds: NewRectangle(1, 2, 30, 40),
		Source: NewRectangle(3, 4, 5, 6), Origin: Vector2{X: 7, Y: 8}, Rotation: 90, Fit: ImageFitCover}
	r.BeginFrame()
	r.Image(props)
	r.EndFrame()
	for _, op := range r.FrameOps() {
		if op.Kind == FrameOpImage {
			if op.ImageSource != props.Source || op.ImageOrigin != props.Origin || op.Rotation != props.Rotation || op.ImageFit != props.Fit {
				t.Fatalf("image props lost: %+v", op)
			}
			return
		}
	}
	t.Fatal("missing image operation")
}

func TestImageCacheFormatsReplacementAndBounds(t *testing.T) {
	var cache imageCache
	dir := t.TempDir()
	for _, format := range []string{"png", "jpeg", "gif"} {
		path := filepath.Join(dir, "tile."+format)
		file, err := os.Create(path)
		if err != nil {
			t.Fatal(err)
		}
		switch format {
		case "png":
			err = png.Encode(file, imageTestPixels())
		case "jpeg":
			err = jpeg.Encode(file, imageTestPixels(), nil)
		case "gif":
			err = gif.Encode(file, imageTestPixels(), nil)
		}
		file.Close()
		if err != nil {
			t.Fatal(err)
		}
		if first := cache.load(path); first == nil || first != cache.load(path) {
			t.Fatalf("%s not decoded and reused", format)
		}
	}
	path := filepath.Join(dir, "tile.png")
	first := cache.load(path)
	pixels := image.NewNRGBA(image.Rect(0, 0, 2, 1))
	pixels.SetNRGBA(0, 0, color.NRGBA{G: 255, A: 255})
	writeTestImage(t, path, pixels)
	stamp := time.Now().Add(time.Second)
	if err := os.Chtimes(path, stamp, stamp); err != nil {
		t.Fatal(err)
	}
	if second := cache.load(path); second == first || second.At(0, 0) != pixels.At(0, 0) {
		t.Fatal("changed image was not reloaded")
	}
	if cache.load(filepath.Join(dir, "missing.png")) != nil {
		t.Fatal("missing asset unexpectedly decoded")
	}
	for i := 0; i < imageCacheEntries+5; i++ {
		path := filepath.Join(dir, time.Unix(int64(i), 0).Format("150405")+".png")
		writeTestImage(t, path, pixels)
		cache.load(path)
		if len(cache.entries) > imageCacheEntries || cache.bytes > imageCacheBytes {
			t.Fatal("image cache exceeded its budget")
		}
	}
}
