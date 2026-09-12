package kryon

import "testing"

func TestStyleSheetCascadeInGo(t *testing.T) {
	button := StyleSheet_StyleDefaultSelector()
	button.Kind = StyleSheet_StyleKindButton()

	primary := StyleSheet_StyleDefaultSelector()
	primary.ClassName = 3

	facts := StyleSheet_StyleDefaultFacts(StyleSheet_StyleKindButton())
	facts.ClassName = 3

	base := StyleData{Fields: uint32(StyleOpacity), Opacity: 1}
	rules := []StyleRule{
		{
			Selector: button,
			State:    StyleSheet_StyleStateAny(),
			Style: StyleData{
				Fields:     uint32(StyleBackground),
				Background: 0x111111ff,
			},
		},
		{
			Selector: primary,
			State:    StyleSheet_StyleStateAny(),
			Order:    1,
			Style: StyleData{
				Fields:     uint32(StyleBackground | StylePaddingX),
				Background: 0x222222ff,
				PaddingX:   12,
			},
		},
		{
			Selector: primary,
			State:    int32(ButtonStateHover),
			Order:    2,
			Style: StyleData{
				Fields:     uint32(StyleBackground),
				Background: 0x333333ff,
			},
		},
	}

	cascade := StyleSheet_BeginStyleCascade(base)
	for _, rule := range rules {
		cascade = StyleSheet_ApplyStyleRule(cascade, rule, facts, int32(ButtonStateHover))
	}
	result := StyleSheet_FinishStyleCascade(cascade)

	if result.Background != 0x333333ff {
		t.Fatalf("hover class rule did not win: 0x%08x", result.Background)
	}
	if result.PaddingX != 12 {
		t.Fatalf("normal class metric was not retained: %v", result.PaddingX)
	}
	if result.Opacity != 1 {
		t.Fatalf("base opacity was not retained: %v", result.Opacity)
	}
}

func TestStylePackRegistryInGo(t *testing.T) {
	button := StyleSheet_StyleDefaultSelector()
	button.Kind = StyleSheet_StyleKindButton()

	vanilla := []StyleRule{{
		Selector: button,
		State:    StyleSheet_StyleStateAny(),
		Style: StyleData{
			Fields:     uint32(StyleBackground),
			Background: 0x111111ff,
		},
	}}
	glow := []StyleRule{{
		Selector: button,
		State:    StyleSheet_StyleStateAny(),
		Style: StyleData{
			Fields:     uint32(StyleBackground),
			Background: 0x222222ff,
		},
	}}

	ClearStylePacks()
	defer ClearStylePacks()
	version := StylePackVersion()
	base := StyleData{Fields: uint32(StyleOpacity), Opacity: 1}
	facts := StyleSheet_StyleControlFacts(StyleSheet_StyleKindButton(), 0, 0,
		int32(ButtonToneNeutral), int32(ButtonEmphasisSoft),
		int32(ControlSizeMedium), int32(ButtonStateNormal))

	if resolved := ResolveActiveStyle(base, facts, int32(ButtonStateNormal)); resolved.Background != 0 || resolved.Opacity != 1 {
		t.Fatalf("unstyled active resolution changed base: %#v", resolved)
	}
	if RegisterStylePack(StylePack{}) {
		t.Fatal("empty style pack registered")
	}
	if StylePackVersion() != version {
		t.Fatal("failed registration changed version")
	}
	if !RegisterStylePack(StylePack{ID: "vanilla", Label: "Vanilla", Sheet: vanilla}) {
		t.Fatal("vanilla pack did not register")
	}
	if GetActiveStylePackID() != "vanilla" {
		t.Fatalf("first pack was not active: %q", GetActiveStylePackID())
	}
	if resolved := ResolveActiveStyle(base, facts, int32(ButtonStateNormal)); resolved.Background != 0x111111ff {
		t.Fatalf("vanilla did not resolve: 0x%08x", resolved.Background)
	}
	if !RegisterStylePack(StylePack{ID: "glow", Label: "Glow", Sheet: glow}) {
		t.Fatal("glow pack did not register")
	}
	if !SetActiveStylePack("glow") {
		t.Fatal("glow pack did not activate")
	}
	if resolved := ResolveActiveStyle(base, facts, int32(ButtonStateNormal)); resolved.Background != 0x222222ff {
		t.Fatalf("glow did not resolve: 0x%08x", resolved.Background)
	}
	options := GetStylePackOptions()
	if len(options) != 2 || options[0].Active || !options[1].Active {
		t.Fatalf("bad options: %#v", options)
	}
	if !RegisterStylePack(StylePack{ID: "glow", Label: "Glow Updated", Sheet: vanilla}) {
		t.Fatal("replacement glow pack did not register")
	}
	if resolved := ResolveActiveStyle(base, facts, int32(ButtonStateNormal)); resolved.Background != 0x111111ff {
		t.Fatalf("replacement active pack did not resolve: 0x%08x", resolved.Background)
	}
}

