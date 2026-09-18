package kryon

import (
	"fmt"
	"strings"
	"testing"
)

func TestSharedCascadeFixture(t *testing.T) {
	source := kssFixtureText(t, "../../tests/fixtures/kss/cascade.txt")
	lines := strings.Split(strings.TrimSpace(source), "\n")
	if len(lines) != 11 {
		t.Fatalf("expected 11 cases, got %d", len(lines))
	}
	for _, line := range lines {
		var kinds, attributes, classes, names, specificity, layer, order int32
		var score, currentLayer, currentSpecificity, currentOrder, present, wins int32
		count, err := fmt.Sscan(line, &kinds, &attributes, &classes, &names,
			&specificity, &layer, &order, &score, &currentLayer, &currentSpecificity, &currentOrder, &present, &wins)
		if err != nil || count != 13 {
			t.Fatalf("invalid fixture %q: %v", line, err)
		}
		if actual := StyleSheet_StyleSpecificity(kinds, attributes, classes, names); actual != specificity {
			t.Fatalf("specificity %q: got %d", line, actual)
		}
		if actual := StyleSheet_StylePriorityScore(layer, specificity, order); actual != score {
			t.Fatalf("score %q: got %d", line, actual)
		}
		priority := StylePriority{Present: true, Layer: layer, Specificity: specificity, Order: order}
		current := StylePriority{Present: present != 0, Layer: currentLayer, Specificity: currentSpecificity, Order: currentOrder}
		if actual := StyleSheet_StylePriorityWins(priority, current); actual != (wins != 0) {
			t.Fatalf("winner %q: got %v", line, actual)
		}
	}
}
