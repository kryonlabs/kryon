#include "ui_internal.h"

#define UI_TREE_MAX_NODES 4096

static UIWidgetNode ui_tree_nodes[UI_TREE_MAX_NODES];
static int ui_tree_node_count = 0;
static int ui_tree_screen_id = 0;
static int ui_tree_building = 0;

typedef struct UIWidgetOps {
    int (*measure_height)(UIWidgetNode node);
} UIWidgetOps;

static UINodeId
ui_tree_add(int id, UIWidgetKind kind, Rectangle bounds, const void *props)
{
    UIWidgetNode *node;
    int index;

    if(ui_tree_node_count >= UI_TREE_MAX_NODES)
        return -1;
    index = ui_tree_node_count++;
    node = &ui_tree_nodes[index];
    memset(node, 0, sizeof(*node));
    node->id = id;
    node->kind = kind;
    node->bounds = bounds;
    node->props = props;
    node->parent = -1;
    node->first_child = -1;
    node->next_sibling = -1;
    return index;
}

static UIWidgetNode *
ui_tree_node(UINodeId id)
{
    if(id < 0 || id >= ui_tree_node_count)
        return NULL;
    return &ui_tree_nodes[id];
}

static UIWidgetNode
ui_node(int id, UIWidgetKind kind, Rectangle bounds)
{
    UIWidgetNode node;

    memset(&node, 0, sizeof(node));
    node.id = id;
    node.kind = kind;
    node.bounds = bounds;
    node.parent = -1;
    node.first_child = -1;
    node.next_sibling = -1;
    return node;
}

static void
ui_tree_store_node(UINodeId id, UIWidgetNode src)
{
    UIWidgetNode *dst;

    dst = ui_tree_node(id);
    if(dst == NULL)
        return;
    src.parent = dst->parent;
    src.first_child = dst->first_child;
    src.next_sibling = dst->next_sibling;
    *dst = src;
}

static int
ui_measure_bounds_height(UIWidgetNode node)
{
    return (int)ceilf(node.bounds.height);
}

static int
ui_measure_paragraph(UIWidgetNode node)
{
    if(node.props != NULL)
        return ui_paragraph_height(*(const UIParagraph *)node.props);
    return ui_paragraph_height(node.data.paragraph);
}

static int
ui_measure_readonly_text_box(UIWidgetNode node)
{
    const UIReadonlyTextBox *box;

    box = node.props != NULL ? node.props : &node.data.readonly_text_box;
    return ui_readonly_text_box_height(box->text, box->font,
                                       (int)box->bounds.width,
                                       box->style, box->line_gap);
}

static int
ui_measure_label_text_field(UIWidgetNode node)
{
    if(node.props != NULL)
        return ui_label_text_field_height(*(const UILabelTextField *)node.props);
    return ui_label_text_field_height(node.data.label_text_field);
}

static int
ui_measure_section_label(UIWidgetNode node)
{
    if(node.props != NULL)
        return ui_section_label_height(*(const UISectionLabel *)node.props);
    return ui_section_label_height(node.data.section_label);
}

static int
ui_measure_checkbox_row(UIWidgetNode node)
{
    if(node.props != NULL)
        return ui_checkbox_row_height(*(const UICheckboxRow *)node.props);
    return ui_checkbox_row_height(node.data.checkbox_row);
}

static int
ui_measure_button_row(UIWidgetNode node)
{
    if(node.props != NULL)
        return ui_button_row_height(*(const UIButtonRow *)node.props);
    return ui_button_row_height(node.data.button_row);
}

static int
ui_measure_bottom_nav(UIWidgetNode node)
{
    if(node.bounds.height > 0)
        return (int)ceilf(node.bounds.height);
    return ui_bottom_nav_height();
}

static int
ui_measure_tab_bar(UIWidgetNode node)
{
    if(node.bounds.height > 0)
        return (int)ceilf(node.bounds.height);
    return ui_tab_bar_height();
}

static int
ui_measure_theme_settings(UIWidgetNode node)
{
    if(node.props != NULL)
        return ui_theme_settings_height(*(const UIThemeSettings *)node.props);
    return ui_theme_settings_height(node.data.theme_settings);
}

static int
ui_measure_theme_picker(UIWidgetNode node)
{
    return ui_theme_picker_height((int)node.bounds.width);
}

static int
ui_measure_paragraph_modal(UIWidgetNode node)
{
    if(node.props != NULL)
        return ui_paragraph_modal_height(*(const UIParagraphModalMeasure *)node.props);
    return ui_paragraph_modal_height(node.data.paragraph_modal);
}

static int
ui_measure_title_bar(UIWidgetNode node)
{
    if(node.bounds.height > 0)
        return (int)ceilf(node.bounds.height);
    return ui_title_bar_height();
}