func TestStylePickerEmptyRegistryInGo(t *testing.T) {
	ClearStylePacks()
	defer ClearStylePacks()

	r := New(AppConfig{}).(*runtime)
	if r.StylePicker(StylePickerProps{Bounds: NewRectangle(0, 0, 120, 28), ID: 42}) {
		t.Fatal("empty style picker reported a change")
	}
}

func TestParseStyleSheetInGo(t *testing.T) {
	id, rules, err := ParseStyleSheet(`
@pack smoke;
tokens {
  color { accent: #2f6bff; face: #111111; }
  length { line: 2; }
}
@layer components;
Button[tone=Accent][emphasis=Filled]:hover {
  background: accent;
  border-width: line;
}
Segment:selected {
  foreground: accent;
}
MenuItem:selected {
  background: accent;
}
ListBoxItem:selected {
  foreground: accent;
}
TreeViewItem:selected {
  background: accent;
}
ListBoxMultiItem:selected {
  foreground: accent;
}
DragDropTarget:hover {
  border: accent;
}
SpinboxValue {
  background: accent;
}
ColorPickerSwatch {
  border: accent;
}
SliderThumb {
  background: accent;
}
Scroll {
  background: face;
}
ScrollThumb:pressed {
  background: accent;
}
ToggleThumb {
  background: accent;
}
Menu[role=Bar] {
  background: accent;
}
Progress[role=Fill] {
  background: accent;
}
Separator[role=Bullet] {
  foreground: accent;
}
Checkbox[role=Mark] {
  background: accent;
}
Radio[role=Ring] {
  border: accent;
}
PanedView[role=Handle] {
  background: accent;
}
Toast[role=Label] {
  foreground: accent;
}
Collapsible[role=TreeHeader] {
  foreground: accent;
}
TitleBar[role=Title] {
  foreground: accent;
}
Toolbar[role=Divider] {
  border: accent;
}
Modal[role=Scrim] {
  background: face;
}
TableView[role=Cell] {
  foreground: accent;
}
Guide[role=Anchor] {
  border: accent;
}
Image[role=Label] {
  foreground: accent;
}
Popup[role=Panel] {
  border: accent;
}
Canvas {
  background: accent;
}
DragValue {
  border: accent;
}
Focus[role=Box]:focus {
  border: accent;
}
`)
	if err != nil {
		t.Fatal(err)
	}
	if id != "smoke" || len(rules) != 31 {
		t.Fatalf("bad parse result: id=%q len=%d", id, len(rules))
	}
	if rules[0].Selector.Kind != StyleSheet_StyleKindButton() ||
		rules[0].Selector.Tone != int32(ButtonToneAccent) ||
		rules[0].State != int32(ButtonStateHover) {
		t.Fatalf("bad selector: %#v", rules[0])
	}
	if rules[0].Style.Background != 0x2f6bffff || rules[0].Style.BorderWidth != 2 {
		t.Fatalf("bad style: %#v", rules[0].Style)
	}
	if rules[1].Selector.Kind != StyleSheet_StyleKindSegment() ||
		rules[1].State != int32(ButtonStateSelected) ||
		rules[1].Style.Foreground != 0x2f6bffff {
		t.Fatalf("bad segment rule: %#v", rules[1])
	}
	if rules[2].Selector.Kind != StyleSheet_StyleKindMenuItem() ||
		rules[2].State != int32(ButtonStateSelected) ||
		rules[2].Style.Background != 0x2f6bffff {
		t.Fatalf("bad menu item rule: %#v", rules[2])
	}
	if rules[3].Selector.Kind != StyleSheet_StyleKindListBoxItem() ||
		rules[3].State != int32(ButtonStateSelected) ||
		rules[3].Style.Foreground != 0x2f6bffff {
		t.Fatalf("bad list box item rule: %#v", rules[3])
	}
	if rules[4].Selector.Kind != StyleSheet_StyleKindTreeViewItem() ||
		rules[4].State != int32(ButtonStateSelected) ||
		rules[4].Style.Background != 0x2f6bffff {
		t.Fatalf("bad tree view item rule: %#v", rules[4])
	}
	if rules[5].Selector.Kind != StyleSheet_StyleKindListBoxMultiItem() ||
		rules[5].State != int32(ButtonStateSelected) ||
		rules[5].Style.Foreground != 0x2f6bffff {
		t.Fatalf("bad multi select item rule: %#v", rules[5])
	}
	if rules[6].Selector.Kind != StyleSheet_StyleKindDragDropTarget() ||
		rules[6].State != int32(ButtonStateHover) ||
		rules[6].Style.Border != 0x2f6bffff {
		t.Fatalf("bad drag drop target rule: %#v", rules[6])
	}
	if rules[7].Selector.Kind != StyleSheet_StyleKindSpinboxValue() ||
		rules[7].Style.Background != 0x2f6bffff {
		t.Fatalf("bad spinbox value rule: %#v", rules[7])
	}
	if rules[8].Selector.Kind != StyleSheet_StyleKindColorPickerSwatch() ||
		rules[8].Style.Border != 0x2f6bffff {
		t.Fatalf("bad color picker swatch rule: %#v", rules[8])
	}
	if rules[9].Selector.Kind != StyleSheet_StyleKindSliderThumb() ||
		rules[9].Style.Background != 0x2f6bffff {
		t.Fatalf("bad slider thumb rule: %#v", rules[9])
	}
	if rules[10].Selector.Kind != StyleSheet_StyleKindScroll() ||
		rules[10].Style.Background != 0x111111ff {
		t.Fatalf("bad scroll rule: %#v", rules[10])
	}
	if rules[11].Selector.Kind != StyleSheet_StyleKindScrollThumb() ||
		rules[11].State != int32(ButtonStatePressed) ||
		rules[11].Style.Background != 0x2f6bffff {
		t.Fatalf("bad scroll thumb rule: %#v", rules[11])
	}
	if rules[12].Selector.Kind != StyleSheet_StyleKindToggleThumb() ||
		rules[12].Style.Background != 0x2f6bffff {
		t.Fatalf("bad toggle thumb rule: %#v", rules[12])
	}
	if rules[13].Selector.Kind != StyleSheet_StyleKindMenu() ||
		rules[13].Selector.Role != 1 ||
		rules[13].Style.Background != 0x2f6bffff {
		t.Fatalf("bad menu role rule: %#v", rules[13])
	}
	if rules[14].Selector.Kind != StyleSheet_StyleKindProgress() ||
		rules[14].Selector.Role != 5 ||
		rules[14].Style.Background != 0x2f6bffff {
		t.Fatalf("bad progress role rule: %#v", rules[14])
	}
	if rules[15].Selector.Kind != StyleSheet_StyleKindSeparator() ||
		rules[15].Selector.Role != 8 ||
		rules[15].Style.Foreground != 0x2f6bffff {
		t.Fatalf("bad separator role rule: %#v", rules[15])
	}
	if rules[16].Selector.Kind != StyleSheet_StyleKindCheckbox() ||
		rules[16].Selector.Role != 10 ||
		rules[16].Style.Background != 0x2f6bffff {
		t.Fatalf("bad checkbox role rule: %#v", rules[16])
	}
	if rules[17].Selector.Kind != StyleSheet_StyleKindRadio() ||
		rules[17].Selector.Role != 11 ||
		rules[17].Style.Border != 0x2f6bffff {
		t.Fatalf("bad radio role rule: %#v", rules[17])
	}
	if rules[18].Selector.Kind != StyleSheet_StyleKindPanedView() ||
		rules[18].Selector.Role != 12 ||
		rules[18].Style.Background != 0x2f6bffff {
		t.Fatalf("bad paned view role rule: %#v", rules[18])
	}
	if rules[19].Selector.Kind != StyleSheet_StyleKindToast() ||
		rules[19].Selector.Role != 6 ||
		rules[19].Style.Foreground != 0x2f6bffff {
		t.Fatalf("bad toast role rule: %#v", rules[19])
	}
	if rules[20].Selector.Kind != StyleSheet_StyleKindCollapsible() ||
		rules[20].Selector.Role != 14 ||
		rules[20].Style.Foreground != 0x2f6bffff {
		t.Fatalf("bad collapsible role rule: %#v", rules[20])
	}
	if rules[21].Selector.Kind != StyleSheet_StyleKindTitleBar() ||
		rules[21].Selector.Role != 16 ||
		rules[21].Style.Foreground != 0x2f6bffff {
		t.Fatalf("bad title bar role rule: %#v", rules[21])
	}
	if rules[22].Selector.Kind != StyleSheet_StyleKindToolbar() ||
		rules[22].Selector.Role != 18 ||
		rules[22].Style.Border != 0x2f6bffff {
		t.Fatalf("bad toolbar role rule: %#v", rules[22])
	}
	if rules[23].Selector.Kind != StyleSheet_StyleKindModal() ||
		rules[23].Selector.Role != 19 ||
		rules[23].Style.Background != 0x111111ff {
		t.Fatalf("bad modal role rule: %#v", rules[23])
	}
	if rules[24].Selector.Kind != StyleSheet_StyleKindTableView() ||
		rules[24].Selector.Role != 22 ||
		rules[24].Style.Foreground != 0x2f6bffff {
		t.Fatalf("bad table view role rule: %#v", rules[24])
	}
	if rules[25].Selector.Kind != StyleSheet_StyleKindGuide() ||
		rules[25].Selector.Role != 24 ||
		rules[25].Style.Border != 0x2f6bffff {
		t.Fatalf("bad guide role rule: %#v", rules[25])
	}
	if rules[26].Selector.Kind != StyleSheet_StyleKindImage() ||
		rules[26].Selector.Role != 6 ||
		rules[26].Style.Foreground != 0x2f6bffff {
		t.Fatalf("bad image role rule: %#v", rules[26])
	}
	if rules[27].Selector.Kind != StyleSheet_StyleKindPopup() ||
		rules[27].Selector.Role != 2 ||
		rules[27].Style.Border != 0x2f6bffff {
		t.Fatalf("bad popup role rule: %#v", rules[27])
	}
	if rules[28].Selector.Kind != StyleSheet_StyleKindCanvas() ||
		rules[28].Style.Background != 0x2f6bffff {
		t.Fatalf("bad canvas rule: %#v", rules[28])
	}
	if rules[29].Selector.Kind != StyleSheet_StyleKindDragValue() ||
		rules[29].Style.Border != 0x2f6bffff {
		t.Fatalf("bad drag value rule: %#v", rules[29])
	}
	if rules[30].Selector.Kind != StyleSheet_StyleKindFocus() ||
		rules[30].Selector.Role != 9 ||
		rules[30].State != int32(ButtonStateFocus) ||
		rules[30].Style.Border != 0x2f6bffff {
		t.Fatalf("bad focus role rule: %#v", rules[30])
	}
}

