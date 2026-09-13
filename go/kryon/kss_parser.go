package kryon

import (
	"fmt"
	"strconv"
	"strings"
	"unicode"
)

type styleParser struct {
	source string
	pos    int
	layer  int32
	tokens styleTokens
}

type styleTokens struct {
	colors    map[string]uint32
	lengths   map[string]float32
	materials map[string]int32
}

func ParseStyleSheet(source string) (string, []StyleRule, error) {
	p := styleParser{source: source}
	var pack string
	var rules []StyleRule

	for {
		p.skip()
		if p.done() {
			return pack, rules, nil
		}
		if p.peek() == '@' {
			name, err := p.directive()
			if err != nil {
				return "", nil, err
			}
			if name != "" {
				pack = name
			}
			continue
		}
		if name, ok := p.ident(); ok && strings.EqualFold(name, "tokens") {
			if err := p.tokensBlock(); err != nil {
				return "", nil, err
			}
			continue
		} else if ok {
			p.pos -= len(name)
		}
		rule, err := p.rule(len(rules))
		if err != nil {
			return "", nil, err
		}
		rules = append(rules, rule)
	}
}

func (p *styleParser) done() bool { return p.pos >= len(p.source) }
func (p *styleParser) peek() byte { return p.source[p.pos] }

func (p *styleParser) skip() {
	for !p.done() {
		r := rune(p.source[p.pos])
		if unicode.IsSpace(r) {
			p.pos++
			continue
		}
		if strings.HasPrefix(p.source[p.pos:], "//") {
			p.pos += 2
			for !p.done() && p.peek() != '\n' {
				p.pos++
			}
			continue
		}
		if strings.HasPrefix(p.source[p.pos:], "/*") {
			p.pos += 2
			for !p.done() && !strings.HasPrefix(p.source[p.pos:], "*/") {
				p.pos++
			}
			if !p.done() {
				p.pos += 2
			}
			continue
		}
		return
	}
}

func identStart(b byte) bool {
	return unicode.IsLetter(rune(b)) || b == '_' || b == '-'
}

func identChar(b byte) bool {
	return unicode.IsLetter(rune(b)) || unicode.IsDigit(rune(b)) || b == '_' || b == '-' || b == '.'
}

func selectorIdentChar(b byte) bool {
	return unicode.IsLetter(rune(b)) || unicode.IsDigit(rune(b)) || b == '_' || b == '-'
}

func (p *styleParser) ident() (string, bool) {
	p.skip()
	if p.done() || !identStart(p.peek()) {
		return "", false
	}
	start := p.pos
	for !p.done() && identChar(p.peek()) {
		p.pos++
	}
	return p.source[start:p.pos], true
}

func (p *styleParser) selectorIdent() (string, bool) {
	p.skip()
	if !p.done() && p.peek() == '*' {
		p.pos++
		return "*", true
	}
	if p.done() || !identStart(p.peek()) {
		return "", false
	}
	start := p.pos
	for !p.done() && selectorIdentChar(p.peek()) {
		p.pos++
	}
	return p.source[start:p.pos], true
}

func (p *styleParser) expect(ch byte) bool {
	p.skip()
	if p.done() || p.peek() != ch {
		return false
	}
	p.pos++
	return true
}

func (p *styleParser) directive() (string, error) {
	p.pos++
	name, ok := p.ident()
	if !ok {
		return "", p.err("expected directive name")
	}
	value, ok := p.ident()
	if !ok {
		return "", p.err("expected directive value")
	}
	if !p.expect(';') {
		return "", p.err("expected ';'")
	}
	switch strings.ToLower(name) {
	case "pack":
		return value, nil
	case "layer":
		switch strings.ToLower(value) {
		case "reset", "base", "defaults":
			p.layer = 0
		case "components", "widgets":
			p.layer = 1
		case "app":
			p.layer = 2
		case "overrides":
			p.layer = 3
		default:
			return "", p.err("unknown layer %q", value)
		}
		return "", nil
	default:
		return "", p.err("unknown directive @%s", name)
	}
}

