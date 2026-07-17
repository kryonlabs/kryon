#include "ui_internal.h"
#include "ui_layout.h"
#include "locale.h"

#define UI_THEME_SETTINGS_ROW_H 36
#define UI_THEME_SETTINGS_ROW_GAP 14


static const ThemeId theme_picker_order[THEME_COUNT] = {
    THEME_SKY,
    THEME_OCEAN,
    THEME_COBALT,
    THEME_FOREST,
    THEME_SAGE,
    THEME_MINT,
    THEME_SUNSET,
    THEME_INK,
    THEME_DAWN,
    THEME_CHERRY,
    THEME_LAVENDER,
    THEME_MONO
};

static int
ui_theme_clampi(int value, int min_value, int max_value)
{
    if(value < min_value)
        return min_value;
    if(value > max_value)
        return max_value;
    return value;
}

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
    const char *key = NULL;
    const char *label;

    switch(NormalizeTheme(theme)) {
    case THEME_SKY: key = "theme_sky"; break;
    case THEME_OCEAN: key = "theme_ocean"; break;
    case THEME_FOREST: key = "theme_forest"; break;
    case THEME_SUNSET: key = "theme_sunset"; break;
    case THEME_LAVENDER: key = "theme_lavender"; break;
    case THEME_CHERRY: key = "theme_cherry"; break;
    case THEME_DAWN: key = "theme_dawn"; break;
    case THEME_SAGE: key = "theme_sage"; break;
    case THEME_INK: key = "theme_sepia"; break;
    case THEME_MONO: key = "theme_mono"; break;
    case THEME_MINT: key = "theme_mint"; break;
    case THEME_COBALT: key = "theme_cobalt"; break;
    default: break;
    }

    if(key == NULL)
        return GetThemeLabel(theme);
    label = GetLocaleText(key);
    if(label == NULL || label[0] == '\0' || strcmp(label, key) == 0)
        return GetThemeLabel(theme);
    return label;
}

static const char *
ui_theme_settings_text(const char *text, const char *fallback)
{
    return text != NULL && text[0] != '\0' ? text : fallback;
}

int
GetUIThemeSettingsHeight(UIThemeSettings settings)
{
    int rows = 1;

    if(settings.theme_source == NULL || settings.theme_mode == NULL ||
       settings.theme_id == NULL)
        return 0;
    if(*settings.theme_source == THEME_SOURCE_APP || settings.allow_system_mode)
        rows++;
    if(*settings.theme_source == THEME_SOURCE_APP)
        rows++;
    return rows * (GetUIFontSize() + ScaleUIPx(8) + ScaleUIPx(UI_THEME_SETTINGS_ROW_H)) +
           (rows - 1) * ScaleUIPx(UI_THEME_SETTINGS_ROW_GAP);
}

int
DrawUIThemeSettings(UIThemeSettings settings, UIThemeSettingsState *state)
{
    const char *source_options[2];
    const char *mode_options[3];
    const char *theme_options[THEME_COUNT];
    int y = settings.y;
    int label_gap = ScaleUIPx(8);
    int row_h = ScaleUIPx(UI_THEME_SETTINGS_ROW_H);
    int row_gap = ScaleUIPx(UI_THEME_SETTINGS_ROW_GAP);
    int font = GetUIFontSize();
    int source_count;

    if(state != NULL)
        memset(state, 0, sizeof(*state));
    if(settings.theme_source == NULL || settings.theme_mode == NULL ||
       settings.theme_id == NULL || settings.w <= 0)
        return y;

    source_count = settings.allow_system_source ? 2 : 1;
    source_options[THEME_SOURCE_APP] =
        ui_theme_settings_text(settings.source_app_label, "App");
    source_options[THEME_SOURCE_SYSTEM] =
        ui_theme_settings_text(settings.source_system_label, "System");
    *settings.theme_source = ui_theme_clampi(*settings.theme_source,
                                             THEME_SOURCE_APP,
                                             source_count - 1);
    DrawUIText(ui_theme_settings_text(settings.theme_label, "Theme"),
               settings.x, y, font, c_text);
    DrawUIDropdownButton(settings.id_base, settings.x, y + font + label_gap,
                         settings.w, row_h, source_options, source_count,
                         settings.theme_source);
    if(state != NULL)
        state->draw_source_menu = 1;
    y += font + label_gap + row_h + row_gap;

    if(*settings.theme_source == THEME_SOURCE_APP || settings.allow_system_mode) {
        mode_options[THEME_MODE_SYSTEM] =
            ui_theme_settings_text(settings.mode_system_label, "Follow device");
        mode_options[THEME_MODE_LIGHT] =
            ui_theme_settings_text(settings.mode_light_label, "Light");
        mode_options[THEME_MODE_DARK] =
            ui_theme_settings_text(settings.mode_dark_label, "Dark");
        *settings.theme_mode = ui_theme_clampi(*settings.theme_mode,
                                               THEME_MODE_SYSTEM,
                                               THEME_MODE_DARK);
        DrawUIText(ui_theme_settings_text(settings.mode_label, "Mode"),
                   settings.x, y, font, c_text);
        DrawUIDropdownButton(settings.id_base + 1, settings.x, y + font + label_gap,
                             settings.w, row_h, mode_options, 3,
                             settings.theme_mode);
        if(state != NULL)
            state->draw_mode_menu = 1;
        y += font + label_gap + row_h + row_gap;
    } else {
        *settings.theme_mode = THEME_MODE_SYSTEM;
        if(settings.system_theme_label != NULL && settings.system_theme_label[0] != '\0') {
            DrawUIText(settings.system_theme_label, settings.x, y,
                       GetUISmallFontSize(), DarkenUIColor(c_text, 28));
            y += GetUISmallFontSize() + row_gap;
        }
    }

    if(*settings.theme_source == THEME_SOURCE_APP) {
        for(int i = 0; i < THEME_COUNT; i++)
            theme_options[i] = ui_theme_label((ThemeId)i);
        *settings.theme_id = ui_theme_clampi(*settings.theme_id, 0,
                                             THEME_COUNT - 1);
        DrawUIText(ui_theme_settings_text(settings.palette_label, "Palette"),
                   settings.x, y, font, c_text);
        DrawUIDropdownButton(settings.id_base + 2, settings.x, y + font + label_gap,
                             settings.w, row_h, theme_options, THEME_COUNT,
                             settings.theme_id);
        if(state != NULL)
            state->draw_palette_menu = 1;
        y += font + label_gap + row_h + row_gap;
    }

    return y - row_gap;
}

