#ifndef UI_INTERNAL_H
#define UI_INTERNAL_H

#include "ui.h"
#include "ui_clip.h"
#include "ui_window.h"
#include "ui_dpi.h"
#include "kryon.h"
#include "ui_text_layout.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(PLATFORM_WEB)
#include <emscripten.h>
#endif

extern Color c_text, c_bg, c_surface, c_circle, c_button, c_button_hover, c_icon, c_link;
extern Camera2D g_ui_camera;
void ui_begin_world_clip(Rectangle rect);
extern Texture2D g_ui_gear_icon;
extern Texture2D g_ui_x_icon;
extern unsigned long g_ui_frame_serial;
extern float g_theme_content_alpha;
int UIContentDisabled(void);
int ui_current_input_clip(Rectangle *bounds);
extern int g_ui_slider_active_id;
extern int g_ui_pointer_dragging;
extern int g_ui_pointer_owner;
extern int g_ui_scroll_gesture_pending;

enum {
    UI_POINTER_OWNER_NONE = 0,
    UI_POINTER_OWNER_SCROLL,
    UI_POINTER_OWNER_HORIZONTAL_SLIDER,
    UI_POINTER_OWNER_VERTICAL_SLIDER,
    UI_POINTER_OWNER_REORDER,
    UI_POINTER_OWNER_TEXT_SELECTION,
    UI_POINTER_OWNER_TEXT_FIELD_PAN,
    UI_POINTER_OWNER_SWIPE
};

Vector2 ui_mouse_world(void);
void ui_camera_ensure_sane(void);
void MarkCursor(int cursor);
void MarkClickable(void);
void MarkDisabled(void);
int ui_pointer_drag_is_horizontal(void);
int UIHoverEffectsEnabled(void);
const char *ui_inspect_control_id(char *buf, size_t buf_size,
                                  const char *kind, int numeric_id,
                                  const char *label);
int UIReleaseConsumed(void);
void UIConsumeRelease(void);
int UIPointerReleaseConsumed(void);
void UIConsumePointerRelease(void);
int UIPointerReleaseAvailable(Vector2 point);
int UIPointerReleaseOutside(Rectangle bounds);
int mouse_release_activates_rect(Rectangle bounds, Vector2 mouse, int active);
void ClearTextInputFocus(void);
int UIHandleCircleClick(Vector2 center, float radius, int disabled, int *hover);
int ui_base_input_captures_click(Vector2 point, int include_pointer_drag);
int ui_input_captures_click_internal(Vector2 point, int include_pointer_drag);
int dropdown_captures(Vector2 point);
void dropdown_close(int id);
void ui_dropdown_overlays(void);
void ui_draw_menu_overlays(void);
void ui_tab_bar_finish_frame(void);
void ui_tab_scope_finish_frame(void);
int *ui_tab_bar_owned_scroll(int id, int *fallback);
void PushUIInputClip(Rectangle bounds);
void PopUIInputClip(void);
int ui_clampi(int value, int min_value, int max_value);
int ui_classic_style(void);
int ui_modern_style(void);
float ui_radius_px(Rectangle bounds, float radius_px);
int ui_control_bevel_enabled(void);
int ui_touch_target_min(void);
Color ui_alpha(Color color, unsigned char alpha);
Rectangle ui_centered_min_hit_rect(int x, int y, int w, int h,
                                   int min_w, int min_h);
int ui_default_style(void);
Color ui_default_on_color(Color color);
ThemeScheme ui_default_scheme(void);
Color ui_default_surface_container(void);
Color ui_default_surface_variant(void);
Color ui_default_outline(void);
void ui_default_state_layer(Rectangle bounds, Color on_color,
                             int hovered, int focused, int pressed);
void ui_default_focus(Rectangle bounds);
void ui_default_elevation(Rectangle bounds, float radius, int level);
void ui_default_ripple(Rectangle bounds, Color on_color, int key, int pressed);
void ui_draw_control_background(Rectangle bounds, Color background,
                                Color border, float classic_radius);

/* Draws a filled box with an outline: rounded when radius > 0, otherwise a
 * plain rectangle with a 1px line border. Used by text input, text area, and
 * read-only box backgrounds. */
