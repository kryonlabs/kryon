package kryon

import (
	"strings"
	"testing"
)

func assembleKssFormat(source string, result KssFormatResult) string {
	var out strings.Builder
	for i := 0; i < int(result.Count); i++ {
		segment := result.Segments[i]
		switch segment.Kind {
		case 0:
			out.WriteString(source[segment.Start : segment.Start+segment.Length])
		case 1:
			out.WriteByte(segment.Atom)
		default:
			out.WriteByte(10)
			for space := int32(0); space < segment.Length; space++ {
				out.WriteByte(32)
			}
		}
	}
	return out.String()
}

func TestKssFormatterRoundTrip(t *testing.T) {
	host, ok := active().(*runtime)
	if !ok {
		host = New(AppConfig{}).(*runtime)
	}
	ugly := "@pack fmt.go;\n      tokens {\n   color { accent: #112233; }\n}\n\n\n" +
		"Button {\n    background: accent;\n  radius: 0;\n}\n" +
		"Button:pressed { background: #304050; }"
	first := host.KssFormatter_KssFormat(ugly)
	if !first.Ok {
		t.Fatalf("format failed: %s", string(first.Diagnostic[:first.DiagnosticLength]))
	}
	once := assembleKssFormat(ugly, first)
	second := host.KssFormatter_KssFormat(once)
	if !second.Ok {
		t.Fatalf("reformat failed: %s", string(second.Diagnostic[:second.DiagnosticLength]))
	}
	twice := assembleKssFormat(once, second)
	if once != twice {
		t.Fatalf("formatter not idempotent:\n%q\n%q", once, twice)
	}
	id, rules, err := ParseStyleSheet(ugly)
	if err != nil || id != "fmt.go" || len(rules) != 2 {
		t.Fatalf("original parse: %v %q %d", err, id, len(rules))
	}
	id2, rules2, err := ParseStyleSheet(once)
	if err != nil || id2 != id || len(rules2) != len(rules) {
		t.Fatalf("formatted parse: %v %q %d", err, id2, len(rules2))
	}
	for i := range rules {
		if rules[i].Style.Background != rules2[i].Style.Background ||
			rules[i].Style.Radius != rules2[i].Style.Radius ||
			rules[i].Style.Fields != rules2[i].Style.Fields {
			t.Fatalf("round-trip changed winners at %d", i)
		}
	}
}
