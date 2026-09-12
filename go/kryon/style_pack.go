package kryon

type StylePack struct {
	ID          string
	Label       string
	Description string
	Sheet       []StyleRule
}

type StylePackOption struct {
	ID          string
	Label       string
	Description string
	Active      bool
}

type StylePickerProps struct {
	Bounds   Rectangle
	ID       int32
	Disabled bool
}

var stylePacks []StylePack
var activeStylePack = -1
var stylePackVersion uint64

func RegisterStylePack(pack StylePack) bool {
	if pack.ID == "" || pack.Sheet == nil {
		return false
	}
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

func RegisterStylePackSource(source, label, description string) bool {
	id, rules, err := ParseStyleSheet(source)
	if err != nil || id == "" || len(rules) == 0 {
		return false
	}
	if label == "" {
		label = id
	}
	copied := append([]StyleRule(nil), rules...)
	return RegisterStylePack(StylePack{
		ID:          id,
		Label:       label,
		Description: description,
		Sheet:       copied,
	})
}

func ClearStylePacks() {
	stylePacks = nil
	activeStylePack = -1
	stylePackVersion++
}

func EnsureBuiltInStylePacks() bool {
	if len(stylePacks) == 0 {
		return RegisterBuiltInStylePacks()
	}
	if activeStylePack < 0 {
		return SetActiveStylePack("kryon.material")
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

func ResolveStyle(sheet []StyleRule, base StyleData, facts StyleFacts, activeState int32) StyleData {
	if len(sheet) == 0 {
		return base
	}
	cascade := StyleSheet_BeginStyleCascade(base)
	for _, rule := range sheet {
		cascade = StyleSheet_ApplyStyleRule(cascade, rule, facts, activeState)
	}
	return StyleSheet_FinishStyleCascade(cascade)
}

func ResolveActiveStyle(base StyleData, facts StyleFacts, activeState int32) StyleData {
	pack := GetActiveStylePack()
	if pack == nil {
		return base
	}
	return ResolveStyle(pack.Sheet, base, facts, activeState)
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
