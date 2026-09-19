#ifndef INTERFACE_INTERNAL_H
#define INTERFACE_INTERNAL_H

#include "ui_draw.h"
#include "ui_tree.h"
#include "ui_tree_node_internal.h"
#include "ui_clip.h"
#include "theme.h"
#include "ui_color.h"
#include "ui_window.h"
#include "ui_dpi.h"
#include "ui_icons.h"
#include "ui_layout.h"
#include "ui_scaling.h"
#include "ui_transition.h"
#include "kryon.h"
#include "kry_input.h"
#include "ui_numeric_internal.h"
#include "ui_scroll_internal.h"
#include "ui_widget_internal.h"
#include "ui_rows_internal.h"
#include "ui_text_layout.h"
#include "ui_grapheme.h"
#include "runtime/text_input.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(PLATFORM_WEB)
#include <emscripten.h>
#endif

extern Color c_text, c_bg, c_surface, c_circle, c_button;
extern Camera2D g_ui_camera;
void ui_begin_world_clip(Rectangle rect);
int ui_text_fits_bounds(const char *text, int x, int y, int font_size,
                        Rectangle bounds);
extern Texture2D g_ui_gear_icon;
extern Texture2D g_ui_x_icon;
extern unsigned long g_ui_frame_serial;
extern float g_theme_content_alpha;
void DrawIcon(IconType type, Rectangle bounds, Color tint);
void DrawProfileImageIcon(IconType type, Rectangle bounds, int dark_mode);
int GetFontSize(void);
int GetSmallFontSize(void);
int GetTitleFontSize(const char *title, int max_width);
int FitFontSize(const char *text, int max_width,
                int preferred_size, int min_size);
int ContentDisabled(void);
int ui_current_input_clip(Rectangle *bounds);
extern int g_ui_slider_active_id;
extern int g_ui_pointer_dragging;
extern int g_ui_pointer_owner;
extern int g_ui_scroll_gesture_pending;

typedef struct {
    Rectangle bounds;
    int view_width;
    int view_height;
    Color scrim;
    int dismiss_disabled;
} DismissibleOverlayProps;

typedef struct {
    int closed;
    int outside_released;
    int release_consumed;
} DismissibleOverlayResult;

DismissibleOverlayResult DismissibleOverlay(DismissibleOverlayProps overlay);

typedef struct {
    int font_size;
    Color color;
    int italic;
    int selectable;
} TextStyle;

typedef struct {
    int id;
    const char *text;
    Rectangle bounds;
    int font_size;
    int line_gap;
    Color color;
} SelectableTextBlock;

typedef struct {
    Rectangle anchor;
    const char *text;
} GuideStep;

typedef struct {
    const GuideStep *steps;
    int count;
    int *step;
    int view_width;
    int view_height;
    int reserved_top;
    int reserved_bottom;
    int max_width;
    int line_gap;
    int paragraph_font;
    Texture2D close_icon;
    Texture2D back_icon;
    Texture2D next_icon;
    Texture2D done_icon;
} GuideOverlayProps;

typedef struct {
    int closed;
    int finished;
    int changed;
    int step;
} GuideResult;

typedef struct {
    int valid;
    int step;
    int count;
    int paragraph_height;
    int text_clip_height;
    int text_clipped;
    Rectangle tip;
    Rectangle text;
    Rectangle close_button;
    Rectangle back_button;
    Rectangle next_button;
} GuideOverlayDebug;

typedef struct {
    const char *text;
    int font;
    Color color;
} InfoRow;

typedef struct TextInputAppearance {
    uint32_t fields;
    Color background;
    Color border;
    Color focus_border;
    Color text;
    Color cursor;
    float radius;
    int padding_x;
    int padding_y;
    int line_gap;
} TextInputAppearance;

/* Prepared retained painting only: no editing-state pointers survive
 * submission. This is internal host storage, not a public widget surface. */
typedef struct TextInputPaint {
    TextInputAppearance appearance;
    int class_name;
    int cursor;
    int focused;
    int editable;
    int caret;
    int font;
    int font_token;
    int selection_start;
    int selection_end;
    int composition_start;
    int composition_end;
    int scroll_x;
} TextInputPaint;

