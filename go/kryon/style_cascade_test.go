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

func TestParsedStyleFieldTraceCarriesSourceSpans(t *testing.T) {
	ClearStyleModules()
	defer ClearStyleModules()
	if !RegisterStyleModule("base", "Button { background: #111111; }\n") {
		t.Fatal("register base module")
	}
	source := `@pack inspect;
@import <base>;
@layer components;
Button { background: #222222; }
Button.primary { background: #333333; }
Button { foreground: #eeeeee; }
@layer reset;
Button { background: #444444; }
`
	parsed, err := ParseStyleSheetTrace(source)
	if err != nil {
		t.Fatalf("parse trace: %v", err)
	}
	if parsed.ID != "inspect" {
		t.Fatalf("pack id = %q", parsed.ID)
	}
	if len(parsed.Rules) != 5 || len(parsed.Sources) != len(parsed.Rules) {
		t.Fatalf("rules/sources = %d/%d", len(parsed.Rules), len(parsed.Sources))
	}
	if len(parsed.Files) != 2 || parsed.Files[1] != "base" {
		t.Fatalf("files = %#v", parsed.Files)
	}
	if parsed.Sources[0].File != "base" || parsed.Sources[0].Origin.File != 1 || parsed.Sources[0].Origin.Line != 1 {
		t.Fatalf("imported source = %+v", parsed.Sources[0])
	}
	if parsed.Sources[2].Origin.File != 0 || parsed.Sources[2].Origin.Line != 5 || parsed.Sources[2].Span.SelectorLength <= 0 || parsed.Sources[2].Span.BodyLength <= 0 {
		t.Fatalf("main source span = %+v", parsed.Sources[2])
	}

	facts := StyleSheet_StyleDefaultFacts(StyleSheet_StyleKindButton())
	facts.ClassName = StyleClassID("primary")
	trace := TraceParsedStyleField(parsed, StyleData{}, facts, int32(ButtonStateNormal), uint32(StyleBackground))
	if trace.Result.Background != 0x333333ff {
		t.Fatalf("resolved background = %#x", trace.Result.Background)
	}
	if len(trace.Candidates) != len(parsed.Rules) {
		t.Fatalf("candidate count = %d", len(trace.Candidates))
	}
	if !trace.Candidates[0].Decision.Matched || !trace.Candidates[0].Decision.FieldPresent || !trace.Candidates[0].Decision.Wins || trace.Candidates[0].Source.File != "base" {
		t.Fatalf("imported candidate = %+v", trace.Candidates[0])
	}
	if !trace.Candidates[2].Decision.Wins || trace.Candidates[2].Decision.Specificity <= trace.Candidates[1].Decision.Specificity {
		t.Fatalf("class candidate did not win by specificity: %+v after %+v", trace.Candidates[2].Decision, trace.Candidates[1].Decision)
	}
	if !trace.Candidates[3].Decision.Matched || trace.Candidates[3].Decision.FieldPresent || trace.Candidates[3].Decision.Wins {
		t.Fatalf("missing-field candidate = %+v", trace.Candidates[3].Decision)
	}
	if !trace.Candidates[4].Decision.Matched || !trace.Candidates[4].Decision.FieldPresent || trace.Candidates[4].Decision.Wins || !trace.Candidates[4].Decision.CurrentPresent {
		t.Fatalf("losing reset-layer candidate = %+v", trace.Candidates[4].Decision)
	}
	if trace.Candidates[4].Decision.CurrentLayer != trace.Candidates[2].Decision.Layer || trace.Candidates[4].Source.Origin.Line != 8 {
		t.Fatalf("loser source/current priority = %+v source %+v", trace.Candidates[4].Decision, trace.Candidates[4].Source)
	}
}