void ui_draw_box_background(Rectangle bounds, float radius, Color background,
                            Color border);
/* Blink phase of the text caret: on roughly every other half-second. */
int ui_caret_blink_visible(void);
/* Navigate to a URL: in-browser redirect on web, platform opener otherwise.
 * A no-op for a NULL/empty url. */
void ui_open_url(const char *url);
void RenderFrameOverlays(void);
void RenderFocus(Rectangle bounds);
int ui_readonly_text_box_height(const char *text, int font, int width,
                               TextInputStyle style, int line_gap);
int ui_label_text_field_height(LabelTextFieldProps row);
int ui_section_label_height(SectionLabelProps label);
int ui_checkbox_row_height(CheckboxRowProps row);
int GetUIButtonRowHeight(ButtonRowProps row);
int ui_bottom_nav_height(void);
int ui_tab_bar_height(void);
int ui_theme_settings_height(ThemeSettingsProps settings);
int ui_theme_picker_height(int w);
int ui_paragraph_modal_height(ParagraphModalMeasureProps measure);
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
void RenderTextEx(const char *text, int x, int y, int font_size, Color color,
                  int selectable);
void RenderTextStyled(const char *text, int x, int y, TextStyle style);
void RenderNonSelectableText(const char *text, int x, int y, int font_size,
                             Color color);
void DrawScaledUIText(const char *text, int x, int y, int scale, Color color);
void DrawCenteredUIText(const char *text, int center_x, int center_y,
                        int font_size, Color color);
int MeasureUISelectableTextBlock(const char *text, int width, int font_size,
                                 int line_gap);
int RenderSelectableTextBlock(SelectableTextBlock block);
void DrawTextLayout(TextLayout *layout, int x, int *y, int font_size,
                      Color color);
void DrawTextLayoutAligned(TextLayout *layout, int x, int *y, int font_size,
                           Color color, int width, int align);
void ui_draw_paragraph(ParagraphSpec paragraph, int x, int *y);
void ui_draw_paragraph_aligned(ParagraphSpec paragraph, int x, int *y,
                               int align);
void RenderBevel(int x, int y, int w, int h, Color light, Color dark);
void RenderTextLines(const char **lines, int count, int x, int *y, int font,
                     int line_h, Color color);
void ui_paint_text_box(const char *text, Rectangle bounds, int font,
                       Color color, int wrap, int align, int vertical_align,
                       int font_token, int letter_spacing);
int ui_set_text_letter_spacing(int spacing);
int ui_get_text_letter_spacing(void);
void RenderTransitionFade(const UITransition *transition, int width,
                          int height, Color color);
int ui_scrollbar(int x, int y, int viewport_h, int content_h,
                 int *scroll_offset, int max_scroll, int overlay);
