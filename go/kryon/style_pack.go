package kryon

type StylePack struct {
	ID          string
	Label       string
	Description string
	Sheet       []StyleRule
}

type stylePackSource struct {
	source  string
	variant string
}

var stylePacks []StylePack
var stylePackSources map[string]stylePackSource
var activeStylePack = -1
var stylePackVersion uint64
var activeStyleTheme string

type styleCacheKey struct {
	Facts StyleFacts
	State int32
}

type styleCacheEntry struct {
	key     styleCacheKey
	version uint64
	valid   bool
	value   StyleData
}

var activeStyleCache [256]styleCacheEntry

func styleCacheSlot(facts StyleFacts, state int32) uint32 {
	values := [...]int32{facts.Kind, facts.Name, facts.ClassName, facts.Role,
		facts.Tone, facts.Emphasis, facts.Size, facts.State, facts.Validation,
		facts.Orientation, facts.Placement, state}
	hash := uint32(2166136261)
	for _, value := range values {
		hash = (hash ^ uint32(value)) * 16777619
	}
	return hash % uint32(len(activeStyleCache))
}

func RegisterStylePack(pack StylePack) bool {
	if pack.ID == "" || pack.Sheet == nil {
		return false
	}
	delete(stylePackSources, pack.ID)
	for i := range stylePacks {
		if stylePacks[i].ID == pack.ID {
			stylePacks[i] = pack
			stylePackVersion++
			return true
		}
	}
	stylePacks = append(stylePacks, pack)
	if activeStylePack < 0 {
		activeStylePack = len(stylePacks) - 1
	}
	stylePackVersion++
	return true
}

// RegisterStylePackSource retains source for theme re-resolution. Parsing and
// overlay decisions are owned by the generated KSS module.
func RegisterStylePackSource(source, label, description string) bool {
	id, rules, err := parseStyleEnvironment(source, nil, "", activeStyleTheme)
	if err != nil || id == "" || len(rules) == 0 {
		return false
	}
	if label == "" {
		label = id
	}
	packs := []StylePack{{
		ID: id, Label: label, Description: description, Sheet: rules,
	}}
	sources := []stylePackSource{{source: source}}
	for _, variant := range ParseStyleVariants(source) {
		if variant.Name == "" {
			continue
		}
		_, variantRules, variantErr := parseStyleEnvironment(source, nil, variant.Name, activeStyleTheme)
		if variantErr != nil || len(variantRules) == 0 {
			return false
		}
		packs = append(packs, StylePack{
			ID: id + "." + variant.Name, Label: variant.Label, Description: description,
			Sheet: variantRules,
		})
		sources = append(sources, stylePackSource{source: source, variant: variant.Name})
	}
	if stylePackSources == nil {
		stylePackSources = make(map[string]stylePackSource)
	}
	for i, pack := range packs {
		RegisterStylePack(pack)
		stylePackSources[pack.ID] = sources[i]
	}
	return true
}

// SetStyleTheme re-resolves source packs and their declared variants without
// changing selection. Typed sheets and explicit color substitutions stay as
// registered. A failed parse leaves all packs and the active theme unchanged.
func SetStyleTheme(theme string) bool {
	if theme == activeStyleTheme {
		return true
	}
	packs := append([]StylePack(nil), stylePacks...)
	for i := range packs {
		pack := &packs[i]
		stored, ok := stylePackSources[pack.ID]
		if !ok {
			continue
		}
		_, rules, err := parseStyleEnvironment(stored.source, nil, stored.variant, theme)
		if err != nil || len(rules) == 0 {
			return false
		}
		pack.Sheet = rules
	}
	stylePacks = packs
	activeStyleTheme = theme
	stylePackVersion++
	return true
}

func ClearStylePacks() {
	activeStyleTheme = ""
	stylePacks = nil
	stylePackSources = nil
	activeStylePack = -1
	stylePackVersion++
}

func EnsureBuiltInStylePacks() bool {
	if len(stylePacks) == 0 {
		return RegisterBuiltInStylePacks()
	}
	if activeStylePack < 0 {
		return SetActiveStylePack("material")
	}
	return true
}

func GetStylePackCount() int {
	return len(stylePacks)
}