static const UIWidgetOps ui_widget_ops[] = {
    [UI_WIDGET_SCREEN_NODE] = {ui_measure_bounds_height},
    [UI_WIDGET_BACKGROUND_NODE] = {ui_measure_bounds_height},
    [UI_WIDGET_TEXT_NODE] = {ui_measure_bounds_height},
    [UI_WIDGET_RECT_NODE] = {ui_measure_bounds_height},
    [UI_WIDGET_LINE_NODE] = {ui_measure_bounds_height},
    [UI_WIDGET_BUTTON_NODE] = {ui_measure_bounds_height},
    [UI_WIDGET_TEXT_FIELD_NODE] = {ui_measure_bounds_height},
    [UI_WIDGET_DROPDOWN_NODE] = {ui_measure_bounds_height},
    [UI_WIDGET_SLIDER_NODE] = {ui_measure_bounds_height},
    [UI_WIDGET_TOGGLE_NODE] = {ui_measure_bounds_height},
    [UI_WIDGET_CHECKBOX_NODE] = {ui_measure_bounds_height},
    [UI_WIDGET_THEME_SETTINGS_NODE] = {ui_measure_theme_settings},
    [UI_WIDGET_PARAGRAPH_NODE] = {ui_measure_paragraph},
    [UI_WIDGET_READONLY_TEXT_BOX_NODE] = {ui_measure_readonly_text_box},
    [UI_WIDGET_LABEL_TEXT_FIELD_NODE] = {ui_measure_label_text_field},
    [UI_WIDGET_SECTION_LABEL_NODE] = {ui_measure_section_label},
    [UI_WIDGET_CHECKBOX_ROW_NODE] = {ui_measure_checkbox_row},
    [UI_WIDGET_BUTTON_ROW_NODE] = {ui_measure_button_row},
    [UI_WIDGET_BOTTOM_NAV_NODE] = {ui_measure_bottom_nav},
    [UI_WIDGET_TAB_BAR_NODE] = {ui_measure_tab_bar},
    [UI_WIDGET_THEME_PICKER_NODE] = {ui_measure_theme_picker},
    [UI_WIDGET_PARAGRAPH_MODAL_NODE] = {ui_measure_paragraph_modal},
    [UI_WIDGET_TITLE_BAR_NODE] = {ui_measure_title_bar},
    [UI_WIDGET_CUSTOM_NODE] = {ui_measure_bounds_height},
};

void
UIBeginTree(int screen_id)
{
    ui_tree_screen_id = screen_id;
    ui_tree_node_count = 0;
    ui_tree_building = 1;
    ui_tree_add(screen_id, UI_WIDGET_SCREEN_NODE,
                (Rectangle){0, 0, ui_view_width, ui_view_height}, NULL);
}

void
UIEndTree(void)
{
    ui_tree_building = 0;
}

void
UIReconcileTree(void)
{
}

void
UILayoutTree(void)
{
}

void
UIRouteInput(void)
{
}

void
UIUpdateTree(void)
{
}

void
UIRenderTree(void)
{
}

void
UIRenderOverlays(void)
{
    UIRenderFrameOverlays();
}

const UIWidgetNode *
UIGetTreeNodes(int *count)
{
    if(count != NULL)
        *count = ui_tree_node_count;
    return ui_tree_nodes;
}

int
UIGetNodeHeight(UIWidgetNode node)
{
    const UIWidgetOps *ops;

    if(node.kind < 0 ||
       node.kind >= (int)(sizeof(ui_widget_ops) / sizeof(ui_widget_ops[0])))
        return ui_measure_bounds_height(node);
    ops = &ui_widget_ops[node.kind];
    if(ops->measure_height == NULL)
        return ui_measure_bounds_height(node);
    return ops->measure_height(node);
}

int
UIGetNodeHeightById(int id)
{
    int i;

    for(i = ui_tree_node_count - 1; i >= 0; i--) {
        if(ui_tree_nodes[i].id == id)
            return UIGetNodeHeight(ui_tree_nodes[i]);
    }
    return 0;
}

UIWidgetNode
UINodeParagraph(UIParagraph paragraph, int x, int y)
{
    UIWidgetNode node;

    node = ui_node(0, UI_WIDGET_PARAGRAPH_NODE,
                   (Rectangle){x, y, paragraph.width, 0});
    node.data.paragraph = paragraph;
    return node;
}

UIWidgetNode
UINodeReadonlyTextBox(UIReadonlyTextBox box)
{
    UIWidgetNode node;

    node = ui_node(0, UI_WIDGET_READONLY_TEXT_BOX_NODE, box.bounds);
    node.data.readonly_text_box = box;
    return node;
}

UIWidgetNode
UINodeLabelTextField(UILabelTextField row, int x, int y, int w)
{
    UIWidgetNode node;

    node = ui_node(row.field.focus_id, UI_WIDGET_LABEL_TEXT_FIELD_NODE,
                   (Rectangle){x, y, w, 0});
    node.data.label_text_field = row;
    return node;
}

