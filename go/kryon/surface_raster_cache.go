package kryon

import (
	"container/list"
	"image"
	"sync"
)

const surfaceRasterBudget = 32 << 20
const surfaceRasterEntryLimit = 8 << 20
const surfaceRasterCountLimit = 128

// Cache the output of shared material sampling, never its geometry policy.
// Keys include clipping, scale, fractional coordinates, colors and all layer
// parameters. Cached spans are immutable and preserve source-over rounding.
type surfaceRasterKey struct {
	command SurfaceDrawing
	pixels  image.Rectangle
}

type surfaceRasterSpan struct {
	x, y, offset, length int
	opaque               bool
}

type surfaceRaster struct {
	pixels []byte
	spans  []surfaceRasterSpan
}

func (r *surfaceRaster) bytes() int {
	return cap(r.pixels) + cap(r.spans)*48
}

type surfaceRasterEntry struct {
	key    surfaceRasterKey
	raster *surfaceRaster
}

type surfaceRasterCache struct {
	mu      sync.Mutex
	entries map[surfaceRasterKey]*list.Element
	order   list.List
	bytes   int
}

var surfaceRasters surfaceRasterCache

func (c *surfaceRasterCache) get(key surfaceRasterKey) *surfaceRaster {
	c.mu.Lock()
	defer c.mu.Unlock()
	if element := c.entries[key]; element != nil {
		c.order.MoveToFront(element)
		return element.Value.(surfaceRasterEntry).raster
	}
	return nil
}

func (c *surfaceRasterCache) put(key surfaceRasterKey, raster *surfaceRaster) {
	size := raster.bytes()
	if size > surfaceRasterEntryLimit {
		return
	}
	c.mu.Lock()
	defer c.mu.Unlock()
	if c.entries == nil {
		c.entries = make(map[surfaceRasterKey]*list.Element)
	}
	if c.entries[key] != nil {
		return
	}
	for c.bytes+size > surfaceRasterBudget || len(c.entries) >= surfaceRasterCountLimit {
		oldest := c.order.Back()
		entry := oldest.Value.(surfaceRasterEntry)
		delete(c.entries, entry.key)
		c.bytes -= entry.raster.bytes()
		c.order.Remove(oldest)
	}
	c.entries[key] = c.order.PushFront(surfaceRasterEntry{key, raster})
	c.bytes += size
}

func renderSurfaceDrawing(img *image.RGBA, command SurfaceDrawing) {
	if !command.Visible {
		return
	}
	pixels := clipRect(img, command.Area)
	if pixels.Empty() {
		return
	}
	if int64(pixels.Dx())*int64(pixels.Dy())*4 > surfaceRasterEntryLimit {
		renderSurfaceDrawingUncached(img, command)
		return
	}
	key := surfaceRasterKey{command, pixels}
	raster := surfaceRasters.get(key)
	if raster == nil {
		raster = sampleSurfaceRaster(command, pixels)
		surfaceRasters.put(key, raster)
	}
	for _, span := range raster.spans {
		source := raster.pixels[span.offset : span.offset+span.length*4]
		if span.opaque {
			destination := img.PixOffset(span.x, span.y)
			copy(img.Pix[destination:destination+len(source)], source)
			continue
		}
		for i := 0; i < span.length; i++ {
			at := i * 4
			blendPixel(img, span.x+i, span.y, Color{source[at], source[at+1], source[at+2], source[at+3]})
		}
	}
}

func sampleSurfaceRaster(command SurfaceDrawing, pixels image.Rectangle) *surfaceRaster {
	raster := &surfaceRaster{}
	layer, bounds := command.Layer, command.Bounds
	for y := pixels.Min.Y; y < pixels.Max.Y; y++ {
		shade := Surface_SampleColor(layer, (float32(y)+0.5-bounds.Y)/bounds.Height)
		for x := pixels.Min.X; x < pixels.Max.X; x++ {
			coverage := Surface_SampleCoverage(layer, float32(x)-bounds.X, float32(y)-bounds.Y, command.Scale)
			coverage *= Surface_SegmentCoverage(float32(x)-command.Surface.X,
				command.Segment.X-command.Surface.X, command.Segment.Width, command.Surface.Width)
			if coverage == 0 {
				continue
			}
			color := unpackRGBA(shade)
			if coverage != 1 {
				color = unpackRGBA(Surface_Opacity(shade, coverage))
			}
			if color.A == 0 {
				continue
			}
			last := len(raster.spans) - 1
			if last >= 0 && raster.spans[last].y == y && raster.spans[last].x+raster.spans[last].length == x && raster.spans[last].opaque == (color.A == 255) {
				raster.spans[last].length++
			} else {
				raster.spans = append(raster.spans, surfaceRasterSpan{x: x, y: y, offset: len(raster.pixels), length: 1, opaque: color.A == 255})
			}
			raster.pixels = append(raster.pixels, color.R, color.G, color.B, color.A)
		}
	}
	return raster
}
