package kryon

// StyleFieldCandidateTrace records one rule's effect on one inspected field.
type StyleFieldCandidateTrace struct {
	RuleIndex int
	Rule      StyleRule
	Source    StyleRuleSource
	Decision  StyleFieldDecision
}

// StyleFieldTrace records the resolved value and every matched, missing-field,
// winning, and losing candidate considered for a single StyleData field.
type StyleFieldTrace struct {
	Field      uint32
	Result     StyleData
	Candidates []StyleFieldCandidateTrace
}

// TraceStyleField resolves the sheet while retaining per-rule source and
// winner/loser facts for inspector surfaces. The shared generated cascade still
// owns matching, specificity, priority, and field-presence semantics.
func TraceStyleField(sheet []StyleRule, sources []StyleRuleSource, base StyleData, facts StyleFacts, activeState int32, field uint32) StyleFieldTrace {
	cascade := StyleSheet_BeginStyleCascade(base)
	candidates := make([]StyleFieldCandidateTrace, 0, len(sheet))
	for i, rule := range sheet {
		current := StyleSheet_StyleCascadeFieldPriority(cascade, field)
		decision := StyleSheet_StyleRuleFieldDecision(current, rule, facts, activeState, field)
		var source StyleRuleSource
		if i >= 0 && i < len(sources) {
			source = sources[i]
		}
		candidates = append(candidates, StyleFieldCandidateTrace{
			RuleIndex: i,
			Rule:      rule,
			Source:    source,
			Decision:  decision,
		})
		cascade = StyleSheet_ApplyStyleRule(cascade, rule, facts, activeState)
	}
	return StyleFieldTrace{
		Field:      field,
		Result:     StyleSheet_FinishStyleCascade(cascade),
		Candidates: candidates,
	}
}

// TraceParsedStyleField is the convenience form for sheets parsed with
// ParseStyleSheetTrace or ParseStyleSheetVariantTrace.
func TraceParsedStyleField(sheet *ParsedStyleSheet, base StyleData, facts StyleFacts, activeState int32, field uint32) StyleFieldTrace {
	if sheet == nil {
		return StyleFieldTrace{Field: field, Result: base}
	}
	return TraceStyleField(sheet.Rules, sheet.Sources, base, facts, activeState, field)
}