UIWidgetNode
UINodeSectionLabel(UISectionLabel label, int x, int y)
{
    UIWidgetNode node;

    node = ui_node(0, UI_WIDGET_SECTION_LABEL_NODE, (Rectangle){x, y, 0, 0});
    node.data.section_label = label;
    return node;
}

UIWidgetNode
UINodeCheckboxRow(UICheckboxRow row, int x, int y)
{
    UIWidgetNode node;

    node = ui_node(0, UI_WIDGET_CHECKBOX_ROW_NODE, (Rectangle){x, y, 0, 0});
    node.data.checkbox_row = row;
    return node;
}

UIWidgetNode
UINodeButtonRow(UIButtonRow row)
{
    UIWidgetNode node;

    node = ui_node(0, UI_WIDGET_BUTTON_ROW_NODE,
                   (Rectangle){row.x, row.y, row.width, 0});
    node.data.button_row = row;
    return node;
}

UIWidgetNode
UINodeBottomNav(UIBottomNav nav)
{
    UIWidgetNode node;
    int height;

    height = nav.height > 0 ? nav.height : 0;
    node = ui_node(0, UI_WIDGET_BOTTOM_NAV_NODE,
                   (Rectangle){0, 0, nav.view_width, height});
    return node;
}

UIWidgetNode
UINodeTopNav(UITopNav nav)
{
    return ui_node(nav.id, UI_WIDGET_CUSTOM_NODE,
                   (Rectangle){nav.x, nav.y, nav.width, nav.height});
}

UIWidgetNode
UINodeTabBar(UITabBar bar)
{
    return ui_node(0, UI_WIDGET_TAB_BAR_NODE, bar.bounds);
}

UIWidgetNode
UINodeThemeSettings(UIThemeSettings settings)
{
    UIWidgetNode node;

    node = ui_node(settings.id_base, UI_WIDGET_THEME_SETTINGS_NODE,
                   (Rectangle){settings.x, settings.y, settings.w, 0});
    node.data.theme_settings = settings;
    return node;
}

UIWidgetNode
UINodeThemePicker(int x, int y, int w)
{
    return ui_node(0, UI_WIDGET_THEME_PICKER_NODE, (Rectangle){x, y, w, 0});
}

UIWidgetNode
UINodeParagraphModal(UIParagraphModalMeasure measure)
{
    UIWidgetNode node;

    node = ui_node(0, UI_WIDGET_PARAGRAPH_MODAL_NODE,
                   (Rectangle){0, 0, measure.width, 0});
    node.data.paragraph_modal = measure;
    return node;
}

UIWidgetNode
UINodeTitleBar(int height)
{
    return ui_node(0, UI_WIDGET_TITLE_BAR_NODE,
                   (Rectangle){0, 0, ui_view_width, height});
}

void
UIBackground(Color color)
{
    ui_tree_add(0, UI_WIDGET_BACKGROUND_NODE,
                (Rectangle){0, 0, ui_view_width, ui_view_height}, NULL);
    DrawRectangleRec((Rectangle){0, 0, GetUIViewWidth(), GetUIViewHeight()},
                     color);
}

void
UITextNode(const char *text, int x, int y, int font_size, Color color)
{
    ui_tree_add(0, UI_WIDGET_TEXT_NODE, (Rectangle){x, y, 0, 0}, text);
    UIRenderText(text, x, y, font_size, color);
}

void
UITextInRectNode(const char *text, Rectangle rect, int font_size, Color color)
{
    ui_tree_add(0, UI_WIDGET_TEXT_NODE, rect, text);
    UIRenderTextInRect(text, rect, font_size, color);
}

void
UIParagraphNode(UIParagraph paragraph, int x, int *y)
{
    UIWidgetNode node;
    UINodeId id;
    int start_y = y != NULL ? *y : 0;

    id = ui_tree_add(0, UI_WIDGET_PARAGRAPH_NODE,
                     (Rectangle){x, start_y, paragraph.width, 0}, NULL);
    node = UINodeParagraph(paragraph, x, start_y);
    ui_tree_store_node(id, node);
    UIRenderParagraph(paragraph, x, y);
}

void
UITextLinesNode(const char **lines, int count, int x, int *y, int font,
                int line_h, Color color)
{
    int start_y = y != NULL ? *y : 0;

    ui_tree_add(0, UI_WIDGET_TEXT_NODE,
                (Rectangle){x, start_y, 0, count * line_h}, lines);
    UIRenderTextLines(lines, count, x, y, font, line_h, color);
}

void
UIRectNode(int x, int y, int w, int h, Color fill, Color border)
{
    ui_tree_add(0, UI_WIDGET_RECT_NODE, (Rectangle){x, y, w, h}, NULL);
    DrawRectangleRec((Rectangle){x, y, w, h}, fill);
    if(border.a != 0)
        DrawRectangleLinesEx((Rectangle){x, y, w, h}, 1, border);
}

