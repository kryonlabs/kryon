package kryon

import (
	"os"
	"strconv"
	"strings"
	"testing"
)

// Matched-fixture and invalid-input coverage for the shared KSS module
// (runtime/kss_parser.kry); assertions mirror tests/kss_matched_test.c and
// tests/web_kss_strict_test.mjs.

func kssFixtureText(t *testing.T, path string) string {
	t.Helper()
	data, err := os.ReadFile(path)
	if err != nil {
		t.Fatalf("read fixture: %v", err)
	}
	return string(data)
}

type kssCollected struct {
	rules   []StyleRule
	origins []KssOrigin
	spans   []KssRuleSpan
	parser  KssParser
}

func kssCollectFixture(t *testing.T, source, module, variant string) kssCollected {
	t.Helper()
	host, ok := active().(*runtime)
	if !ok {
		t.Fatal("no concrete runtime host")
	}
	env := KssParser_KssDefaultEnvironment()
	env = KssParser_KssEnvironmentWithNames(env, "dark", "high", "comfortable", "mouse", "desktop", variant)
	parser := KssParser_KssBegin(source, "matched.kss", env)
	if variant != "" {
		parser = KssParser_KssSetVariant(parser, KssParser_KssMakeName(variant))
	}
	var collected kssCollected
	for {
		if parser.Status == KssStatusRule {
			collected.rules = append(collected.rules, parser.Rule)
			collected.origins = append(collected.origins, parser.Origin)
			collected.spans = append(collected.spans, parser.RuleSpan)
			parser.Status = KssStatusContinue
			continue
		}
		if parser.Status != KssStatusContinue {
			break
		}
		parser = host.KssParser_KssStep(parser)
		if parser.Status == KssStatusNeedImport {
			if name := kssNameText(parser.PendingImport); name == "matched-module" {
				parser = KssParser_KssProvideImport(parser, module, "matched_module.kss")
			} else {
				parser = KssParser_KssFailImport(parser)
			}
		}
	}
	if parser.Status != KssStatusDone {
		t.Fatalf("parse failed: %s", strings.TrimSpace(string(parser.Diagnostic[:parser.DiagnosticLength])))
	}
	collected.parser = parser
	return collected
}