func (p *styleParser) tokensBlock() error {
	if !p.expect('{') {
		return p.err("expected '{' after tokens")
	}
	for {
		p.skip()
		if p.done() {
			return p.err("unterminated tokens block")
		}
		if p.peek() == '}' {
			p.pos++
			return nil
		}
		if err := p.tokenGroup(); err != nil {
			return err
		}
	}
}

func (p *styleParser) tokenGroup() error {
	group, ok := p.ident()
	if !ok {
		return p.err("expected token group")
	}
	group = strings.ToLower(group)
	if group != "color" && group != "length" && group != "number" && group != "duration" && group != "material" {
		return p.err("unknown token group %q", group)
	}
	if !p.expect('{') {
		return p.err("expected '{' after token group")
	}
	for {
		p.skip()
		if p.done() {
			return p.err("unterminated token group")
		}
		if p.peek() == '}' {
			p.pos++
			return nil
		}
		name, ok := p.ident()
		if !ok {
			return p.err("expected token name")
		}
		if !p.expect(':') {
			return p.err("expected ':' after token name")
		}
		switch group {
		case "color":
			value, err := p.color()
			if err != nil {
				return err
			}
			if p.tokens.colors == nil {
				p.tokens.colors = map[string]uint32{}
			}
			p.tokens.colors[name] = value
		case "length", "number":
			value, err := p.number()
			if err != nil {
				return err
			}
			if p.tokens.lengths == nil {
				p.tokens.lengths = map[string]float32{}
			}
			p.tokens.lengths[name] = value
		case "duration":
			value, err := p.duration()
			if err != nil {
				return err
			}
			if p.tokens.lengths == nil {
				p.tokens.lengths = map[string]float32{}
			}
			p.tokens.lengths[name] = value
		case "material":
			value, err := p.materialValue()
			if err != nil {
				return err
			}
			if p.tokens.materials == nil {
				p.tokens.materials = map[string]int32{}
			}
			p.tokens.materials[name] = value
		}
		if !p.expect(';') {
			return p.err("expected ';'")
		}
	}
}

func (p *styleParser) rule(order int) (StyleRule, error) {
	rule := StyleRule{
		Selector: StyleSheet_StyleDefaultSelector(),
		State:    StyleSheet_StyleStateAny(),
		Layer:    p.layer,
		Order:    int32(order),
	}
	kind, ok := p.selectorIdent()
	if !ok {
		return rule, p.err("expected selector")
	}
	mapped, ok := styleKind(kind)
	if !ok {
		return rule, p.err("unknown selector %q", kind)
	}
	rule.Selector.Kind = mapped

	for {
		p.skip()
		if p.done() {
			return rule, p.err("unterminated selector")
		}
		switch p.peek() {
		case '[':
			p.pos++
			if err := p.attr(&rule.Selector); err != nil {
				return rule, err
			}
		case '.':
			p.pos++
			className, ok := p.selectorIdent()
			if !ok {
				return rule, p.err("expected class name")
			}
			rule.Selector.ClassName = StyleClassID(className)
		case ':':
			p.pos++
			state, ok := p.ident()
			if !ok {
				return rule, p.err("expected state")
			}
			mapped, ok := styleState(state)
			if !ok {
				return rule, p.err("unknown state %q", state)
			}
			rule.State = mapped
		case '{':
			p.pos++
			for {
				p.skip()
				if p.done() {
					return rule, p.err("unterminated rule")
				}
				if p.peek() == '}' {
					p.pos++
					return rule, nil
				}
				if err := p.property(&rule.Style); err != nil {
					return rule, err
				}
			}
		default:
			return rule, p.err("expected selector suffix or block")
		}
	}
}