UIThemeSettingsResult
DrawUIThemeSettingsMenus(UIThemeSettings settings, UIThemeSettingsState *state)
{
    UIThemeSettingsResult result = {0};

    if(state == NULL)
        return result;
    if(state->draw_source_menu && DrawUIDropdownMenu(settings.id_base))
        result.source_changed = 1;
    if(state->draw_mode_menu && DrawUIDropdownMenu(settings.id_base + 1))
        result.mode_changed = 1;
    if(state->draw_palette_menu && DrawUIDropdownMenu(settings.id_base + 2))
        result.palette_changed = 1;
    result.changed = result.source_changed || result.mode_changed ||
                     result.palette_changed;
    if(settings.theme_source != NULL)
        *settings.theme_source = ui_theme_clampi(*settings.theme_source,
                                                 THEME_SOURCE_APP,
                                                 settings.allow_system_source
                                                     ? THEME_SOURCE_SYSTEM
                                                     : THEME_SOURCE_APP);
    if(settings.theme_mode != NULL)
        *settings.theme_mode = ui_theme_clampi(*settings.theme_mode,
                                               THEME_MODE_SYSTEM,
                                               THEME_MODE_DARK);
    if(settings.theme_id != NULL)
        *settings.theme_id = ui_theme_clampi(*settings.theme_id, 0,
                                             THEME_COUNT - 1);
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
    int selected = theme_id != NULL ? *theme_id : THEME_SKY;
    UIThemeGridLayout layout = ui_theme_grid_layout(w);
    int start_x = x + (w - layout.row_width) / 2;
    Vector2 mouse_world = ui_mouse_world();

    if(selected < 0 || selected >= THEME_COUNT)
        selected = THEME_SKY;

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
        DrawUIText(name, cx - name_w / 2,
                        cy + layout.circle_size / 2 + layout.label_gap,
                        small_font, c_text);
    }

    return changed;
}

int
DrawUIThemeSwitcher(int x, int y, int w, const char *label,
                       const char *light_label, const char *dark_label,
                       int *theme_id, int *dark_mode)
{
    int changed = 0;
    int font = GetUIFontSize();
    int dark = dark_mode != NULL ? *dark_mode : 0;

    DrawUIText(label ? label : "Theme", x, y, font, c_text);

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
    if(DrawUIToggleSwitch(toggle_x, toggle_y, toggle_w, toggle_h, &dark,
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
DrawUIThemePicker(int x, int y, int w, int dark_mode,
                     int *theme_id)
{
    int changed = 0;

    if(ui_draw_theme_grid(x, y + ScaleUIPx(12), w, dark_mode != 0, theme_id))
        changed = 1;

    return changed;
}

int
GetUIThemePickerHeight(int w)
{
    return ScaleUIPx(12) + ui_theme_grid_layout(w).height;
}