func TestKssMatchedFixture(t *testing.T) {
	ClearStyleModules()
	source := kssFixtureText(t, "../../tests/fixtures/kss/matched.kss")
	module := kssFixtureText(t, "../../tests/fixtures/kss/matched_module.kss")
	collected := kssCollectFixture(t, source, module, "")

	if len(collected.rules) != 5 {
		t.Fatalf("rules = %d, want 5", len(collected.rules))
	}
	surface := collected.rules[0]
	if surface.Selector.Kind != StyleSheet_StyleKindSurface() || surface.Style.PaddingX != 7 {
		t.Fatalf("imported surface rule mismatch: %+v", surface)
	}
	if collected.spans[0].File != 1 {
		t.Fatalf("imported rule provenance file = %d, want 1", collected.spans[0].File)
	}
	contrast := collected.rules[1]
	if contrast.Style.BorderWidth != 2 || contrast.Layer != 0 {
		t.Fatalf("contrast(high) rule mismatch: %+v", contrast)
	}
	button := collected.rules[2]
	if button.Layer != 1 ||
		button.Style.Background != 0xffcc00ff ||
		button.Style.Foreground != 0xf0f0f0ff ||
		button.Style.Border != 0x445566ff ||
		button.Style.Radius != 0 ||
		button.Style.Fields&uint32(StyleRadius) == 0 ||
		button.Style.PaddingY != 80 ||
		button.Style.LetterSpacing != 2 ||
		button.Style.Material != MaterialFlat {
		t.Fatalf("button rule mismatch: %+v", button.Style)
	}
	if button.Style.Typeface != "semibold" {
		t.Fatalf("typeface = %q", button.Style.Typeface)
	}
	if collected.origins[2].File != 0 || collected.origins[2].Line != 26 {
		t.Fatalf("button origin = %+v, want file 0 line 23", collected.origins[2])
	}
	pressed := collected.rules[3]
	if pressed.State != int32(ButtonStatePressed) || pressed.Style.Background != 0x304050ff {
		t.Fatalf("pressed rule mismatch: %+v", pressed)
	}
	quiet := collected.rules[4]
	if quiet.Style.Background != 0x00000000 ||
		quiet.Style.Foreground != 0x000000ff ||
		quiet.Style.Border != 0xffffffff {
		t.Fatalf("named-color rule mismatch: %+v", quiet.Style)
	}

	glow := kssCollectFixture(t, source, module, "glow")
	if len(glow.rules) != 6 {
		t.Fatalf("glow rules = %d, want 6", len(glow.rules))
	}
	if glow.rules[2].Style.Background != 0x00ff00ff ||
		glow.rules[2].Style.LetterSpacing != 2 {
		t.Fatalf("variant overlay mismatch: %+v", glow.rules[2].Style)
	}
	if glow.rules[3].Style.LetterSpacing != 9 || glow.rules[3].Layer != 1 {
		t.Fatalf("variant rule mismatch: %+v", glow.rules[3])
	}
	if glow.rules[5].Style.Background != 0x00000000 {
		t.Fatalf("variant tail rule mismatch: %+v", glow.rules[5].Style)
	}
	if glow.parser.VariantCount != 2 ||
		kssNameText(glow.parser.Variants[0].Name) != "glow" ||
		kssNameText(glow.parser.Variants[0].Label) != "Glow" {
		t.Fatalf("variant declaration mismatch: %d", glow.parser.VariantCount)
	}

	/* Resolution parity anchor: identical winners to the C suite. */
	resolve := func(rules []StyleRule, className string, state int32) StyleData {
		facts := StyleSheet_StyleDefaultFacts(StyleSheet_StyleKindButton())
		if className != "" {
			facts.ClassName = StyleClassID(className)
		}
		return ResolveStyle(rules, StyleData{}, facts, state)
	}
	resBase := resolve(collected.rules, "", int32(ButtonStateNormal))
	if resBase.Background != 0xffcc00ff || resBase.Foreground != 0xf0f0f0ff ||
		resBase.Border != 0x445566ff || resBase.Radius != 0 ||
		resBase.Fields&uint32(StyleRadius) == 0 || resBase.PaddingY != 80 ||
		resBase.BorderWidth != 2 || resBase.LetterSpacing != 2 ||
		resBase.Material != MaterialFlat || resBase.Typeface != "semibold" {
		t.Fatalf("base resolution mismatch: %+v", resBase)
	}
	resPressed := resolve(collected.rules, "", int32(ButtonStatePressed))
	if resPressed.Background != 0x304050ff {
		t.Fatalf("pressed resolution mismatch: %+v", resPressed)
	}
	resQuiet := resolve(collected.rules, "quiet", int32(ButtonStateNormal))
	if resQuiet.Background != 0x00000000 || resQuiet.Foreground != 0x000000ff ||
		resQuiet.Border != 0xffffffff {
		t.Fatalf("quiet resolution mismatch: %+v", resQuiet)
	}
	resGlow := resolve(glow.rules, "", int32(ButtonStateNormal))
	if resGlow.LetterSpacing != 9 || resGlow.Background != 0x00ff00ff {
		t.Fatalf("glow resolution mismatch: %+v", resGlow)
	}
}

func TestKssInvalidInputSweep(t *testing.T) {
	source := kssFixtureText(t, "../../tests/fixtures/kss/matched.kss")
	for cut := 0; cut <= len(source); cut++ {
		_, _, err := ParseStyleSheet(source[:cut])
		_ = err
	}
	seed := uint32(0x5eed1234)
	corpus := []string{
		source,
		"@theme dark { accent: #ffcc00; } @env contrast(high) { Button { background: accent; } } tokens { color { a: #1; } }",
	}
	for _, body := range corpus {
		mutated := []byte(body)
		for iteration := 0; iteration < 3000; iteration++ {
			copy(mutated, body)
			for flip := 0; flip < 3; flip++ {
				seed = seed*1103515245 + 12345
				index := (seed >> 8) % uint32(len(body))
				seed = seed*1103515245 + 12345
				mutated[index] = byte(seed >> 16)
			}
			_, _, _ = ParseStyleSheet(string(mutated))
		}
	}
}

func TestKssEnvironmentNames(t *testing.T) {
	source := kssFixtureText(t, "../../tests/fixtures/kss/environments.txt")
	cases := strings.Split(strings.TrimSpace(source), "\n")
	if len(cases) != 7 {
		t.Fatal("missing environment cases")
	}
	for _, line := range cases {
		fields := strings.Fields(line)
		if len(fields) != 11 {
			t.Fatalf("invalid case %q", line)
		}
		for i := 0; i < 6; i++ {
			if fields[i] == "-" {
				fields[i] = ""
			}
		}
		env := KssParser_KssEnvironmentWithNames(KssParser_KssDefaultEnvironment(),
			fields[0], fields[1], fields[2], fields[3], fields[4], fields[5])
		for i, value := range []int32{env.Theme, env.Contrast, env.Density, env.Pointer, env.Platform} {
			expected, err := strconv.Atoi(fields[i+6])
			if err != nil || value != int32(expected) {
				t.Fatalf("case %q axis %d = %d", line, i, value)
			}
		}
		if kssNameText(env.Variant) != fields[5] {
			t.Fatalf("variant for %q", line)
		}
	}
}

