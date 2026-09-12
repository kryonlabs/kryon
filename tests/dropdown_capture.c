#include "kryon.h"
#include "kry_inject.h"
#include "../examples/kryon_example_font.h"
#include "../src/ui/dropdown_store.h"
#include "../src/ui/ui_blend_internal.h"

#include <stdio.h>

#define WIDTH 768
#define HEIGHT 960
#define SAMPLE_COUNT 9

static DropdownStore *stores[SAMPLE_COUNT];
static const DropdownOption theme_items[] = {
    {.label = "Light", .icon_type = ICON_SUN},
    {.label = "Dark", .icon_type = ICON_MOON},
    {.label = "System", .icon_type = ICON_PLATFORMS_BROWSER},
    {.label = "High contrast", .icon_type = ICON_EYE},
    {.label = "Auto (Beta)", .icon_type = ICON_GEAR, .separator_before = 1},
    {.label = "Legacy", .icon_type = ICON_LIGHTOFF, .disabled = 1}
};
static const DropdownOption fonts[] = {
    {.label = "Inter", .icon_type = ICON_EDIT},
    {.label = "Roboto", .icon_type = ICON_EDIT},
    {.label = "Mono", .icon_type = ICON_EDIT},
    {.label = "Poppins", .icon_type = ICON_EDIT},
    {.label = "Open Sans", .icon_type = ICON_EDIT},
    {.label = "Comic Sans", .icon_type = ICON_EDIT, .disabled = 1}
};

static void
label(const char *text, int x, int y, int font, Color color)
{
    Text((TextProps){.text = text, .bounds = {x, y, 720, font + 8},
        .font = font, .color = color});
}

/* Each specimen owns a normal dropdown store, so the board can show two open
 * menus without changing the application's one-open-menu interaction rule.
 * Every specimen is painted by the runtime, driven with actual input. */
static void
sample(int index, Rectangle bounds, const DropdownOption *items, int count,
       int chosen, int open, int hovered, int focused, int disabled)
{
    DropdownStore *previous = dropdown_store_swap(stores[index]);
    int id = 9100 + index;
    InjectReset();
    if(open)
        InjectKeyTap(KEY_SPACE);
    if(hovered)
        InjectMousePosition(bounds.x + 60, bounds.y + bounds.height / 2);
    else if(open && index == 5)
        InjectMousePosition(bounds.x + 80, bounds.y + bounds.height * 3.5f + 4);
    else
        InjectMousePosition(-100, -100);
    InjectPump();
    BeginUIFrame(WIDTH, HEIGHT, 1.0f);
    SetUIFocus(focused || open ? id : 0);
    BeginDisabled(disabled);
    Dropdown((DropdownProps){.id = id, .bounds = bounds,
        .items = items, .option_count = count, .selected_index = &chosen});
    EndDisabled();
    EndUIFrame();
    dropdown_store_swap(previous);
}

