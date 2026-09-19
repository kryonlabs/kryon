package kryon

import "strings"

const (
	textLayoutCacheEntries = 64
	textLayoutCacheBytes   = 1 << 20
	textLayoutCacheInput   = 64 << 10
)

type textLayoutKey struct {
	text    string
	width   float32
	font    int32
	fontID  uint32
	spacing int32
}

type measuredTextLayout struct {
	lines  []string
	widths []float32
	bytes  int
}

type textLayoutCache struct {
	entries    map[textLayoutKey]measuredTextLayout
	order      []textLayoutKey
	oldest     int
	bytes      int
	generation uint64
}

func (cache *textLayoutCache) layout(key textLayoutKey, generation uint64, measure func(string) int) measuredTextLayout {
	if cache.generation != generation {
		*cache = textLayoutCache{generation: generation}
	}
	if result, ok := cache.entries[key]; ok {
		return result
	}
	result := measuredTextLayout{lines: layoutTextLines(key.text, key.width, measure)}
	result.widths = make([]float32, len(result.lines))
	result.bytes = len(key.text) + len(result.lines)*24
	for i, line := range result.lines {
		result.widths[i] = float32(measure(line))
		result.bytes += len(line)
	}
	if key.width != key.width || len(key.text) > textLayoutCacheInput || result.bytes > textLayoutCacheBytes {
		return result
	}
	if cache.entries == nil {
		cache.entries = make(map[textLayoutKey]measuredTextLayout)
	}
	// FIFO eviction bounds retained text and descriptors independently of how
	// many distinct paragraphs, widths, or style revisions an app produces.
	for len(cache.entries) >= textLayoutCacheEntries || cache.bytes+result.bytes > textLayoutCacheBytes {
		old := cache.order[cache.oldest]
		cache.bytes -= cache.entries[old].bytes
		delete(cache.entries, old)
		cache.order[cache.oldest] = textLayoutKey{}
		cache.oldest = (cache.oldest + 1) % textLayoutCacheEntries
	}
	// Borrowed substrings must not keep a caller's entire document alive.
	key.text = strings.Clone(key.text)
	for i, line := range result.lines {
		result.lines[i] = strings.Clone(line)
	}
	index := (cache.oldest + len(cache.entries)) % textLayoutCacheEntries
	if index == len(cache.order) {
		cache.order = append(cache.order, key)
	} else {
		cache.order[index] = key
	}
	cache.entries[key] = result
	cache.bytes += result.bytes
	return result
}
