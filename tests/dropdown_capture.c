#include "kryon.h"
#include "kry_inject.h"
#include "../examples/kryon_example_font.h"

#include <stdio.h>

/* Capture actual dropdowns beside buttons using the same theme. */
static void
frame(RenderTexture2D target, int dark)
{
    static const char *options[] = {"Light", "Dark", "System", "High contrast",
        "A long option label that must stop before the selection indicator"};
    static int selected = 1;
    InjectPump();
    BeginDrawing();
    BeginTextureMode(target);
    ClearBackground(GetThemeBackground());
    BeginUIFrame(880, 640, 1.0f);
    BeginTree(Key("dropdown-capture"));
    Text((TextProps){.text = dark ? "Dropdowns / Dark" : "Dropdowns / Light",
        .bounds = {40, 26, 780, 40}, .font = 26});
    Text((TextProps){.text = "Shared button materials and theme colors",
        .bounds = {40, 74, 780, 30}, .font = 16});
    Button((ButtonProps){.bounds = {40, 126, 180, 42}, .label = "Neutral button",
        .tone = ButtonToneNeutral, .emphasis = ButtonEmphasisSoft, .id = 9200});
    Button((ButtonProps){.bounds = {248, 126, 180, 42}, .label = "Accent button",
        .tone = ButtonToneAccent, .emphasis = ButtonEmphasisSoft, .id = 9201});
    Dropdown(9100, 40, 210, 388, 44, options, 5, &selected);
    Dropdown(9101, 480, 210, 340, 44, options, 5, &selected);
    BeginDisabled(1);
    Dropdown(9102, 480, 288, 340, 44, options, 5, &selected);
    EndDisabled();
    Dropdown(9103, 480, 390, 160, 30, options, 5, &selected);
    Dropdown(9104, 480, 458, 340, 54, options, 5, &selected);
    EndTree();
    EndUIFrame();
    EndTextureMode();
    EndDrawing();
}

int
main(int argc, char **argv)
{
    if(argc != 2)
        return 2;
    SetConfigFlags(FLAG_WINDOW_HIDDEN);
    InitWindow(880, 640, "Dropdown verification");
    if(!IsWindowReady())
        return 1;
    SetTargetFPS(60);
    LoadExampleUIFont();
    InitUI(880, 640, 1.0f);
    SetThemeStyle(THEME_STYLE_DEFAULT);
    RenderTexture2D target = LoadRenderTexture(880, 640);
    for(int dark = 0; dark < 2; dark++) {
        SetThemeMode(dark ? THEME_MODE_DARK : THEME_MODE_LIGHT);
        ApplyCurrentTheme();
        InjectReset();
        InjectKeyTap(KEY_ESCAPE);
        frame(target, dark);
        SetUIFocus(9100);
        InjectKeyTap(KEY_SPACE);
        frame(target, dark);
        for(int i = 0; i < 24; i++) {
            InjectMousePosition(180, 366);
            frame(target, dark);
        }
        Image image = LoadImageFromTexture(target.texture);
        ImageFlipVertical(&image);
        char path[1024];
        snprintf(path, sizeof(path), "%s/%s.png", argv[1], dark ? "dark" : "light");
        int saved = ExportImage(image, path);
        UnloadImage(image);
        if(!saved)
            return 1;
    }
    UnloadRenderTexture(target);
    UnloadExampleUIFont();
    CloseWindow();
    return 0;
}