typedef struct {
    int x;
    int y;
    int width;
    int row_height;
    int padding_x;
    const InfoRow *rows;
    int row_count;
    Color background;
    Color separator;
    Color default_text;
} InfoRowsProps;

typedef struct {
    int id;
    int x;
    int y;
    int icon_size;
    int icon_padding;
    Texture2D icon;
    int *open;
    int *value;
    int min;
    int max;
    int popup_width;
    int popup_height;
} IconSliderPopupProps;

typedef struct {
    int route;
    const char *label;
    Texture2D icon;
} NavigationBarOption;

typedef struct {
    int id;
    const char *title;
    int *routes;
    int *route_count;
    int max_route_count;
    const char **slot_labels;
    const NavigationBarOption *options;
    int option_count;
    const char *add_label;
    const char *cancel_label;
    const char *save_label;
    const char *reset_label;
    Texture2D close_icon;
} NavigationBarConfigProps;

typedef struct {
    int action;
    int changed;
} NavigationBarConfigResult;

typedef struct {
    int x;
    int y;
    int width;
    int height;
    const char *username;
    const char *subtitle;
    const char *friends_text;
    Texture2D pfp_icon;
    const Texture2D *icons;
    IconType pfp_icon_type;
    int content_padding_x;
    int current_frame;
    int block_click_frame;
} SidebarAccountHeaderProps;

typedef struct {
    int pfp_clicked;
    int username_clicked;
    int friends_clicked;
    int height;
} SidebarAccountHeaderResult;

typedef struct {
    const char *title;
    const Texture2D *icons;
    IconType *selected_icon_type;
    Texture2D close_icon;
    int max_width;
    int *scroll_offset;
} ProfileImagePickerProps;

typedef struct {
    int closed;
    int changed;
    int selected_index;
    IconType selected_icon_type;
} ProfileImagePickerResult;

typedef struct {
    int x;
    int y;
    int w;
    int h;
    int content_x;
    int content_y;
    int content_w;
    int content_h;
    int left_clicked;
    int right_clicked;
} PanelFrame;

enum {
    POINTER_OWNER_NONE = 0,
    POINTER_OWNER_SCROLL,
    POINTER_OWNER_HORIZONTAL_SLIDER,
    POINTER_OWNER_VERTICAL_SLIDER,
    POINTER_OWNER_REORDER,
    POINTER_OWNER_TEXT_SELECTION,
    POINTER_OWNER_TEXT_FIELD_PAN,
    POINTER_OWNER_SWIPE
};

Vector2 ui_mouse_world(void);
Vector2 ui_primary_pointer_world(void);
int ui_primary_pointer_pressed(void);
int ui_primary_pointer_down(void);
int ui_primary_pointer_released(void);
void ui_camera_ensure_sane(void);
void MarkCursor(int cursor);
void MarkClickable(void);
void MarkDisabled(void);
int ui_pointer_drag_is_horizontal(void);
int ui_pointer_dragged_this_click(void);
int HoverEffectsEnabled(void);
const char *ui_inspect_control_id(char *buf, size_t buf_size,
                                  const char *kind, int numeric_id,
                                  const char *label);
int ReleaseConsumed(void);
void ConsumeRelease(void);
int press_started_inside(Rectangle bounds);
void ClearTextInputFocus(void);
int HandleCircleClick(Vector2 center, float radius, int disabled, int *hover);
int ui_base_input_captures_click(Vector2 point, int include_pointer_drag);
int ui_input_captures_click_internal(Vector2 point, int include_pointer_drag);
int dropdown_captures(Vector2 point);
void dropdown_close(int id);
void ui_dropdown_overlays(void);
void ui_draw_menu_overlays(void);
void ui_tab_bar_finish_frame(void);
void ui_tab_scope_finish_frame(void);
int *ui_tab_bar_owned_scroll(int id, int *fallback);
void PushInputClip(Rectangle bounds);
void PopInputClip(void);
int ui_classic_style(void);
float ui_radius_px(Rectangle bounds, float radius_px);
int ui_control_bevel_enabled(void);
int ui_touch_target_min(void);
Color ui_alpha(Color color, unsigned char alpha);
Rectangle ui_centered_min_hit_rect(int x, int y, int w, int h,
                                   int min_w, int min_h);
