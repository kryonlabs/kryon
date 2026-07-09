#include "ui.h"
#include "ui_layout.h"
#include "locale.h"


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