func (p *styleParser) attr(selector *StyleSelector) error {
	name, ok := p.ident()
	if !ok {
		return p.err("expected selector attribute")
	}
	if !p.expect('=') {
		return p.err("expected '='")
	}
	value, ok := p.ident()
	if !ok {
		return p.err("expected selector attribute value")
	}
	if !p.expect(']') {
		return p.err("expected ']'")
	}
	switch strings.ToLower(name) {
	case "tone":
		v, ok := styleTone(value)
		if !ok {
			return p.err("unknown tone %q", value)
		}
		selector.Tone = v
	case "emphasis":
		v, ok := styleEmphasis(value)
		if !ok {
			return p.err("unknown emphasis %q", value)
		}
		selector.Emphasis = v
	case "size":
		v, ok := styleSize(value)
		if !ok {
			return p.err("unknown size %q", value)
		}
		selector.Size = v
	case "state":
		v, ok := styleState(value)
		if !ok {
			return p.err("unknown state %q", value)
		}
		selector.State = v
	case "role":
		v, ok := styleRole(value)
		if !ok {
			return p.err("unknown role %q", value)
		}
		selector.Role = v
	case "class":
		selector.ClassName = StyleClassID(value)
	default:
		return p.err("unknown selector attribute %q", name)
	}
	return nil
}

func (p *styleParser) property(style *StyleData) error {
	name, ok := p.ident()
	if !ok {
		return p.err("expected property")
	}
	if !p.expect(':') {
		return p.err("expected ':'")
	}
	switch strings.ToLower(name) {
	case "background":
		v, err := p.colorValue()
		if err != nil {
			return err
		}
		style.Fields |= StyleBackground
		style.Background = v
	case "foreground":
		v, err := p.colorValue()
		if err != nil {
			return err
		}
		style.Fields |= StyleForeground
		style.Foreground = v
	case "border":
		v, err := p.colorValue()
		if err != nil {
			return err
		}
		style.Fields |= StyleBorder
		style.Border = v
	case "focus":
		v, err := p.colorValue()
		if err != nil {
			return err
		}
		style.Fields |= StyleFocus
		style.Focus = v
	case "background-end":
		v, err := p.colorValue()
		if err != nil {
			return err
		}
		style.Fields |= StyleBackgroundEnd
		style.BackgroundEnd = v
	case "radius":
		return p.numberField(&style.Fields, StyleRadius, &style.Radius)
	case "border-width":
		return p.numberField(&style.Fields, StyleBorderWidth, &style.BorderWidth)
	case "opacity":
		return p.numberField(&style.Fields, StyleOpacity, &style.Opacity)
	case "padding-x":
		return p.numberField(&style.Fields, StylePaddingX, &style.PaddingX)
	case "padding-y":
		return p.numberField(&style.Fields, StylePaddingY, &style.PaddingY)
	case "gap":
		return p.numberField(&style.Fields, StyleGap, &style.Gap)
	case "font-size":
		return p.numberField(&style.Fields, StyleFontSize, &style.FontSize)
	case "letter-spacing":
		return p.numberField(&style.Fields, StyleLetterSpacing, &style.LetterSpacing)
	case "icon-size":
		return p.numberField(&style.Fields, StyleIconSize, &style.IconSize)
	case "offset-x":
		return p.numberField(&style.Fields, StyleContentOffset, &style.OffsetX)
	case "offset-y":
		return p.numberField(&style.Fields, StyleContentOffset, &style.OffsetY)
	case "material":
		mapped, err := p.materialValue()
		if err != nil {
			return err
		}
		style.Fields |= StyleMaterial
		style.Material = mapped
	case "typeface":
		value, ok := p.ident()
		if !ok {
			return p.err("expected typeface name")
		}
		style.Fields |= StyleTypeface
		style.Typeface = value
	default:
		return p.err("unknown property %q", name)
	}
	if !p.expect(';') {
		return p.err("expected ';'")
	}
	return nil
}

func (p *styleParser) numberField(fields *uint32, field uint32, out *float32) error {
	value, err := p.numberValue()
	if err != nil {
		return err
	}
	*fields |= field
	*out = value
	if !p.expect(';') {
		return p.err("expected ';'")
	}
	return nil
}