ThemeScheme ui_default_scheme(void);
void ui_default_elevation(Rectangle bounds, float radius, int level);
void ui_default_ripple(Rectangle bounds, Color on_color, int key, int pressed);
void ui_draw_control_background(Rectangle bounds, Color background,
                                Color border, float classic_radius);


/* Blink phase of the text caret: on roughly every other half-second. */
int ui_caret_blink_visible(void);
/* Navigate to a URL: in-browser redirect on web, platform opener otherwise.
 * A no-op for a NULL/empty url. */
void ui_open_url(const char *url);
void ui_page_semantic_next(SemanticKind kind, const char *label,
                           const char *href, const char *role, int level,
                           int tab_index);
void RenderFrameOverlays(void);
void RenderFocus(Rectangle bounds);
int ui_label_text_field_height(LabelTextFieldProps row);
int ui_section_label_height(SectionLabelProps label);
int ui_checkbox_row_height(CheckboxRowProps row);
int GetButtonRowHeight(ButtonRowProps row);
int ui_navigation_bar_height(void);
int ui_tab_bar_height(void);
int ui_title_bar_height(void);
int ui_paragraph_height(ParagraphSpec paragraph);
void RenderText(const char *text, int x, int y, int font_size, Color color);
typedef struct TextNavigationInput {
    const char *text;
    const TextAreaProps *area;
    int font;
    int key;
    int shift;
    int modifier;
    int secure;
} TextNavigationInput;
int ui_text_line_start(const char *text, int cursor);
int ui_text_line_end(const char *text, int cursor);
int ui_text_move_vertical(const char *text, int cursor, int font, int direction);
int ui_text_area_move_page(TextAreaProps area, int cursor, int direction);
int ui_text_word_left(const char *text, int cursor);
int ui_text_word_right(const char *text, int cursor);
int ui_text_navigation_key(int multiline);
int ui_text_navigate(TextNavigationInput input, int *anchor, int *cursor);
int TextWidth(const char *text, int font_size);
int TextHeight(const char *text, int font_size);
int TextLineHeight(int font_size);
int TextBaselineY(const char *text, int box_y, int box_h, int font_size);
int ControlTextBaselineY(const char *text, int box_y, int box_h, int font);
int PushTextSelectable(int selectable);
void PopTextSelectable(int token);
void RenderTextEx(const char *text, int x, int y, int font_size, Color color,
                  int selectable);
void RenderTextStyled(const char *text, int x, int y, TextStyle style);
void RenderNonSelectableText(const char *text, int x, int y, int font_size,
                             Color color);
void DrawScaledText(const char *text, int x, int y, int scale, Color color);
void DrawCenteredText(const char *text, int center_x, int center_y,
                        int font_size, Color color);
void RenderControlTextInRect(const char *text, Rectangle rect,
                             int font_size, Color color);
void DrawFittedTextInRect(const char *text, Rectangle rect,
                          int preferred_size, int min_size, Color color);
int MeasureSelectableTextBlock(const char *text, int width, int font_size,
                                 int line_gap);
int RenderSelectableTextBlock(SelectableTextBlock block);
void DrawTextLayout(TextLayout *layout, int x, int *y, int font_size,
                      Color color);
void DrawTextLayoutAligned(TextLayout *layout, int x, int *y, int font_size,
                           Color color, int width, int align);
void ui_draw_paragraph(ParagraphSpec paragraph, int x, int *y);
void ui_draw_paragraph_aligned(ParagraphSpec paragraph, int x, int *y,
                               int align);
void ui_draw_paragraph_aligned_color(ParagraphSpec paragraph, int x, int *y,
                                     int align, Color color);
void RenderBevel(int x, int y, int w, int h, Color light, Color dark);
void RenderTextLines(const char **lines, int count, int x, int *y, int font,
                     int line_h, Color color);
void ui_paint_text_box(const char *text, Rectangle bounds, int font,
                       Color color, int wrap, int align, int vertical_align,
                       int font_token, int letter_spacing);
int ui_set_text_letter_spacing(int spacing);
int ui_set_text_strikethrough(int enabled);
int ui_get_text_letter_spacing(void);
void RenderTransitionFade(const TransitionState *transition, int width,
                          int height, Color color);
