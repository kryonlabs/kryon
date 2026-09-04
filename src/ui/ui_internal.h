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
extern Texture2D g_ui_gear_icon;
extern Texture2D g_ui_x_icon;
extern unsigned long g_ui_frame_serial;
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
void MarkUICursor(int cursor);
void MarkUIClickable(void);
void MarkUIDisabled(void);
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
void ClearTextInputFocus(void);
int UIHandleCircleClick(Vector2 center, float radius, int disabled, int *hover);
int ui_base_input_captures_click(Vector2 point, int include_pointer_drag);
int ui_input_captures_click_internal(Vector2 point, int include_pointer_drag);
int ui_dropdown_captures_click(Vector2 point);
void ui_dropdown_close(int id);
void ui_draw_dropdown_overlays(void);
void ui_draw_menu_overlays(void);
void PushUIInputClip(Rectangle bounds);
void PopUIInputClip(void);
int ui_clampi(int value, int min_value, int max_value);
int ui_retro_style(void);
int ui_modern_style(void);
float ui_radius_px(Rectangle bounds, float radius_px);
int ui_control_bevel_enabled(void);
int ui_touch_target_min(void);
Color ui_alpha(Color color, unsigned char alpha);
Rectangle ui_centered_min_hit_rect(int x, int y, int w, int h,
                                   int min_w, int min_h);
int ui_material_style(void);
Color ui_material_on_color(Color color);
UIMaterialScheme ui_material_scheme(void);
Color ui_material_surface_container(void);
Color ui_material_surface_variant(void);
Color ui_material_outline(void);
void ui_material_state_layer(Rectangle bounds, Color on_color,
                             int hovered, int focused, int pressed);
void ui_material_focus(Rectangle bounds);
void ui_material_elevation(Rectangle bounds, float radius, int level);
void ui_material_ripple(Rectangle bounds, Color on_color, int key, int pressed);
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
void DrawUIFrameOverlays(void);
void DrawUIFocus(Rectangle bounds);
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
void DrawUIText(const char *text, int x, int y, int font_size, Color color);
void DrawUITextEx(const char *text, int x, int y, int font_size, Color color,
                  int selectable);
void DrawUITextStyled(const char *text, int x, int y, TextStyle style);
void DrawUINonSelectableText(const char *text, int x, int y, int font_size,
                             Color color);
void DrawScaledUIText(const char *text, int x, int y, int scale, Color color);
void DrawCenteredUIText(const char *text, int center_x, int center_y,
                        int font_size, Color color);
void DrawUITextInRect(const char *text, Rectangle rect, int font_size,
                      Color color);
int MeasureUISelectableTextBlock(const char *text, int width, int font_size,
                                 int line_gap);
int DrawUISelectableTextBlock(SelectableTextBlock block);
void DrawTextLayout(TextLayout *layout, int x, int *y, int font_size,
                      Color color);
void ui_draw_paragraph(ParagraphSpec paragraph, int x, int *y);
void DrawUIBevel(int x, int y, int w, int h, Color light, Color dark);
void DrawUITextLines(const char **lines, int count, int x, int *y, int font,
                     int line_h, Color color);
void DrawUITransitionFade(const UITransition *transition, int width,
                          int height, Color color);
int DrawUIScrollbar(int x, int y, int viewport_h, int content_h,
                    int *scroll_offset, int max_scroll);
int RenderButton(ButtonSpec button);
int HandleButton(ButtonSpec button);
void PaintButton(ButtonSpec button, int hovered, int pressed);
int DrawUIIconButton(IconButtonProps button);
int DrawUIHref(HrefProps link);
int RenderTextInputControl(TextInputProps input);
void DrawTextInput(Rectangle bounds, const char *text, int cursor_position,
                     int focused, int cursor_visible, int font,
                     TextInputStyle style);
int RenderTextField(TextFieldProps field);
int RenderTextArea(TextAreaProps area);
int ui_text_area_cursor_at_point(TextAreaProps area, int mouse_x, int mouse_y);
void ui_text_area_reveal_cursor(TextAreaProps area, int cursor);
void ui_paint_text_area(TextAreaProps area, int cursor, int focused,
                        int selection_start, int selection_end);
int DrawUIReadonlyTextBox(ReadonlyTextBoxProps box);
void DrawCustomIcon(int x, int y, int size, Texture2D icon, Color tint);
int DrawUIIconBtn(int x, int y, UIIconSize size, Texture2D icon, int *hover);
int DrawUIPaddedIconBtn(int x, int y, int size, int padding, Texture2D icon,
                        int *hover);
int DrawUIInfoButton(int center_x, int center_y, int diameter);
int RenderTextButton(int x, int y, const char *label, int *hover);
int RenderStyledButton(int x, int y, int w, int h, const char *label,
                        ButtonStyle style, int disabled, int *hover);
void DrawUIIconLink(int x, int y, int icon_size, Texture2D icon,
                    const char *url);
int DrawUISlider(int id, int x, int y, int w, const char *label, int min,
                 int max, int *value, const char *suffix,
                 const char *value_text_override);
int DrawUIVerticalSlider(int id, int x, int y, int h, int min, int max,
                         int *value);
int DrawUIVerticalSliderWithMarks(int id, int x, int y, int h, int min,
                                  int max, int *value,
                                  UIVerticalSliderMarkCallback callback,
                                  void *callback_user_data);
int DrawUIToggleSwitch(int x, int y, int w, int h, int *value,
                       const char *off_label, const char *on_label);
int DrawUICheckboxToggle(int x, int y, const char *label, int *value);
int DrawDisabledUICheckboxToggle(int x, int y, const char *label,
                                 int *value, int disabled);
