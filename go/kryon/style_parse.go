package kryon

import (
	"fmt"
	"strings"
)

// styleModules feeds '@import <id>' resolution for the shared KSS parser.
var styleModules = map[string]string{}

// RegisterStyleModule registers an importable KSS source by id.
func RegisterStyleModule(id, source string) bool {
	if id == "" || source == "" {
		return false
	}
	styleModules[id] = source
	return true
}

// ClearStyleModules removes every registered style module.
func ClearStyleModules() {
	styleModules = map[string]string{}
}

// GetStyleModuleCount reports how many style modules are registered.
func GetStyleModuleCount() int {
	return len(styleModules)
}

// StringSlice supplies the shared KSS parser's borrowed substring view.
func (r *runtime) StringSlice(source string, start int32, length int32) string {
	if start < 0 || length < 0 || start+length > int32(len(source)) {
		return ""
	}
	return source[start : start+length]
}

func kssNameText(name KssName) string {
	if name.Length < 0 || int(name.Length) > len(name.Bytes) {
		return ""
	}
	return string(name.Bytes[:name.Length])
}

func kssRunParser(source string, colors []StyleColorToken) (KssParser, []StyleRule, bool) {
	host, ok := active().(*runtime)
	if !ok {
		host = New(AppConfig{}).(*runtime)
	}
	parser := KssParser_KssBegin(source, "", KssParser_KssDefaultEnvironment())
	for _, color := range colors {
		parser = KssParser_KssAddColorOverride(parser, color.Name, color.Color)
	}
	var rules []StyleRule
	for parser.Status == KssStatusContinue || parser.Status == KssStatusRule {
		if parser.Status == KssStatusRule {
			rules = append(rules, parser.Rule)
			parser.Status = KssStatusContinue
			continue
		}
		parser = host.KssParser_KssStep(parser)
		if parser.Status == KssStatusNeedImport {
			name := kssNameText(parser.PendingImport)
			if module, ok := styleModules[name]; ok {
				parser = KssParser_KssProvideImport(parser, module, name)
			} else {
				parser = KssParser_KssFailImport(parser)
			}
		}
	}
	return parser, rules, parser.Status == KssStatusDone
}

// ParseStyleSheet parses KSS source through the shared runtime module.
// Diagnostics carry file, line, and column provenance.
func ParseStyleSheet(source string) (string, []StyleRule, error) {
	return parseStyleVariant(source, nil)
}

func parseStyleVariant(source string, colors []StyleColorToken) (string, []StyleRule, error) {
	parser, rules, ok := kssRunParser(source, colors)
	if !ok {
		text := strings.TrimSpace(string(parser.Diagnostic[:parser.DiagnosticLength]))
		if text == "" {
			text = "style sheet did not complete"
		}
		return "", nil, fmt.Errorf("kss: %s", text)
	}
	return kssNameText(parser.Pack), rules, nil
}