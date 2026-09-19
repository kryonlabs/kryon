package kryon

import (
	"image"
	"image/draw"
	_ "image/gif"
	_ "image/jpeg"
	_ "image/png"
	"os"
	"path/filepath"
	"sync"
	"time"
)

const (
	imageCacheEntries = 64
	imageCacheBytes   = 32 << 20
	imageDecodeBytes  = 64 << 20
)

type imageCacheEntry struct {
	path    string
	size    int64
	changed time.Time
	pixels  *image.NRGBA
}

type imageCache struct {
	mu      sync.Mutex
	entries []imageCacheEntry
	bytes   int
}

var assetImages imageCache

func (cache *imageCache) load(path string) image.Image {
	if path == "" {
		return nil
	}
	path, err := filepath.Abs(path)
	if err != nil {
		return nil
	}
	info, err := os.Stat(path)
	if err != nil || !info.Mode().IsRegular() || info.Size() > imageDecodeBytes {
		return nil
	}
	cache.mu.Lock()
	defer cache.mu.Unlock()
	for i, entry := range cache.entries {
		if entry.path != path {
			continue
		}
		if entry.size == info.Size() && entry.changed == info.ModTime() {
			return entry.pixels
		}
		cache.bytes -= len(entry.pixels.Pix)
		copy(cache.entries[i:], cache.entries[i+1:])
		cache.entries[len(cache.entries)-1] = imageCacheEntry{}
		cache.entries = cache.entries[:len(cache.entries)-1]
		break
	}
	file, err := os.Open(path)
	if err != nil {
		return nil
	}
	defer file.Close()
	config, _, err := image.DecodeConfig(file)
	if err != nil || config.Width <= 0 || config.Height <= 0 ||
		config.Width > imageDecodeBytes/4/config.Height {
		return nil
	}
	if _, err := file.Seek(0, 0); err != nil {
		return nil
	}
	decoded, _, err := image.Decode(file)
	if err != nil {
		return nil
	}
	pixels := image.NewNRGBA(image.Rect(0, 0, config.Width, config.Height))
	draw.Draw(pixels, pixels.Bounds(), decoded, decoded.Bounds().Min, draw.Src)
	bytes := len(pixels.Pix)
	if bytes > imageCacheBytes {
		return pixels
	}
	for len(cache.entries) >= imageCacheEntries || cache.bytes+bytes > imageCacheBytes {
		cache.bytes -= len(cache.entries[0].pixels.Pix)
		copy(cache.entries, cache.entries[1:])
		cache.entries[len(cache.entries)-1] = imageCacheEntry{}
		cache.entries = cache.entries[:len(cache.entries)-1]
	}
	cache.entries = append(cache.entries, imageCacheEntry{path: path, size: info.Size(), changed: info.ModTime(), pixels: pixels})
	cache.bytes += bytes
	return pixels
}