void
UILineNode(int x1, int y1, int x2, int y2, Color color)
{
    int x = x1 < x2 ? x1 : x2;
    int y = y1 < y2 ? y1 : y2;
    int w = abs(x2 - x1);
    int h = abs(y2 - y1);

    ui_tree_add(0, UI_WIDGET_LINE_NODE, (Rectangle){x, y, w, h}, NULL);
    DrawLine(x1, y1, x2, y2, color);
}

void
UIBevelNode(int x, int y, int w, int h, Color light, Color dark)
{
    ui_tree_add(0, UI_WIDGET_RECT_NODE, (Rectangle){x, y, w, h}, NULL);
    UIRenderBevel(x, y, w, h, light, dark);
}

int
UIButtonNode(UIButton button)
{
    ui_tree_add(button.focus_id, UI_WIDGET_BUTTON_NODE, button.bounds,
                &button);
    return UIRenderButton(button);
}

int
UIIconButtonNode(UIIconButton button)
{
    ui_tree_add(button.focus_id, UI_WIDGET_BUTTON_NODE, button.bounds,
                &button);
    return UIRenderIconButton(button);
}

int
UIHrefNode(UIHref link)
{
    if(link.bounds.height <= 0)
        link.bounds.height = GetUITextHeight(link.text, link.font);
    ui_tree_add(link.focus_id, UI_WIDGET_TEXT_NODE, link.bounds, &link);
    return UIRenderHref(link);
}

int
UITextInputControlNode(UITextInput input)
{
    ui_tree_add(input.focus_id, UI_WIDGET_TEXT_FIELD_NODE, input.bounds,
                &input);
    return UIRenderTextInputControl(input);
}

int
UIGenericButtonNode(int id, int x, int y, int w, int h,
                    const char *label, UIButtonStyle style,
                    int disabled, int *hover)
{
    ui_tree_add(id, UI_WIDGET_BUTTON_NODE, (Rectangle){x, y, w, h}, hover);
    return UIRenderGenericButton(x, y, w, h, label, style, disabled, hover);
}

int
UITextFieldNode(UITextField field)
{
    ui_tree_add(field.focus_id, UI_WIDGET_TEXT_FIELD_NODE, field.bounds,
                &field);
    return UIRenderTextField(field);
}

int
UIReadonlyTextBoxNode(UIReadonlyTextBox box)
{
    UIWidgetNode node;
    UINodeId id;

    id = ui_tree_add(0, UI_WIDGET_READONLY_TEXT_BOX_NODE, box.bounds, NULL);
    node = UINodeReadonlyTextBox(box);
    ui_tree_store_node(id, node);
    return UIRenderReadonlyTextBox(box);
}

int
UIIconBtnNode(int id, int x, int y, UIIconSize size, Texture2D icon,
              int *hover)
{
    int s = GetUIIconButtonSize(size);

    ui_tree_add(id, UI_WIDGET_BUTTON_NODE, (Rectangle){x, y, s, s}, hover);
    return UIRenderIconBtn(x, y, size, icon, hover);
}

int
UIPaddedIconBtnNode(int id, int x, int y, int size, int padding,
                    Texture2D icon, int *hover)
{
    ui_tree_add(id, UI_WIDGET_BUTTON_NODE,
                (Rectangle){x, y, size + padding * 2, size + padding * 2},
                hover);
    return UIRenderPaddedIconBtn(x, y, size, padding, icon, hover);
}

int
UIInfoButtonNode(int id, int center_x, int center_y, int diameter)
{
    ui_tree_add(id, UI_WIDGET_BUTTON_NODE,
                (Rectangle){center_x - diameter / 2, center_y - diameter / 2,
                            diameter, diameter}, NULL);
    return UIRenderInfoButton(center_x, center_y, diameter);
}

int
UITextButtonNode(int id, int x, int y, const char *label, int *hover)
{
    int font = GetUISmallFontSize();
    int w = MeasureUIText(label != NULL ? label : "", font) + ScaleUIPx(16);
    int h = GetUITextLineHeight(font) + ScaleUIPx(8);

    ui_tree_add(id, UI_WIDGET_BUTTON_NODE, (Rectangle){x, y, w, h}, hover);
    return UIRenderTextButton(x, y, label, hover);
}

void
UIIconLinkNode(int id, int x, int y, int icon_size, Texture2D icon,
               const char *url)
{
    ui_tree_add(id, UI_WIDGET_BUTTON_NODE,
                (Rectangle){x, y, icon_size, icon_size}, url);
    UIRenderIconLink(x, y, icon_size, icon, url);
}

void
UIIconTextureNode(int id, int x, int y, int size, Texture2D icon, Color tint)
{
    ui_tree_add(id, UI_WIDGET_CUSTOM_NODE, (Rectangle){x, y, size, size},
                NULL);
    UIRenderIconTexture(x, y, size, icon, tint);
}

