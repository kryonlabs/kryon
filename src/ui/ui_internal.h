#ifndef UI_INTERNAL_H
#define UI_INTERNAL_H

#include "ui.h"
#include "ui_clip.h"
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
extern int g_ui_slider_active_id;
extern int g_ui_pointer_dragging;
extern int g_ui_pointer_owner;

enum {
    UI_POINTER_OWNER_NONE = 0,
    UI_POINTER_OWNER_SCROLL,
    UI_POINTER_OWNER_HORIZONTAL_SLIDER,
    UI_POINTER_OWNER_VERTICAL_SLIDER,
    UI_POINTER_OWNER_REORDER
};

Vector2 ui_mouse_world(void);
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
int ui_base_input_captures_click(Vector2 point, int include_pointer_drag);
int ui_input_captures_click_internal(Vector2 point, int include_pointer_drag);
int ui_dropdown_captures_click(Vector2 point);
void ui_dropdown_close(int id);
void ui_draw_dropdown_overlays(void);
void PushUIInputClip(Rectangle bounds);
void PopUIInputClip(void);
int ui_clampi(int value, int min_value, int max_value);
int ui_retro_style(void);
int ui_modern_style(void);
float ui_control_radius(float classic_radius);
int ui_control_bevel_enabled(void);
int ui_touch_target_min(void);
Color ui_alpha(Color color, unsigned char alpha);
Rectangle ui_centered_min_hit_rect(int x, int y, int w, int h,
                                   int min_w, int min_h);
int ui_material_style(void);
Color ui_material_on_color(Color color);
Color ui_material_surface_container(void);
Color ui_material_outline(void);
void ui_material_state_layer(Rectangle bounds, Color on_color,
                             int hovered, int focused, int pressed);
void ui_material_focus(Rectangle bounds);
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

void UIRenderFrameOverlays(void);
void UIRenderFocus(Rectangle bounds);
int ui_readonly_text_box_height(const char *text, int font, int width,
                               UITextInputStyle style, int line_gap);
int ui_label_text_field_height(UILabelTextField row);
int ui_section_label_height(UISectionLabel label);
int ui_checkbox_row_height(UICheckboxRow row);
int ui_button_row_height(UIButtonRow row);
int ui_bottom_nav_height(void);
int ui_tab_bar_height(void);
int ui_theme_settings_height(UIThemeSettings settings);
int ui_theme_picker_height(int w);
int ui_paragraph_modal_height(UIParagraphModalMeasure measure);
int ui_title_bar_height(void);
int ui_paragraph_height(UIParagraph paragraph);
void UIRenderText(const char *text, int x, int y, int font_size, Color color);
void UIRenderTextEx(const char *text, int x, int y, int font_size, Color color,
                  int selectable);
void UIRenderNonSelectableText(const char *text, int x, int y, int font_size,
                             Color color);
void DrawScaledUIText(const char *text, int x, int y, int scale, Color color);
void DrawCenteredUIText(const char *text, int center_x, int center_y,
                        int font_size, Color color);
void UIRenderTextInRect(const char *text, Rectangle rect, int font_size,
                      Color color);
void DrawCenteredUIControlText(const char *text, int center_x, int center_y,
                               int font, Color color);
void DrawLeftUIControlTextInRect(const char *text, Rectangle rect,
                                 int font_size, Color color);
void DrawFittedUITextInRect(const char *text, Rectangle rect,
                            int preferred_size, int min_size, Color color);
void UIRenderTextLayout(UITextLayout *layout, int x, int *y, int font_size,
                      Color color);
void UIRenderParagraph(UIParagraph paragraph, int x, int *y);
void UIRenderBevel(int x, int y, int w, int h, Color light, Color dark);
void UIRenderTextLines(const char **lines, int count, int x, int *y, int font,
                     int line_h, Color color);
void UIRenderTransitionFade(const UITransition *transition, int width,
                          int height, Color color);
