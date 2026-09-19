package kryon

import (
	"fmt"
	"strings"
)

// styleModules feeds '@import <id>' resolution for the shared KSS parser.
var styleModules = map[string]string{}
var styleParseInvocationCount int

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

func resetStyleParseInvocationCount() {
	styleParseInvocationCount = 0
}

func currentStyleParseInvocationCount() int {
	return styleParseInvocationCount
}

func kssNameText(name KssName) string {
	if name.Length < 0 || int(name.Length) > len(name.Bytes) {
		return ""
	}
	return string(name.Bytes[:name.Length])
}

func kssSourceFileName(file KssSourceFile) string {
	if file.Length < 0 || int(file.Length) > len(file.Name) {
		return ""
	}
	return string(file.Name[:file.Length])
}

func kssRunParser(source string, colors []StyleColorToken, variant string) (KssParser, []StyleRule, bool) {
	parser, rules, _, _, ok := kssRunParserTraceEnvironment(source, colors, variant, "")
	return parser, rules, ok
}

func kssRunParserEnvironment(source string, colors []StyleColorToken, variant, theme string) (KssParser, []StyleRule, bool) {
	parser, rules, _, _, ok := kssRunParserTraceEnvironment(source, colors, variant, theme)
	return parser, rules, ok
}

func kssRunParserTraceEnvironment(source string, colors []StyleColorToken, variant, theme string) (KssParser, []StyleRule, []KssOrigin, []KssRuleSpan, bool) {
	styleParseInvocationCount++
	host, ok := active().(*runtime)
	if !ok {
		host = New(AppConfig{}).(*runtime)
	}
	environment := KssParser_KssEnvironmentWithNames(KssParser_KssDefaultEnvironment(), theme, "", "", "", "", variant)
	parser := KssParser_KssBegin(source, "", environment)
	for _, color := range colors {
		parser = KssParser_KssAddColorOverride(parser, color.Name, color.Color)
	}
	var rules []StyleRule
	var origins []KssOrigin
	var spans []KssRuleSpan
	for parser.Status == KssStatusContinue || parser.Status == KssStatusRule {
		if parser.Status == KssStatusRule {
			rules = append(rules, parser.Rule)
			origins = append(origins, parser.Origin)
			spans = append(spans, parser.RuleSpan)
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
	return parser, rules, origins, spans, parser.Status == KssStatusDone
}

// ParseStyleSheet parses KSS source through the shared runtime module.
// Diagnostics carry file, line, and column provenance.
func ParseStyleSheet(source string) (string, []StyleRule, error) {
	return parseStyleVariant(source, nil, "")
}

// StyleVariantInfo names one '@variant name "Label"' declaration in a sheet.
type StyleVariantInfo struct {
	Name  string
	Label string
}

// StyleRuleSource records where a generated StyleRule came from.
type StyleRuleSource struct {
	Origin KssOrigin
	Span   KssRuleSpan
	File   string
}

// ParsedStyleSheet keeps parsed rules together with source/provenance facts for inspectors.
type ParsedStyleSheet struct {
	ID      string
	Rules   []StyleRule
	Sources []StyleRuleSource
	Files   []string
}

// ParseStyleSheetTrace parses KSS and retains source-span provenance for each rule.
func ParseStyleSheetTrace(source string) (*ParsedStyleSheet, error) {
	return parseStyleTraceEnvironment(source, nil, "", "")
}

// ParseStyleSheetVariantTrace parses KSS with a declared variant active and keeps rule provenance.
func ParseStyleSheetVariantTrace(source, variant string) (*ParsedStyleSheet, error) {
	return parseStyleTraceEnvironment(source, nil, variant, "")
}

func parseStyleTraceEnvironment(source string, colors []StyleColorToken, variant, theme string) (*ParsedStyleSheet, error) {
	parser, rules, origins, spans, ok := kssRunParserTraceEnvironment(source, colors, variant, theme)
	if !ok {
		text := strings.TrimSpace(string(parser.Diagnostic[:parser.DiagnosticLength]))
		if text == "" {
			text = "style sheet did not complete"
		}
		return nil, fmt.Errorf("kss: %s", text)
	}
	fileCount := int(parser.FileCount)
	if fileCount > len(parser.Files) {
		fileCount = len(parser.Files)
	}
	files := make([]string, fileCount)
	for i := 0; i < fileCount; i++ {
		files[i] = kssSourceFileName(parser.Files[i])
	}
	sources := make([]StyleRuleSource, len(rules))
	for i := range rules {
		file := ""
		fileIndex := int(spans[i].File)
		if fileIndex >= 0 && fileIndex < len(files) {
			file = files[fileIndex]
		}
		sources[i] = StyleRuleSource{Origin: origins[i], Span: spans[i], File: file}
	}
	return &ParsedStyleSheet{ID: kssNameText(parser.Pack), Rules: rules, Sources: sources, Files: files}, nil
}

// ParseStyleVariants returns every variant a sheet declares, with the labels
// hosts show in style pickers. Declarations are reported even when inactive.
func ParseStyleVariants(source string) []StyleVariantInfo {
	parser, _, ok := kssRunParser(source, nil, "")
	if !ok {
		return nil
	}
	count := int(parser.VariantCount)
	if count > len(parser.Variants) {
		count = len(parser.Variants)
	}
	variants := make([]StyleVariantInfo, 0, count)
	for i := 0; i < count; i++ {
		variants = append(variants, StyleVariantInfo{
			Name:  kssNameText(parser.Variants[i].Name),
			Label: kssNameText(parser.Variants[i].Label),
		})
	}
	return variants
}

// ParseStyleSheetVariant parses with a declared pack variant active, so
// '@variant' blocks named by the variant contribute rules and overlays.
func ParseStyleSheetVariant(source, variant string) (string, []StyleRule, error) {
	return parseStyleVariant(source, nil, variant)
}

func parseStyleVariant(source string, colors []StyleColorToken, variant string) (string, []StyleRule, error) {
	return parseStyleEnvironment(source, colors, variant, "")
}

func parseStyleEnvironment(source string, colors []StyleColorToken, variant, theme string) (string, []StyleRule, error) {
	parser, rules, ok := kssRunParserEnvironment(source, colors, variant, theme)
	if !ok {
		text := strings.TrimSpace(string(parser.Diagnostic[:parser.DiagnosticLength]))
		if text == "" {
			text = "style sheet did not complete"
		}
		return "", nil, fmt.Errorf("kss: %s", text)
	}
	return kssNameText(parser.Pack), rules, nil
}
