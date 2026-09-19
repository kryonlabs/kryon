package kryon

import (
	"strings"
	"sync"
)

const (
	textRowsCacheEntries = 32
	textRowsCacheBytes   = 4 << 20
	textRowsCacheInput   = 256 << 10
)

type textRowsKey struct {
	text   string
	width  int
	font   int32
	fontID uint32
}

type textRowsEntry struct {
	key   textRowsKey
	rows  []textAreaRenderLine
	bytes int
}

// RenderFrame also works without a runtime instance. Its host cache therefore
// owns immutable snapshots and a mutex, shared by painting and pointer hit tests.
// It stores row data only; cursor, selection, scrolling and colors are dynamic.
type textRowsCache struct {
	mu         sync.Mutex
	entries    []textRowsEntry
	bytes      int
	generation uint64
}

var editableRows textRowsCache

func (cache *textRowsCache) layout(key textRowsKey, generation uint64, measure func(string, int32) int) []textAreaRenderLine {
	cache.mu.Lock()
	defer cache.mu.Unlock()
	if cache.generation != generation {
		cache.entries = nil
		cache.bytes = 0
		cache.generation = generation
	}
	for _, entry := range cache.entries {
		if entry.key == key {
			return entry.rows
		}
	}
	rows := measuredTextRows(key.text, key.width, key.font, true, false, measure)
	// Include descriptors and a conservative fixed per-entry overhead.
	bytes := len(key.text) + cap(rows)*64 + 128
	if len(key.text) > textRowsCacheInput || bytes > textRowsCacheBytes {
		return rows
	}
	for len(cache.entries) >= textRowsCacheEntries || cache.bytes+bytes > textRowsCacheBytes {
		cache.bytes -= cache.entries[0].bytes
		copy(cache.entries, cache.entries[1:])
		cache.entries[len(cache.entries)-1] = textRowsEntry{}
		cache.entries = cache.entries[:len(cache.entries)-1]
	}
	// Clone the source once, then rebase every row. A small substring must not
	// pin an arbitrarily large document owned by the caller.
	key.text = strings.Clone(key.text)
	for i := range rows {
		rows[i].text = key.text[rows[i].start:rows[i].end]
	}
	cache.entries = append(cache.entries, textRowsEntry{key: key, rows: rows, bytes: bytes})
	cache.bytes += bytes
	return rows
}
