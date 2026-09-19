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

func TestStyleFieldDecisionTrace(t *testing.T) {
	facts := StyleSheet_StyleDefaultFacts(StyleSheet_StyleKindButton())
	cascade := StyleSheet_BeginStyleCascade(StyleData{})
	base := StyleRule{Selector: StyleSheet_StyleDefaultSelector(), Layer: 0, Order: 1,
		Style: StyleData{Fields: uint32(StyleBackground), Background: 0x112233ff}}
	base.Selector.Kind = StyleSheet_StyleKindButton()

	current := StyleSheet_StyleCascadeFieldPriority(cascade, StyleBackground)
	decision := StyleSheet_StyleRuleFieldDecision(current, base, facts, int32(ButtonStateNormal), StyleBackground)
	if !decision.Matched || !decision.FieldPresent || !decision.Wins || decision.CurrentPresent {
		t.Fatalf("initial decision = %+v", decision)
	}
	if decision.Layer != 0 || decision.Specificity != 1 || decision.Order != 1 {
		t.Fatalf("initial priority = %+v", decision)
	}

	cascade = StyleSheet_ApplyStyleRule(cascade, base, facts, int32(ButtonStateNormal))
	current = StyleSheet_StyleCascadeFieldPriority(cascade, StyleBackground)
	later := base
	later.Order = 0
	later.Style.Background = 0x445566ff
	decision = StyleSheet_StyleRuleFieldDecision(current, later, facts, int32(ButtonStateNormal), StyleBackground)
	if !decision.Matched || !decision.FieldPresent || decision.Wins || !decision.CurrentPresent || decision.CurrentOrder != 1 {
		t.Fatalf("losing decision = %+v", decision)
	}

	decision = StyleSheet_StyleRuleFieldDecision(current, later, facts, int32(ButtonStateNormal), StyleForeground)
	if !decision.Matched || decision.FieldPresent || decision.Wins {
		t.Fatalf("missing field decision = %+v", decision)
	}

	unmatched := later
	unmatched.Selector.Kind = StyleSheet_StyleKindText()
	decision = StyleSheet_StyleRuleFieldDecision(current, unmatched, facts, int32(ButtonStateNormal), StyleBackground)
	if decision.Matched || decision.FieldPresent || decision.Wins {
		t.Fatalf("unmatched decision = %+v", decision)
	}
}