int UIRenderScrollbar(int x, int y, int viewport_h, int content_h,
                    int *scroll_offset, int max_scroll);
int UIRenderButton(UIButton button);
int UIRenderIconButton(UIIconButton button);
int UIRenderHref(UIHref link);
int UIRenderTextInputControl(UITextInput input);
void UIRenderTextInput(Rectangle bounds, const char *text, int cursor_position,
                     int focused, int cursor_visible, int font,
                     UITextInputStyle style);
int UIRenderTextField(UITextField field);
int UIRenderTextArea(UITextArea area);
int UIRenderReadonlyTextBox(UIReadonlyTextBox box);
void UIRenderIconTexture(int x, int y, int size, Texture2D icon, Color tint);
int UIRenderIconBtn(int x, int y, UIIconSize size, Texture2D icon, int *hover);
int UIRenderPaddedIconBtn(int x, int y, int size, int padding, Texture2D icon,
                        int *hover);
int UIRenderInfoButton(int center_x, int center_y, int diameter);
int UIRenderTextButton(int x, int y, const char *label, int *hover);
int UIRenderGenericButton(int x, int y, int w, int h, const char *label,
                        UIButtonStyle style, int disabled, int *hover);
void UIRenderIconLink(int x, int y, int icon_size, Texture2D icon,
                    const char *url);
int UIRenderSlider(int id, int x, int y, int w, const char *label, int min,
                 int max, int *value, const char *suffix);
int UIRenderVerticalSlider(int id, int x, int y, int h, int min, int max,
                         int *value);
int UIRenderVerticalSliderWithMarks(int id, int x, int y, int h, int min,
                                  int max, int *value,
                                  UIVerticalSliderMarkCallback callback,
                                  void *callback_user_data);
int UIRenderToggleSwitch(int x, int y, int w, int h, int *value,
                       const char *off_label, const char *on_label);
int UIRenderCheckboxToggle(int x, int y, const char *label, int *value);
int DrawDisabledUICheckboxToggle(int x, int y, const char *label,
                                 int *value, int disabled);
int UIRenderDropdown(int id, int x, int y, int w, int h,
                   const char **options, int option_count,
                   int *selected_index);
int UIRenderDropdownEx(int id, int x, int y, int w, int h,
                     const UIDropdownOption *options, int option_count,
                     int *selected_index);
int UIRenderLocaleDropdown(int id, int x, int y, int w, int h,
                         int *selected_index);
void UIRenderInfoRows(UIInfoRows rows);
int UIRenderLabelTextField(UILabelTextField row, int x, int y, int w);
int UIRenderSectionLabel(UISectionLabel label, int x, int y);
int UIRenderCheckboxRow(UICheckboxRow row, int x, int y);
int UIRenderOverlayButton(UIOverlayButton button);
int UIRenderButtonRow(UIButtonRow row);
int UIRenderIconSliderPopup(UIIconSliderPopup popup);
UIIconRowResult UIRenderBottomIconRow(UIBottomIconRow row);
UIBottomNavResult UIRenderBottomNav(UIBottomNav nav);
UIBottomNavConfigResult UIRenderBottomNavConfigModal(UIBottomNavConfigModal modal);
UIToolbarResult UIRenderToolbar(UIToolbar toolbar);
UIToolbarHeaderResult UIRenderToolbarHeader(UIToolbarHeader header);
int UIRenderSubtabBar(UISubtabBar bar);
int UIRenderTabBar(UITabBar bar);
void UIRenderSeparator(Rectangle bounds, int vertical);
UIMenuBarResult UIRenderMenuBar(int id, Rectangle bounds, const UIMenu *menus,
                              int menu_count, int *open_index);
int UIRenderPopupMenu(int id, int x, int y, const UIMenuItem *items,
                    int item_count);