static void
board(RenderTexture2D target, int dark)
{
    BeginDrawing();
    BeginTextureMode(target);
    ui_blend_capture();
    Color background = GetThemeBackground();
    Color text = GetThemeText();
    Color accent = GetTheme().colors.accent;
    ClearBackground(background);
    /* Diffuse background light makes the translucent material visible. */
    DrawCircleGradient((Vector2){690, 650}, 340, Fade(accent, dark ? 0.22f : 0.08f), Fade(accent, 0));
    DrawCircleGradient((Vector2){-70, 920}, 310, Fade(accent, dark ? 0.18f : 0.09f), Fade(accent, 0));
    DrawRectangleRoundedLinesEx((Rectangle){4, 4, 760, 950}, 0.025f, 12, 1,
        Fade(accent, dark ? 0.38f : 0.13f));
    BeginUIFrame(WIDTH, HEIGHT, 1.0f);
    label(dark ? "DARK THEME" : "LIGHT THEME", 28, 16, 22, text);
    label("FROSTED GLASS / SHARED BUTTON STYLING", 28, 48, 12, Fade(text, 0.72f));
    const char *states[] = {"Normal", "Hover", "Focus", "Disabled"};
    for(int i = 0; i < 4; i++)
        label(states[i], 30 + i * 186, 96, 14, text);
    DrawLine(28, 174, 740, 174, Fade(accent, 0.26f));
    label("SINGLE SELECT", 38, 190, 18, text);
    label("FONT OPTIONS", 400, 190, 18, text);
    DrawLine(28, 624, 740, 624, Fade(accent, 0.26f));
    label("SIZES", 28, 640, 18, text);
    label("Compact", 30, 695, 14, text);
    label("Regular", 238, 695, 14, text);
    label("Large", 475, 695, 14, text);
    DrawLine(28, 814, 740, 814, Fade(accent, 0.26f));
    label("BUTTON REFERENCE / SAME STATES", 28, 833, 16, text);
    ButtonState button_states[] = {ButtonStateNormal, ButtonStateHover,
        ButtonStateFocus, ButtonStateDisabled};
    for(int i = 0; i < 4; i++) {
        Button((ButtonProps){.bounds = {30 + i * 186, 869, 158, 42},
            .label = "Button", .id = 9200 + i, .tone = ButtonToneNeutral,
            .emphasis = ButtonEmphasisSoft, .state = button_states[i]});
    }
    label("Arrow keys to navigate   /   Enter to select   /   Escape to close", 28, 927, 13, Fade(text, 0.60f));
    EndUIFrame();
    for(int i = 0; i < 4; i++)
        sample(i, (Rectangle){30 + i * 186, 122, 158, 42}, theme_items, 6, 2,
            0, i == 1, i == 2, i == 3);
    sample(4, (Rectangle){40, 230, 302, 44}, theme_items, 6, 1, 1, 0, 0, 0);
    sample(5, (Rectangle){400, 230, 326, 44}, fonts, 6, 0, 1, 0, 0, 0);
    sample(6, (Rectangle){30, 725, 162, 32}, theme_items, 6, 2, 0, 0, 0, 0);
    sample(7, (Rectangle){238, 721, 190, 42}, theme_items, 6, 2, 0, 0, 0, 0);
    sample(8, (Rectangle){475, 715, 250, 54}, theme_items, 6, 2, 0, 0, 0, 0);
    EndTextureMode();
    EndDrawing();
}

int
main(int argc, char **argv)
{
    if(argc != 2)
        return 2;
    SetConfigFlags(FLAG_WINDOW_HIDDEN);
    InitWindow(WIDTH, HEIGHT, "Dropdown verification");
    if(!IsWindowReady())
        return 1;
    SetTargetFPS(60);
    LoadExampleUIFont();
    InitUI(WIDTH, HEIGHT, 1.0f);
    SetThemeStyle(THEME_STYLE_DEFAULT);
    RenderTexture2D target = LoadRenderTexture(WIDTH, HEIGHT);
    Image panels[2];
    for(int dark = 0; dark < 2; dark++) {
        SetThemeMode(dark ? THEME_MODE_DARK : THEME_MODE_LIGHT);
        ApplyCurrentTheme();
        for(int i = 0; i < SAMPLE_COUNT; i++)
            stores[i] = dropdown_store_new();
        for(int tick = 0; tick < 24; tick++)
            board(target, dark);
        panels[dark] = LoadImageFromTexture(target.texture);
        ImageFlipVertical(&panels[dark]);
        char path[1024];
        snprintf(path, sizeof(path), "%s/%s.png", argv[1], dark ? "dark" : "light");
        if(!ExportImage(panels[dark], path))
            return 1;
        for(int i = 0; i < SAMPLE_COUNT; i++)
            dropdown_store_free(stores[i]);
    }
    Image combined = GenImageColor(1536, 1024, (Color){3, 25, 48, 255});
    ImageDrawImageRec(&combined, panels[1], (Rectangle){0, 0, WIDTH, HEIGHT},
        (Vector2){0, 64}, WHITE);
    ImageDrawImageRec(&combined, panels[0], (Rectangle){0, 0, WIDTH, HEIGHT},
        (Vector2){768, 64}, WHITE);
    ImageDrawTextEx(&combined, GetUIFontForCodepoint('K', 24), "K R Y O N", (Vector2){28, 20}, 24, 1, WHITE);
    ImageDrawTextEx(&combined, GetUIFontForCodepoint('D', 30), "DROPDOWNS / FROSTED GLASS", (Vector2){472, 16}, 30, 1, WHITE);
    char path[1024];
    snprintf(path, sizeof(path), "%s/board.png", argv[1]);
    int saved = ExportImage(combined, path);
    UnloadImage(combined);
    UnloadImage(panels[0]);
    UnloadImage(panels[1]);
    UnloadRenderTexture(target);
    UnloadExampleUIFont();
    CloseWindow();
    return saved ? 0 : 1;
}
