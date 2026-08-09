#include "ui_internal.h"
#include "embedded_assets.h"

#define UI_TREE_MAX_NODES 4096
#define UI_TREE_MAX_DEPTH 128
#define UI_SPRITE_CACHE_MAX 128

typedef struct UISpriteCacheEntry {
    char path[512];
    Texture2D texture;
    int loaded;
} UISpriteCacheEntry;

static UIWidgetNode ui_tree_nodes[UI_TREE_MAX_NODES];
static int ui_tree_node_count = 0;
static int ui_tree_screen_id = 0;
static int ui_tree_building = 0;
static UINodeId ui_tree_stack[UI_TREE_MAX_DEPTH];
static int ui_tree_stack_depth = 0;
static UISpriteCacheEntry ui_sprite_cache[UI_SPRITE_CACHE_MAX];

typedef struct UIWidgetOps {
    int (*measure_height)(UIWidgetNode node);
} UIWidgetOps;

static UIWidgetNode *ui_tree_node(UINodeId id);

static UINodeId
ui_tree_add(int id, UIWidgetKind kind, Rectangle bounds, const void *props)
{
    UIWidgetNode *node;
    UIWidgetNode *parent;
    UINodeId parent_id;
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
    if(ui_tree_building && ui_tree_stack_depth > 0) {
        parent_id = ui_tree_stack[ui_tree_stack_depth - 1];
        parent = ui_tree_node(parent_id);
        if(parent != NULL) {
            node->parent = parent_id;
            if(parent->first_child < 0) {
                parent->first_child = index;
            } else {
                UINodeId child = parent->first_child;

                while(child >= 0 && ui_tree_nodes[child].next_sibling >= 0)
                    child = ui_tree_nodes[child].next_sibling;
                if(child >= 0)
                    ui_tree_nodes[child].next_sibling = index;
            }
        }
    }
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
    [UI_WIDGET_GROUP_NODE] = {ui_measure_bounds_height},
    [UI_WIDGET_SPRITE_NODE] = {ui_measure_bounds_height},
    [UI_WIDGET_CUSTOM_NODE] = {ui_measure_bounds_height},
};

void
UIBeginTree(int screen_id)
{
    UINodeId root;

    ui_tree_screen_id = screen_id;
    ui_tree_node_count = 0;
    ui_tree_building = 1;
    ui_tree_stack_depth = 0;
    root = ui_tree_add(screen_id, UI_WIDGET_SCREEN_NODE,
                       (Rectangle){0, 0, ui_view_width, ui_view_height}, NULL);
    if(root >= 0)
        ui_tree_stack[ui_tree_stack_depth++] = root;
}

void
UIEndTree(void)
{
    ui_tree_building = 0;
    ui_tree_stack_depth = 0;
}

UINodeId
UIBeginNodeGroup(int id, Rectangle bounds)
{
    UINodeId node;

    node = ui_tree_add(id, UI_WIDGET_GROUP_NODE, bounds, NULL);
    if(node >= 0 && ui_tree_stack_depth < UI_TREE_MAX_DEPTH)
        ui_tree_stack[ui_tree_stack_depth++] = node;
    return node;
}