func (p *styleParser) numberValue() (float32, error) {
	save := p.pos
	if value, err := p.number(); err == nil {
		return value, nil
	}
	p.pos = save
	name, ok := p.ident()
	if !ok {
		return 0, p.err("expected number")
	}
	if value, ok := p.tokens.lengths[name]; ok {
		return value, nil
	}
	return 0, p.err("unknown length token %q", name)
}

func (p *styleParser) number() (float32, error) {
	p.skip()
	start := p.pos
	for !p.done() {
		b := p.peek()
		if (b >= '0' && b <= '9') || b == '.' || b == '-' || b == '+' {
			p.pos++
			continue
		}
		break
	}
	if start == p.pos {
		return 0, p.err("expected number")
	}
	v, err := strconv.ParseFloat(p.source[start:p.pos], 32)
	if err != nil {
		return 0, p.err("invalid number")
	}
	return float32(v), nil
}

func (p *styleParser) duration() (float32, error) {
	value, err := p.number()
	if err != nil {
		return 0, err
	}
	save := p.pos
	unit, ok := p.ident()
	if !ok {
		return value, nil
	}
	switch strings.ToLower(unit) {
	case "ms":
		return value, nil
	case "s":
		return value * 1000, nil
	default:
		p.pos = save
		return 0, p.err("expected duration unit")
	}
}

func (p *styleParser) colorValue() (uint32, error) {
	save := p.pos
	if value, err := p.color(); err == nil {
		return value, nil
	}
	p.pos = save
	name, ok := p.ident()
	if !ok {
		return 0, p.err("expected hex color")
	}
	if value, ok := p.tokens.colors[name]; ok {
		return value, nil
	}
	return 0, p.err("unknown color token %q", name)
}

func (p *styleParser) color() (uint32, error) {
	p.skip()
	if p.done() || p.peek() != '#' {
		return 0, p.err("expected hex color")
	}
	p.pos++
	start := p.pos
	for !p.done() {
		b := p.peek()
		if !((b >= '0' && b <= '9') || (b >= 'a' && b <= 'f') || (b >= 'A' && b <= 'F')) {
			break
		}
		p.pos++
	}
	text := p.source[start:p.pos]
	if len(text) != 6 && len(text) != 8 {
		return 0, p.err("expected 6 or 8 hex digits")
	}
	v, err := strconv.ParseUint(text, 16, 32)
	if err != nil {
		return 0, p.err("invalid hex color")
	}
	if len(text) == 6 {
		v = (v << 8) | 0xff
	}
	return uint32(v), nil
}

func (p *styleParser) materialValue() (int32, error) {
	name, ok := p.ident()
	if !ok {
		return 0, p.err("expected material")
	}
	if value, ok := styleMaterial(name); ok {
		return value, nil
	}
	if value, ok := p.tokens.materials[name]; ok {
		return value, nil
	}
	return 0, p.err("unknown material %q", name)
}

func (p *styleParser) err(format string, args ...any) error {
	return fmt.Errorf(format+" at byte %d", append(args, p.pos)...)
}

