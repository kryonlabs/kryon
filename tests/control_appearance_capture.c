#include "kryon.h"
#include "kry_inject.h"
#include "ui_internal.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>

static void
render_column(int x, int fancy)
{
    static char field_text[] = "freelancermap.de";
    static int cursor = 15;
    static int selected = 1;
    static const char *items[] = {"Morning", "Inner Breeze", "Focus"};
    const char *title = fancy ? "Glow / fancy" : "Simple / no glow";
    const char *subtitle = fancy ? "glass, lightfield, blur layers" : "flat material, no glow layers";
    ThemeScheme scheme;

    SetFancyEffectsEnabled(fancy);
    scheme = GetThemeScheme();
    Text((TextProps){.bounds = {x, 28, 320, 32}, .text = title,
        .font = Text24, .typeface = "semibold", .color = scheme.on_surface,
        .wrap = TextWrapNone});
    Text((TextProps){.bounds = {x, 60, 360, 24}, .text = subtitle,
        .font = Text14, .color = scheme.on_surface_variant, .wrap = TextWrapNone});

    Text((TextProps){.bounds = {x, 112, 260, 24}, .text = "Text",
        .font = Text18, .color = scheme.on_surface, .wrap = TextWrapNone});
    Text((TextProps){.bounds = {x, 142, 360, 28}, .text = "Human tomorrow, quieter power.",
        .font = Text18, .color = scheme.on_surface_variant, .wrap = TextWrapNone});

    Text((TextProps){.bounds = {x, 214, 260, 24}, .text = "TextInput",
        .font = Text18, .color = scheme.on_surface, .wrap = TextWrapNone});
    DrawTextInput((Rectangle){x, 246, 300, 44}, field_text, cursor, 1, 1,
        Text18, (TextInputStyle){0}, 8100 + fancy);

    Text((TextProps){.bounds = {x, 330, 260, 24}, .text = "Button",
        .font = Text18, .color = scheme.on_surface, .wrap = TextWrapNone});
    Button((ButtonProps){.bounds = {x, 362, 220, 44}, .label = "Save changes",
        .id = 8200 + fancy, .tone = ButtonToneAccent,
        .emphasis = ButtonEmphasisFilled});

    Text((TextProps){.bounds = {x, 450, 260, 24}, .text = "Dropdown",
        .font = Text18, .color = scheme.on_surface, .wrap = TextWrapNone});
    Dropdown(8300 + fancy, x, 482, 260, 44, items, 3, &selected);
}

int
main(int argc, char **argv)
{
    const char *out = argc > 1 ? argv[1] : "build/linux-x86_64/control-appearance-side-by-side.png";
    const int width = 900;
    const int height = 620;
    RenderTexture2D target;
    Image image;

    SetConfigFlags(FLAG_WINDOW_HIDDEN | FLAG_MSAA_4X_HINT);
    InitWindow(width, height, "Kryon appearance comparison");
    SetTargetFPS(60);
    SetThemeMode(THEME_MODE_DARK);
    target = LoadRenderTexture(width, height);

    InjectReset();
    InjectMousePosition(-1000, -1000);
    InjectPump();
    BeginDrawing();
    BeginTextureMode(target);
    ClearBackground(GetThemeBackground());
    BeginUIFrame(width, height, 1.0f);
    BeginTree(Key("control-appearance-capture"));

    DrawRectangle(0, 0, width / 2, height, GetThemeBackground());
    DrawRectangle(width / 2, 0, width / 2, height, GetColor(0x061a30ff));
    DrawRectangle(width / 2 - 1, 24, 2, height - 48, GetThemeBorder());
    render_column(72, 1);
    render_column(522, 0);

    EndTree();
    EndUIFrame();
    EndTextureMode();
    EndDrawing();

    image = LoadImageFromTexture(target.texture);
    ImageFlipVertical(&image);
    if(!ExportImage(image, out)) {
        fprintf(stderr, "failed to write %s\n", out);
        UnloadImage(image);
        UnloadRenderTexture(target);
        CloseWindow();
        return 1;
    }
    UnloadImage(image);
    UnloadRenderTexture(target);
    CloseWindow();
    printf("%s\n", out);
    fflush(NULL);
    _Exit(0);
}
