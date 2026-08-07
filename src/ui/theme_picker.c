#include "ui_internal.h"
#include "ui_layout.h"

#define UI_THEME_SETTINGS_ROW_H 30
#define UI_THEME_SETTINGS_ROW_GAP 10

static const ThemeId theme_picker_order[THEME_COUNT] = {
    THEME_COBALT,
    THEME_CHERRY,
    THEME_DAWN,
    THEME_FOREST,
    THEME_LAVENDER,
    THEME_MINT,
    THEME_MONO,
    THEME_OCEAN,
    THEME_SAGE,
    THEME_INK,
    THEME_SKY,
    THEME_SUNSET
};

typedef struct {
    int circle_size;
    int label_gap;
    int col_gap;
    int cell_w;
    int per_row;
    int row_width;
    int row_step;
    int row_count;
    int height;
} UIThemeGridLayout;

static const char *
ui_theme_label(ThemeId theme)
{
    return GetThemeLabel(theme);
}

static const char *
ui_theme_settings_text(const char *text, const char *fallback)
{
    return text != NULL && text[0] != '\0' ? text : fallback;
}

static int
ui_theme_settings_show_mode(UIThemeSettings settings)
{
    if(settings.theme_source != NULL &&
       *settings.theme_source == THEME_SOURCE_SYSTEM &&
       !SystemThemeSupportsMode())
        return 0;
    return 1;
}

static int
ui_theme_palette_option_count(UIThemeSettings settings)
{
    return THEME_COUNT + (settings.allow_system_source ? 1 : 0);
}

static int
ui_theme_palette_index(UIThemeSettings settings)
{
    int offset = settings.allow_system_source ? 1 : 0;
    int theme = settings.theme_id != NULL ? *settings.theme_id : THEME_SKY;

    if(settings.allow_system_source && settings.theme_source != NULL &&
       *settings.theme_source == THEME_SOURCE_SYSTEM)
        return 0;
    for(int i = 0; i < THEME_COUNT; i++) {
        if(theme_picker_order[i] == NormalizeTheme(theme))
            return i + offset;
    }
    return offset;
}

static ThemeId
ui_theme_palette_theme_at(UIThemeSettings settings, int index)
{
    int offset = settings.allow_system_source ? 1 : 0;

    index -= offset;
    if(index < 0 || index >= THEME_COUNT)
        return THEME_SKY;
    return theme_picker_order[index];
}

int
ui_theme_settings_height(UIThemeSettings settings)
{
    int rows = 1;

    if(settings.theme_source == NULL || settings.theme_mode == NULL ||
       settings.theme_id == NULL)
        return 0;
    if(ui_theme_settings_show_mode(settings))
        rows++;
    return rows * (GetUIFontSize() + ScaleUIPx(8) + ScaleUIPx(UI_THEME_SETTINGS_ROW_H)) +
           (rows - 1) * ScaleUIPx(UI_THEME_SETTINGS_ROW_GAP);
}

int
UIRenderThemeSettings(UIThemeSettings settings, UIThemeSettingsState *state)
{
    const char *mode_options[3];
    const char *theme_options[THEME_COUNT + 1];
    int y = settings.y;
    int label_gap = ScaleUIPx(8);
    int row_h = ScaleUIPx(UI_THEME_SETTINGS_ROW_H);
    int row_gap = ScaleUIPx(UI_THEME_SETTINGS_ROW_GAP);
    int font = GetUIFontSize();
    int palette_count;
    int palette_index;
    int show_mode;

    if(state != NULL)
        memset(state, 0, sizeof(*state));
    if(settings.theme_source == NULL || settings.theme_mode == NULL ||
       settings.theme_id == NULL || settings.w <= 0)
        return y;

    show_mode = ui_theme_settings_show_mode(settings);
    if(!show_mode && *settings.theme_source == THEME_SOURCE_SYSTEM &&
       *settings.theme_mode != THEME_MODE_SYSTEM)
        *settings.theme_mode = THEME_MODE_SYSTEM;

    mode_options[THEME_MODE_SYSTEM] =
        ui_theme_settings_text(settings.mode_system_label, "System");
    mode_options[THEME_MODE_LIGHT] =
        ui_theme_settings_text(settings.mode_light_label, "Light");
    mode_options[THEME_MODE_DARK] =
        ui_theme_settings_text(settings.mode_dark_label, "Dark");
    *settings.theme_mode = ui_clampi(*settings.theme_mode,
                                     THEME_MODE_SYSTEM,
                                     THEME_MODE_DARK);
    if(show_mode) {
        UIRenderText(ui_theme_settings_text(settings.mode_label, "Mode"),
                   settings.x, y, font, c_text);
        if(UIRenderDropdown(settings.id_base + 1, settings.x, y + font + label_gap,
                          settings.w, row_h, mode_options, 3,
                          settings.theme_mode)) {
            if(state != NULL)
                state->draw_mode_menu = 2;
        } else if(state != NULL) {
            state->draw_mode_menu = 1;
        }
        y += font + label_gap + row_h + row_gap;
    } else {
        ui_dropdown_close(settings.id_base + 1);
    }

    palette_count = ui_theme_palette_option_count(settings);
    palette_index = ui_theme_palette_index(settings);
    if(settings.allow_system_source) {
        theme_options[0] = ui_theme_settings_text(settings.source_system_label,
                                                  "System");
        for(int i = 0; i < THEME_COUNT; i++)
            theme_options[i + 1] = ui_theme_label(theme_picker_order[i]);
    } else {
        for(int i = 0; i < THEME_COUNT; i++)
            theme_options[i] = ui_theme_label(theme_picker_order[i]);
    }
    if(state != NULL) {
        state->palette_index = ui_clampi(palette_index, 0, palette_count - 1);
        palette_index = state->palette_index;
    }
    UIRenderText(ui_theme_settings_text(settings.palette_label, "Color"),
               settings.x, y, font, c_text);
    if(UIRenderDropdown(settings.id_base + 2, settings.x, y + font + label_gap,
                      settings.w, row_h, theme_options, palette_count,
                      state != NULL ? &state->palette_index : &palette_index)) {
        if(state != NULL)
            state->draw_palette_menu = 2;
    } else if(state != NULL) {
        state->draw_palette_menu = 1;
    }
    y += font + label_gap + row_h + row_gap;

    if(settings.theme_style != NULL)
        *settings.theme_style = THEME_STYLE_RETRO;
    ui_dropdown_close(settings.id_base + 3);

    return y - row_gap;
}