int
UIDropdownNode(int id, int x, int y, int w, int h,
               const char **options, int option_count, int *selected_index)
{
    ui_tree_add(id, UI_WIDGET_DROPDOWN_NODE, (Rectangle){x, y, w, h},
                selected_index);
    return UIRenderDropdown(id, x, y, w, h, options, option_count,
                          selected_index);
}

int
UIDropdownNodeEx(int id, int x, int y, int w, int h,
                 const UIDropdownOption *options, int option_count,
                 int *selected_index)
{
    ui_tree_add(id, UI_WIDGET_DROPDOWN_NODE, (Rectangle){x, y, w, h},
                selected_index);
    return UIRenderDropdownEx(id, x, y, w, h, options, option_count,
                            selected_index);
}

int
UILocaleDropdownNode(int id, int x, int y, int w, int h, int *selected_index)
{
    ui_tree_add(id, UI_WIDGET_DROPDOWN_NODE, (Rectangle){x, y, w, h},
                selected_index);
    return UIRenderLocaleDropdown(id, x, y, w, h, selected_index);
}

int
UISliderNode(int id, int x, int y, int w, const char *label,
             int min, int max, int *value, const char *suffix)
{
    ui_tree_add(id, UI_WIDGET_SLIDER_NODE,
                (Rectangle){x, y, w, ScaleUIPx(56)}, value);
    return UIRenderSlider(id, x, y, w, label, min, max, value, suffix);
}

int
UIVerticalSliderNode(int id, int x, int y, int h, int min, int max,
                     int *value)
{
    ui_tree_add(id, UI_WIDGET_SLIDER_NODE,
                (Rectangle){x - ScaleUIPx(18), y, ScaleUIPx(36), h},
                value);
    return UIRenderVerticalSlider(id, x, y, h, min, max, value);
}

int
UIVerticalSliderWithMarksNode(int id, int x, int y, int h, int min,
                              int max, int *value,
                              UIVerticalSliderMarkCallback callback,
                              void *callback_user_data)
{
    ui_tree_add(id, UI_WIDGET_SLIDER_NODE,
                (Rectangle){x - ScaleUIPx(18), y, ScaleUIPx(36), h},
                value);
    return UIRenderVerticalSliderWithMarks(id, x, y, h, min, max, value,
                                         callback, callback_user_data);
}

int
UIToggleNode(int id, int x, int y, int w, int h, int *value,
             const char *off_label, const char *on_label)
{
    ui_tree_add(id, UI_WIDGET_TOGGLE_NODE, (Rectangle){x, y, w, h}, value);
    (void)id;
    return UIRenderToggleSwitch(x, y, w, h, value, off_label, on_label);
}

int
UICheckboxNode(int id, int x, int y, const char *label, int *value)
{
    ui_tree_add(id, UI_WIDGET_CHECKBOX_NODE, (Rectangle){x, y, 0, 0}, value);
    (void)id;
    return UIRenderCheckboxToggle(x, y, label, value);
}

int
UIThemeSettingsNode(UIThemeSettings settings, UIThemeSettingsState *state,
                    UIThemeSettingsResult *result)
{
    UIWidgetNode node;
    UINodeId id;
    UIThemeSettingsResult next = {0};

    id = ui_tree_add(settings.id_base, UI_WIDGET_THEME_SETTINGS_NODE,
                     (Rectangle){settings.x, settings.y, settings.w, 0},
                     NULL);
    node = UINodeThemeSettings(settings);
    ui_tree_store_node(id, node);
    UIRenderThemeSettings(settings, state);
    next = UIRenderThemeSettingsMenus(settings, state);
    if(result != NULL)
        *result = next;
    return next.changed;
}

void
UISeparatorNode(Rectangle bounds, int vertical)
{
    ui_tree_add(0, UI_WIDGET_CUSTOM_NODE, bounds, NULL);
    UIRenderSeparator(bounds, vertical);
}

UIMenuBarResult
UIMenuBarNode(int id, Rectangle bounds, const UIMenu *menus,
              int menu_count, int *open_index)
{
    ui_tree_add(id, UI_WIDGET_CUSTOM_NODE, bounds, open_index);
    return UIRenderMenuBar(id, bounds, menus, menu_count, open_index);
}

int
UIPopupMenuNode(int id, int x, int y, const UIMenuItem *items,
                int item_count)
{
    ui_tree_add(id, UI_WIDGET_CUSTOM_NODE, (Rectangle){x, y, 0, 0}, items);
    return UIRenderPopupMenu(id, x, y, items, item_count);
}

int
UIRadioNode(UIRadioButton radio)
{
    ui_tree_add(radio.id, UI_WIDGET_CUSTOM_NODE, radio.bounds, &radio);
    return UIRenderRadioButton(radio);
}

void
UIProgressNode(UIProgressBar progress)
{
    ui_tree_add(0, UI_WIDGET_CUSTOM_NODE, progress.bounds, &progress);
    UIRenderProgressBar(progress);
}