func styleKind(name string) (int32, bool) {
	switch strings.ToLower(name) {
	case "any", "*":
		return StyleSheet_StyleKindAny(), true
	case "app":
		return StyleSheet_StyleKindApp(), true
	case "button":
		return StyleSheet_StyleKindButton(), true
	case "text":
		return StyleSheet_StyleKindText(), true
	case "textfield":
		return StyleSheet_StyleKindTextField(), true
	case "textarea":
		return StyleSheet_StyleKindTextArea(), true
	case "surface":
		return StyleSheet_StyleKindSurface(), true
	case "dropdown":
		return StyleSheet_StyleKindDropdown(), true
	case "card":
		return StyleSheet_StyleKindCard(), true
	case "slider":
		return StyleSheet_StyleKindSlider(), true
	case "sliderthumb":
		return StyleSheet_StyleKindSliderThumb(), true
	case "toggle":
		return StyleSheet_StyleKindToggle(), true
	case "togglethumb":
		return StyleSheet_StyleKindToggleThumb(), true
	case "scroll":
		return StyleSheet_StyleKindScroll(), true
	case "scrollthumb":
		return StyleSheet_StyleKindScrollThumb(), true
	case "checkbox":
		return StyleSheet_StyleKindCheckbox(), true
	case "radio":
		return StyleSheet_StyleKindRadio(), true
	case "progress":
		return StyleSheet_StyleKindProgress(), true
	case "separator":
		return StyleSheet_StyleKindSeparator(), true
	case "navigationbar":
		return StyleSheet_StyleKindNavigationBar(), true
	case "navigationbaritem":
		return StyleSheet_StyleKindNavigationBarItem(), true
	case "selectable":
		return StyleSheet_StyleKindSelectable(), true
	case "fieldset":
		return StyleSheet_StyleKindFieldset(), true
	case "plot":
		return StyleSheet_StyleKindPlot(), true
	case "plotmark":
		return StyleSheet_StyleKindPlotMark(), true
	case "link":
		return StyleSheet_StyleKindLink(), true
	case "tabbar":
		return StyleSheet_StyleKindTabBar(), true
	case "tab":
		return StyleSheet_StyleKindTab(), true
	case "tabclose":
		return StyleSheet_StyleKindTabClose(), true
	case "segmentedcontrol":
		return StyleSheet_StyleKindSegmentedControl(), true
	case "segment":
		return StyleSheet_StyleKindSegment(), true
	case "menu":
		return StyleSheet_StyleKindMenu(), true
	case "menuitem":
		return StyleSheet_StyleKindMenuItem(), true
	case "menuseparator":
		return StyleSheet_StyleKindMenuSeparator(), true
	case "listbox":
		return StyleSheet_StyleKindListBox(), true
	case "listboxitem":
		return StyleSheet_StyleKindListBoxItem(), true
	case "treeview":
		return StyleSheet_StyleKindTreeView(), true
	case "treeviewitem":
		return StyleSheet_StyleKindTreeViewItem(), true
	case "listboxmulti", "multiselectlist":
		return StyleSheet_StyleKindListBoxMulti(), true
	case "listboxmultiitem", "multiselectitem":
		return StyleSheet_StyleKindListBoxMultiItem(), true
	case "dragdroptarget":
		return StyleSheet_StyleKindDragDropTarget(), true
	case "spinbox":
		return StyleSheet_StyleKindSpinbox(), true
	case "spinboxvalue":
		return StyleSheet_StyleKindSpinboxValue(), true
	case "colorpicker":
		return StyleSheet_StyleKindColorPicker(), true
	case "colorpickerswatch":
		return StyleSheet_StyleKindColorPickerSwatch(), true
	case "panedview":
		return StyleSheet_StyleKindPanedView(), true
	case "toast":
		return StyleSheet_StyleKindToast(), true
	case "collapsible":
		return StyleSheet_StyleKindCollapsible(), true
	case "titlebar":
		return StyleSheet_StyleKindTitleBar(), true
	case "toolbar":
		return StyleSheet_StyleKindToolbar(), true
	case "modal":
		return StyleSheet_StyleKindModal(), true
	case "tableview":
		return StyleSheet_StyleKindTableView(), true
	case "guide":
		return StyleSheet_StyleKindGuide(), true
	case "image":
		return StyleSheet_StyleKindImage(), true
	case "focus":
		return StyleSheet_StyleKindFocus(), true
	case "popup":
		return StyleSheet_StyleKindPopup(), true
	case "canvas":
		return StyleSheet_StyleKindCanvas(), true
	case "drag":
		return StyleSheet_StyleKindDrag(), true
	case "dragvalue":
		return StyleSheet_StyleKindDragValue(), true
	case "heading":
		return StyleSheet_StyleKindHeading(), true
	case "paragraphtext":
		return StyleSheet_StyleKindParagraphText(), true
	case "page":
		return StyleSheet_StyleKindPage(), true
	case "section":
		return StyleSheet_StyleKindSection(), true
	case "reorder":
		return StyleSheet_StyleKindReorder(), true
	default:
		return 0, false
	}
}