UIThemeSettingsResult
UIRenderThemeSettingsMenus(UIThemeSettings settings, UIThemeSettingsState *state)
{
    UIThemeSettingsResult result = {0};

    if(state == NULL)
        return result;
    if(state->draw_mode_menu == 2)
        result.mode_changed = 1;
    if(state->draw_palette_menu == 2) {
        int previous_source = settings.theme_source != NULL ? *settings.theme_source : THEME_SOURCE_APP;
        int previous_theme = settings.theme_id != NULL ? *settings.theme_id : THEME_SKY;
        int palette_count = ui_theme_palette_option_count(settings);

        state->palette_index = ui_clampi(state->palette_index, 0, palette_count - 1);
        if(settings.allow_system_source && state->palette_index == 0) {
            if(settings.theme_source != NULL)
                *settings.theme_source = THEME_SOURCE_SYSTEM;
        } else {
            if(settings.theme_source != NULL)
                *settings.theme_source = THEME_SOURCE_APP;
            if(settings.theme_id != NULL)
                *settings.theme_id = ui_theme_palette_theme_at(settings,
                                                               state->palette_index);
        }
        result.palette_changed = 1;
        result.source_changed =
            settings.theme_source != NULL && previous_source != *settings.theme_source;
        if(settings.theme_id != NULL && previous_theme != *settings.theme_id)
            result.palette_changed = 1;
    }
    result.changed = result.source_changed || result.mode_changed ||
                     result.palette_changed;
    if(settings.theme_source != NULL)
        *settings.theme_source = ui_clampi(*settings.theme_source,
                                                 THEME_SOURCE_APP,
                                                 settings.allow_system_source
                                                     ? THEME_SOURCE_SYSTEM
                                                     : THEME_SOURCE_APP);
    if(settings.theme_mode != NULL)
        *settings.theme_mode = ui_clampi(*settings.theme_mode,
                                               THEME_MODE_SYSTEM,
                                               THEME_MODE_DARK);
    if(settings.theme_source != NULL && settings.theme_mode != NULL &&
       *settings.theme_source == THEME_SOURCE_SYSTEM &&
       !ui_theme_settings_show_mode(settings) &&
       *settings.theme_mode != THEME_MODE_SYSTEM) {
        *settings.theme_mode = THEME_MODE_SYSTEM;
        result.mode_changed = 1;
        result.changed = 1;
    }
    if(settings.theme_id != NULL)
        *settings.theme_id = ui_clampi(*settings.theme_id, 0,
                                             THEME_COUNT - 1);
    if(settings.theme_style != NULL)
        *settings.theme_style = THEME_STYLE_RETRO;
    return result;
}

static UIThemeGridLayout
ui_theme_grid_layout(int w)
{
    UIThemeGridLayout layout = {0};
    int small_font = UI_TEXT_12;

    int row_gap = ScaleUIPx(14);
    layout.circle_size = ScaleUIPx(24);
    layout.label_gap = ScaleUIPx(6);
    layout.col_gap = ScaleUIPx(10);
    layout.cell_w = layout.circle_size;
    for(int i = 0; i < THEME_COUNT; i++) {
        int name_w = MeasureUIText(ui_theme_label((ThemeId)i), small_font) + ScaleUIPx(8);
        if(name_w > layout.cell_w)
            layout.cell_w = name_w;
    }

    layout.per_row = (w + layout.col_gap) / (layout.cell_w + layout.col_gap);
    if(layout.per_row < 1)
        layout.per_row = 1;
    if(layout.per_row > 3)
        layout.per_row = 3;
    if(layout.per_row > THEME_COUNT)
        layout.per_row = THEME_COUNT;
    layout.row_width = layout.per_row * layout.cell_w + (layout.per_row - 1) * layout.col_gap;

    layout.row_step = layout.circle_size + layout.label_gap + small_font + row_gap;
    layout.row_count = (THEME_COUNT + layout.per_row - 1) / layout.per_row;
    layout.height = layout.circle_size + layout.label_gap + small_font;
    if(layout.row_count > 1)
        layout.height += (layout.row_count - 1) * layout.row_step;

    return layout;
}