void
UIEndNodeGroup(void)
{
    if(ui_tree_stack_depth > 1)
        ui_tree_stack_depth--;
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
DrawUITree(void)
{
}

void
DrawUIOverlays(void)
{
    DrawUIFrameOverlays();
}

const UIWidgetNode *
UIGetTreeNodes(int *count)
{
    if(count != NULL)
        *count = ui_tree_node_count;
    return ui_tree_nodes;
}

const UIWidgetNode *
UIGetNode(UINodeId id)
{
    return ui_tree_node(id);
}

UINodeId
UIHitTestNode(Vector2 point)
{
    int i;

    for(i = ui_tree_node_count - 1; i >= 0; i--) {
        if(ui_tree_nodes[i].bounds.width <= 0 ||
           ui_tree_nodes[i].bounds.height <= 0)
            continue;
        if(CheckCollisionPointRec(point, ui_tree_nodes[i].bounds))
            return i;
    }
    return -1;
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

static const char *
ui_sprite_file_ext(const char *path)
{
    const char *dot;

    if(path == NULL)
        return "";
    dot = strrchr(path, '.');
    return dot != NULL ? dot : "";
}

static Texture2D
ui_load_sprite_texture(const char *path)
{
    const EmbeddedAsset *asset;
    Image image;
    Texture2D texture = {0};
    int free_slot = -1;

    if(path == NULL || path[0] == '\0')
        return texture;
    for(int i = 0; i < UI_SPRITE_CACHE_MAX; i++) {
        if(ui_sprite_cache[i].loaded &&
           strcmp(ui_sprite_cache[i].path, path) == 0)
            return ui_sprite_cache[i].texture;
        if(!ui_sprite_cache[i].loaded && free_slot < 0)
            free_slot = i;
    }
    if(FileExists(path))
        texture = LoadTexture(path);
    else {
        asset = GetEmbeddedAsset(path);
        if(asset == NULL)
            return texture;
        image = LoadImageFromMemory(ui_sprite_file_ext(path), asset->data,
                                    (int)asset->size);
        if(image.data == NULL)
            return texture;
        texture = LoadTextureFromImage(image);
        UnloadImage(image);
    }
    if(texture.id != 0 && free_slot >= 0) {
        snprintf(ui_sprite_cache[free_slot].path,
                 sizeof(ui_sprite_cache[free_slot].path), "%s", path);
        ui_sprite_cache[free_slot].texture = texture;
        ui_sprite_cache[free_slot].loaded = 1;
    }
    return texture;
}

static Rectangle
ui_sprite_fit_rect(UISprite sprite, Texture2D texture)
{
    Rectangle dst = sprite.bounds;
    float src_w = sprite.source.width != 0.0f ? fabsf(sprite.source.width)
                                              : (float)texture.width;
    float src_h = sprite.source.height != 0.0f ? fabsf(sprite.source.height)
                                               : (float)texture.height;
    float scale;

    if(src_w <= 0.0f || src_h <= 0.0f || dst.width <= 0.0f || dst.height <= 0.0f)
        return dst;
    if(sprite.fit == UI_SPRITE_FIT_CONTAIN || sprite.fit == UI_SPRITE_FIT_COVER) {
        float sx = dst.width / src_w;
        float sy = dst.height / src_h;

        scale = sprite.fit == UI_SPRITE_FIT_COVER
                    ? (sx > sy ? sx : sy)
                    : (sx < sy ? sx : sy);
        dst.width = src_w * scale;
        dst.height = src_h * scale;
        dst.x = sprite.bounds.x + (sprite.bounds.width - dst.width) * 0.5f;
        dst.y = sprite.bounds.y + (sprite.bounds.height - dst.height) * 0.5f;
    }
    return dst;
}

void
UISpriteNode(UISprite sprite)
{
    Texture2D texture;
    Rectangle source;
    Rectangle dst;
    Color tint;

    ui_tree_add(0, UI_WIDGET_SPRITE_NODE, sprite.bounds, sprite.asset_path);
    texture = ui_load_sprite_texture(sprite.asset_path);
    tint = sprite.tint.a == 0 ? WHITE : sprite.tint;
    if(texture.id == 0) {
        DrawRectangleRec(sprite.bounds, GetThemeSurface());
        DrawRectangleLinesEx(sprite.bounds, 1.0f, GetThemeButtonHover());
        DrawUIText("Missing image", (int)sprite.bounds.x + ScaleUIPx(8),
                   (int)sprite.bounds.y + ScaleUIPx(8), UI_TEXT_12,
                   GetThemeIcon());
        return;
    }
    source = sprite.source;
    if(source.width == 0.0f || source.height == 0.0f)
        source = (Rectangle){0, 0, (float)texture.width, (float)texture.height};
    dst = ui_sprite_fit_rect(sprite, texture);
    DrawTexturePro(texture, source, dst, sprite.origin, sprite.rotation, tint);
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
    DrawUIText(text, x, y, font_size, color);
}

void
UITextInRectNode(const char *text, Rectangle rect, int font_size, Color color)
{
    ui_tree_add(0, UI_WIDGET_TEXT_NODE, rect, text);
    DrawUITextInRect(text, rect, font_size, color);
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
    DrawUIParagraph(paragraph, x, y);
}

void
UITextLinesNode(const char **lines, int count, int x, int *y, int font,
                int line_h, Color color)
{
    int start_y = y != NULL ? *y : 0;

    ui_tree_add(0, UI_WIDGET_TEXT_NODE,
                (Rectangle){x, start_y, 0, count * line_h}, lines);
    DrawUITextLines(lines, count, x, y, font, line_h, color);
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
    DrawUIBevel(x, y, w, h, light, dark);
}

int
UIButtonNode(UIButton button)
{
    ui_tree_add(button.focus_id, UI_WIDGET_BUTTON_NODE, button.bounds,
                &button);
    return DrawUIButton(button);
}

int
UIIconButtonNode(UIIconButton button)
{
    ui_tree_add(button.focus_id, UI_WIDGET_BUTTON_NODE, button.bounds,
                &button);
    return DrawUIIconButton(button);
}

int
UIHrefNode(UIHref link)
{
    if(link.bounds.height <= 0)
        link.bounds.height = GetUITextHeight(link.text, link.font);
    ui_tree_add(link.focus_id, UI_WIDGET_TEXT_NODE, link.bounds, &link);
    return DrawUIHref(link);
}

int
UITextInputControlNode(UITextInput input)
{
    ui_tree_add(input.focus_id, UI_WIDGET_TEXT_FIELD_NODE, input.bounds,
                &input);
    return DrawUITextInputControl(input);
}

int
UIGenericButtonNode(int id, int x, int y, int w, int h,
                    const char *label, UIButtonStyle style,
                    int disabled, int *hover)
{
    ui_tree_add(id, UI_WIDGET_BUTTON_NODE, (Rectangle){x, y, w, h}, hover);
    return DrawUIGenericButton(x, y, w, h, label, style, disabled, hover);
}

int
UITextFieldNode(UITextField field)
{
    ui_tree_add(field.focus_id, UI_WIDGET_TEXT_FIELD_NODE, field.bounds,
                &field);
    return DrawUITextField(field);
}

int
UIReadonlyTextBoxNode(UIReadonlyTextBox box)
{
    UIWidgetNode node;
    UINodeId id;

    id = ui_tree_add(0, UI_WIDGET_READONLY_TEXT_BOX_NODE, box.bounds, NULL);
    node = UINodeReadonlyTextBox(box);
    ui_tree_store_node(id, node);
    return DrawUIReadonlyTextBox(box);
}

int
UIIconBtnNode(int id, int x, int y, UIIconSize size, Texture2D icon,
              int *hover)
{
    int s = GetUIIconButtonSize(size);

    ui_tree_add(id, UI_WIDGET_BUTTON_NODE, (Rectangle){x, y, s, s}, hover);
    return DrawUIIconBtn(x, y, size, icon, hover);
}

int
UIPaddedIconBtnNode(int id, int x, int y, int size, int padding,
                    Texture2D icon, int *hover)
{
    ui_tree_add(id, UI_WIDGET_BUTTON_NODE,
                (Rectangle){x, y, size + padding * 2, size + padding * 2},
                hover);
    return DrawUIPaddedIconBtn(x, y, size, padding, icon, hover);
}

int
UIInfoButtonNode(int id, int center_x, int center_y, int diameter)
{
    ui_tree_add(id, UI_WIDGET_BUTTON_NODE,
                (Rectangle){center_x - diameter / 2, center_y - diameter / 2,
                            diameter, diameter}, NULL);
    return DrawUIInfoButton(center_x, center_y, diameter);
}

int
UITextButtonNode(int id, int x, int y, const char *label, int *hover)
{
    int font = GetUISmallFontSize();
    int w = MeasureUIText(label != NULL ? label : "", font) + ScaleUIPx(16);
    int h = GetUITextLineHeight(font) + ScaleUIPx(8);

    ui_tree_add(id, UI_WIDGET_BUTTON_NODE, (Rectangle){x, y, w, h}, hover);
    return DrawUITextButton(x, y, label, hover);
}

void
UIIconLinkNode(int id, int x, int y, int icon_size, Texture2D icon,
               const char *url)
{
    ui_tree_add(id, UI_WIDGET_BUTTON_NODE,
                (Rectangle){x, y, icon_size, icon_size}, url);
    DrawUIIconLink(x, y, icon_size, icon, url);
}

void
UIIconTextureNode(int id, int x, int y, int size, Texture2D icon, Color tint)
{
    ui_tree_add(id, UI_WIDGET_CUSTOM_NODE, (Rectangle){x, y, size, size},
                NULL);
    DrawUIIconTexture(x, y, size, icon, tint);
}

int
UIDropdownNode(int id, int x, int y, int w, int h,
               const char **options, int option_count, int *selected_index)
{
    ui_tree_add(id, UI_WIDGET_DROPDOWN_NODE, (Rectangle){x, y, w, h},
                selected_index);
    return DrawUIDropdown(id, x, y, w, h, options, option_count,
                          selected_index);
}

int
UIDropdownNodeEx(int id, int x, int y, int w, int h,
                 const UIDropdownOption *options, int option_count,
                 int *selected_index)
{
    ui_tree_add(id, UI_WIDGET_DROPDOWN_NODE, (Rectangle){x, y, w, h},
                selected_index);
    return DrawUIDropdownEx(id, x, y, w, h, options, option_count,
                            selected_index);
}

int
UILocaleDropdownNode(int id, int x, int y, int w, int h, int *selected_index)
{
    ui_tree_add(id, UI_WIDGET_DROPDOWN_NODE, (Rectangle){x, y, w, h},
                selected_index);
    return DrawUILocaleDropdown(id, x, y, w, h, selected_index);
}

int
UISliderNode(int id, int x, int y, int w, const char *label,
             int min, int max, int *value, const char *suffix)
{
    ui_tree_add(id, UI_WIDGET_SLIDER_NODE,
                (Rectangle){x, y, w, ScaleUIPx(56)}, value);
    return DrawUISlider(id, x, y, w, label, min, max, value, suffix);
}

int
UIVerticalSliderNode(int id, int x, int y, int h, int min, int max,
                     int *value)
{
    ui_tree_add(id, UI_WIDGET_SLIDER_NODE,
                (Rectangle){x - ScaleUIPx(18), y, ScaleUIPx(36), h},
                value);
    return DrawUIVerticalSlider(id, x, y, h, min, max, value);
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
    return DrawUIVerticalSliderWithMarks(id, x, y, h, min, max, value,
                                         callback, callback_user_data);
}

int
UIToggleNode(int id, int x, int y, int w, int h, int *value,
             const char *off_label, const char *on_label)
{
    ui_tree_add(id, UI_WIDGET_TOGGLE_NODE, (Rectangle){x, y, w, h}, value);
    (void)id;
    return DrawUIToggleSwitch(x, y, w, h, value, off_label, on_label);
}

int
UICheckboxNode(int id, int x, int y, const char *label, int *value)
{
    ui_tree_add(id, UI_WIDGET_CHECKBOX_NODE, (Rectangle){x, y, 0, 0}, value);
    (void)id;
    return DrawUICheckboxToggle(x, y, label, value);
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
    DrawUIThemeSettings(settings, state);
    next = DrawUIThemeSettingsMenus(settings, state);
    if(result != NULL)
        *result = next;
    return next.changed;
}

void
UISeparatorNode(Rectangle bounds, int vertical)
{
    ui_tree_add(0, UI_WIDGET_CUSTOM_NODE, bounds, NULL);
    DrawUISeparator(bounds, vertical);
}

UIMenuBarResult
UIMenuBarNode(int id, Rectangle bounds, const UIMenu *menus,
              int menu_count, int *open_index)
{
    ui_tree_add(id, UI_WIDGET_CUSTOM_NODE, bounds, open_index);
    return DrawUIMenuBar(id, bounds, menus, menu_count, open_index);
}

int
UIPopupMenuNode(int id, int x, int y, const UIMenuItem *items,
                int item_count)
{
    ui_tree_add(id, UI_WIDGET_CUSTOM_NODE, (Rectangle){x, y, 0, 0}, items);
    return DrawUIPopupMenu(id, x, y, items, item_count);
}

int
UIRadioNode(UIRadioButton radio)
{
    ui_tree_add(radio.id, UI_WIDGET_CUSTOM_NODE, radio.bounds, &radio);
    return DrawUIRadioButton(radio);
}

void
UIProgressNode(UIProgressBar progress)
{
    ui_tree_add(0, UI_WIDGET_CUSTOM_NODE, progress.bounds, &progress);
    DrawUIProgressBar(progress);
}

int
UISpinboxNode(UISpinbox spinbox)
{
    ui_tree_add(spinbox.id, UI_WIDGET_CUSTOM_NODE, spinbox.bounds, &spinbox);
    return DrawUISpinbox(spinbox);
}

int
UIComboboxNode(UICombobox combo)
{
    ui_tree_add(combo.id, UI_WIDGET_DROPDOWN_NODE, combo.bounds, &combo);
    return DrawUICombobox(combo);
}

void
UILabelFrameNode(UILabelFrame frame)
{
    ui_tree_add(0, UI_WIDGET_CUSTOM_NODE, frame.bounds, &frame);
    DrawUILabelFrame(frame);
}

void
UIImageBoxNode(UIImageBox image)
{
    ui_tree_add(0, UI_WIDGET_CUSTOM_NODE, image.bounds, &image);
    DrawUIImageBox(image);
}

int
UIListBoxNode(UIListBox list)
{
    ui_tree_add(list.id, UI_WIDGET_CUSTOM_NODE, list.bounds, &list);
    return DrawUIListBox(list);
}

int
UITreeViewNode(UITreeView tree)
{
    ui_tree_add(tree.id, UI_WIDGET_CUSTOM_NODE, tree.bounds, &tree);
    return DrawUITreeView(tree);
}

int
UICascadingTreeViewNode(UICascadingTreeView tree)
{
    ui_tree_add(tree.id, UI_WIDGET_CUSTOM_NODE, tree.bounds, &tree);
    return DrawUICascadingTreeView(tree);
}

int
UISourceViewNode(UISourceView source)
{
    ui_tree_add(0, UI_WIDGET_CUSTOM_NODE, source.bounds, &source);
    return DrawUISourceView(source);
}

int
UITableViewNode(UITableView table)
{
    ui_tree_add(table.id, UI_WIDGET_CUSTOM_NODE, table.bounds, &table);
    return DrawUITableView(table);
}

int
UITextAreaNode(UITextArea area)
{
    ui_tree_add(area.focus_id, UI_WIDGET_TEXT_FIELD_NODE, area.bounds, &area);
    return DrawUITextArea(area);
}

void
UICanvasGridNode(Rectangle bounds, int step, Color color)
{
    ui_tree_add(0, UI_WIDGET_CUSTOM_NODE, bounds, NULL);
    DrawUICanvasGrid(bounds, step, color);
}

int
UINotebookNode(UINotebook notebook)
{
    ui_tree_add(0, UI_WIDGET_CUSTOM_NODE, notebook.bounds, &notebook);
    return DrawUINotebook(notebook);
}

int
UIPanedViewNode(UIPanedView panes)
{
    ui_tree_add(panes.id, UI_WIDGET_CUSTOM_NODE, panes.bounds, &panes);
    return DrawUIPanedView(panes);
}

int
UICollapsibleNode(UICollapsible section)
{
    ui_tree_add(0, UI_WIDGET_CUSTOM_NODE, section.bounds, &section);
    return DrawUICollapsible(section);
}

int
UIColorPickerNode(Rectangle bounds, Color *color)
{
    ui_tree_add(0, UI_WIDGET_CUSTOM_NODE, bounds, color);
    return DrawUIColorPicker(bounds, color);
}

int
UIActionModalNode(UIModalSpec modal)
{
    ui_tree_add(0, UI_WIDGET_CUSTOM_NODE, (Rectangle){0, 0, 0, 0}, &modal);
    return DrawUIActionModal(modal);
}

int
UIMessageDialogNode(UIMessageDialog dialog)
{
    ui_tree_add(0, UI_WIDGET_CUSTOM_NODE, (Rectangle){0, 0, 0, 0}, &dialog);
    return DrawUIMessageDialog(dialog);
}

int
UIConfirmDialogNode(UIConfirmDialog dialog)
{
    ui_tree_add(0, UI_WIDGET_CUSTOM_NODE, (Rectangle){0, 0, 0, 0}, &dialog);
    return DrawUIConfirmDialog(dialog);
}

int
UIPromptDialogNode(UIPromptDialog dialog)
{
    ui_tree_add(0, UI_WIDGET_CUSTOM_NODE, (Rectangle){0, 0, 0, 0}, &dialog);
    return DrawUIPromptDialog(dialog);
}

void
UIFocusNode(Rectangle bounds)
{
    ui_tree_add(0, UI_WIDGET_CUSTOM_NODE, bounds, NULL);
    DrawUIFocus(bounds);
}

void
UIFocusDebugOverlayNode(const UIAccessibilityNode *nodes, int count)
{
    ui_tree_add(0, UI_WIDGET_CUSTOM_NODE,
                (Rectangle){0, 0, ui_view_width, ui_view_height}, nodes);
    DrawUIFocusDebugOverlay(nodes, count);
}

UIGuideResult
UIGuideOverlayNode(UIGuideOverlay guide)
{
    ui_tree_add(0, UI_WIDGET_CUSTOM_NODE, (Rectangle){0, 0, guide.view_width, guide.view_height}, &guide);
    return DrawUIGuideOverlay(guide);
}

int
UIThemeSwitcherNode(int x, int y, int w, const char *label,
                    const char *light_label, const char *dark_label,
                    int *theme_id, int *dark_mode)
{
    ui_tree_add(0, UI_WIDGET_THEME_SETTINGS_NODE,
                (Rectangle){x, y, w, ScaleUIPx(58)}, theme_id);
    return DrawUIThemeSwitcher(x, y, w, label, light_label, dark_label,
                               theme_id, dark_mode);
}

int
UIThemePickerNode(int x, int y, int w, int dark_mode, int *theme_id)
{
    ui_tree_add(0, UI_WIDGET_THEME_PICKER_NODE,
                (Rectangle){x, y, w, 0}, theme_id);
    return DrawUIThemePicker(x, y, w, dark_mode, theme_id);
}

void
UITutorialImagePlaceholderNode(const char *label, int x, int y, int w, int h)
{
    ui_tree_add(0, UI_WIDGET_CUSTOM_NODE, (Rectangle){x, y, w, h}, label);
    DrawUITutorialImagePlaceholder(label, x, y, w, h);
}

void
UITutorialImageNode(Texture2D texture, const char *fallback, int x, int y,
                    int w, int h)
{
    ui_tree_add(0, UI_WIDGET_CUSTOM_NODE, (Rectangle){x, y, w, h}, fallback);
    DrawUITutorialImage(texture, fallback, x, y, w, h);
}

void
UITransitionFadeNode(const UITransition *transition, int width, int height,
                     Color color)
{
    ui_tree_add(0, UI_WIDGET_CUSTOM_NODE, (Rectangle){0, 0, width, height},
                transition);
    DrawUITransitionFade(transition, width, height, color);
}

void
UIInfoRowsNode(UIInfoRows rows)
{
    ui_tree_add(0, UI_WIDGET_CUSTOM_NODE,
                (Rectangle){rows.x, rows.y, rows.width,
                            rows.row_height * rows.row_count}, &rows);
    DrawUIInfoRows(rows);
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
    return DrawUILabelTextField(row, x, y, w);
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
    return DrawUISectionLabel(label, x, y);
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
    return DrawUICheckboxRow(row, x, y);
}

int
UIOverlayButtonNode(UIOverlayButton button)
{
    ui_tree_add(0, UI_WIDGET_BUTTON_NODE, button.bounds, &button);
    return DrawUIOverlayButton(button);
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
    return DrawUIButtonRow(row);
}

int
UIIconSliderPopupNode(UIIconSliderPopup popup)
{
    ui_tree_add(popup.id, UI_WIDGET_CUSTOM_NODE,
                (Rectangle){popup.x, popup.y, 0, 0}, &popup);
    return DrawUIIconSliderPopup(popup);
}

UIIconRowResult
UIBottomIconRowNode(UIBottomIconRow row)
{
    ui_tree_add(0, UI_WIDGET_CUSTOM_NODE,
                (Rectangle){0, 0, row.view_width, row.view_height}, &row);
    return DrawUIBottomIconRow(row);
}

UIBottomNavResult
UIBottomNavNode(UIBottomNav nav)
{
    ui_tree_add(0, UI_WIDGET_BOTTOM_NAV_NODE,
                (Rectangle){0, 0, nav.view_width, nav.view_height}, &nav);
    return DrawUIBottomNav(nav);
}

UIBottomNavConfigResult
UIBottomNavConfigNode(UIBottomNavConfigModal modal)
{
    ui_tree_add(modal.id, UI_WIDGET_CUSTOM_NODE, (Rectangle){0, 0, 0, 0}, &modal);
    return DrawUIBottomNavConfigModal(modal);
}

UITopNavResult
UITopNavNode(UITopNav nav)
{
    ui_tree_add(nav.id, UI_WIDGET_CUSTOM_NODE,
                (Rectangle){nav.x, nav.y, nav.width, nav.height}, &nav);
    return DrawUITopNav(nav);
}

UIToolbarResult
UIToolbarNode(UIToolbar toolbar)
{
    ui_tree_add(toolbar.id, UI_WIDGET_CUSTOM_NODE,
                (Rectangle){toolbar.x, toolbar.y, toolbar.width, toolbar.height}, &toolbar);
    return DrawUIToolbar(toolbar);
}

UIToolbarHeaderResult
UIToolbarHeaderNode(UIToolbarHeader header)
{
    ui_tree_add(header.toolbar.id, UI_WIDGET_CUSTOM_NODE,
                (Rectangle){0, 0, header.toolbar.width, header.toolbar.height}, &header);
    return DrawUIToolbarHeader(header);
}

int
UISubtabBarNode(UISubtabBar bar)
{
    ui_tree_add(0, UI_WIDGET_TAB_BAR_NODE, bar.bounds, &bar);
    return DrawUISubtabBar(bar);
}

int
UITabBarNode(UITabBar bar)
{
    ui_tree_add(0, UI_WIDGET_TAB_BAR_NODE, bar.bounds, &bar);
    return DrawUITabBar(bar);
}

UISidebarAccountHeaderResult
UISidebarAccountHeaderNode(UISidebarAccountHeader header)
{
    ui_tree_add(0, UI_WIDGET_CUSTOM_NODE,
                (Rectangle){header.x, header.y, header.width, header.height}, &header);
    return DrawUISidebarAccountHeader(header);
}

UIProfilePicturePickerResult
UIProfilePicturePickerNode(UIProfilePicturePickerModal modal)
{
    ui_tree_add(0, UI_WIDGET_CUSTOM_NODE, (Rectangle){0, 0, 0, 0}, &modal);
    return DrawUIProfilePicturePickerModal(modal);
}

void
UIReorderHandleNode(int id, int x, int y, int w, int h, int active)
{
    ui_tree_add(id, UI_WIDGET_CUSTOM_NODE, (Rectangle){x, y, w, h}, NULL);
    DrawUIReorderHandle(x, y, w, h, active);
}

void
UIReorderPlaceholderNode(Rectangle bounds)
{
    ui_tree_add(0, UI_WIDGET_CUSTOM_NODE, bounds, NULL);
    DrawUIReorderPlaceholder(bounds);
}

int
UIModalNode(const char *title, const char *message,
            const char *cancel_btn, const char *confirm_btn)
{
    ui_tree_add(0, UI_WIDGET_CUSTOM_NODE, (Rectangle){0, 0, 0, 0}, title);
    return DrawUIModal(title, message, cancel_btn, confirm_btn);
}

int
UIModal3ButtonNode(const char *title, const char *message,
                   const char *left_btn, const char *middle_btn,
                   const char *right_btn)
{
    ui_tree_add(0, UI_WIDGET_CUSTOM_NODE, (Rectangle){0, 0, 0, 0}, title);
    return DrawUIModal3Button(title, message, left_btn, middle_btn, right_btn);
}

void
UITitleBarNode(const char *title, int height)
{
    ui_tree_add(0, UI_WIDGET_TITLE_BAR_NODE,
                (Rectangle){0, 0, ui_view_width, height}, title);
    DrawUITitleBar(title, height);
}

int
UIReturnTitleBarNode(Texture2D return_icon, const char *title, int height)
{
    ui_tree_add(0, UI_WIDGET_TITLE_BAR_NODE,
                (Rectangle){0, 0, ui_view_width, height}, title);
    return DrawUIReturnTitleBar(return_icon, title, height);
}

int
UIReturnDropdownTitleBarNode(Texture2D return_icon,
                             UITitleBarDropdown dropdown, int height)
{
    ui_tree_add(dropdown.id, UI_WIDGET_TITLE_BAR_NODE,
                (Rectangle){0, 0, ui_view_width, height}, &dropdown);
    return DrawUIReturnDropdownTitleBar(return_icon, dropdown, height);
}

UIPanelFrame
UIModalFrameNode(int width, int height, const char *title,
                 Texture2D left_icon, Texture2D right_icon)
{
    ui_tree_add(0, UI_WIDGET_CUSTOM_NODE, (Rectangle){0, 0, width, height}, title);
    return DrawUIModalFrame(width, height, title, left_icon, right_icon);
}