func styleRole(name string) (int32, bool) {
	switch strings.ToLower(name) {
	case "any":
		return StyleSheet_StyleAny(), true
	case "bar", "menubar":
		return 1, true
	case "popup", "panel", "menupopup":
		return 2, true
	case "context", "menucontext":
		return 3, true
	case "track":
		return 4, true
	case "fill":
		return 5, true
	case "label", "text":
		return 6, true
	case "line":
		return 7, true
	case "bullet":
		return 8, true
	case "box":
		return 9, true
	case "mark", "check":
		return 10, true
	case "ring":
		return 11, true
	case "handle":
		return 12, true
	case "header":
		return 13, true
	case "treeheader":
		return 14, true
	case "close":
		return 15, true
	case "title":
		return 16, true
	case "action":
		return 17, true
	case "divider":
		return 18, true
	case "scrim":
		return 19, true
	case "message":
		return 20, true
	case "row":
		return 21, true
	case "cell":
		return 22, true
	case "selection", "selected":
		return 23, true
	case "anchor":
		return 24, true
	case "placeholder":
		return 25, true
	case "option":
		return 26, true
	case "scrollbar":
		return 27, true
	default:
		return 0, false
	}
}

func styleState(name string) (int32, bool) {
	switch strings.ToLower(name) {
	case "any":
		return StyleSheet_StyleStateAny(), true
	case "normal":
		return int32(ButtonStateNormal), true
	case "hover":
		return int32(ButtonStateHover), true
	case "pressed", "press":
		return int32(ButtonStatePressed), true
	case "focus", "focused":
		return int32(ButtonStateFocus), true
	case "disabled":
		return int32(ButtonStateDisabled), true
	case "loading":
		return int32(ButtonStateLoading), true
	case "selected":
		return int32(ButtonStateSelected), true
	default:
		return 0, false
	}
}

func styleTone(name string) (int32, bool) {
	switch strings.ToLower(name) {
	case "any":
		return StyleSheet_StyleAny(), true
	case "neutral":
		return int32(ButtonToneNeutral), true
	case "accent":
		return int32(ButtonToneAccent), true
	case "danger":
		return int32(ButtonToneDanger), true
	case "success":
		return int32(ButtonToneSuccess), true
	case "warning":
		return int32(ButtonToneWarning), true
	default:
		return 0, false
	}
}

func styleEmphasis(name string) (int32, bool) {
	switch strings.ToLower(name) {
	case "any":
		return StyleSheet_StyleAny(), true
	case "filled":
		return int32(ButtonEmphasisFilled), true
	case "soft":
		return int32(ButtonEmphasisSoft), true
	case "outline":
		return int32(ButtonEmphasisOutline), true
	case "ghost":
		return int32(ButtonEmphasisGhost), true
	case "link":
		return int32(ButtonEmphasisLink), true
	default:
		return 0, false
	}
}

func styleSize(name string) (int32, bool) {
	switch strings.ToLower(name) {
	case "any":
		return StyleSheet_StyleAny(), true
	case "medium":
		return int32(ControlSizeMedium), true
	case "small":
		return int32(ControlSizeSmall), true
	case "large":
		return int32(ControlSizeLarge), true
	default:
		return 0, false
	}
}

func styleMaterial(name string) (int32, bool) {
	switch strings.ToLower(name) {
	case "lightfield":
		return int32(MaterialLightfield), true
	case "flat":
		return int32(MaterialFlat), true
	case "glass":
		return int32(MaterialGlass), true
	default:
		return 0, false
	}
}