func TestRegisterStylePackSourceInGo(t *testing.T) {
	ClearStylePacks()
	defer ClearStylePacks()

	if !RegisterStylePackSource(`
@pack app.brand;
Button[tone=Accent]:hover {
  background: #123456;
  radius: 5;
}
`, "Brand", "App brand") {
		t.Fatal("style pack source did not register")
	}
	if GetActiveStylePackID() != "app.brand" {
		t.Fatalf("source pack was not active: %q", GetActiveStylePackID())
	}
	if GetActiveStylePack().Label != "Brand" {
		t.Fatalf("bad label: %#v", GetActiveStylePack())
	}
	facts := StyleSheet_StyleControlFacts(StyleSheet_StyleKindButton(), 0, 0,
		int32(ButtonToneAccent), int32(ButtonEmphasisSoft),
		int32(ControlSizeMedium), int32(ButtonStateHover))
	resolved := ResolveActiveStyle(StyleData{}, facts, int32(ButtonStateHover))
	if resolved.Background != 0x123456ff || resolved.Radius != 5 {
		t.Fatalf("source pack did not resolve: %#v", resolved)
	}
	if !RegisterStylePackSource("@pack app.brand; Button { background: #abcdef; }", "Brand Updated", "") {
		t.Fatal("style pack source replacement did not register")
	}
	if GetStylePackCount() != 1 || GetActiveStylePack().Label != "Brand Updated" {
		t.Fatalf("source pack replacement failed: %#v", GetStylePackOptions())
	}
	if RegisterStylePackSource("@pack empty;", "Empty", "") {
		t.Fatal("empty style pack source registered")
	}
}