int
UISpinboxNode(UISpinbox spinbox)
{
    ui_tree_add(spinbox.id, UI_WIDGET_CUSTOM_NODE, spinbox.bounds, &spinbox);
    return UIRenderSpinbox(spinbox);
}

int
UIComboboxNode(UICombobox combo)
{
    ui_tree_add(combo.id, UI_WIDGET_DROPDOWN_NODE, combo.bounds, &combo);
    return UIRenderCombobox(combo);
}

void
UILabelFrameNode(UILabelFrame frame)
{
    ui_tree_add(0, UI_WIDGET_CUSTOM_NODE, frame.bounds, &frame);
    UIRenderLabelFrame(frame);
}

void
UIImageBoxNode(UIImageBox image)
{
    ui_tree_add(0, UI_WIDGET_CUSTOM_NODE, image.bounds, &image);
    UIRenderImageBox(image);
}

int
UIListBoxNode(UIListBox list)
{
    ui_tree_add(list.id, UI_WIDGET_CUSTOM_NODE, list.bounds, &list);
    return UIRenderListBox(list);
}

int
UITreeViewNode(UITreeView tree)
{
    ui_tree_add(tree.id, UI_WIDGET_CUSTOM_NODE, tree.bounds, &tree);
    return UIRenderTreeView(tree);
}

int
UICascadingTreeViewNode(UICascadingTreeView tree)
{
    ui_tree_add(tree.id, UI_WIDGET_CUSTOM_NODE, tree.bounds, &tree);
    return UIRenderCascadingTreeView(tree);
}

int
UISourceViewNode(UISourceView source)
{
    ui_tree_add(0, UI_WIDGET_CUSTOM_NODE, source.bounds, &source);
    return UIRenderSourceView(source);
}

int
UITableViewNode(UITableView table)
{
    ui_tree_add(table.id, UI_WIDGET_CUSTOM_NODE, table.bounds, &table);
    return UIRenderTableView(table);
}

int
UITextAreaNode(UITextArea area)
{
    ui_tree_add(area.focus_id, UI_WIDGET_TEXT_FIELD_NODE, area.bounds, &area);
    return UIRenderTextArea(area);
}

void
UICanvasGridNode(Rectangle bounds, int step, Color color)
{
    ui_tree_add(0, UI_WIDGET_CUSTOM_NODE, bounds, NULL);
    UIRenderCanvasGrid(bounds, step, color);
}

int
UINotebookNode(UINotebook notebook)
{
    ui_tree_add(0, UI_WIDGET_CUSTOM_NODE, notebook.bounds, &notebook);
    return UIRenderNotebook(notebook);
}

int
UIPanedViewNode(UIPanedView panes)
{
    ui_tree_add(panes.id, UI_WIDGET_CUSTOM_NODE, panes.bounds, &panes);
    return UIRenderPanedView(panes);
}

int
UICollapsibleNode(UICollapsible section)
{
    ui_tree_add(0, UI_WIDGET_CUSTOM_NODE, section.bounds, &section);
    return UIRenderCollapsible(section);
}

int
UIColorPickerNode(Rectangle bounds, Color *color)
{
    ui_tree_add(0, UI_WIDGET_CUSTOM_NODE, bounds, color);
    return UIRenderColorPicker(bounds, color);
}

int
UIActionModalNode(UIModalSpec modal)
{
    ui_tree_add(0, UI_WIDGET_CUSTOM_NODE, (Rectangle){0, 0, 0, 0}, &modal);
    return UIRenderActionModal(modal);
}

int
UIMessageDialogNode(UIMessageDialog dialog)
{
    ui_tree_add(0, UI_WIDGET_CUSTOM_NODE, (Rectangle){0, 0, 0, 0}, &dialog);
    return UIRenderMessageDialog(dialog);
}

int
UIConfirmDialogNode(UIConfirmDialog dialog)
{
    ui_tree_add(0, UI_WIDGET_CUSTOM_NODE, (Rectangle){0, 0, 0, 0}, &dialog);
    return UIRenderConfirmDialog(dialog);
}

int
UIPromptDialogNode(UIPromptDialog dialog)
{
    ui_tree_add(0, UI_WIDGET_CUSTOM_NODE, (Rectangle){0, 0, 0, 0}, &dialog);
    return UIRenderPromptDialog(dialog);
}

void
UIFocusNode(Rectangle bounds)
{
    ui_tree_add(0, UI_WIDGET_CUSTOM_NODE, bounds, NULL);
    UIRenderFocus(bounds);
}

void
UIFocusDebugOverlayNode(const UIAccessibilityNode *nodes, int count)
{
    ui_tree_add(0, UI_WIDGET_CUSTOM_NODE,
                (Rectangle){0, 0, ui_view_width, ui_view_height}, nodes);
    UIRenderFocusDebugOverlay(nodes, count);
}

UIGuideResult
UIGuideOverlayNode(UIGuideOverlay guide)
{
    ui_tree_add(0, UI_WIDGET_CUSTOM_NODE, (Rectangle){0, 0, guide.view_width, guide.view_height}, &guide);
    return UIRenderGuideOverlay(guide);
}