int ui_scrollbar(int x, int y, int viewport_h, int content_h,
                 int *scroll_offset, int max_scroll, int overlay);
void ui_scrollbar_cancel(int *scroll_offset);
int ui_button_render(ButtonSpec button);
int ui_focusable_pressed(Rectangle bounds, int id, int disabled, int *focused);
void ui_accessibility_prepare(int id, int kind, int enabled);
int ui_accessibility_apply_list(ListBoxProps list, int *item);
void ui_accessibility_list_items(ListBoxProps list, int row_height, int scroll);
int ui_accessibility_take_activation(int id);
int ui_numeric_focus_id(int id, int component, int integer);
Style ResolveButtonStyle(ButtonProps button, ButtonState state);
Style ui_resolve_button_style_kind(ButtonProps button, ButtonState state,
                                   int style_kind);
StyleFrame ui_resolve_button_spec_frame(ButtonSpec button, ButtonState state,
                                        int automatic, float h, float p,
                                        float f, int style_kind);
TextInputAppearance ui_resolve_text_input_appearance(TextInputAppearance style,
                                           int style_kind, int class_name);
TextInputMetrics ui_text_input_metrics_for_appearance(TextInputAppearance style,
                                                 int style_kind,
                                                 int class_name,
                                                 int default_line_gap);
int ButtonNode(ButtonSpec button);
int HandleButton(ButtonSpec button);
Color ui_paint_button(ButtonSpec button, int hovered, int pressed);
typedef struct {
    Rectangle bounds;
    Texture2D icon;
    IconType icon_type;
    int icon_size;
    int icon_padding;
    int focus_id;
    int disabled;
    Color background;
    Color hover_background;
    Color icon_color;
    Color border;
    float radius;
} IconActionSpec;

int RenderIconAction(IconActionSpec action);
int RenderLink(LinkProps link);
int ui_text_input_default_font(int style_kind, int class_name);
int ui_text_input_control_render(TextInputProps input);
void DrawTextInput(Rectangle bounds, const char *text, int cursor_position,
                     int focused, int cursor_visible, int font,
                     int focus_id, int class_name);
int ui_text_field_render(TextFieldProps field);
int ui_text_field_render_filtered(TextFieldProps field,
                                  TextInputFilter filter,
                                  void *filter_user_data);
int ui_text_area_render(TextAreaProps area);
int ui_text_area_cursor_at_point(TextAreaProps area, int mouse_x, int mouse_y);
void ui_text_area_reveal_cursor(TextAreaProps area, int cursor);
void ui_paint_text_area(TextAreaProps area, int cursor, int focused,
                        int selection_start, int selection_end);
void ui_paint_text_area_composition(TextAreaProps area, int cursor, int focused,
                                    int selection_start, int selection_end,
                                    int composition_start,
                                    int composition_end);
void DrawCustomIcon(int x, int y, int size, Texture2D icon, Color tint);
int RenderButtonInfoIndicator(int center_x, int center_y, int diameter);
int ui_text_button_render(int x, int y, const char *label, int *hover);
int ui_render_slider(int id, int x, int y, int w, const char *label, int min,
                     int max, int *value, const char *suffix,
                     const char *value_text_override);
int ui_render_vertical_slider(int id, int x, int y, int h, int min, int max,
                              int *value);
int ui_render_vertical_slider_active(int id, int x, int y, int h, int min,
                                     int max, int *value);
int ui_render_vertical_slider_with_marks(
    int id, int x, int y, int h, int min, int max, int *value,
    SliderMarkCallback callback, void *callback_user_data);
int ToggleSwitch(int x, int y, int w, int h, int *value,
                 int class_name, const char *off_label, const char *on_label,
                 int focused);
int RenderCheckboxToggle(int x, int y, const char *label, int *value);
int DrawDisabledCheckboxToggle(int x, int y, const char *label,
                                 int *value, int disabled);
