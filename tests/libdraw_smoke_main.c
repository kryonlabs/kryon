#include "kryon.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void kry_libdraw_texture_preserve_colors(Texture2D texture);

static int failures;

static void
check(const char *name, int ok)
{
    if(!ok) {
        fprintf(stderr, "libdraw smoke failed: %s\n", name);
        failures++;
    }
}

int
main(void)
{
    const char *out = getenv("KRYON_LIBDRAW_SMOKE_OUT");
    const char *options[] = {"First", "Second", "Third"};
    char field[64] = "TaijiOS";
    int cursor = (int)strlen(field);
    int focused = 0;
    int slider = 42;
    int checked = 1;
    int selected = 1;
    int frame;
    Image shot;
    unsigned char sprite_pixels[8] = {
        220, 20, 20, 255,
        20, 220, 40, 255
    };
    unsigned char alpha_pixels[4] = {0, 240, 0, 128};
    unsigned char mask_pixels[4] = {0, 0, 0, 255};
    unsigned char icon_pixels[8] = {
        0, 0, 0, 255,
        255, 255, 255, 255
    };
    unsigned char quad_pixels[16] = {
        240, 20, 20, 255,
        20, 220, 40, 255,
        20, 40, 230, 255,
        245, 245, 245, 255
    };
    Image sprite = {sprite_pixels, 2, 1, 1,
                    PIXELFORMAT_UNCOMPRESSED_R8G8B8A8};
    Image alpha = {alpha_pixels, 1, 1, 1,
                   PIXELFORMAT_UNCOMPRESSED_R8G8B8A8};
    Image mask = {mask_pixels, 1, 1, 1,
                  PIXELFORMAT_UNCOMPRESSED_R8G8B8A8};
    Image icon = {icon_pixels, 2, 1, 1,
                  PIXELFORMAT_UNCOMPRESSED_R8G8B8A8};
    Image quad = {quad_pixels, 2, 2, 1,
                  PIXELFORMAT_UNCOMPRESSED_R8G8B8A8};
    Texture2D sprite_texture;
    Texture2D alpha_texture;
    Texture2D mask_texture;
    Texture2D icon_texture;
    Texture2D quad_texture;
    RenderTexture2D target;
    Font atlas_font;
    Image atlas_image;
    int atlas_codepoints[3] = {'?', 'A', 'm'};

    if(out == NULL || out[0] == '\0')
        out = "/tmp/kryon-libdraw-smoke.png";

    InitWindow(360, 260, "Kryon libdraw smoke");
    check("window ready", IsWindowReady());
    if(!IsWindowReady())
        return 2;

    SetTargetFPS(60);
    sprite_texture = LoadTextureFromImage(sprite);
    alpha_texture = LoadTextureFromImage(alpha);
    mask_texture = LoadTextureFromImage(mask);
    icon_texture = LoadTextureFromImage(icon);
    kry_libdraw_texture_preserve_colors(icon_texture);
    quad_texture = LoadTextureFromImage(quad);
    target = LoadRenderTexture(12, 12);
    atlas_font = LoadFontEx("fonts/noto/NotoSans-Regular.ttf", 24,
                            atlas_codepoints, 3);
    check("sprite texture", sprite_texture.id != 0);
    check("alpha texture", alpha_texture.id != 0);
    check("mask texture", mask_texture.id != 0);
    check("icon texture", icon_texture.id != 0);
    check("quad texture", quad_texture.id != 0);
    check("render texture", target.texture.id != 0);
    check("font atlas", IsFontValid(atlas_font));
    check("font metrics", GetGlyphInfo(atlas_font, 'm').advanceX >
                              GetGlyphInfo(GetFontDefault(), 'm').advanceX);
    atlas_image = LoadImageFromTexture(atlas_font.texture);
    if(atlas_image.data != NULL) {
        Rectangle rec = GetGlyphAtlasRec(atlas_font, 'A');
        int opaque = 0;
        int antialias = 0;
        int transparent = 0;
        int y;

        for(y = 0; y < (int)rec.height; y++) {
            int x;

            for(x = 0; x < (int)rec.width; x++) {
                unsigned char *px = (unsigned char *)atlas_image.data +
                                    ((size_t)((int)rec.y + y) *
                                         atlas_image.width +
                                     (int)rec.x + x) * 4;
                if(px[3] > 200)
                    opaque = 1;
                if(px[3] > 0 && px[3] < 255)
                    antialias = 1;
                if(px[3] == 0)
                    transparent = 1;
            }
        }
        check("font atlas glyph coverage", opaque && antialias && transparent);
    }
    UnloadImage(atlas_image);
    for(frame = 0; frame < 3 && !WindowShouldClose(); frame++) {
        BeginTextureMode(target);
        ClearBackground(BLANK);
        DrawRectangle(0, 0, 12, 12, (Color){36, 90, 210, 255});
        EndTextureMode();

        BeginDrawing();
        ClearBackground((Color){18, 20, 24, 255});
        DrawRectangle(12, 12, 120, 42, (Color){42, 84, 132, 255});
        DrawRectangleLinesEx((Rectangle){12, 12, 120, 42}, 3.0f,
                             (Color){230, 230, 236, 255});
        DrawCircle(292, 46, 24, (Color){0, 190, 120, 255});
        DrawLine(0, 0, GetScreenWidth(), GetScreenHeight(),
                 (Color){180, 60, 70, 255});
        DrawRectangleGradientV(260, 92, 72, 92,
                               (Color){32, 52, 96, 255},
                               (Color){126, 58, 45, 255});
        DrawRectangleRounded((Rectangle){178, 14, 48, 32}, 0.5f, 12,
                             (Color){210, 170, 22, 255});
        DrawTexturePro(icon_texture, (Rectangle){0, 0, 2, 1},
                       (Rectangle){232, 18, 24, 12}, (Vector2){0, 0},
                       0.0f, (Color){255, 0, 0, 255});
        DrawLineEx((Vector2){220, 206}, (Vector2){270, 206}, 8.0f,
                   (Color){235, 56, 62, 255});
        DrawRectangle(286, 206, 20, 20, (Color){0, 0, 0, 255});
        DrawTexturePro(alpha_texture, (Rectangle){0, 0, 1, 1},
                       (Rectangle){286, 206, 20, 20}, (Vector2){0, 0},
                       0.0f, WHITE);
        DrawTexturePro(mask_texture, (Rectangle){0, 0, 1, 1},
                       (Rectangle){142, 206, 12, 12}, (Vector2){0, 0},
                       0.0f, (Color){245, 40, 60, 255});
        DrawTexturePro(sprite_texture, (Rectangle){1, 0, 1, 1},
                       (Rectangle){304, 206, 24, 20},
                       (Vector2){0, 0}, 0.0f, WHITE);
        DrawTexturePro(quad_texture, (Rectangle){0, 0, 2, 2},
                       (Rectangle){80, 206, 20, 20},
                       (Vector2){10, 10}, 90.0f, WHITE);
        DrawTexturePro(quad_texture, (Rectangle){0, 0, 2, 0.5f},
                       (Rectangle){116, 206, 20, 4},
                       (Vector2){0, 0}, 0.0f, WHITE);
        DrawTexturePro(target.texture, (Rectangle){0, 0, 12, 12},
                       (Rectangle){336, 206, 12, 12},
                       (Vector2){0, 0}, 0.0f, WHITE);
        DrawText("libdraw backend", 18, 68, 16,
                 (Color){242, 242, 246, 255});

        BeginUI(0x19c01);
        Button((ButtonProps){.bounds = {18, 96, 96, 34},
                             .label = "Button",
                             .style = ButtonStylePrimary,
                             .font = Text16,
                             .id = 1001});
        TextField((TextFieldProps){.bounds = {124, 96, 138, 34},
                                   .text = field,
                                   .text_size = sizeof(field),
                                   .cursor_position = &cursor,
                                   .focused = &focused,
                                   .max_codepoints = 63,
                                   .font = Text16,
                                   .focus_id = 1002});
        Dropdown(1003, 18, 140, 150, 34, options, 3, &selected);
        Slider(1004, 18, 188, 160, "Value", 0, 100, &slider, "%", NULL);
        Checkbox(1005, 198, 188, "Checked", &checked);
        EndUI();

        EndDrawing();
    }

    shot = LoadImageFromScreen();
    check("screen capture data", shot.data != NULL);
    check("screen capture dimensions",
          shot.width == GetRenderWidth() && shot.height == GetRenderHeight());
    if(shot.data != NULL && shot.width > 348 && shot.height > 226) {
        unsigned char *px = (unsigned char *)shot.data +
                            ((size_t)216 * shot.width + 318) * 4;
        unsigned char *rounded_corner = (unsigned char *)shot.data +
                                        ((size_t)14 * shot.width + 178) * 4;
        unsigned char *rounded_top_edge = (unsigned char *)shot.data +
                                          ((size_t)14 * shot.width + 188) * 4;
        unsigned char *rounded_center = (unsigned char *)shot.data +
                                        ((size_t)30 * shot.width + 202) * 4;
        unsigned char *icon_black_px = (unsigned char *)shot.data +
                                       ((size_t)24 * shot.width + 238) * 4;
        unsigned char *icon_white_px = (unsigned char *)shot.data +
                                       ((size_t)24 * shot.width + 250) * 4;
        unsigned char *line_px = (unsigned char *)shot.data +
                                 ((size_t)209 * shot.width + 245) * 4;
        unsigned char *alpha_px = (unsigned char *)shot.data +
                                  ((size_t)216 * shot.width + 296) * 4;
        unsigned char *rotated_px = (unsigned char *)shot.data +
                                    ((size_t)201 * shot.width + 85) * 4;
        unsigned char *fractional_px = (unsigned char *)shot.data +
                                       ((size_t)207 * shot.width + 121) * 4;
        unsigned char *mask_px = (unsigned char *)shot.data +
                                 ((size_t)211 * shot.width + 148) * 4;
        unsigned char *target_px = (unsigned char *)shot.data +
                                   ((size_t)212 * shot.width + 342) * 4;

        check("texture source rectangle green pixel",
              px[1] > 180 && px[0] < 80 && px[2] < 100);
        check("rounded rectangle corner clipped",
              !(rounded_corner[0] > 180 && rounded_corner[1] > 130));
        check("rounded rectangle radius matches raylib",
              rounded_top_edge[0] > 180 && rounded_top_edge[1] > 130);
        check("rounded rectangle center filled",
              rounded_center[0] > 180 && rounded_center[1] > 130);
        check("color-preserved icon keeps black",
              icon_black_px[0] < 40 && icon_black_px[1] < 40 &&
                  icon_black_px[2] < 40);
        check("color-preserved icon keeps white",
              icon_white_px[0] > 220 && icon_white_px[1] > 220 &&
                  icon_white_px[2] > 220);
        check("thick line covers radius",
              line_px[0] > 180 && line_px[1] < 100 && line_px[2] < 100);
        check("texture alpha blended",
              alpha_px[1] > 80 && alpha_px[1] < 180 && alpha_px[0] < 20);
        check("rotated texture samples expected source pixel",
              rotated_px[0] > 180 && rotated_px[1] < 80 && rotated_px[2] < 80);
        check("fractional source rectangle draws",
              fractional_px[0] > 180 && fractional_px[1] < 80 &&
                  fractional_px[2] < 80);
        check("monochrome mask texture tints",
              mask_px[0] > 180 && mask_px[1] < 90 && mask_px[2] < 110);
        check("render texture draws back to window",
              target_px[2] > 160 && target_px[0] < 90);
    }
    check("export image", ExportImage(shot, out));
    UnloadImage(shot);
    UnloadRenderTexture(target);
    UnloadFont(atlas_font);
    UnloadTexture(quad_texture);
    UnloadTexture(icon_texture);
    UnloadTexture(mask_texture);
    UnloadTexture(alpha_texture);
    UnloadTexture(sprite_texture);
    check("export exists", FileExists(out));

    CloseWindow();
    return failures == 0 ? 0 : 1;
}