int
UIThemeSwitcherNode(int x, int y, int w, const char *label,
                    const char *light_label, const char *dark_label,
                    int *theme_id, int *dark_mode)
{
    ui_tree_add(0, UI_WIDGET_THEME_SETTINGS_NODE,
                (Rectangle){x, y, w, ScaleUIPx(58)}, theme_id);
    return UIRenderThemeSwitcher(x, y, w, label, light_label, dark_label,
                               theme_id, dark_mode);
}

int
UIThemePickerNode(int x, int y, int w, int dark_mode, int *theme_id)
{
    ui_tree_add(0, UI_WIDGET_THEME_PICKER_NODE,
                (Rectangle){x, y, w, 0}, theme_id);
    return UIRenderThemePicker(x, y, w, dark_mode, theme_id);
}

void
UITutorialImagePlaceholderNode(const char *label, int x, int y, int w, int h)
{
    ui_tree_add(0, UI_WIDGET_CUSTOM_NODE, (Rectangle){x, y, w, h}, label);
    UIRenderTutorialImagePlaceholder(label, x, y, w, h);
}

void
UITutorialImageNode(Texture2D texture, const char *fallback, int x, int y,
                    int w, int h)
{
    ui_tree_add(0, UI_WIDGET_CUSTOM_NODE, (Rectangle){x, y, w, h}, fallback);
    UIRenderTutorialImage(texture, fallback, x, y, w, h);
}

void
UITransitionFadeNode(const UITransition *transition, int width, int height,
                     Color color)
{
    ui_tree_add(0, UI_WIDGET_CUSTOM_NODE, (Rectangle){0, 0, width, height},
                transition);
    UIRenderTransitionFade(transition, width, height, color);
}

void
UIInfoRowsNode(UIInfoRows rows)
{
    ui_tree_add(0, UI_WIDGET_CUSTOM_NODE,
                (Rectangle){rows.x, rows.y, rows.width,
                            rows.row_height * rows.row_count}, &rows);
    UIRenderInfoRows(rows);
}

int
UILabelTextFieldNode(UILabelTextField row, int x, int y, int w)
{
    UIWidgetNode node;
    UINodeId id;

    id = ui_tree_add(row.field.focus_id, UI_WIDGET_LABEL_TEXT_FIELD_NODE,
                     (Rectangle){x, y, w, 0}, NULL);
    node = UINodeLabelTextField(row, x, y, w);
    ui_tree_store_node(id, node);
    return UIRenderLabelTextField(row, x, y, w);
}

int
UISectionLabelNode(UISectionLabel label, int x, int y)
{
    UIWidgetNode node;
    UINodeId id;

    id = ui_tree_add(0, UI_WIDGET_SECTION_LABEL_NODE,
                     (Rectangle){x, y, 0, 0}, NULL);
    node = UINodeSectionLabel(label, x, y);
    ui_tree_store_node(id, node);
    return UIRenderSectionLabel(label, x, y);
}

int
UICheckboxRowNode(UICheckboxRow row, int x, int y)
{
    UIWidgetNode node;
    UINodeId id;

    id = ui_tree_add(0, UI_WIDGET_CHECKBOX_ROW_NODE,
                     (Rectangle){x, y, 0, 0}, NULL);
    node = UINodeCheckboxRow(row, x, y);
    ui_tree_store_node(id, node);
    return UIRenderCheckboxRow(row, x, y);
}

int
UIOverlayButtonNode(UIOverlayButton button)
{
    ui_tree_add(0, UI_WIDGET_BUTTON_NODE, button.bounds, &button);
    return UIRenderOverlayButton(button);
}

int
UIButtonRowNode(UIButtonRow row)
{
    UIWidgetNode node;
    UINodeId id;

    id = ui_tree_add(0, UI_WIDGET_BUTTON_ROW_NODE,
                     (Rectangle){row.x, row.y, row.width, 0}, NULL);
    node = UINodeButtonRow(row);
    ui_tree_store_node(id, node);
    return UIRenderButtonRow(row);
}

int
UIIconSliderPopupNode(UIIconSliderPopup popup)
{
    ui_tree_add(popup.id, UI_WIDGET_CUSTOM_NODE,
                (Rectangle){popup.x, popup.y, 0, 0}, &popup);
    return UIRenderIconSliderPopup(popup);
}

UIIconRowResult
UIBottomIconRowNode(UIBottomIconRow row)
{
    ui_tree_add(0, UI_WIDGET_CUSTOM_NODE,
                (Rectangle){0, 0, row.view_width, row.view_height}, &row);
    return UIRenderBottomIconRow(row);
}

UIBottomNavResult
UIBottomNavNode(UIBottomNav nav)
{
    ui_tree_add(0, UI_WIDGET_BOTTOM_NAV_NODE,
                (Rectangle){0, 0, nav.view_width, nav.view_height}, &nav);
    return UIRenderBottomNav(nav);
}