int ui_dropdown(DropdownProps props);
void RenderInfoRows(InfoRowsProps rows);
int RenderLabelTextField(LabelTextFieldProps row, int x, int y, int w);
int RenderSectionLabel(SectionLabelProps label, int x, int y);
int RenderCheckboxRow(CheckboxRowProps row, int x, int y);
int RenderButtonRow(ButtonRowProps row);
int RenderIconSliderPopup(IconSliderPopupProps popup);
IconRowResult RenderBottomIconRow(BottomIconRowProps row);
NavigationBarResult RenderNavigationBar(NavigationBarProps nav);
NavigationBarConfigResult RenderNavigationBarConfigModal(NavigationBarConfigProps modal);
ToolbarResult RenderToolbar(ToolbarProps toolbar);
int ui_tab_bar_keyboard_input(TabBarProps bar);
int RenderTabBar(TabBarProps bar);
DropZone GetPaneDropZone(Rectangle bounds, Vector2 mouse);
void RenderSeparator(SeparatorProps separator);
int RenderDragDrop(DragDropProps drag_drop);
int RenderListBoxMulti(ListBoxProps list);
int RenderSelectable(SelectableProps selectable);
int RenderCheckbox(CheckboxProps checkbox);
void RenderBullet(Rectangle bounds);
int RenderColorPicker(ColorPickerProps picker);
MenuResult RenderMenuGroups(int id, int class_name, Rectangle bounds,
                            const MenuGroup *menus, int menu_count,
                            int *open_index);
int RenderPopupMenu(int id, int class_name, int x, int y,
                    const MenuItem *items, int item_count);
int RenderContextMenu(MenuProps menu);
MenuResult RenderMenu(MenuProps menu);
int RenderRadio(RadioProps radio);
void RenderProgress(ProgressProps progress);
void RenderPlotLines(PlotProps plot);
void RenderPlotHistogram(PlotProps plot);
int ui_update_drag_continuous(DragContinuousProps drag);
int ui_update_drag_discrete(DragDiscreteProps drag);
void ui_paint_drag_continuous(DragContinuousProps drag);
void ui_paint_drag_discrete(DragDiscreteProps drag);
Rectangle ui_slider_bounds(SliderProps slider);
int ui_update_slider(SliderProps slider);
void ui_paint_slider(SliderProps slider);
int ui_update_slider_continuous(SliderContinuousProps slider, int vertical);
int ui_update_slider_discrete(SliderDiscreteProps slider, int vertical);
void ui_paint_slider_continuous(SliderContinuousProps slider, int vertical);
void ui_paint_slider_discrete(SliderDiscreteProps slider, int vertical);
int ui_update_slider_angle(SliderAngleProps slider);
void ui_paint_slider_angle(SliderAngleProps slider);
int RenderInputContinuous(InputContinuousProps input);
int RenderInputDiscrete(InputDiscreteProps input);
int RenderInputPrecise(InputPreciseProps input);
int RenderSpinbox(SpinboxProps spinbox);
void RenderFieldset(FieldsetProps frame);
int RenderListBox(ListBoxProps list);
int RenderTreeView(TreeViewProps tree);
int RenderTableView(TableViewProps table);
void ui_consume_focus_tab(void);
void RenderCanvasGrid(Rectangle bounds, int step, Color color);
int RenderPanedView(PanedViewProps panes);
int RenderCollapsible(CollapsibleProps section);
void RenderFocusDebugOverlay(const AccessibilityNode *nodes, int count);
GuideResult RenderGuideOverlay(GuideOverlayProps guide);
void RenderTutorialImagePlaceholder(const char *label, int x, int y,
                                    int w, int h);
void RenderTutorialImage(Texture2D texture, const char *fallback,
                         int x, int y, int w, int h);
void RenderImage(ImageProps image);
int RenderActionModal(ModalProps modal);
int RenderTitleBar(TitleBarProps title_bar);
PanelFrame RenderModalFrame(int width, int height, const char *title,
                              Texture2D left_icon, Texture2D right_icon);
SidebarAccountHeaderResult RenderSidebarAccountHeader(SidebarAccountHeaderProps header);
ProfileImagePickerResult RenderProfileImagePickerModal(ProfileImagePickerProps modal);
void RenderReorderHandle(int x, int y, int w, int h, int active);
void RenderReorderPlaceholder(Rectangle bounds);
void RenderToast(void);
void RenderInspectOverlay(void);

/* Internal focus pass hooks. Public frames use BeginInterfaceFrame,
 * SetFrameCamera, and EndInterfaceFrame to manage focus automatically. */