func TestBuiltInStylePacksInGo(t *testing.T) {
	ClearStylePacks()
	defer ClearStylePacks()

	styleKinds := []struct {
		name string
		kind int32
	}{
		{"App", StyleSheet_StyleKindApp()},
		{"Button", StyleSheet_StyleKindButton()},
		{"Text", StyleSheet_StyleKindText()},
		{"TextField", StyleSheet_StyleKindTextField()},
		{"TextArea", StyleSheet_StyleKindTextArea()},
		{"Surface", StyleSheet_StyleKindSurface()},
		{"Dropdown", StyleSheet_StyleKindDropdown()},
		{"Card", StyleSheet_StyleKindCard()},
		{"Slider", StyleSheet_StyleKindSlider()},
		{"SliderThumb", StyleSheet_StyleKindSliderThumb()},
		{"Toggle", StyleSheet_StyleKindToggle()},
		{"ToggleThumb", StyleSheet_StyleKindToggleThumb()},
		{"Scroll", StyleSheet_StyleKindScroll()},
		{"ScrollThumb", StyleSheet_StyleKindScrollThumb()},
		{"Checkbox", StyleSheet_StyleKindCheckbox()},
		{"Radio", StyleSheet_StyleKindRadio()},
		{"Progress", StyleSheet_StyleKindProgress()},
		{"Separator", StyleSheet_StyleKindSeparator()},
		{"NavigationBar", StyleSheet_StyleKindNavigationBar()},
		{"NavigationBarItem", StyleSheet_StyleKindNavigationBarItem()},
		{"Selectable", StyleSheet_StyleKindSelectable()},
		{"Fieldset", StyleSheet_StyleKindFieldset()},
		{"Plot", StyleSheet_StyleKindPlot()},
		{"PlotMark", StyleSheet_StyleKindPlotMark()},
		{"Link", StyleSheet_StyleKindLink()},
		{"TabBar", StyleSheet_StyleKindTabBar()},
		{"Tab", StyleSheet_StyleKindTab()},
		{"TabClose", StyleSheet_StyleKindTabClose()},
		{"SegmentedControl", StyleSheet_StyleKindSegmentedControl()},
		{"Segment", StyleSheet_StyleKindSegment()},
		{"Menu", StyleSheet_StyleKindMenu()},
		{"MenuItem", StyleSheet_StyleKindMenuItem()},
		{"MenuSeparator", StyleSheet_StyleKindMenuSeparator()},
		{"ListBox", StyleSheet_StyleKindListBox()},
		{"ListBoxItem", StyleSheet_StyleKindListBoxItem()},
		{"TreeView", StyleSheet_StyleKindTreeView()},
		{"TreeViewItem", StyleSheet_StyleKindTreeViewItem()},
		{"ListBoxMulti", StyleSheet_StyleKindListBoxMulti()},
		{"ListBoxMultiItem", StyleSheet_StyleKindListBoxMultiItem()},
		{"DragDropTarget", StyleSheet_StyleKindDragDropTarget()},
		{"Spinbox", StyleSheet_StyleKindSpinbox()},
		{"SpinboxValue", StyleSheet_StyleKindSpinboxValue()},
		{"ColorPickerSwatch", StyleSheet_StyleKindColorPickerSwatch()},
		{"PanedView", StyleSheet_StyleKindPanedView()},
		{"Toast", StyleSheet_StyleKindToast()},
		{"Collapsible", StyleSheet_StyleKindCollapsible()},
		{"TitleBar", StyleSheet_StyleKindTitleBar()},
		{"Toolbar", StyleSheet_StyleKindToolbar()},
		{"Modal", StyleSheet_StyleKindModal()},
		{"TableView", StyleSheet_StyleKindTableView()},
		{"Guide", StyleSheet_StyleKindGuide()},
		{"Image", StyleSheet_StyleKindImage()},
		{"Focus", StyleSheet_StyleKindFocus()},
		{"Popup", StyleSheet_StyleKindPopup()},
		{"Canvas", StyleSheet_StyleKindCanvas()},
		{"Drag", StyleSheet_StyleKindDrag()},
		{"DragValue", StyleSheet_StyleKindDragValue()},
	}
	stateKinds := []struct {
		name  string
		kind  int32
		state ButtonState
	}{
		{"Button:hover", StyleSheet_StyleKindButton(), ButtonStateHover},
		{"Button:pressed", StyleSheet_StyleKindButton(), ButtonStatePressed},
		{"Button:focus", StyleSheet_StyleKindButton(), ButtonStateFocus},
		{"Button:disabled", StyleSheet_StyleKindButton(), ButtonStateDisabled},
		{"Button:loading", StyleSheet_StyleKindButton(), ButtonStateLoading},
		{"Dropdown:hover", StyleSheet_StyleKindDropdown(), ButtonStateHover},
		{"Dropdown:pressed", StyleSheet_StyleKindDropdown(), ButtonStatePressed},
		{"Dropdown:focus", StyleSheet_StyleKindDropdown(), ButtonStateFocus},
		{"Dropdown:disabled", StyleSheet_StyleKindDropdown(), ButtonStateDisabled},
		{"Dropdown:selected", StyleSheet_StyleKindDropdown(), ButtonStateSelected},
		{"Slider:hover", StyleSheet_StyleKindSlider(), ButtonStateHover},
		{"Slider:disabled", StyleSheet_StyleKindSlider(), ButtonStateDisabled},
		{"SliderThumb:disabled", StyleSheet_StyleKindSliderThumb(), ButtonStateDisabled},
		{"ScrollThumb:hover", StyleSheet_StyleKindScrollThumb(), ButtonStateHover},
		{"ScrollThumb:pressed", StyleSheet_StyleKindScrollThumb(), ButtonStatePressed},
		{"Toggle:hover", StyleSheet_StyleKindToggle(), ButtonStateHover},
		{"Toggle:pressed", StyleSheet_StyleKindToggle(), ButtonStatePressed},
		{"Toggle:disabled", StyleSheet_StyleKindToggle(), ButtonStateDisabled},
		{"ToggleThumb:disabled", StyleSheet_StyleKindToggleThumb(), ButtonStateDisabled},
		{"Checkbox:hover", StyleSheet_StyleKindCheckbox(), ButtonStateHover},
		{"Checkbox:pressed", StyleSheet_StyleKindCheckbox(), ButtonStatePressed},
		{"Checkbox:selected", StyleSheet_StyleKindCheckbox(), ButtonStateSelected},
		{"Checkbox:disabled", StyleSheet_StyleKindCheckbox(), ButtonStateDisabled},
		{"Radio:hover", StyleSheet_StyleKindRadio(), ButtonStateHover},
		{"Radio:selected", StyleSheet_StyleKindRadio(), ButtonStateSelected},
		{"Radio:disabled", StyleSheet_StyleKindRadio(), ButtonStateDisabled},
		{"NavigationBarItem:hover", StyleSheet_StyleKindNavigationBarItem(), ButtonStateHover},
		{"NavigationBarItem:selected", StyleSheet_StyleKindNavigationBarItem(), ButtonStateSelected},
		{"NavigationBarItem:disabled", StyleSheet_StyleKindNavigationBarItem(), ButtonStateDisabled},
		{"Tab:hover", StyleSheet_StyleKindTab(), ButtonStateHover},
		{"Tab:pressed", StyleSheet_StyleKindTab(), ButtonStatePressed},
		{"Tab:selected", StyleSheet_StyleKindTab(), ButtonStateSelected},
		{"Tab:disabled", StyleSheet_StyleKindTab(), ButtonStateDisabled},
		{"TabClose:hover", StyleSheet_StyleKindTabClose(), ButtonStateHover},
		{"Segment:hover", StyleSheet_StyleKindSegment(), ButtonStateHover},
		{"Segment:pressed", StyleSheet_StyleKindSegment(), ButtonStatePressed},
		{"Segment:selected", StyleSheet_StyleKindSegment(), ButtonStateSelected},
		{"Segment:disabled", StyleSheet_StyleKindSegment(), ButtonStateDisabled},
		{"MenuItem:hover", StyleSheet_StyleKindMenuItem(), ButtonStateHover},
		{"MenuItem:pressed", StyleSheet_StyleKindMenuItem(), ButtonStatePressed},
		{"MenuItem:selected", StyleSheet_StyleKindMenuItem(), ButtonStateSelected},
		{"MenuItem:disabled", StyleSheet_StyleKindMenuItem(), ButtonStateDisabled},
		{"ListBoxItem:hover", StyleSheet_StyleKindListBoxItem(), ButtonStateHover},
		{"ListBoxItem:selected", StyleSheet_StyleKindListBoxItem(), ButtonStateSelected},
		{"ListBoxItem:disabled", StyleSheet_StyleKindListBoxItem(), ButtonStateDisabled},
		{"TreeViewItem:hover", StyleSheet_StyleKindTreeViewItem(), ButtonStateHover},
		{"TreeViewItem:selected", StyleSheet_StyleKindTreeViewItem(), ButtonStateSelected},
		{"TreeViewItem:disabled", StyleSheet_StyleKindTreeViewItem(), ButtonStateDisabled},
		{"ListBoxMultiItem:hover", StyleSheet_StyleKindListBoxMultiItem(), ButtonStateHover},
		{"ListBoxMultiItem:selected", StyleSheet_StyleKindListBoxMultiItem(), ButtonStateSelected},
		{"ListBoxMultiItem:focus", StyleSheet_StyleKindListBoxMultiItem(), ButtonStateFocus},
		{"ListBoxMultiItem:disabled", StyleSheet_StyleKindListBoxMultiItem(), ButtonStateDisabled},
		{"DragDropTarget:hover", StyleSheet_StyleKindDragDropTarget(), ButtonStateHover},
		{"Link:hover", StyleSheet_StyleKindLink(), ButtonStateHover},
		{"Link:disabled", StyleSheet_StyleKindLink(), ButtonStateDisabled},
		{"Focus:focus", StyleSheet_StyleKindFocus(), ButtonStateFocus},
	}
	roleKinds := []struct {
		name string
		kind int32
		role int32
	}{
		{"Menu[role=Bar]", StyleSheet_StyleKindMenu(), 1},
		{"Menu[role=Popup]", StyleSheet_StyleKindMenu(), 2},
		{"Menu[role=Context]", StyleSheet_StyleKindMenu(), 3},
		{"Progress[role=Track]", StyleSheet_StyleKindProgress(), 4},
		{"Progress[role=Fill]", StyleSheet_StyleKindProgress(), 5},
		{"Progress[role=Label]", StyleSheet_StyleKindProgress(), 6},
		{"Separator[role=Line]", StyleSheet_StyleKindSeparator(), 7},
		{"Separator[role=Label]", StyleSheet_StyleKindSeparator(), 6},
		{"Separator[role=Bullet]", StyleSheet_StyleKindSeparator(), 8},
		{"Checkbox[role=Box]", StyleSheet_StyleKindCheckbox(), 9},
		{"Checkbox[role=Mark]", StyleSheet_StyleKindCheckbox(), 10},
		{"Checkbox[role=Label]", StyleSheet_StyleKindCheckbox(), 6},
		{"Radio[role=Ring]", StyleSheet_StyleKindRadio(), 11},
		{"Radio[role=Mark]", StyleSheet_StyleKindRadio(), 10},
		{"Radio[role=Label]", StyleSheet_StyleKindRadio(), 6},
		{"Slider[role=Track]", StyleSheet_StyleKindSlider(), 4},
		{"Slider[role=Fill]", StyleSheet_StyleKindSlider(), 5},
		{"Slider[role=Label]", StyleSheet_StyleKindSlider(), 6},
		{"Toggle[role=Track]", StyleSheet_StyleKindToggle(), 4},
		{"Toggle[role=Fill]", StyleSheet_StyleKindToggle(), 5},
		{"Toggle[role=Label]", StyleSheet_StyleKindToggle(), 6},
		{"PanedView[role=Handle]", StyleSheet_StyleKindPanedView(), 12},
		{"Toast[role=Label]", StyleSheet_StyleKindToast(), 6},
		{"Collapsible[role=Header]", StyleSheet_StyleKindCollapsible(), 13},
		{"Collapsible[role=TreeHeader]", StyleSheet_StyleKindCollapsible(), 14},
		{"Collapsible[role=Close]", StyleSheet_StyleKindCollapsible(), 15},
		{"TitleBar[role=Bar]", StyleSheet_StyleKindTitleBar(), 1},
		{"TitleBar[role=Title]", StyleSheet_StyleKindTitleBar(), 16},
		{"TitleBar[role=Action]", StyleSheet_StyleKindTitleBar(), 17},
		{"Toolbar[role=Bar]", StyleSheet_StyleKindToolbar(), 1},
		{"Toolbar[role=Divider]", StyleSheet_StyleKindToolbar(), 18},
		{"Toolbar[role=Action]", StyleSheet_StyleKindToolbar(), 17},
		{"Guide[role=Bar]", StyleSheet_StyleKindGuide(), 1},
		{"Guide[role=Divider]", StyleSheet_StyleKindGuide(), 18},
		{"Modal[role=Panel]", StyleSheet_StyleKindModal(), 2},
		{"Modal[role=Title]", StyleSheet_StyleKindModal(), 16},
		{"Modal[role=Action]", StyleSheet_StyleKindModal(), 17},
		{"Modal[role=Scrim]", StyleSheet_StyleKindModal(), 19},
		{"Modal[role=Message]", StyleSheet_StyleKindModal(), 20},
		{"Modal[role=Close]", StyleSheet_StyleKindModal(), 15},
		{"TableView[role=Panel]", StyleSheet_StyleKindTableView(), 2},
		{"TableView[role=Header]", StyleSheet_StyleKindTableView(), 13},
		{"TableView[role=Divider]", StyleSheet_StyleKindTableView(), 18},
		{"TableView[role=Row]", StyleSheet_StyleKindTableView(), 21},
		{"TableView[role=Cell]", StyleSheet_StyleKindTableView(), 22},
		{"TableView[role=Selection]", StyleSheet_StyleKindTableView(), 23},
		{"Guide[role=Panel]", StyleSheet_StyleKindGuide(), 2},
		{"Guide[role=Label]", StyleSheet_StyleKindGuide(), 6},
		{"Guide[role=Close]", StyleSheet_StyleKindGuide(), 15},
		{"Guide[role=Action]", StyleSheet_StyleKindGuide(), 17},
		{"Guide[role=Scrim]", StyleSheet_StyleKindGuide(), 19},
		{"Guide[role=Anchor]", StyleSheet_StyleKindGuide(), 24},
		{"Image[role=Label]", StyleSheet_StyleKindImage(), 6},
		{"Focus[role=Box]", StyleSheet_StyleKindFocus(), 9},
		{"Focus[role=Label]", StyleSheet_StyleKindFocus(), 6},
		{"Popup[role=Panel]", StyleSheet_StyleKindPopup(), 2},
	}
	packIDs := []string{
		"kryon.material",
		"kryon.tk",
		"kryon.vanilla",
		"kryon.glow",
		"kryon.lightfield",
	}

	if !RegisterBuiltInStylePacks() {
		t.Fatal("built-in style packs did not register")
	}
	if GetActiveStylePackID() != "kryon.material" {
		t.Fatalf("material was not active: %q", GetActiveStylePackID())
	}
	if FindStylePack("kryon.tk") == nil ||
		FindStylePack("kryon.vanilla") == nil ||
		FindStylePack("kryon.glow") == nil ||
		FindStylePack("kryon.lightfield") == nil {
		t.Fatalf("missing built-ins: %#v", GetStylePackOptions())
	}
	for _, packID := range packIDs {
		if !SetActiveStylePack(packID) {
			t.Fatalf("%s did not activate", packID)
		}
		for _, styleKind := range styleKinds {
			resolved := ResolveActiveStyle(StyleData{},
				StyleSheet_StyleDefaultFacts(styleKind.kind),
				int32(ButtonStateNormal))
			if resolved.Fields == 0 {
				t.Fatalf("%s did not resolve %s", packID, styleKind.name)
			}
		}
		pack := GetActiveStylePack()
		if pack == nil {
			t.Fatalf("%s did not expose an active pack", packID)
		}
		for _, styleState := range stateKinds {
			if !stylePackHasStateRule(pack, styleState.kind, int32(styleState.state)) {
				t.Fatalf("%s did not declare %s", packID, styleState.name)
			}
		}
		for _, styleRole := range roleKinds {
			if !stylePackHasRoleRule(pack, styleRole.kind, styleRole.role) {
				t.Fatalf("%s did not declare %s", packID, styleRole.name)
			}
		}
	}
	if !SetActiveStylePack("kryon.material") {
		t.Fatal("material did not reactivate")
	}
	facts := StyleSheet_StyleControlFacts(StyleSheet_StyleKindButton(), 0, 0,
		int32(ButtonToneAccent), int32(ButtonEmphasisFilled),
		int32(ControlSizeMedium), int32(ButtonStateHover))
	resolved := ResolveActiveStyle(StyleData{}, facts, int32(ButtonStateHover))
	if resolved.Background != 0xd5bbffff || resolved.Material != int32(MaterialFlat) {
		t.Fatalf("material did not resolve: %#v", resolved)
	}
	if !SetActiveStylePack("kryon.vanilla") {
		t.Fatal("vanilla did not activate")
	}
	resolved = ResolveActiveStyle(StyleData{}, facts, int32(ButtonStateHover))
	if resolved.Background != 0x245be0ff || resolved.Material != int32(MaterialFlat) {
		t.Fatalf("vanilla did not resolve: %#v", resolved)
	}
	if !SetActiveStylePack("kryon.glow") {
		t.Fatal("glow did not activate")
	}
	field := StyleSheet_StyleDefaultFacts(StyleSheet_StyleKindTextField())
	resolved = ResolveActiveStyle(StyleData{}, field, int32(ButtonStateNormal))
	if resolved.Material != int32(MaterialGlass) || resolved.BackgroundEnd != 0x171b24ff {
		t.Fatalf("glow did not resolve: %#v", resolved)
	}
	if !SetActiveStylePack("kryon.lightfield") {
		t.Fatal("lightfield did not activate")
	}
	surface := StyleSheet_StyleDefaultFacts(StyleSheet_StyleKindSurface())
	resolved = ResolveActiveStyle(StyleData{}, surface, int32(ButtonStateNormal))
	if resolved.Material != int32(MaterialLightfield) || resolved.BackgroundEnd != 0x222936ee {
		t.Fatalf("lightfield did not resolve: %#v", resolved)
	}
}