void ui_scrollbar_cancel(int *scroll_offset);
int ui_button_render(ButtonSpec button);
int ui_focusable_pressed(Rectangle bounds, int id, int disabled, int *focused);
int ui_numeric_focus_id(int id, int component, int integer);
Style ResolveButtonStyle(ButtonProps button, ButtonState state);
int HandleButton(ButtonSpec button);
Color ui_paint_button(ButtonSpec button, int hovered, int pressed);
typedef struct {
    Rectangle bounds;
    Texture2D icon;
    UIIconType icon_type;
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
int RenderHref(HrefProps link);
int ui_text_input_control_render(TextInputProps input);
void DrawTextInput(Rectangle bounds, const char *text, int cursor_position,
                     int focused, int cursor_visible, int font,
                     TextInputStyle style, int focus_id);
int ui_text_field_render(TextFieldProps field);
int ui_text_area_render(TextAreaProps area);
int ui_text_area_cursor_at_point(TextAreaProps area, int mouse_x, int mouse_y);
void ui_text_area_reveal_cursor(TextAreaProps area, int cursor);
void ui_paint_text_area(TextAreaProps area, int cursor, int focused,
                        int selection_start, int selection_end);
void ui_paint_text_area_composition(TextAreaProps area, int cursor, int focused,
                                    int selection_start, int selection_end,
                                    int composition_start,
                                    int composition_end);
int RenderReadonlyTextBox(ReadonlyTextBoxProps box);
void DrawCustomIcon(int x, int y, int size, Texture2D icon, Color tint);
int RenderInfoButton(int center_x, int center_y, int diameter);
int ui_text_button_render(int x, int y, const char *label, int *hover);
void RenderIconLink(int x, int y, int icon_size, Texture2D icon,
                    const char *url);
int ui_render_slider(int id, int x, int y, int w, const char *label, int min,
                     int max, int *value, const char *suffix,
                     const char *value_text_override);
int ui_render_vertical_slider(int id, int x, int y, int h, int min, int max,
                              int *value);
int ui_render_vertical_slider_with_marks(
    int id, int x, int y, int h, int min, int max, int *value,
    UIVerticalSliderMarkCallback callback, void *callback_user_data);
int RenderToggleSwitch(int x, int y, int w, int h, int *value,
                       const char *off_label, const char *on_label);
int RenderCheckboxToggle(int x, int y, const char *label, int *value);
int DrawDisabledUICheckboxToggle(int x, int y, const char *label,
                                 int *value, int disabled);
int ui_dropdown(ComboboxProps props);
void RenderInfoRows(InfoRowsProps rows);
int RenderLabelTextField(LabelTextFieldProps row, int x, int y, int w);
int RenderSectionLabel(SectionLabelProps label, int x, int y);
int RenderCheckboxRow(CheckboxRowProps row, int x, int y);
int RenderOverlayButton(OverlayButtonProps button);
int RenderButtonRow(ButtonRowProps row);
int RenderIconSliderPopup(IconSliderPopupProps popup);
IconRowResult RenderBottomIconRow(BottomIconRowProps row);
BottomNavResult RenderBottomNav(BottomNavProps nav);
BottomNavConfigResult RenderBottomNavConfigModal(BottomNavConfigProps modal);
TopNavResult RenderTopNav(TopNavProps nav);
ToolbarResult RenderToolbar(ToolbarProps toolbar);
ToolbarHeaderResult RenderToolbarHeader(ToolbarHeaderProps header);
int RenderSubtabBar(SubtabBarProps bar);
int ui_tab_bar_keyboard_input(TabBarProps bar);
int RenderTabBar(TabBarProps bar);
PaneTabBarResult RenderPaneTabBar(PaneTabBar bar);
PaneDropZone GetPaneDropZone(Rectangle bounds, Vector2 mouse);
void RenderPaneDropPreview(Rectangle bounds, PaneDropZone zone);
void RenderSeparator(Rectangle bounds, int vertical);
void RenderSeparatorText(SeparatorTextProps separator);
int RenderDragDropSource(DragDropSourceProps source);
int RenderDragDropTarget(DragDropTargetProps target);
int RenderMultiSelectList(MultiSelectListProps list);
int RenderSelectable(SelectableProps selectable);
int RenderCheckboxFlags(CheckboxFlagsProps checkbox);
int RenderInvisibleButton(InvisibleButtonProps button);
int RenderArrowButton(ArrowButtonProps button);
void RenderBullet(Rectangle bounds);
int RenderColorEdit3(ColorEditProps edit);
int RenderColorEdit4(ColorEditProps edit);
int RenderColorPicker3(ColorEditProps picker);
int RenderColorPicker4(ColorEditProps picker);
int RenderColorButton(ColorButtonProps button);
MenuBarResult RenderMenuBar(int id, Rectangle bounds, const Menu *menus,
                              int menu_count, int *open_index);
int RenderPopupMenu(int id, int x, int y, const MenuItem *items,
                    int item_count);
int RenderContextMenu(ContextMenuProps menu);
int RenderRadioButton(RadioButtonProps radio);
void RenderProgressBar(ProgressBarProps progress);
void RenderPlotLines(PlotProps plot);
void RenderPlotHistogram(PlotProps plot);
int ui_update_drag_float(DragFloatProps drag);
int ui_update_drag_int(DragIntProps drag);
void ui_paint_drag_float(DragFloatProps drag);
void ui_paint_drag_int(DragIntProps drag);
int ui_update_slider_float(SliderFloatProps slider, int vertical);
int ui_update_slider_int(SliderIntProps slider, int vertical);
void ui_paint_slider_float(SliderFloatProps slider, int vertical);
void ui_paint_slider_int(SliderIntProps slider, int vertical);
int ui_update_slider_angle(SliderAngleProps slider);
void ui_paint_slider_angle(SliderAngleProps slider);
int RenderInputFloat(InputFloatProps input);
int RenderInputInt(InputIntProps input);
int RenderInputDouble(InputDoubleProps input);
int RenderSpinbox(SpinboxProps spinbox);
void RenderLabelFrame(LabelFrameProps frame);
void RenderImageBox(ImageBoxProps image);
int RenderListBox(ListBoxProps list);
int RenderTreeView(TreeViewProps tree);
int RenderCascadingTreeView(CascadingTreeViewProps tree);
int RenderSourceView(SourceViewProps source);
int RenderTableView(TableViewProps table);
void ui_consume_focus_tab(void);
void RenderCanvasGrid(Rectangle bounds, int step, Color color);
int RenderNotebook(NotebookProps notebook);
int RenderPanedView(PanedViewProps panes);
int RenderCollapsible(CollapsibleProps section);
int RenderMessageDialog(MessageDialogProps dialog);
int RenderConfirmDialog(ConfirmDialogProps dialog);
int RenderPromptDialog(PromptDialogProps dialog);
int RenderTextPopover(TextPopoverProps popover);
int RenderPickerDialog(PickerDialogProps picker);
int RenderColorPicker(Rectangle bounds, Color *color);
void RenderFocusDebugOverlay(const UIAccessibilityNode *nodes, int count);
UIGuideResult RenderGuideOverlay(GuideOverlayProps guide);
int DrawThemeSettings(ThemeSettingsProps settings, ThemeSettingsState *state);
ThemeSettingsResult DrawThemeSettingsMenus(ThemeSettingsProps settings,
                                               ThemeSettingsState *state);
int RenderThemeSwitcher(int x, int y, int w, const char *label,
                        const char *light_label, const char *dark_label,
                        int *theme_id, int *dark_mode);
int RenderThemePicker(int x, int y, int w, int dark_mode, int *theme_id);
void RenderTutorialImagePlaceholder(const char *label, int x, int y,
                                    int w, int h);
void RenderTutorialImage(Texture2D texture, const char *fallback,
                         int x, int y, int w, int h);
int RenderActionModal(ModalProps modal);
int RenderModal(const char *title, const char *message,
                const char *cancel_btn, const char *confirm_btn);
int RenderModal3Button(const char *title, const char *message,
                       const char *left_btn, const char *middle_btn,
                       const char *right_btn);
void RenderTitleBar(const char *title, int height);
int RenderReturnTitleBar(Texture2D return_icon, const char *title,
                         int height);
int RenderReturnDropdownTitleBar(Texture2D return_icon,
                                 UITitleBarDropdown dropdown, int height);
UIPanelFrame RenderModalFrame(int width, int height, const char *title,
                              Texture2D left_icon, Texture2D right_icon);
SidebarAccountHeaderResult RenderSidebarAccountHeader(SidebarAccountHeaderProps header);
ProfilePicturePickerResult RenderProfilePicturePickerModal(ProfilePicturePickerProps modal);
void RenderReorderHandle(int x, int y, int w, int h, int active);
void RenderReorderPlaceholder(Rectangle bounds);
void RenderToast(void);
void RenderInspectOverlay(void);

/* Retained submissions borrow this destination until EndTree. The caller
 * separately captures immediate drawing and owns the texture lifetime. */
RenderTexture2D ui_tree_set_paint_target(RenderTexture2D target);
void ui_tree_heading(const char *text, Rectangle bounds, int font, Color color, int level);
void ui_tree_submit_text_input(Rectangle bounds, const char *text,
                               UIWidgetTextInputPaint paint, int id);
void ui_paint_text_input(Rectangle bounds, const char *text,
                         UIWidgetTextInputPaint paint);

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
                                  TextInputStyle style,
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
int ui_text_insert_codepoint(char *text, size_t text_size, int *cursor,
                             int codepoint, int max_codepoints);
int ui_text_insert_text(char *text, size_t text_size, int *cursor,
                        const char *input, int allow_newlines,
                        TextInputFilter filter, void *filter_user_data,
                        int max_codepoints);

#endif