void BeginFocusScope(void);
void EndFocusScope(void);

/* Lowered host scopes for .kry block widgets. These are not public widget
 * names; parser/codegen and native tests use them to implement lexical blocks. */
void DisabledScope(int disabled);
void DisabledEndScope(void);
Rectangle ScrollScope(Rectangle bounds, int content_height, int *scroll_offset);
void ScrollEndScope(void);
NodeId CardScope(CardProps card);
NodeId ButtonScope(ButtonProps button);
int PopupScope(PopupProps popup);
void popup_close_scope(void);
void PopupEndScope(void);
Rectangle TableCellScope(TableViewProps table, int row, int column);
void TableCellEndScope(void);
CanvasResult CanvasScope(Canvas canvas);
void CanvasEndScope(Canvas canvas);

/* Retained submissions borrow this destination until EndTree. The caller
 * separately captures immediate drawing and owns the texture lifetime. */
RenderTexture2D ui_tree_set_paint_target(RenderTexture2D target);
int GetNodeHeight(TreeNode node);
void ui_tree_heading(const char *text, Rectangle bounds, int font, Color color, int level);
TreeNode NodeParagraph(ParagraphSpec paragraph, int x, int y);
TreeNode NodeNavigationBar(NavigationBarProps nav);
TreeNode NodeTabBar(TabBarProps bar);
TreeNode NodeTitleBar(int height);
void ui_tree_submit_text_input(Rectangle bounds, const char *text,
                               TextInputPaint paint, int id);
void ui_paint_text_input(Rectangle bounds, const char *text,
                         TextInputPaint paint);

/* UTF-8 codec and text-buffer helpers (implemented in ui_text_edit.c). */
int ui_utf8_next_offset(const char *text, int offset);
int ui_utf8_prev_offset(const char *text, int offset);
typedef struct TextCompositionView {
    char *text;
    int cursor;
    int selection_start;
    int selection_end;
    int composition_start;
    int composition_end;
} TextCompositionView;
typedef struct TextCompositionResult {
    int text_changed;
    int presentation_changed;
    int selection_changed;
} TextCompositionResult;
int ui_text_composition_view(const char *text, int selection_start,
                             int selection_end, const char *preedit,
                             int preedit_cursor,
                             int preedit_selection_length,
                             TextCompositionView *view);
void ui_text_composition_view_free(TextCompositionView *view);
TextCompositionResult ui_text_composition_apply(
    TextEdit edit, int *anchor, const void *owner, int focused,
    int read_only, int allow_newlines);
int ui_text_composition_get(const void *owner, const char **text,
                            int *cursor, int *selection_length);
int ui_text_composition_cancel(const void *owner);
int ui_active_font_token(void);
void ui_draw_text_with_font_token(const char *text, int x, int y,
                                  int font_size, Color color, int token);
void ui_text_begin_frame(void);
int ui_text_cursor_at_x(const char *text, int font, int text_x, int mouse_x);
void ui_draw_text_input_selection(Rectangle bounds, const char *text,
                                  int cursor, int focused, int font,
                                  TextInputAppearance style,
                                  int selection_start, int selection_end);
int ui_utf8_codepoint_count(const char *text);
int ui_utf8_encode(int codepoint, char out[5]);
int ui_text_delete_range(char *text, size_t text_size, int *cursor,
                         int start, int end);
int ui_text_delete_key(char *text, size_t text_size, int *anchor, int *cursor,
                       int key, int modifier, int secure);
int ui_text_copy_range(const char *text, int start, int end);
int ui_text_paste_clipboard(TextEdit edit, int allow_newlines);
int ui_text_insert_ascii(char *text, size_t text_size, int *cursor, char ch,
                         int max_codepoints);
int ui_text_insert_newline(char *text, size_t text_size, int *cursor,
                           int max_codepoints);
int ui_text_insert_codepoint(char *text, size_t text_size, int *cursor,
                             int codepoint, int max_codepoints);
int ui_text_insert_text(char *text, size_t text_size, int *cursor,
                        const char *input, int allow_newlines,
                        TextInputFilter filter, void *filter_user_data,
                        int max_codepoints);

#endif