UIBottomNavConfigResult
UIBottomNavConfigNode(UIBottomNavConfigModal modal)
{
    ui_tree_add(modal.id, UI_WIDGET_CUSTOM_NODE, (Rectangle){0, 0, 0, 0}, &modal);
    return UIRenderBottomNavConfigModal(modal);
}

UITopNavResult
UITopNavNode(UITopNav nav)
{
    ui_tree_add(nav.id, UI_WIDGET_CUSTOM_NODE,
                (Rectangle){nav.x, nav.y, nav.width, nav.height}, &nav);
    return UIRenderTopNav(nav);
}

UIToolbarResult
UIToolbarNode(UIToolbar toolbar)
{
    ui_tree_add(toolbar.id, UI_WIDGET_CUSTOM_NODE,
                (Rectangle){toolbar.x, toolbar.y, toolbar.width, toolbar.height}, &toolbar);
    return UIRenderToolbar(toolbar);
}

UIToolbarHeaderResult
UIToolbarHeaderNode(UIToolbarHeader header)
{
    ui_tree_add(header.toolbar.id, UI_WIDGET_CUSTOM_NODE,
                (Rectangle){0, 0, header.toolbar.width, header.toolbar.height}, &header);
    return UIRenderToolbarHeader(header);
}

int
UISubtabBarNode(UISubtabBar bar)
{
    ui_tree_add(0, UI_WIDGET_TAB_BAR_NODE, bar.bounds, &bar);
    return UIRenderSubtabBar(bar);
}

int
UITabBarNode(UITabBar bar)
{
    ui_tree_add(0, UI_WIDGET_TAB_BAR_NODE, bar.bounds, &bar);
    return UIRenderTabBar(bar);
}

UISidebarAccountHeaderResult
UISidebarAccountHeaderNode(UISidebarAccountHeader header)
{
    ui_tree_add(0, UI_WIDGET_CUSTOM_NODE,
                (Rectangle){header.x, header.y, header.width, header.height}, &header);
    return UIRenderSidebarAccountHeader(header);
}

UIProfilePicturePickerResult
UIProfilePicturePickerNode(UIProfilePicturePickerModal modal)
{
    ui_tree_add(0, UI_WIDGET_CUSTOM_NODE, (Rectangle){0, 0, 0, 0}, &modal);
    return UIRenderProfilePicturePickerModal(modal);
}

void
UIReorderHandleNode(int id, int x, int y, int w, int h, int active)
{
    ui_tree_add(id, UI_WIDGET_CUSTOM_NODE, (Rectangle){x, y, w, h}, NULL);
    UIRenderReorderHandle(x, y, w, h, active);
}

void
UIReorderPlaceholderNode(Rectangle bounds)
{
    ui_tree_add(0, UI_WIDGET_CUSTOM_NODE, bounds, NULL);
    UIRenderReorderPlaceholder(bounds);
}

int
UIModalNode(const char *title, const char *message,
            const char *cancel_btn, const char *confirm_btn)
{
    ui_tree_add(0, UI_WIDGET_CUSTOM_NODE, (Rectangle){0, 0, 0, 0}, title);
    return UIRenderModal(title, message, cancel_btn, confirm_btn);
}

int
UIModal3ButtonNode(const char *title, const char *message,
                   const char *left_btn, const char *middle_btn,
                   const char *right_btn)
{
    ui_tree_add(0, UI_WIDGET_CUSTOM_NODE, (Rectangle){0, 0, 0, 0}, title);
    return UIRenderModal3Button(title, message, left_btn, middle_btn, right_btn);
}

void
UITitleBarNode(const char *title, int height)
{
    ui_tree_add(0, UI_WIDGET_TITLE_BAR_NODE,
                (Rectangle){0, 0, ui_view_width, height}, title);
    UIRenderTitleBar(title, height);
}

int
UIReturnTitleBarNode(Texture2D return_icon, const char *title, int height)
{
    ui_tree_add(0, UI_WIDGET_TITLE_BAR_NODE,
                (Rectangle){0, 0, ui_view_width, height}, title);
    return UIRenderReturnTitleBar(return_icon, title, height);
}

int
UIReturnDropdownTitleBarNode(Texture2D return_icon,
                             UITitleBarDropdown dropdown, int height)
{
    ui_tree_add(dropdown.id, UI_WIDGET_TITLE_BAR_NODE,
                (Rectangle){0, 0, ui_view_width, height}, &dropdown);
    return UIRenderReturnDropdownTitleBar(return_icon, dropdown, height);
}

UIPanelFrame
UIModalFrameNode(int width, int height, const char *title,
                 Texture2D left_icon, Texture2D right_icon)
{
    ui_tree_add(0, UI_WIDGET_CUSTOM_NODE, (Rectangle){0, 0, width, height}, title);
    return UIRenderModalFrame(width, height, title, left_icon, right_icon);
}