static int
ui_draw_theme_grid(int x, int circle_y, int w, int dark, int *theme_id)
{
    int changed = 0;
    int small_font = UI_TEXT_12;
    int selected = theme_id != NULL ? *theme_id : THEME_SUNSET;
    UIThemeGridLayout layout = ui_theme_grid_layout(w);
    int start_x = x + (w - layout.row_width) / 2;
    Vector2 mouse_world = ui_mouse_world();

    if(selected < 0 || selected >= THEME_COUNT)
        selected = THEME_SUNSET;

    for(int i = 0; i < THEME_COUNT; i++) {
        ThemeId theme = theme_picker_order[i];
        int row = i / layout.per_row;
        int col = i % layout.per_row;
        int cell_x = start_x + col * (layout.cell_w + layout.col_gap);
        int cx = cell_x + layout.cell_w / 2;
        int cy = circle_y + row * layout.row_step;
        Color theme_color = c_circle;

        if(!GetThemeCatalogColor(theme, dark != 0, "circle", &theme_color)) {
            const char *scope = GetThemeScopeName(theme, dark != 0);
            theme_color = GetThemeColor(scope, "circle");
        }

        Rectangle bounds = {
            (float)(cx - layout.circle_size / 2 - ScaleUIPx(4)),
            (float)(cy - layout.circle_size / 2 - ScaleUIPx(4)),
            (float)(layout.circle_size + ScaleUIPx(8)),
            (float)(layout.circle_size + ScaleUIPx(8))
        };
        int is_hovered = CheckCollisionPointRec(mouse_world, bounds) && !UIInputCapturesClick(mouse_world);

        int draw_size = is_hovered ? layout.circle_size + ScaleUIPx(4) : layout.circle_size;
        DrawCircle(cx, cy, draw_size / 2, theme_color);
        DrawCircleLines(cx, cy, draw_size / 2 + (selected == (int)theme ? ScaleUIPx(2) : ScaleUIPx(1)),
                        selected == (int)theme ? c_text : DarkenUIColor(c_bg, 30));

        if(is_hovered) {
            MarkUIClickable();
            if(IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) {
                UIConsumeRelease();
                selected = theme;
                if(theme_id != NULL)
                    *theme_id = theme;
                changed = 1;
            }
        }

        const char *name = ui_theme_label(theme);
        int name_w = MeasureUIText(name, small_font);
        UIRenderText(name, cx - name_w / 2,
                        cy + layout.circle_size / 2 + layout.label_gap,
                        small_font, c_text);
    }

    return changed;
}

int
UIRenderThemeSwitcher(int x, int y, int w, const char *label,
                       const char *light_label, const char *dark_label,
                       int *theme_id, int *dark_mode)
{
    int changed = 0;
    int font = GetUIFontSize();
    int dark = dark_mode != NULL ? *dark_mode : 0;

    UIRenderText(label ? label : "Theme", x, y, font, c_text);

    int light_w = MeasureUIText(light_label ? light_label : "Light", font);
    int dark_w = MeasureUIText(dark_label ? dark_label : "Dark", font);
    int max_label_w = light_w > dark_w ? light_w : dark_w;
    int toggle_w = max_label_w * 2 + ScaleUIPx(32);
    int min_toggle_w = ScaleUIPx(100);
    if(toggle_w < min_toggle_w)
        toggle_w = min_toggle_w;
    if(toggle_w > w)
        toggle_w = w;

    int toggle_h = ScaleUIPx(28);
    int toggle_x = x + w - toggle_w - ScaleUIPx(8);
    int toggle_y = y - ScaleUIPx(2);
    if(UIRenderToggleSwitch(toggle_x, toggle_y, toggle_w, toggle_h, &dark,
                             light_label ? light_label : "Light",
                             dark_label ? dark_label : "Dark")) {
        if(dark_mode != NULL)
            *dark_mode = dark;
        changed = 1;
    }

    if(ui_draw_theme_grid(x, y + ScaleUIPx(64), w, dark, theme_id))
        changed = 1;

    return changed;
}

int
UIRenderThemePicker(int x, int y, int w, int dark_mode,
                     int *theme_id)
{
    int changed = 0;

    if(ui_draw_theme_grid(x, y + ScaleUIPx(12), w, dark_mode != 0, theme_id))
        changed = 1;

    return changed;
}

int
ui_theme_picker_height(int w)
{
    return ScaleUIPx(12) + ui_theme_grid_layout(w).height;
}