int UIRenderContextMenu(UIContextMenu menu);
int UIRenderRadioButton(UIRadioButton radio);
void UIRenderProgressBar(UIProgressBar progress);
int UIRenderSpinbox(UISpinbox spinbox);
int UIRenderCombobox(UICombobox combo);
void UIRenderLabelFrame(UILabelFrame frame);
void UIRenderImageBox(UIImageBox image);
int UIRenderListBox(UIListBox list);
int UIRenderTreeView(UITreeView tree);
int UIRenderCascadingTreeView(UICascadingTreeView tree);
int UIRenderSourceView(UISourceView source);
int UIRenderTableView(UITableView table);
void UIRenderCanvasGrid(Rectangle bounds, int step, Color color);
int UIRenderNotebook(UINotebook notebook);
int UIRenderPanedView(UIPanedView panes);
int UIRenderCollapsible(UICollapsible section);
int UIRenderMessageDialog(UIMessageDialog dialog);
int UIRenderConfirmDialog(UIConfirmDialog dialog);
int UIRenderPromptDialog(UIPromptDialog dialog);
int UIRenderColorPicker(Rectangle bounds, Color *color);
void UIRenderFocusDebugOverlay(const UIAccessibilityNode *nodes, int count);
UIGuideResult UIRenderGuideOverlay(UIGuideOverlay guide);
int UIRenderThemeSettings(UIThemeSettings settings, UIThemeSettingsState *state);
UIThemeSettingsResult UIRenderThemeSettingsMenus(UIThemeSettings settings,
                                               UIThemeSettingsState *state);
int UIRenderThemeSwitcher(int x, int y, int w, const char *label,
                        const char *light_label, const char *dark_label,
                        int *theme_id, int *dark_mode);
int UIRenderThemePicker(int x, int y, int w, int dark_mode, int *theme_id);
void UIRenderTutorialImagePlaceholder(const char *label, int x, int y,
                                    int w, int h);
void UIRenderTutorialImage(Texture2D texture, const char *fallback,
                         int x, int y, int w, int h);
int UIRenderActionModal(UIModalSpec modal);
int UIRenderModal(const char *title, const char *message,
                const char *cancel_btn, const char *confirm_btn);
int UIRenderModal3Button(const char *title, const char *message,
                       const char *left_btn, const char *middle_btn,
                       const char *right_btn);
void UIRenderTitleBar(const char *title, int height);
int UIRenderReturnTitleBar(Texture2D return_icon, const char *title,
                         int height);
int UIRenderReturnDropdownTitleBar(Texture2D return_icon,
                                 UITitleBarDropdown dropdown, int height);
UIPanelFrame UIRenderModalFrame(int width, int height, const char *title,
                              Texture2D left_icon, Texture2D right_icon);
UISidebarAccountHeaderResult UIRenderSidebarAccountHeader(UISidebarAccountHeader header);
UIProfilePicturePickerResult UIRenderProfilePicturePickerModal(UIProfilePicturePickerModal modal);
void UIRenderReorderHandle(int x, int y, int w, int h, int active);
void UIRenderReorderPlaceholder(Rectangle bounds);
void UIRenderToast(void);
void UIRenderInspectOverlay(void);

/* UTF-8 codec and text-buffer helpers (implemented in ui_text_edit.c). */
int ui_utf8_next_offset(const char *text, int offset);
int ui_utf8_prev_offset(const char *text, int offset);
int ui_utf8_codepoint_count(const char *text);
int ui_utf8_encode(int codepoint, char out[5]);
int ui_text_delete_range(char *text, size_t text_size, int *cursor,
                         int start, int end);
int ui_text_insert_ascii(char *text, size_t text_size, int *cursor, char ch,
                         int max_codepoints);
int ui_text_insert_codepoint(char *text, size_t text_size, int *cursor,
                             int codepoint, int max_codepoints);
int ui_text_insert_text(char *text, size_t text_size, int *cursor,
                        const char *input, int allow_newlines,
                        UITextInputFilter filter, void *filter_user_data,
                        int max_codepoints);

#endif