func GetStylePackAt(index int) *StylePack {
	if index < 0 || index >= len(stylePacks) {
		return nil
	}
	return &stylePacks[index]
}

func FindStylePack(id string) *StylePack {
	for i := range stylePacks {
		if stylePacks[i].ID == id {
			return &stylePacks[i]
		}
	}
	return nil
}

func SetActiveStylePack(id string) bool {
	for i := range stylePacks {
		if stylePacks[i].ID == id {
			if activeStylePack != i {
				activeStylePack = i
				stylePackVersion++
			}
			return true
		}
	}
	return false
}

func GetActiveStylePack() *StylePack {
	if activeStylePack < 0 || activeStylePack >= len(stylePacks) {
		return nil
	}
	return &stylePacks[activeStylePack]
}

func GetActiveStylePackID() string {
	pack := GetActiveStylePack()
	if pack == nil {
		return ""
	}
	return pack.ID
}

func GetStylePackOptions() []StylePackOption {
	options := make([]StylePackOption, len(stylePacks))
	for i, pack := range stylePacks {
		options[i] = StylePackOption{
			ID:          pack.ID,
			Label:       pack.Label,
			Description: pack.Description,
			Active:      i == activeStylePack,
		}
	}
	return options
}

func StylePackVersion() uint64 {
	return stylePackVersion
}

func StyleClassID(className string) int32 {
	if className == "" {
		return 0
	}
	var hash uint32 = 2166136261
	for i := 0; i < len(className); i++ {
		hash ^= uint32(className[i])
		hash *= 16777619
	}
	hash &= 0x7fffffff
	if hash == 0 {
		return 1
	}
	return int32(hash)
}

func ResolveStyle(sheet []StyleRule, base StyleData, facts StyleFacts, activeState int32) StyleData {
	if len(sheet) == 0 {
		return base
	}
	cascade := StyleSheet_BeginStyleCascade(base)
	for _, rule := range sheet {
		if StyleSheet_StyleRuleMatches(rule, facts, activeState) {
			cascade = StyleSheet_ApplyStyleRule(cascade, rule, facts, activeState)
		}
	}
	return StyleSheet_FinishStyleCascade(cascade)
}

func ResolveActiveStyle(base StyleData, facts StyleFacts, activeState int32) StyleData {
	pack := GetActiveStylePack()
	if pack == nil {
		return base
	}
	key := styleCacheKey{Facts: facts, State: activeState}
	entry := &activeStyleCache[styleCacheSlot(facts, activeState)]
	if !entry.valid || entry.version != stylePackVersion || entry.key != key {
		*entry = styleCacheEntry{key: key, version: stylePackVersion, valid: true,
			value: ResolveStyle(pack.Sheet, StyleData{}, facts, activeState)}
	}
	return Style_MergeValues(base, entry.value)
}

func (r *runtime) StylePicker(props StylePickerProps) bool {
	count := len(stylePacks)
	if count == 0 && EnsureBuiltInStylePacks() {
		count = len(stylePacks)
	}
	if count == 0 {
		return false
	}

	labels := make([]string, count)
	selected := activeStylePack
	for i, pack := range stylePacks {
		labels[i] = pack.Label
		if labels[i] == "" {
			labels[i] = pack.ID
		}
	}
	if selected < 0 || selected >= count {
		selected = 0
	}

	value := int32(selected)
	changed := r.Dropdown(DropdownProps{
		Bounds:        props.Bounds,
		ID:            props.ID,
		ClassName:     props.ClassName,
		Options:       labels,
		OptionCount:   int32(count),
		SelectedIndex: &value,
		Disabled:      props.Disabled,
	})
	if changed && value >= 0 && int(value) < count {
		return SetActiveStylePack(stylePacks[value].ID)
	}
	return false
}

// StyleColorToken replaces a named color while retaining the source's layout and materials.
type StyleColorToken struct {
	Name  string
	Color uint32
}

func RegisterStylePackVariant(id, source, label string, colors []StyleColorToken) bool {
	if id == "" || len(id) >= 64 {
		return false
	}
	base, rules, err := parseStyleVariant(source, colors, "")
	if err != nil || base == "" || len(rules) == 0 {
		return false
	}
	if label == "" {
		label = id
	}
	return RegisterStylePack(StylePack{ID: id, Label: label, Sheet: rules})
}