func stylePackHasStateRule(pack *StylePack, kind int32, state int32) bool {
	for _, rule := range pack.Sheet {
		if rule.Selector.Kind == kind &&
			(rule.Selector.State == state || rule.State == state) {
			return true
		}
	}
	return false
}

func stylePackHasRoleRule(pack *StylePack, kind int32, role int32) bool {
	for _, rule := range pack.Sheet {
		if rule.Selector.Kind == kind && rule.Selector.Role == role {
			return true
		}
	}
	return false
}

func TestEnsureBuiltInStylePacksInGo(t *testing.T) {
	ClearStylePacks()
	defer ClearStylePacks()

	if !EnsureBuiltInStylePacks() {
		t.Fatal("built-in style packs did not ensure")
	}
	if GetActiveStylePackID() != "kryon.material" {
		t.Fatalf("material was not active: %q", GetActiveStylePackID())
	}
	if len(GetStylePackOptions()) < 5 {
		t.Fatalf("missing built-in options: %#v", GetStylePackOptions())
	}
	if !SetActiveStylePack("kryon.glow") {
		t.Fatal("glow did not activate")
	}
	if !EnsureBuiltInStylePacks() {
		t.Fatal("second ensure failed")
	}
	if GetActiveStylePackID() != "kryon.glow" {
		t.Fatalf("ensure did not preserve selection: %q", GetActiveStylePackID())
	}
}