int DrawUIDropdown(int id, int x, int y, int w, int h,
                   const char **options, int option_count,
                   int *selected_index);
int DrawUIDropdownEx(int id, int x, int y, int w, int h,
                     const UIDropdownOption *options, int option_count,
                     int *selected_index);
int DrawUILocaleDropdown(int id, int x, int y, int w, int h,
                         int *selected_index);
void DrawUIInfoRows(InfoRowsProps rows);
int DrawUILabelTextField(LabelTextFieldProps row, int x, int y, int w);
int DrawUISectionLabel(SectionLabelProps label, int x, int y);
int DrawUICheckboxRow(CheckboxRowProps row, int x, int y);
int DrawUIOverlayButton(OverlayButtonProps button);
int DrawUIButtonRow(ButtonRowProps row);
int DrawUIIconSliderPopup(IconSliderPopupProps popup);
IconRowResult DrawUIBottomIconRow(BottomIconRowProps row);
BottomNavResult DrawUIBottomNav(BottomNavProps nav);
BottomNavConfigResult DrawUIBottomNavConfigModal(BottomNavConfigProps modal);
TopNavResult DrawUITopNav(TopNavProps nav);
ToolbarResult DrawUIToolbar(ToolbarProps toolbar);
ToolbarHeaderResult DrawUIToolbarHeader(ToolbarHeaderProps header);
int DrawUISubtabBar(SubtabBarProps bar);
int DrawUITabBar(TabBarProps bar);
PaneTabBarResult DrawUIPaneTabBar(PaneTabBar bar);
PaneDropZone GetPaneDropZone(Rectangle bounds, Vector2 mouse);
void DrawUIPaneDropPreview(Rectangle bounds, PaneDropZone zone);
void DrawUISeparator(Rectangle bounds, int vertical);
MenuBarResult DrawUIMenuBar(int id, Rectangle bounds, const Menu *menus,
                              int menu_count, int *open_index);
int DrawUIPopupMenu(int id, int x, int y, const MenuItem *items,
                    int item_count);
int DrawUIContextMenu(ContextMenuProps menu);
int DrawUIRadioButton(RadioButtonProps radio);
void DrawUIProgressBar(ProgressBarProps progress);
int DrawUISpinbox(SpinboxProps spinbox);
int DrawUICombobox(ComboboxProps combo);
void DrawUILabelFrame(LabelFrameProps frame);
void DrawUIImageBox(ImageBoxProps image);
int DrawUIListBox(ListBoxProps list);
int DrawUITreeView(TreeViewProps tree);
int DrawUICascadingTreeView(CascadingTreeViewProps tree);
int DrawUISourceView(SourceViewProps source);
int DrawUITableView(TableViewProps table);
void DrawUICanvasGrid(Rectangle bounds, int step, Color color);
int DrawUINotebook(NotebookProps notebook);
int DrawUIPanedView(PanedViewProps panes);
int DrawUICollapsible(CollapsibleProps section);
int DrawUIMessageDialog(MessageDialogProps dialog);
int DrawUIConfirmDialog(ConfirmDialogProps dialog);
int DrawUIPromptDialog(PromptDialogProps dialog);
int DrawUITextPopover(TextPopoverProps popover);
int DrawUIPickerDialog(PickerDialogProps picker);
int DrawUIColorPicker(Rectangle bounds, Color *color);
void DrawUIFocusDebugOverlay(const UIAccessibilityNode *nodes, int count);
UIGuideResult DrawUIGuideOverlay(GuideOverlayProps guide);
int DrawUIThemeSettings(ThemeSettingsProps settings, UIThemeSettingsState *state);
UIThemeSettingsResult DrawUIThemeSettingsMenus(ThemeSettingsProps settings,
                                               UIThemeSettingsState *state);
int DrawUIThemeSwitcher(int x, int y, int w, const char *label,
                        const char *light_label, const char *dark_label,
                        int *theme_id, int *dark_mode);
int DrawUIThemePicker(int x, int y, int w, int dark_mode, int *theme_id);
void DrawUITutorialImagePlaceholder(const char *label, int x, int y,
                                    int w, int h);
void DrawUITutorialImage(Texture2D texture, const char *fallback,
                         int x, int y, int w, int h);
int DrawUIActionModal(ModalProps modal);
int DrawUIModal(const char *title, const char *message,
                const char *cancel_btn, const char *confirm_btn);
int DrawUIModal3Button(const char *title, const char *message,
                       const char *left_btn, const char *middle_btn,
                       const char *right_btn);
void DrawUITitleBar(const char *title, int height);
int DrawUIReturnTitleBar(Texture2D return_icon, const char *title,
                         int height);
int DrawUIReturnDropdownTitleBar(Texture2D return_icon,
                                 UITitleBarDropdown dropdown, int height);
UIPanelFrame DrawUIModalFrame(int width, int height, const char *title,
                              Texture2D left_icon, Texture2D right_icon);
SidebarAccountHeaderResult DrawUISidebarAccountHeader(SidebarAccountHeaderProps header);
ProfilePicturePickerResult DrawUIProfilePicturePickerModal(ProfilePicturePickerProps modal);
void DrawUIReorderHandle(int x, int y, int w, int h, int active);
void DrawUIReorderPlaceholder(Rectangle bounds);
void DrawUIToast(void);
void DrawUIInspectOverlay(void);

/* UTF-8 codec and text-buffer helpers (implemented in ui_text_edit.c). */
int ui_utf8_next_offset(const char *text, int offset);
int ui_utf8_prev_offset(const char *text, int offset);
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
