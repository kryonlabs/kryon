package kryon

import (
	"fmt"
	"reflect"
	"strings"
	"testing"
)

func TestEditableRowsCacheMatchesMeasuredRows(t *testing.T) {
	var cache textRowsCache
	measure := func(text string, font int32) int { return graphemeCount(text) * int(font) / 16 }
	for _, text := range []string{"", "\n\r\n", "# Heading\nnormal", "Cafe\u0301 👩‍💻 क्ष 日本語", "long unbroken line"} {
		for _, width := range []int{0, 1, 5, 80} {
			key := textRowsKey{text: text, width: width, font: 16}
			want := measuredTextRows(text, width, 16, true, false, measure)
			got := cache.layout(key, 1, measure)
			hit := cache.layout(key, 1, func(string, int32) int {
				t.Fatal("cache hit measured text again")
				return 0
			})
			if !reflect.DeepEqual(want, got) || !reflect.DeepEqual(got, hit) {
				t.Fatalf("layout differs for %q at width %d", text, width)
			}
		}
	}
}

func TestEditableRowsCacheInvalidationAndBudget(t *testing.T) {
	var cache textRowsCache
	key := textRowsKey{text: "unchanged", width: 80, font: 16}
	measure := func(text string, font int32) int { return len(text) }
	cache.layout(key, 1, measure)
	for field := 0; field < 4; field++ {
		changed := key
		switch field {
		case 0:
			changed.text = "different"
		case 1:
			changed.width++
		case 2:
			changed.font++
		case 3:
			changed.fontID++
		}
		calls := 0
		cache.layout(changed, 1, func(text string, font int32) int {
			calls++
			return len(text)
		})
		if calls == 0 {
			t.Fatalf("field %d did not invalidate layout", field)
		}
	}
	cache.layout(key, 2, measure)
	if len(cache.entries) != 1 {
		t.Fatal("font replacement retained stale rows")
	}
	for i := 0; i < 100; i++ {
		key.text = fmt.Sprintf("%d\n%s", i, strings.Repeat("line\n", 2000))
		cache.layout(key, 2, measure)
		if len(cache.entries) > textRowsCacheEntries || cache.bytes > textRowsCacheBytes {
			t.Fatal("cache exceeded its budget")
		}
		total := 0
		for _, entry := range cache.entries {
			total += entry.bytes
		}
		if total != cache.bytes {
			t.Fatal("cache byte accounting drifted")
		}
	}
	count, bytes := len(cache.entries), cache.bytes
	key.text = strings.Repeat("x", textRowsCacheInput+1)
	key.width = 0
	cache.layout(key, 2, measure)
	if len(cache.entries) != count || cache.bytes != bytes {
		t.Fatal("oversize document displaced bounded entries")
	}
}
