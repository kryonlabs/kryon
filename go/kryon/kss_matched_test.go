package kryon

import (
	"os"
	"reflect"
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
	if len(lines) != 53 {
		t.Fatalf("expected 53 cases, got %d", len(lines))
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
		} else if fields[0] == "G" {
			count, countErr := strconv.Atoi(fields[2])
			matching, matchingErr := strconv.Atoi(fields[3])
			if countErr != nil || matchingErr != nil {
				t.Fatalf("invalid group %q", line)
			}
			actual = KssParser_KssSelectorGroupMatches(fields[1], int32(count), int32(matching))
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

func TestKssSelectorFacts(t *testing.T) {
	source := kssFixtureText(t, "../../tests/fixtures/kss/selector-facts.tsv")
	lines := strings.Split(strings.TrimSuffix(source, "\n"), "\n")
	if len(lines) != 85 {
		t.Fatal(len(lines))
	}
	for _, line := range lines {
		fields := strings.Split(line, "\t")
		if len(fields) != 4 {
			t.Fatal(line)
		}
		var state KssStateFacts
		var structural KssStructuralFacts
		var identity KssIdentityFacts
		record := reflect.ValueOf(&state).Elem()
		if fields[0] == "R" || fields[0] == "P" {
			record = reflect.ValueOf(&structural).Elem()
		}
		if fields[0] == "I" {
			record = reflect.ValueOf(&identity).Elem()
		}
		if fields[2] != "-" {
			for _, entry := range strings.Split(fields[2], ",") {
				parts := strings.SplitN(entry, "=", 2)
				if len(parts) != 2 {
					t.Fatal(entry)
				}
				words := strings.Split(parts[0], "_")
				for i, word := range words {
					words[i] = strings.ToUpper(word[:1]) + word[1:]
					if word == "id" {
						words[i] = "ID"
					}
				}
				field := record.FieldByName(strings.Join(words, ""))
				if !field.IsValid() {
					t.Fatal(parts[0])
				}
				switch field.Kind() {
				case reflect.Bool:
					field.SetBool(parts[1] == "1")
				case reflect.String:
					field.SetString(parts[1])
				case reflect.Int32:
					value, err := strconv.ParseInt(parts[1], 10, 32)
					if err != nil {
						t.Fatal(err)
					}
					field.SetInt(value)
				default:
					t.Fatal(field.Kind())
				}
			}
		}
		actual := int32(0)
		if fields[0] == "S" {
			if KssParser_KssStateMatches(fields[1], state) {
				actual = 1
			}
		} else if fields[0] == "I" {
			if KssParser_KssIdentityMatches(fields[1], identity) {
				actual = 1
			}
		} else if fields[0] == "P" {
			name, argument, functional := strings.Cut(fields[1], "|")
			actual = KssParser_KssPseudoMatch(name, argument, functional, structural)
		} else {
			actual = KssParser_KssStructuralMatch(fields[1], structural)
		}
		expected, err := strconv.Atoi(fields[3])
		if err != nil || actual != int32(expected) {
			t.Fatalf("%s: got %d", line, actual)
		}
	}
}

func TestKssSelectorGrammar(t *testing.T) {
	host := active().(*runtime)
	lines := strings.Split(strings.TrimSuffix(kssFixtureText(t, "../../tests/fixtures/kss/selector-grammar.tsv"), "\n"), "\n")
	if len(lines) != 42 {
		t.Fatal(len(lines))
	}
	for _, line := range lines {
		fields := strings.Split(line, "\t")
		if len(fields) != 9 {
			t.Fatal(line)
		}
		source := fields[1]
		parser := KssParser_KssBeginSelector(source)
		cursor := KssCursor{Source: source}
		var last KssSelectorAtom
		var lastPart KssSelectorSpan
		valid, count := false, 0
		for steps := 0; steps <= len(source)+1; steps++ {
			if fields[0] == "A" {
				atom := host.KssParser_KssSelectorNext(parser)
				if !atom.Ok {
					break
				}
				if atom.Done {
					valid = true
					break
				}
				if atom.Parser.Cursor.Pos <= parser.Cursor.Pos {
					t.Fatal("selector did not advance", line)
				}
				parser, last = atom.Parser, atom
			} else {
				part := KssParser_KssSelectorPart(cursor, fields[0] == "S")
				if fields[0] == "R" {
					part = KssParser_KssRelativeSelectorPart(cursor)
				}
				if !part.Ok {
					break
				}
				if part.Done {
					valid = true
					break
				}
				if part.Parser.Pos <= cursor.Pos {
					t.Fatal("part did not advance", line)
				}
				cursor, lastPart = part.Parser, part
			}
			count++
		}
		if valid != (fields[2] == "1") {
			t.Fatalf("validity %q: %v", line, valid)
		}
		if !valid {
			continue
		}
		expectedCount, _ := strconv.Atoi(fields[3])
		expectedScore, _ := strconv.Atoi(fields[4])
		if count != expectedCount || parser.Specificity != int32(expectedScore) {
			t.Fatalf("counts %q: %d/%d", line, count, parser.Specificity)
		}
		actual := [4]string{}
		if fields[0] == "A" {
			actual = [4]string{last.Name, last.Value, last.Argument, last.Operation}
			if last.Canonical.Length > 0 {
				actual[0] = kssNameText(last.Canonical)
			}
		} else {
			actual[0] = source[lastPart.Start : lastPart.Start+lastPart.Length]
			if lastPart.Combinator == 32 {
				actual[1] = "space"
			} else if lastPart.Combinator != 0 {
				actual[1] = string(rune(lastPart.Combinator))
			}
		}
		for i, value := range actual {
			expected := fields[i+5]
			if expected == "-" {
				expected = ""
			}
			if value != expected {
				t.Fatalf("value %q field %d: %q, want %q", line, i, value, expected)
			}
		}
	}
}

func TestKssSelectorChains(t *testing.T) {
	lines := strings.Split(strings.TrimSuffix(kssFixtureText(t, "../../tests/fixtures/kss/selector-chains.tsv"), "\n"), "\n")
	if len(lines) != 27 {
		t.Fatal(len(lines))
	}
	for _, line := range lines {
		fields := strings.Split(line, "\t")
		if len(fields) != 5 && len(fields) != 6 {
			t.Fatal(line)
		}
		number := func(value string) int32 {
			parsed, err := strconv.ParseInt(value, 10, 32)
			if err != nil {
				t.Fatal(err)
			}
			return int32(parsed)
		}
		nodes := [][3]int32{}
		for _, text := range strings.Split(fields[2], ";") {
			values := strings.Split(text, ",")
			if len(values) != 3 {
				t.Fatal(text)
			}
			nodes = append(nodes, [3]int32{number(values[0]), number(values[1]), number(values[2])})
		}
		stack := []KssSelectorChainFrame{KssParser_KssSelectorChainBegin(int32(len(fields[1])), number(fields[3]))}
		if len(fields) == 6 {
			stack[0] = KssParser_KssRelativeSelectorBegin(int32(len(fields[1])), number(fields[3]), number(fields[5]))
		}
		actual := false
		for steps := 0; len(stack) > 0 && steps < 512; steps++ {
			frame := stack[len(stack)-1]
			valid := frame.Cursor >= 0 && int(frame.Cursor) < len(nodes) && frame.Part >= 0
			matched := valid && !frame.Entered && (nodes[frame.Cursor][2]&(1<<frame.Part)) != 0
			relation, parent, previous := int32(0), int32(-1), int32(-1)
			if frame.Part >= 0 {
				relation = int32(fields[1][frame.Part])
			}
			if relation == 'D' {
				relation = ' '
			}
			if relation == '0' {
				relation = 0
			}
			if valid {
				parent, previous = nodes[frame.Cursor][0], nodes[frame.Cursor][1]
			}
			result := KssParser_KssSelectorChainStep(frame, matched, relation, parent, previous)
			if result.Action == int32(KssSelectorChainActionKssSelectorAccept) {
				actual = true
				break
			}
			if result.Action == int32(KssSelectorChainActionKssSelectorPush) {
				stack[len(stack)-1] = result.Frame
				stack = append(stack, result.Next)
			} else {
				if result.Action != int32(KssSelectorChainActionKssSelectorPop) {
					t.Fatal(result.Action)
				}
				stack = stack[:len(stack)-1]
			}
		}
		if (!actual && len(stack) > 0) || actual != (fields[4] == "1") {
			t.Fatal(line, actual)
		}
	}
}

func TestKssNthFormulas(t *testing.T) {
	lines := strings.Split(strings.TrimSuffix(kssFixtureText(t, "../../tests/fixtures/kss/nth-formulas.tsv"), "\n"), "\n")
	if len(lines) != 30 {
		t.Fatal(len(lines))
	}
	for _, line := range lines {
		fields := strings.Split(line, "\t")
		if len(fields) != 5 {
			t.Fatal(line)
		}
		step, stepErr := strconv.ParseInt(fields[2], 10, 64)
		offset, offsetErr := strconv.ParseInt(fields[3], 10, 64)
		if stepErr != nil || offsetErr != nil {
			t.Fatal(line)
		}
		expectedText := fields[4]
		if expectedText == "-" {
			expectedText = ""
		}
		if text := kssNameText(KssParser_KssNthText(fields[0])); text != expectedText {
			t.Fatal(line, text)
		}
		formula := KssParser_KssParseNth(fields[0])
		if formula.Ok != (fields[1] == "1") || formula.Step != step || formula.Offset != offset {
			t.Fatal(line, formula)
		}
	}
}

func TestKssCSSProperties(t *testing.T) {
	lines := strings.Split(strings.TrimSuffix(kssFixtureText(t, "../../tests/fixtures/kss/css-properties.tsv"), "\n"), "\n")
	if len(lines) != 512 {
		t.Fatal(len(lines))
	}
	for _, line := range lines {
		fields := strings.Split(line, "\t")
		if len(fields) != 3 {
			t.Fatal(line)
		}
		value, expected := fields[1], fields[2]
		if value == "<empty>" {
			value = ""
		}
		if expected == "<empty>" {
			expected = ""
		}
		switch fields[0] {
		case "P":
			if actual := KssParser_KssCSSPropertyName(value); actual != expected {
				t.Fatal(line, actual)
			}
		case "U":
			if actual := KssParser_KssCSSNeedsPixels(value); actual != (expected == "1") {
				t.Fatal(line, actual)
			}
		case "B":
			if actual := KssParser_KssCSSBorderShorthand(value); actual != (expected == "1") {
				t.Fatal(line, actual)
			}
		default:
			t.Fatal(line)
		}
	}
}

func TestKssCSSExpansion(t *testing.T) {
	for fixture, name := range []string{"css-expansion.tsv", "css-effects.tsv"} {
		lines := strings.Split(strings.TrimSuffix(kssFixtureText(t, "../../tests/fixtures/kss/"+name), "\n"), "\n")
		totals := []int{26, 12}
		columns := []int{5, 8}
		if len(lines) != totals[fixture] {
			t.Fatal(name, len(lines))
		}
		for _, line := range lines {
			fields := strings.Split(line, "\t")
			if len(fields) != columns[fixture] {
				t.Fatal(line)
			}
			for i, value := range fields {
				if value == "<empty>" {
					fields[i] = ""
				}
			}
			if fixture == 0 {
				result := KssParser_KssCSSExpandDeclaration(fields[0], fields[1], fields[2] == "1")
				if result.First != fields[3] || result.Second != fields[4] {
					t.Fatal(line, result)
				}
			} else {
				facts := KssCSSEffectFacts{Background: fields[0], BackgroundEnd: fields[1], OffsetX: fields[2], OffsetY: fields[3], Transform: fields[4]}
				index, err := strconv.Atoi(fields[5])
				if err != nil {
					t.Fatal(line, err)
				}
				if KssParser_KssCSSHasOffsets(facts) != (fields[2] != "" || fields[3] != "") {
					t.Fatal(line)
				}
				result := KssParser_KssCSSEffectAt(facts, int32(index))
				text := result.Prefix + result.First + result.Separator + result.Second + result.Suffix
				if result.Name != fields[6] || text != fields[7] {
					t.Fatal(line, result)
				}
			}
		}
	}
}
