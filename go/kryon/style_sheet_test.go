package kryon

import "testing"

func TestStyleSheetCascadeInGo(t *testing.T) {
	button := StyleSheet_StyleDefaultSelector()
	button.Kind = StyleSheet_StyleKindButton()

	primary := StyleSheet_StyleDefaultSelector()
	primary.ClassName = StyleClassID("primary")

	facts := StyleSheet_StyleDefaultFacts(StyleSheet_StyleKindButton())
	facts.ClassName = StyleClassID("primary")

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

	first := []StyleRule{{
		Selector: button,
		State:    StyleSheet_StyleStateAny(),
		Style: StyleData{
			Fields:     uint32(StyleBackground),
			Background: 0x111111ff,
		},
	}}
	second := []StyleRule{{
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
	if !RegisterStylePack(StylePack{ID: "first", Label: "First", Sheet: first}) {
		t.Fatal("first pack did not register")
	}
	if GetActiveStylePackID() != "first" {
		t.Fatalf("first pack was not active: %q", GetActiveStylePackID())
	}
	if resolved := ResolveActiveStyle(base, facts, int32(ButtonStateNormal)); resolved.Background != 0x111111ff {
		t.Fatalf("first did not resolve: 0x%08x", resolved.Background)
	}
	if !RegisterStylePack(StylePack{ID: "second", Label: "Second", Sheet: second}) {
		t.Fatal("second pack did not register")
	}
	if !SetActiveStylePack("second") {
		t.Fatal("second pack did not activate")
	}
	if resolved := ResolveActiveStyle(base, facts, int32(ButtonStateNormal)); resolved.Background != 0x222222ff {
		t.Fatalf("second pack did not resolve: 0x%08x", resolved.Background)
	}
	options := GetStylePackOptions()
	if len(options) != 2 || options[0].Active || !options[1].Active {
		t.Fatalf("bad options: %#v", options)
	}
	if !RegisterStylePack(StylePack{ID: "second", Label: "Second Updated", Sheet: first}) {
		t.Fatal("replacement second pack did not register")
	}
	if resolved := ResolveActiveStyle(base, facts, int32(ButtonStateNormal)); resolved.Background != 0x111111ff {
		t.Fatalf("replacement active pack did not resolve: 0x%08x", resolved.Background)
	}
}

func TestButtonPropsClassNameResolvesKSSClassInGo(t *testing.T) {
	ClearStylePacks()
	defer ClearStylePacks()

	if !RegisterStylePackSource(`@pack test.classes;
Button.primary {
  background: #123456;
  foreground: #f8f9fa;
}`, "Classes", "") {
		t.Fatal("class style pack did not register")
	}

	props := ButtonProps{
		ClassName: StyleClassID("primary"),
		Tone:      ButtonToneNeutral,
		Emphasis:  ButtonEmphasisSoft,
		Size:      ControlSizeMedium,
	}
	got := resolveMinimalControlState(props, ButtonStateNormal, StyleSheet_StyleKindButton())
	if got.Background != 0x123456ff {
		t.Fatalf("button class background did not resolve: 0x%08x", got.Background)
	}
	if got.Foreground != 0xf8f9faff {
		t.Fatalf("button class foreground did not resolve: 0x%08x", got.Foreground)
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

func TestStylePickerForwardsClassNameInGo(t *testing.T) {
	ClearStylePacks()
	defer ClearStylePacks()

	if !RegisterStylePackSource(`@pack picker.class;
Dropdown.picker {
  background: #203040;
  foreground: #f4f7fb;
  border: #506070;
  material: Flat;
}`, "Picker", "") {
		t.Fatal("style picker class pack did not register")
	}

	r := New(AppConfig{Width: 180, Height: 80}).(*runtime)
	r.BeginFrame()
	r.StylePicker(StylePickerProps{
		Bounds:    NewRectangle(8, 8, 128, 28),
		ID:        43,
		ClassName: StyleClassID("picker"),
	})
	r.EndFrame()

	ops := r.FrameOps()
	for _, op := range ops {
		if op.ID != 43 || op.Kind != FrameOpButton {
			continue
		}
		style := unpackStyle(op.Button.Appearance.Value)
		if style.Background != (Color{0x20, 0x30, 0x40, 0xff}) ||
			style.Foreground != (Color{0xf4, 0xf7, 0xfb, 0xff}) ||
			style.Border != (Color{0x50, 0x60, 0x70, 0xff}) ||
			style.Material != MaterialFlat {
			t.Fatalf("style picker dropdown style = %#v", style)
		}
		return
	}
	t.Fatalf("style picker did not emit dropdown op: %#v", ops)
}

func TestParseStyleSheetInGo(t *testing.T) {
	id, rules, err := ParseStyleSheet(`
@pack smoke;
tokens {
  color { accent: #2f6bff; face: #111111; }
  length { line: 2; }
  duration { fast: 80ms; normal: 0.14s; }
}
@layer components;
Button[tone=Accent][emphasis=Filled]:hover {
  background: accent;
  border-width: line;
}
Button.primary {
  padding-y: fast;
}
Button[class=primary]:pressed {
  focus: accent;
  opacity: normal;
}
* {
  gap: line;
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
Page {
  padding-x: 8;
}
Section {
  gap: 6;
}
Focus[role=Box]:focus {
  border: accent;
}
`)
	if err != nil {
		t.Fatal(err)
	}
	if id != "smoke" || len(rules) != 36 {
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
	if rules[1].Selector.Kind != StyleSheet_StyleKindButton() ||
		rules[1].Selector.ClassName != StyleClassID("primary") ||
		rules[1].Style.PaddingY != 80 {
		t.Fatalf("bad class rule: %#v", rules[1])
	}
	if rules[2].Selector.Kind != StyleSheet_StyleKindButton() ||
		rules[2].Selector.ClassName != StyleClassID("primary") ||
		rules[2].State != int32(ButtonStatePressed) ||
		rules[2].Style.Focus != 0x2f6bffff ||
		rules[2].Style.Opacity != 140 {
		t.Fatalf("bad class attribute rule: %#v", rules[2])
	}
	if rules[3].Selector.Kind != StyleSheet_StyleKindAny() ||
		rules[3].Style.Gap != 2 {
		t.Fatalf("bad any rule: %#v", rules[3])
	}
	if rules[4].Selector.Kind != StyleSheet_StyleKindSegment() ||
		rules[4].State != int32(ButtonStateSelected) ||
		rules[4].Style.Foreground != 0x2f6bffff {
		t.Fatalf("bad segment rule: %#v", rules[4])
	}
	if rules[33].Selector.Kind != StyleSheet_StyleKindPage() ||
		rules[33].Style.PaddingX != 8 ||
		rules[34].Selector.Kind != StyleSheet_StyleKindSection() ||
		rules[34].Style.Gap != 6 ||
		rules[35].Selector.Kind != StyleSheet_StyleKindFocus() ||
		rules[35].Selector.Role != 9 ||
		rules[35].State != int32(ButtonStateFocus) ||
		rules[35].Style.Border != 0x2f6bffff {
		t.Fatalf("bad page/section/focus rule: %#v %#v %#v", rules[33], rules[34], rules[35])
	}
}

func TestParseStyleSheetRejectsLegacyPropertyAliases(t *testing.T) {
	legacyAliases := []string{
		"background-color", "color", "border-color", "focus-color",
		"background_end", "border_width", "padding_x", "padding_y",
		"font_size", "icon_size", "offset_x", "offset_y", "font-family",
	}
	for _, alias := range legacyAliases {
		if _, _, err := ParseStyleSheet("Button { " + alias + ": #111111; }"); err == nil {
			t.Fatalf("legacy KSS property alias %q still parsed", alias)
		}
	}
}

func TestParseStyleSheetRejectsLegacySyntax(t *testing.T) {
	cases := []string{
		"@theme dark; Button { background: #111111; }",
		"@layer legacy; Button { background: #111111; }",
		"tokens { colors { face: #111111; } } Button { background: #111111; }",
	}
	for _, source := range cases {
		if _, _, err := ParseStyleSheet(source); err == nil {
			t.Fatalf("legacy KSS syntax still parsed: %s", source)
		}
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
		{"ColorPicker", StyleSheet_StyleKindColorPicker()},
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
		{"Heading", StyleSheet_StyleKindHeading()},
		{"ParagraphText", StyleSheet_StyleKindParagraphText()},
		{"Page", StyleSheet_StyleKindPage()},
		{"Section", StyleSheet_StyleKindSection()},
		{"Reorder", StyleSheet_StyleKindReorder()},
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
		{"Reorder[role=Handle]:selected", StyleSheet_StyleKindReorder(), ButtonStateSelected},
		{"DragValue:disabled", StyleSheet_StyleKindDragValue(), ButtonStateDisabled},
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
		{"Toolbar[role=BottomBar]", StyleSheet_StyleKindToolbar(), 28},
		{"Toolbar[role=BottomAction]", StyleSheet_StyleKindToolbar(), 29},
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
		{"Reorder[role=Handle]", StyleSheet_StyleKindReorder(), 12},
		{"Reorder[role=Placeholder]", StyleSheet_StyleKindReorder(), 25},
		{"Dropdown[role=Panel]", StyleSheet_StyleKindDropdown(), 2},
		{"Dropdown[role=Option]", StyleSheet_StyleKindDropdown(), 26},
		{"Dropdown[role=Scrollbar]", StyleSheet_StyleKindDropdown(), 27},
		{"Image[role=Label]", StyleSheet_StyleKindImage(), 6},
		{"Focus[role=Box]", StyleSheet_StyleKindFocus(), 9},
		{"Focus[role=Label]", StyleSheet_StyleKindFocus(), 6},
		{"Popup[role=Panel]", StyleSheet_StyleKindPopup(), 2},
	}
	packIDs := []string{
		"material",
		"classic",
		"lightfield",
	}

	if !RegisterBuiltInStylePacks() {
		t.Fatal("built-in style packs did not register")
	}
	if GetActiveStylePackID() != "material" {
		t.Fatalf("material was not active: %q", GetActiveStylePackID())
	}
	if FindStylePack("classic") == nil ||
		FindStylePack("lightfield") == nil {
		t.Fatalf("missing built-ins: %#v", GetStylePackOptions())
	}
	if FindStylePack("glow") != nil {
		t.Fatalf("glow must not be a standalone built-in: %#v", GetStylePackOptions())
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
	if !SetActiveStylePack("material") {
		t.Fatal("material did not reactivate")
	}
	facts := StyleSheet_StyleControlFacts(StyleSheet_StyleKindButton(), 0, 0,
		int32(ButtonToneAccent), int32(ButtonEmphasisFilled),
		int32(ControlSizeMedium), int32(ButtonStateHover))
	resolved := ResolveActiveStyle(StyleData{}, facts, int32(ButtonStateHover))
	if resolved.Background != 0xd5bbffff || resolved.Material != MaterialFlat {
		t.Fatalf("material did not resolve: %#v", resolved)
	}
	if !SetActiveStylePack("lightfield") {
		t.Fatal("lightfield did not activate")
	}
	surface := StyleSheet_StyleDefaultFacts(StyleSheet_StyleKindSurface())
	resolved = ResolveActiveStyle(StyleData{}, surface, int32(ButtonStateNormal))
	if resolved.Material != MaterialLightfield || resolved.BackgroundEnd != 0x222936ee {
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
	if GetActiveStylePackID() != "material" {
		t.Fatalf("material was not active: %q", GetActiveStylePackID())
	}
	if len(GetStylePackOptions()) != 3 {
		t.Fatalf("missing built-in options: %#v", GetStylePackOptions())
	}
	if !SetActiveStylePack("lightfield") {
		t.Fatal("lightfield did not activate")
	}
	if !EnsureBuiltInStylePacks() {
		t.Fatal("second ensure failed")
	}
	if GetActiveStylePackID() != "lightfield" {
		t.Fatalf("ensure did not preserve selection: %q", GetActiveStylePackID())
	}
}

func TestStylePackColorVariant(t *testing.T) {
	ClearStylePacks()
	defer ClearStylePacks()
	source := `@pack base; tokens { color { accent: #112233; } }
        Button { background: accent; radius: 9; material: glass; }
        Button:hover { border: accent; }`
	if !RegisterStylePackSource(source, "Base", "") ||
		!RegisterStylePackVariant("green", source, "Green", []StyleColorToken{{Name: "accent", Color: 0x44aa88ff}}) {
		t.Fatal("could not register variant")
	}
	SetActiveStylePack("green")
	facts := StyleSheet_StyleDefaultFacts(StyleSheet_StyleKindButton())
	value := ResolveActiveStyle(StyleData{}, facts, int32(ButtonStateHover))
	if value.Background != 0x44aa88ff || value.Border != 0x44aa88ff || value.Radius != 9 || value.Material != MaterialGlass {
		t.Fatalf("variant lost color or geometry: %+v", value)
	}
	SetActiveStylePack("base")
	if value := ResolveActiveStyle(StyleData{}, facts, int32(ButtonStateHover)); value.Background != 0x112233ff {
		t.Fatal("base palette was modified")
	}
}

func TestStylePackDeclaredVariant(t *testing.T) {
	ClearStylePacks()
	source := "@pack lf; tokens { color { accent: #112233; } } " +
		"@variant glow \"Glow\" { accent: #00ff00; } " +
		"Button { background: accent; radius: 6; } " +
		"@variant glow \"Glow\" { Button { radius: 12; } }"
	variants := ParseStyleVariants(source)
	if len(variants) != 2 || variants[0].Name != "glow" || variants[0].Label != "Glow" {
		t.Fatalf("variant declarations: %+v", variants)
	}
	if !RegisterStylePackSource(source, "Lightfield", "") {
		t.Fatal("register source")
	}
	if GetStylePackCount() != 2 {
		t.Fatalf("packs = %d, want 2 (base + glow)", GetStylePackCount())
	}
	if !SetActiveStylePack("lf.glow") {
		t.Fatal("activate variant pack")
	}
	facts := StyleSheet_StyleDefaultFacts(StyleSheet_StyleKindButton())
	resolved := ResolveActiveStyle(StyleData{}, facts, int32(ButtonStateHover))
	if resolved.Background != 0x00ff00ff || resolved.Radius != 12 {
		t.Fatalf("variant resolution: %+v", resolved)
	}
	if !SetActiveStylePack("lf") {
		t.Fatal("activate base pack")
	}
	resolved = ResolveActiveStyle(StyleData{}, facts, int32(ButtonStateHover))
	if resolved.Background != 0x112233ff || resolved.Radius != 6 {
		t.Fatalf("base resolution: %+v", resolved)
	}
	ClearStylePacks()
}

func TestReleaseTypedStylePackAvoidsHotPathParsingInGo(t *testing.T) {
	ClearStylePacks()
	defer ClearStylePacks()
	source := `@pack release.typed;
tokens { color { accent: #446688; } }
Button { background: accent; radius: 7; opacity: 0.5; }`
	_, rules, err := ParseStyleSheet(source)
	if err != nil || len(rules) == 0 {
		t.Fatalf("parse release source: %v", err)
	}
	facts := StyleSheet_StyleDefaultFacts(StyleSheet_StyleKindButton())
	facts.State = int32(ButtonStateHover)
	reparsed := ResolveStyle(rules, StyleData{}, facts, int32(ButtonStateHover))

	resetStyleParseInvocationCount()
	if !RegisterStylePack(StylePack{ID: "release.typed", Label: "Release Typed", Sheet: rules}) {
		t.Fatal("typed release pack did not register")
	}
	if !SetActiveStylePack("release.typed") {
		t.Fatal("typed release pack did not activate")
	}
	for i := 0; i < 5; i++ {
		resolved := ResolveActiveStyle(StyleData{}, facts, int32(ButtonStateHover))
		if resolved.Background != reparsed.Background || resolved.Radius != reparsed.Radius || resolved.Opacity != reparsed.Opacity {
			t.Fatalf("typed release resolution changed at %d: %+v vs %+v", i, resolved, reparsed)
		}
	}
	if !SetStyleTheme("dark") {
		t.Fatal("theme switch with only typed packs failed")
	}
	if currentStyleParseInvocationCount() != 0 {
		t.Fatalf("typed release pack parsed on hot path/theme switch: %d", currentStyleParseInvocationCount())
	}
}
