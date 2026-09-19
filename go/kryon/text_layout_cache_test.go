package kryon

import (
	"fmt"
	"reflect"
	"strings"
	"testing"
	"unicode/utf8"

	"golang.org/x/image/font/gofont/gobold"
	"golang.org/x/image/font/gofont/goregular"
)

func TestTextLayoutCacheMatchesUncachedLayout(t *testing.T) {
	var cache textLayoutCache
	for _, text := range []string{"", "one two three", "\n\n", "first\r\nsecond", "cafe\u0301 \u65e5\u672c\u8a9e \U0001f469\u200d\U0001f4bb"} {
		for _, width := range []float32{1, 6, 80} {
			key := textLayoutKey{text: text, width: width}
			want := layoutTextLines(text, width, utf8.RuneCountInString)
			first := cache.layout(key, 1, utf8.RuneCountInString)
			again := cache.layout(key, 1, func(string) int {
				t.Fatal("cache hit measured text again")
				return 0
			})
			if !reflect.DeepEqual(first.lines, want) || !reflect.DeepEqual(first, again) {
				t.Fatalf("cached layout changed %q at width %v", text, width)
			}
			for i, line := range first.lines {
				if first.widths[i] != float32(utf8.RuneCountInString(line)) {
					t.Fatal("cached width does not match line")
				}
			}
		}
	}
}

func TestTextLayoutCacheInvalidationAndBounds(t *testing.T) {
	var cache textLayoutCache
	key := textLayoutKey{text: "one two three", width: 8, font: 16}
	measure := func(text string) int { return len(text) }
	cache.layout(key, 1, measure)
	for field := 0; field < 5; field++ {
		changed := key
		switch field {
		case 0:
			changed.text = "one SIX three"
		case 1:
			changed.width++
		case 2:
			changed.font++
		case 3:
			changed.fontID++
		case 4:
			changed.spacing++
		}
		calls := 0
		cache.layout(changed, 1, func(text string) int {
			calls++
			return len(text)
		})
		if calls == 0 {
			t.Fatalf("field %d did not invalidate layout", field)
		}
	}
	cache.layout(key, 2, measure)
	if len(cache.entries) != 1 {
		t.Fatal("font generation did not discard stale entries")
	}
	for i := 0; i < 300; i++ {
		key.text = fmt.Sprintf("%d %s", i, strings.Repeat("text ", 5000))
		cache.layout(key, 2, measure)
		if len(cache.entries) > textLayoutCacheEntries || cache.bytes > textLayoutCacheBytes {
			t.Fatal("cache exceeded its entry or byte budget")
		}
		bytes := 0
		for _, entry := range cache.entries {
			bytes += entry.bytes
		}
		if bytes != cache.bytes {
			t.Fatal("cache byte accounting drifted during eviction")
		}
	}
	before := len(cache.entries)
	key.text = strings.Repeat("x", textLayoutCacheInput+1)
	cache.layout(key, 2, measure)
	if len(cache.entries) != before {
		t.Fatal("oversize text displaced bounded cache entries")
	}
}

func TestTextLayoutCacheTracksFontReplacement(t *testing.T) {
	r := New(AppConfig{Width: 400, Height: 300}).(*runtime)
	fontMu.Lock()
	previous := activeTextFontName
	fontMu.Unlock()
	t.Cleanup(func() { useTextFont(previous) })
	const name = "layout-cache-test"
	id, ok := registerFontData(name, ".ttf", goregular.TTF)
	if !ok || !useTextFont(name) {
		t.Fatal("could not register test font")
	}
	props := TextProps{Text: "Words with different font metrics", Bounds: NewRectangle(0, 0, 160, 200)}
	draw := func() []FrameOp {
		r.BeginFrame()
		r.Text(props)
		r.EndFrame()
		return r.FrameOps()
	}
	first := draw()
	oldGeneration := r.textLayouts.generation
	newID, ok := registerFontData(name, ".ttf", gobold.TTF)
	if !ok || newID != id {
		t.Fatal("font replacement did not preserve its identity")
	}
	updated := draw()
	if oldGeneration == r.textLayouts.generation || reflect.DeepEqual(first, updated) {
		t.Fatal("font replacement reused stale layout")
	}
	r.textLayouts = textLayoutCache{}
	if !reflect.DeepEqual(updated, draw()) {
		t.Fatal("cached frame differs from freshly measured replacement font")
	}
}
