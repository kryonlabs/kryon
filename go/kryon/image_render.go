package kryon

import (
	"image"
	"image/color"
	"math"
)

func imageOperation(props ImageProps, tint Color) FrameOp {
	return FrameOp{Kind: FrameOpImage, Bounds: props.Bounds, Text: props.AssetPath,
		Color: tint, ImageSource: props.Source, ImageOrigin: props.Origin,
		Rotation: props.Rotation, ImageFit: props.Fit}
}

func renderAssetImage(target *image.RGBA, op FrameOp) {
	if op.Bounds.Width <= 0 || op.Bounds.Height <= 0 || op.Color.A == 0 {
		return
	}
	source := assetImages.load(op.Text)
	if source == nil {
		return
	}
	renderImagePixels(target, source, op)
}

// Decoding and sampling are host services; ImageFitBounds is shared .kry
// geometry. The target has already been restricted by Scroll/Canvas clipping.
func renderImagePixels(target *image.RGBA, pixels image.Image, op FrameOp) {
	source := op.ImageSource
	if source.Width == 0 || source.Height == 0 {
		source = NewRectangle(0, 0, float32(pixels.Bounds().Dx()), float32(pixels.Bounds().Dy()))
	}
	destination := Image_ImageFitBounds(op.Bounds, source,
		int32(pixels.Bounds().Dx()), int32(pixels.Bounds().Dy()), int32(op.ImageFit))
	if destination.Width <= 0 || destination.Height <= 0 {
		return
	}
	for _, value := range []float32{destination.X, destination.Y, destination.Width, destination.Height,
		source.X, source.Y, source.Width, source.Height, op.ImageOrigin.X, op.ImageOrigin.Y, op.Rotation} {
		if math.IsNaN(float64(value)) || math.IsInf(float64(value), 0) {
			return
		}
	}
	clip := image.Rect(int(math.Ceil(float64(op.Bounds.X))), int(math.Ceil(float64(op.Bounds.Y))),
		int(math.Ceil(float64(op.Bounds.X+op.Bounds.Width))), int(math.Ceil(float64(op.Bounds.Y+op.Bounds.Height))))
	clip = clip.Intersect(target.Bounds())
	sine, cosine := math.Sincos(float64(op.Rotation) * math.Pi / 180)
	width, height := math.Abs(float64(source.Width)), math.Abs(float64(source.Height))
	for y := clip.Min.Y; y < clip.Max.Y; y++ {
		for x := clip.Min.X; x < clip.Max.X; x++ {
			if !roundedContains(float32(x)+0.5, float32(y)+0.5, op.Bounds, op.Radius) {
				continue
			}
			// Invert the destination transform at the pixel center. Origin is
			// relative to the destination, in pixels, matching the C backend.
			dx := float64(x) + 0.5 - float64(destination.X)
			dy := float64(y) + 0.5 - float64(destination.Y)
			u := (cosine*dx + sine*dy + float64(op.ImageOrigin.X)) / float64(destination.Width)
			v := (-sine*dx + cosine*dy + float64(op.ImageOrigin.Y)) / float64(destination.Height)
			if u < 0 || u >= 1 || v < 0 || v >= 1 {
				continue
			}
			if source.Width < 0 {
				u = 1 - u
			}
			if source.Height < 0 {
				v = 1 - v
			}
			sx := pixels.Bounds().Min.X + int(math.Floor(float64(source.X)+u*width))
			sy := pixels.Bounds().Min.Y + int(math.Floor(float64(source.Y)+v*height))
			if !image.Pt(sx, sy).In(pixels.Bounds()) {
				continue
			}
			pixel := color.NRGBAModel.Convert(pixels.At(sx, sy)).(color.NRGBA)
			blendPixel(target, x, y, Color{
				R: uint8(uint32(pixel.R) * uint32(op.Color.R) / 255),
				G: uint8(uint32(pixel.G) * uint32(op.Color.G) / 255),
				B: uint8(uint32(pixel.B) * uint32(op.Color.B) / 255),
				A: uint8(uint32(pixel.A) * uint32(op.Color.A) / 255),
			})
		}
	}
}
