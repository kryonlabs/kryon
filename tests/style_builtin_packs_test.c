#include "ui_style_sheet.h"

#include <assert.h>
#include <string.h>

typedef struct StyleKindCase {
    const char *name;
    int kind;
} StyleKindCase;

typedef struct StyleStateCase {
    const char *name;
    int kind;
    int state;
} StyleStateCase;

typedef struct StyleRoleCase {
    const char *name;
    int kind;
    int role;
} StyleRoleCase;

static void
assert_pack_covers_kind(const char *pack_id, StyleKindCase style_kind)
{
    StyleData resolved;

    assert(SetActiveStylePack(pack_id));
    resolved = ResolveActiveStyle((StyleData){0},
        StyleDefaultFacts(style_kind.kind), ButtonStateNormal);
    assert(resolved.fields != 0);
}

static int
pack_has_rule_for_state(const StylePack *pack, int kind, int state)
{
    const StyleSheet *sheet = pack != NULL ? pack->sheet : NULL;

    if(sheet == NULL || sheet->rules == NULL)
        return 0;
    for(int i = 0; i < sheet->rule_count; i++) {
        StyleRule rule = sheet->rules[i];
        if(rule.selector.kind == kind &&
           (rule.selector.state == state || rule.state == state))
            return 1;
    }
    return 0;
}

static void
assert_pack_covers_state(const char *pack_id, StyleStateCase style_state)
{
    const StylePack *pack;

    assert(SetActiveStylePack(pack_id));
    pack = GetActiveStylePack();
    assert(pack != NULL);
    assert(pack_has_rule_for_state(pack, style_state.kind, style_state.state));
}

static int
pack_has_rule_for_role(const StylePack *pack, int kind, int role)
{
    const StyleSheet *sheet = pack != NULL ? pack->sheet : NULL;

    if(sheet == NULL || sheet->rules == NULL)
        return 0;
    for(int i = 0; i < sheet->rule_count; i++) {
        StyleRule rule = sheet->rules[i];
        if(rule.selector.kind == kind && rule.selector.role == role)
            return 1;
    }
    return 0;
}

static void
assert_pack_covers_role(const char *pack_id, StyleRoleCase style_role)
{
    const StylePack *pack;

    assert(SetActiveStylePack(pack_id));
    pack = GetActiveStylePack();
    assert(pack != NULL);
    assert(pack_has_rule_for_role(pack, style_role.kind, style_role.role));
}

