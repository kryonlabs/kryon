package kryon

import (
	"os"
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
	env.Theme = KssThemeDark
	env.Contrast = KssContrastHigh
	env.Density = KssDensityComfortable
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
	if collected.origins[2].File != 0 || collected.origins[2].Line != 27 {
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
	if glow.rules[2].Style.LetterSpacing != 9 || glow.rules[2].Layer != 0 {
		t.Fatalf("variant rule mismatch: %+v", glow.rules[2])
	}
	if glow.rules[3].Style.Background != 0x00ff00ff ||
		glow.rules[3].Style.LetterSpacing != 2 {
		t.Fatalf("variant overlay mismatch: %+v", glow.rules[3].Style)
	}
	if glow.rules[5].Style.Background != 0x00000000 {
		t.Fatalf("variant tail rule mismatch: %+v", glow.rules[5].Style)
	}
	if glow.parser.VariantCount != 1 ||
		kssNameText(glow.parser.Variants[0].Name) != "glow" ||
		kssNameText(glow.parser.Variants[0].Label) != "Glow" {
		t.Fatalf("variant declaration mismatch: %d", glow.parser.VariantCount)
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