func TestKssCSSValues(t *testing.T) {
	host := active().(*runtime)
	source := kssFixtureText(t, "../../tests/fixtures/kss/css-values.kss")
	env := KssParser_KssEnvironmentWithNames(KssParser_KssDefaultEnvironment(), "dark", "", "", "", "", "")
	parser := KssParser_KssBeginDeclarative(source, "css-values.kss", env)
	rules, colors, captions := 0, 0, 0
	for parser.Status == KssStatusContinue || parser.Status == KssStatusRule {
		if parser.Status != KssStatusRule {
			parser = host.KssParser_KssStep(parser)
			continue
		}
		rules++
		for i := int32(0); i < parser.DeclarationCount; i++ {
			entry := parser.Declarations[i]
			if entry.Rule != parser.Rule.Order {
				continue
			}
			name := source[entry.NameStart : entry.NameStart+entry.NameLength]
			text := source[entry.ValueStart : entry.ValueStart+entry.ValueLength]
			value := host.KssParser_KssResolveCSSValue(parser, name, text)
			if !value.Valid {
				t.Fatalf("invalid property %q", name)
			}
			switch name {
			case "background":
				want, origin := uint32(0x112233ff), int32(KssOriginKindKssOriginPack)
				if colors == 2 {
					want, origin = 0x445566ff, int32(KssOriginKindKssOriginTheme)
				}
				if value.Kind != int32(KssCSSValueKindKssCSSColor) || value.Color != want || value.TokenOrigin != origin {
					t.Fatalf("color %d: %+v", colors, value)
				}
				override := host.KssParser_KssOverrideCSSValue(value, "#abcdef", true)
				if override.Kind != int32(KssCSSValueKindKssCSSLiteral) || override.Text != "#abcdef" {
					t.Fatal(override)
				}
				colors++
			case "--caption":
				if value.Text != `"café; }"` {
					t.Fatal(value.Text)
				}
				captions++
			case "width":
				if value.Kind != int32(KssCSSValueKindKssCSSNumber) || value.Number != 150 {
					t.Fatal(value)
				}
			}
		}
		parser.Status = KssStatusContinue
	}
	if parser.Status != KssStatusDone || rules != 4 || colors != 3 || captions != 1 {
		t.Fatalf("status %v, rules %d, colors %d, captions %d", parser.Status, rules, colors, captions)
	}
	if host.KssParser_KssResolveCSSValue(parser, "unknown-property", "1").Valid {
		t.Fatal("unknown property accepted")
	}
	if host.KssParser_KssResolveCSSValue(parser, "width", "1e9999999999999999999999").Kind != int32(KssCSSValueKindKssCSSLiteral) {
		t.Fatal("overflowing exponent accepted")
	}
}

func TestKssSelectorPredicates(t *testing.T) {
	source := kssFixtureText(t, "../../tests/fixtures/kss/selector-predicates.tsv")
	lines := strings.Split(strings.TrimSuffix(source, "\n"), "\n")
	if len(lines) != 40 {
		t.Fatalf("expected 40 cases, got %d", len(lines))
	}
	for _, line := range lines {
		fields := strings.Split(line, "\t")
		if len(fields) != 5 {
			t.Fatalf("invalid fixture %q", line)
		}
		present := fields[1] != "<missing>"
		if !present {
			fields[1] = ""
		}
		for i := 1; i <= 2; i++ {
			if fields[i] == "<empty>" {
				fields[i] = ""
			}
		}
		var actual bool
		if fields[0] == "A" {
			actual = KssParser_KssAttributeMatches(fields[1], fields[2], fields[3], present)
		} else {
			position, err := strconv.ParseInt(fields[2], 10, 32)
			if err != nil || fields[0] != "N" {
				t.Fatalf("invalid position %q", line)
			}
			actual = KssParser_KssNthMatches(fields[1], int32(position))
		}
		if actual != (fields[4] == "1") {
			t.Fatalf("predicate %q returned %v", line, actual)
		}
	}
}