int
main(void)
{
    StyleKindCase style_kinds[] = {
        {"App", StyleKindApp()},
        {"Button", StyleKindButton()},
        {"Text", StyleKindText()},
        {"TextField", StyleKindTextField()},
        {"TextArea", StyleKindTextArea()},
        {"Surface", StyleKindSurface()},
        {"Dropdown", StyleKindDropdown()},
        {"Card", StyleKindCard()},
        {"Slider", StyleKindSlider()},
        {"SliderThumb", StyleKindSliderThumb()},
        {"Toggle", StyleKindToggle()},
        {"ToggleThumb", StyleKindToggleThumb()},
        {"Scroll", StyleKindScroll()},
        {"ScrollThumb", StyleKindScrollThumb()},
        {"Checkbox", StyleKindCheckbox()},
        {"Radio", StyleKindRadio()},
        {"Progress", StyleKindProgress()},
        {"Separator", StyleKindSeparator()},
        {"NavigationBar", StyleKindNavigationBar()},
        {"NavigationBarItem", StyleKindNavigationBarItem()},
        {"Selectable", StyleKindSelectable()},
        {"Fieldset", StyleKindFieldset()},
        {"Plot", StyleKindPlot()},
        {"PlotMark", StyleKindPlotMark()},
        {"Link", StyleKindLink()},
        {"TabBar", StyleKindTabBar()},
        {"Tab", StyleKindTab()},
        {"TabClose", StyleKindTabClose()},
        {"SegmentedControl", StyleKindSegmentedControl()},
        {"Segment", StyleKindSegment()},
        {"Menu", StyleKindMenu()},
        {"MenuItem", StyleKindMenuItem()},
        {"MenuSeparator", StyleKindMenuSeparator()},
        {"ListBox", StyleKindListBox()},
        {"ListBoxItem", StyleKindListBoxItem()},
        {"TreeView", StyleKindTreeView()},
        {"TreeViewItem", StyleKindTreeViewItem()},
        {"ListBoxMulti", StyleKindListBoxMulti()},
        {"ListBoxMultiItem", StyleKindListBoxMultiItem()},
        {"DragDropTarget", StyleKindDragDropTarget()},
        {"Spinbox", StyleKindSpinbox()},
        {"SpinboxValue", StyleKindSpinboxValue()},
        {"ColorPicker", StyleKindColorPicker()},
        {"ColorPickerSwatch", StyleKindColorPickerSwatch()},
        {"PanedView", StyleKindPanedView()},
        {"Toast", StyleKindToast()},
        {"Collapsible", StyleKindCollapsible()},
        {"TitleBar", StyleKindTitleBar()},
        {"Toolbar", StyleKindToolbar()},
        {"Modal", StyleKindModal()},
        {"TableView", StyleKindTableView()},
        {"Guide", StyleKindGuide()},
        {"Image", StyleKindImage()},
        {"Focus", StyleKindFocus()},
        {"Popup", StyleKindPopup()},
        {"Canvas", StyleKindCanvas()},
        {"Drag", StyleKindDrag()},
        {"DragValue", StyleKindDragValue()},
        {"Heading", StyleKindHeading()},
        {"ParagraphText", StyleKindParagraphText()},
        {"Page", StyleKindPage()},
        {"Section", StyleKindSection()},
        {"Reorder", StyleKindReorder()},
    };
    StyleStateCase state_kinds[] = {
        {"Button:hover", StyleKindButton(), ButtonStateHover},
        {"Button:pressed", StyleKindButton(), ButtonStatePressed},
        {"Button:focus", StyleKindButton(), ButtonStateFocus},
        {"Button:disabled", StyleKindButton(), ButtonStateDisabled},
        {"Button:loading", StyleKindButton(), ButtonStateLoading},
        {"Dropdown:hover", StyleKindDropdown(), ButtonStateHover},
        {"Dropdown:pressed", StyleKindDropdown(), ButtonStatePressed},
        {"Dropdown:focus", StyleKindDropdown(), ButtonStateFocus},
        {"Dropdown:disabled", StyleKindDropdown(), ButtonStateDisabled},
        {"Dropdown:selected", StyleKindDropdown(), ButtonStateSelected},
        {"Slider:hover", StyleKindSlider(), ButtonStateHover},
        {"Slider:disabled", StyleKindSlider(), ButtonStateDisabled},
        {"SliderThumb:disabled", StyleKindSliderThumb(), ButtonStateDisabled},
        {"ScrollThumb:hover", StyleKindScrollThumb(), ButtonStateHover},
        {"ScrollThumb:pressed", StyleKindScrollThumb(), ButtonStatePressed},
        {"Toggle:hover", StyleKindToggle(), ButtonStateHover},
        {"Toggle:pressed", StyleKindToggle(), ButtonStatePressed},
        {"Toggle:disabled", StyleKindToggle(), ButtonStateDisabled},
        {"ToggleThumb:disabled", StyleKindToggleThumb(), ButtonStateDisabled},
        {"Checkbox:hover", StyleKindCheckbox(), ButtonStateHover},
        {"Checkbox:pressed", StyleKindCheckbox(), ButtonStatePressed},
        {"Checkbox:selected", StyleKindCheckbox(), ButtonStateSelected},
        {"Checkbox:disabled", StyleKindCheckbox(), ButtonStateDisabled},
        {"Radio:hover", StyleKindRadio(), ButtonStateHover},
        {"Radio:selected", StyleKindRadio(), ButtonStateSelected},
        {"Radio:disabled", StyleKindRadio(), ButtonStateDisabled},
        {"NavigationBarItem:hover", StyleKindNavigationBarItem(), ButtonStateHover},
        {"NavigationBarItem:selected", StyleKindNavigationBarItem(), ButtonStateSelected},
        {"NavigationBarItem:disabled", StyleKindNavigationBarItem(), ButtonStateDisabled},
        {"Tab:hover", StyleKindTab(), ButtonStateHover},
        {"Tab:pressed", StyleKindTab(), ButtonStatePressed},
        {"Tab:selected", StyleKindTab(), ButtonStateSelected},
        {"Tab:disabled", StyleKindTab(), ButtonStateDisabled},
        {"TabClose:hover", StyleKindTabClose(), ButtonStateHover},
        {"Segment:hover", StyleKindSegment(), ButtonStateHover},
        {"Segment:pressed", StyleKindSegment(), ButtonStatePressed},
        {"Segment:selected", StyleKindSegment(), ButtonStateSelected},
        {"Segment:disabled", StyleKindSegment(), ButtonStateDisabled},
        {"MenuItem:hover", StyleKindMenuItem(), ButtonStateHover},
        {"MenuItem:pressed", StyleKindMenuItem(), ButtonStatePressed},
        {"MenuItem:selected", StyleKindMenuItem(), ButtonStateSelected},
        {"MenuItem:disabled", StyleKindMenuItem(), ButtonStateDisabled},
        {"ListBoxItem:hover", StyleKindListBoxItem(), ButtonStateHover},
        {"ListBoxItem:selected", StyleKindListBoxItem(), ButtonStateSelected},
        {"ListBoxItem:disabled", StyleKindListBoxItem(), ButtonStateDisabled},
        {"TreeViewItem:hover", StyleKindTreeViewItem(), ButtonStateHover},
        {"TreeViewItem:selected", StyleKindTreeViewItem(), ButtonStateSelected},
        {"TreeViewItem:disabled", StyleKindTreeViewItem(), ButtonStateDisabled},
        {"ListBoxMultiItem:hover", StyleKindListBoxMultiItem(), ButtonStateHover},
        {"ListBoxMultiItem:selected", StyleKindListBoxMultiItem(), ButtonStateSelected},
        {"ListBoxMultiItem:focus", StyleKindListBoxMultiItem(), ButtonStateFocus},
        {"ListBoxMultiItem:disabled", StyleKindListBoxMultiItem(), ButtonStateDisabled},
        {"DragDropTarget:hover", StyleKindDragDropTarget(), ButtonStateHover},
        {"Reorder[role=Handle]:selected", StyleKindReorder(), ButtonStateSelected},
        {"DragValue:disabled", StyleKindDragValue(), ButtonStateDisabled},
        {"Link:hover", StyleKindLink(), ButtonStateHover},
        {"Link:disabled", StyleKindLink(), ButtonStateDisabled},
        {"Focus:focus", StyleKindFocus(), ButtonStateFocus},
    };
    StyleRoleCase role_kinds[] = {
        {"Menu[role=Bar]", StyleKindMenu(), 1},
        {"Menu[role=Popup]", StyleKindMenu(), 2},
        {"Menu[role=Context]", StyleKindMenu(), 3},
        {"Progress[role=Track]", StyleKindProgress(), 4},
        {"Progress[role=Fill]", StyleKindProgress(), 5},
        {"Progress[role=Label]", StyleKindProgress(), 6},
        {"Separator[role=Line]", StyleKindSeparator(), 7},
        {"Separator[role=Label]", StyleKindSeparator(), 6},
        {"Separator[role=Bullet]", StyleKindSeparator(), 8},
        {"Checkbox[role=Box]", StyleKindCheckbox(), 9},
        {"Checkbox[role=Mark]", StyleKindCheckbox(), 10},
        {"Checkbox[role=Label]", StyleKindCheckbox(), 6},
        {"Radio[role=Ring]", StyleKindRadio(), 11},
        {"Radio[role=Mark]", StyleKindRadio(), 10},
        {"Radio[role=Label]", StyleKindRadio(), 6},
        {"Slider[role=Track]", StyleKindSlider(), 4},
        {"Slider[role=Fill]", StyleKindSlider(), 5},
        {"Slider[role=Label]", StyleKindSlider(), 6},
        {"Toggle[role=Track]", StyleKindToggle(), 4},
        {"Toggle[role=Fill]", StyleKindToggle(), 5},
        {"Toggle[role=Label]", StyleKindToggle(), 6},
        {"PanedView[role=Handle]", StyleKindPanedView(), 12},
        {"Toast[role=Label]", StyleKindToast(), 6},
        {"Collapsible[role=Header]", StyleKindCollapsible(), 13},
        {"Collapsible[role=TreeHeader]", StyleKindCollapsible(), 14},
        {"Collapsible[role=Close]", StyleKindCollapsible(), 15},
        {"TitleBar[role=Bar]", StyleKindTitleBar(), 1},
        {"TitleBar[role=Title]", StyleKindTitleBar(), 16},
        {"TitleBar[role=Action]", StyleKindTitleBar(), 17},
        {"Toolbar[role=Bar]", StyleKindToolbar(), 1},
        {"Toolbar[role=Divider]", StyleKindToolbar(), 18},
        {"Toolbar[role=Action]", StyleKindToolbar(), 17},
        {"Toolbar[role=BottomBar]", StyleKindToolbar(), 28},
        {"Toolbar[role=BottomAction]", StyleKindToolbar(), 29},
        {"Guide[role=Bar]", StyleKindGuide(), 1},
        {"Guide[role=Divider]", StyleKindGuide(), 18},
        {"Modal[role=Panel]", StyleKindModal(), 2},
        {"Modal[role=Title]", StyleKindModal(), 16},
        {"Modal[role=Action]", StyleKindModal(), 17},
        {"Modal[role=Scrim]", StyleKindModal(), 19},
        {"Modal[role=Message]", StyleKindModal(), 20},
        {"Modal[role=Close]", StyleKindModal(), 15},
        {"TableView[role=Panel]", StyleKindTableView(), 2},
        {"TableView[role=Header]", StyleKindTableView(), 13},
        {"TableView[role=Divider]", StyleKindTableView(), 18},
        {"TableView[role=Row]", StyleKindTableView(), 21},
        {"TableView[role=Cell]", StyleKindTableView(), 22},
        {"TableView[role=Selection]", StyleKindTableView(), 23},
        {"Guide[role=Panel]", StyleKindGuide(), 2},
        {"Guide[role=Label]", StyleKindGuide(), 6},
        {"Guide[role=Close]", StyleKindGuide(), 15},
        {"Guide[role=Action]", StyleKindGuide(), 17},
        {"Guide[role=Scrim]", StyleKindGuide(), 19},
        {"Guide[role=Anchor]", StyleKindGuide(), 24},
        {"Reorder[role=Handle]", StyleKindReorder(), 12},
        {"Reorder[role=Placeholder]", StyleKindReorder(), 25},
        {"Dropdown[role=Panel]", StyleKindDropdown(), 2},
        {"Dropdown[role=Option]", StyleKindDropdown(), 26},
        {"Dropdown[role=Scrollbar]", StyleKindDropdown(), 27},
        {"Image[role=Label]", StyleKindImage(), 6},
        {"Focus[role=Box]", StyleKindFocus(), 9},
        {"Focus[role=Label]", StyleKindFocus(), 6},
        {"Popup[role=Panel]", StyleKindPopup(), 2},
    };
    const char *pack_ids[] = {
        "material",
        "tk",
        "vanilla",
        "lightfield",
    };
    StyleFacts accent = StyleControlFacts(StyleKindButton(), 0, 0,
        ButtonToneAccent, ButtonEmphasisFilled, ControlSizeMedium,
        ButtonStateHover);
    StyleFacts card = StyleControlFacts(StyleKindCard(), 0, 0,
        ButtonToneNeutral, ButtonEmphasisFilled, ControlSizeLarge,
        ButtonStateNormal);
    StyleFacts surface = StyleDefaultFacts(StyleKindSurface());
    StyleFacts field = StyleDefaultFacts(StyleKindTextField());
    StyleData base = {0};
    StyleData resolved;

    ClearStylePacks();
    assert(RegisterBuiltInStylePacks());
    assert(GetStylePackCount() >= 4);
    assert(strcmp(GetActiveStylePackId(), "material") == 0);
    assert(FindStylePack("material") != NULL);
    assert(FindStylePack("tk") != NULL);
    assert(FindStylePack("vanilla") != NULL);
    assert(FindStylePack("lightfield") != NULL);
    assert(FindStylePack("glow") == NULL);

    for(size_t p = 0; p < sizeof(pack_ids) / sizeof(pack_ids[0]); p++)
        for(size_t k = 0; k < sizeof(style_kinds) / sizeof(style_kinds[0]); k++)
            assert_pack_covers_kind(pack_ids[p], style_kinds[k]);

    for(size_t p = 0; p < sizeof(pack_ids) / sizeof(pack_ids[0]); p++)
        for(size_t k = 0; k < sizeof(state_kinds) / sizeof(state_kinds[0]); k++)
            assert_pack_covers_state(pack_ids[p], state_kinds[k]);

    for(size_t p = 0; p < sizeof(pack_ids) / sizeof(pack_ids[0]); p++)
        for(size_t k = 0; k < sizeof(role_kinds) / sizeof(role_kinds[0]); k++)
            assert_pack_covers_role(pack_ids[p], role_kinds[k]);

    for(size_t p = 0; p < sizeof(pack_ids) / sizeof(pack_ids[0]); p++) {
        StyleFacts selected = StyleControlFacts(StyleKindButton(), 0, 0,
            ButtonToneNeutral, ButtonEmphasisSoft, ControlSizeMedium,
            ButtonStateSelected);
        assert(SetActiveStylePack(pack_ids[p]));
        resolved = ResolveActiveStyle(base, selected, ButtonStateSelected);
        assert(resolved.border_width == 2.0f);
        assert(resolved.border == resolved.focus);
        resolved = ResolveActiveStyle(base, StyleDefaultFacts(StyleKindDropdown()),
                                      ButtonStateNormal);
        assert(resolved.offset_x >= resolved.icon_size / 2.0f);
    }

    assert(SetActiveStylePack("material"));
    resolved = ResolveActiveStyle(base, accent, ButtonStateHover);
    assert(resolved.background == 0xd5bbffffu);
    assert(resolved.foreground == 0x171022ffu);
    assert(resolved.material == MaterialFlat);

    resolved = ResolveActiveStyle(base, card, ButtonStateNormal);
    assert(resolved.background == 0x1a1f29ffu);
    assert(resolved.material == MaterialFlat);

    assert(SetActiveStylePack("lightfield"));
    resolved = ResolveActiveStyle(base, surface, ButtonStateNormal);
    assert(resolved.material == MaterialLightfield);
    assert(resolved.background_end == 0x222936eeu);

    assert(SetActiveStylePack("tk"));
    resolved = ResolveActiveStyle(base, accent, ButtonStateHover);
    assert(resolved.background == 0x2f6bffffu);
    assert(resolved.radius == 3.0f);

    assert(SetActiveStylePack("vanilla"));
    resolved = ResolveActiveStyle(base, accent, ButtonStateHover);
    assert(resolved.background == 0x245be0ffu);
    assert(resolved.material == MaterialFlat);

    assert(SetActiveStylePack("lightfield"));
    resolved = ResolveActiveStyle(base, field, ButtonStateNormal);
    assert(resolved.background_end == 0x2b3342eeu);
    assert(resolved.material == MaterialLightfield);

    assert(EnsureBuiltInStylePacks());
    assert(strcmp(GetActiveStylePackId(), "lightfield") == 0);

    ClearStylePacks();
    assert(EnsureBuiltInStylePacks());
    assert(strcmp(GetActiveStylePackId(), "material") == 0);

    ClearStylePacks();
    return 0;
}
